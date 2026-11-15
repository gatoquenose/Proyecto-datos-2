#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

#include "TempFile.hpp"
#include "storage/BTree.hpp"
#include "storage/FilePageStore.hpp"
#include "storage/FileUserRepository.hpp"
#include "storage/MemoryPageStore.hpp"
#include "storage/UserFile.hpp"

namespace tecflix::users {

using testing::TempFile;

namespace {

constexpr std::uint64_t kQuota = 5'000'000'000ULL;

User makeUser(int number) {
    const std::string suffix = std::to_string(number);
    return User{"id-" + suffix, "user" + suffix, "user" + suffix + "@tec.cr", "hash" + suffix, kQuota, 0};
}

// Repositorio con índice en memoria y users.dat en un archivo temporal.
class FileUserRepositoryTest : public ::testing::Test {
protected:
    FileUserRepositoryTest()
        : temp_(std::string{"repo_"} + ::testing::UnitTest::GetInstance()->current_test_info()->name() + ".dat"),
          file_(temp_.path()),
          index_(pages_),
          repository_(file_, index_) {}

    TempFile temp_;
    UserFile file_;
    MemoryPageStore pages_;
    BTree index_;
    FileUserRepository repository_;
};

}  // namespace

TEST_F(FileUserRepositoryTest, AddedUserIsFoundByIdUsernameAndEmail) {
    ASSERT_EQ(repository_.add(makeUser(1)), AddResult::Added);

    EXPECT_EQ(repository_.findById("id-1")->username, "user1");
    EXPECT_EQ(repository_.findByUsername("user1")->id, "id-1");
    EXPECT_EQ(repository_.findByEmail("user1@tec.cr")->id, "id-1");
}

TEST_F(FileUserRepositoryTest, UnknownUserIsNotFound) {
    EXPECT_FALSE(repository_.findById("nadie").has_value());
    EXPECT_FALSE(repository_.findByUsername("nadie").has_value());
    EXPECT_FALSE(repository_.findByEmail("nadie@tec.cr").has_value());
}

TEST_F(FileUserRepositoryTest, SearchesIgnoreCaseButKeepTheOriginalSpelling) {
    User user = makeUser(1);
    user.username = "Alice";
    user.email = "Alice@Tec.cr";
    repository_.add(user);

    EXPECT_EQ(repository_.findByUsername("ALICE")->username, "Alice");
    EXPECT_EQ(repository_.findByEmail("alice@TEC.CR")->email, "Alice@Tec.cr");
}

TEST_F(FileUserRepositoryTest, RejectsDuplicatesWithoutStoringAnything) {
    repository_.add(makeUser(1));
    User sameUsername = makeUser(2);
    sameUsername.username = "USER1";
    User sameEmail = makeUser(3);
    sameEmail.email = "user1@tec.cr";
    User sameId = makeUser(4);
    sameId.id = "id-1";

    EXPECT_EQ(repository_.add(sameUsername), AddResult::UsernameTaken);
    EXPECT_EQ(repository_.add(sameEmail), AddResult::EmailTaken);
    EXPECT_EQ(repository_.add(sameId), AddResult::IdTaken);

    EXPECT_EQ(file_.recordCount(), 1U);
    EXPECT_FALSE(repository_.findById("id-2").has_value());
    EXPECT_FALSE(repository_.findByUsername("user3").has_value());
}

TEST_F(FileUserRepositoryTest, RejectsInvalidUsers) {
    User empty = makeUser(1);
    empty.username.clear();
    User tooLong = makeUser(2);
    tooLong.email = std::string(kMaxEmailLength + 1, 'x');

    EXPECT_THROW(repository_.add(empty), std::invalid_argument);
    EXPECT_THROW(repository_.add(tooLong), std::invalid_argument);
}

TEST_F(FileUserRepositoryTest, UpdateChangesUsedBytes) {
    repository_.add(makeUser(1));
    User changed = makeUser(1);
    changed.usedBytes = 4096;

    EXPECT_EQ(repository_.update(changed), UpdateResult::Updated);

    EXPECT_EQ(repository_.findById("id-1")->usedBytes, 4096U);
}

TEST_F(FileUserRepositoryTest, UpdateMovesTheEmailInTheIndex) {
    repository_.add(makeUser(1));
    User changed = makeUser(1);
    changed.email = "nuevo@tec.cr";

    EXPECT_EQ(repository_.update(changed), UpdateResult::Updated);

    EXPECT_EQ(repository_.findByEmail("nuevo@tec.cr")->id, "id-1");
    EXPECT_FALSE(repository_.findByEmail("user1@tec.cr").has_value());
}

TEST_F(FileUserRepositoryTest, UpdateRejectsEmailOfAnotherUser) {
    repository_.add(makeUser(1));
    repository_.add(makeUser(2));
    User changed = makeUser(2);
    changed.email = "user1@tec.cr";

    EXPECT_EQ(repository_.update(changed), UpdateResult::EmailTaken);

    EXPECT_EQ(repository_.findById("id-2")->email, "user2@tec.cr");
}

TEST_F(FileUserRepositoryTest, UsernameAndQuotaCannotBeChanged) {
    repository_.add(makeUser(1));
    User newUsername = makeUser(1);
    newUsername.username = "otro";
    User newQuota = makeUser(1);
    newQuota.quotaBytes = kQuota * 2;

    EXPECT_EQ(repository_.update(newUsername), UpdateResult::ImmutableFieldChanged);
    EXPECT_EQ(repository_.update(newQuota), UpdateResult::ImmutableFieldChanged);

    EXPECT_EQ(repository_.findById("id-1")->username, "user1");
    EXPECT_EQ(repository_.findById("id-1")->quotaBytes, kQuota);
}

TEST_F(FileUserRepositoryTest, UpdateOfUnknownUserReportsNotFound) {
    EXPECT_EQ(repository_.update(makeUser(9)), UpdateResult::NotFound);
}

TEST_F(FileUserRepositoryTest, RemovedUserDisappearsFromEverySearch) {
    repository_.add(makeUser(1));

    EXPECT_TRUE(repository_.remove("id-1"));

    EXPECT_FALSE(repository_.findById("id-1").has_value());
    EXPECT_FALSE(repository_.findByUsername("user1").has_value());
    EXPECT_FALSE(repository_.findByEmail("user1@tec.cr").has_value());
    EXPECT_FALSE(repository_.remove("id-1"));
}

TEST_F(FileUserRepositoryTest, UsernameAndEmailCanBeReusedAfterRemoval) {
    repository_.add(makeUser(1));
    repository_.remove("id-1");
    User newcomer = makeUser(1);
    newcomer.id = "id-nuevo";

    EXPECT_EQ(repository_.add(newcomer), AddResult::Added);

    EXPECT_EQ(repository_.findByUsername("user1")->id, "id-nuevo");
    EXPECT_EQ(repository_.findByEmail("user1@tec.cr")->id, "id-nuevo");
}

TEST_F(FileUserRepositoryTest, HandlesManyUsers) {
    constexpr int kCount = 1500;
    for (int number = 0; number < kCount; ++number) {
        ASSERT_EQ(repository_.add(makeUser(number)), AddResult::Added);
    }

    for (int number = 0; number < kCount; ++number) {
        ASSERT_EQ(repository_.findByEmail("user" + std::to_string(number) + "@tec.cr")->id,
                  "id-" + std::to_string(number));
    }
}

TEST(FileUserRepositoryPersistenceTest, DataSurvivesRestart) {
    const TempFile dataFile{"persist_users.dat"};
    const TempFile indexFile{"persist_users.idx"};
    {
        UserFile file{dataFile.path()};
        FilePageStore pages{indexFile.path()};
        BTree index{pages};
        FileUserRepository repository{file, index};
        repository.add(makeUser(1));
        repository.add(makeUser(2));
        repository.remove("id-1");
    }

    UserFile file{dataFile.path()};
    FilePageStore pages{indexFile.path()};
    BTree index{pages};
    const FileUserRepository repository{file, index};

    EXPECT_FALSE(repository.findByUsername("user1").has_value());
    EXPECT_EQ(repository.findByUsername("user2")->id, "id-2");
}

}  // namespace tecflix::users
