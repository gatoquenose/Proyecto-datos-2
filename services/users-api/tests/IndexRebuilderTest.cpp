#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "TempFile.hpp"
#include "storage/BTree.hpp"
#include "storage/FilePageStore.hpp"
#include "storage/FileUserRepository.hpp"
#include "storage/IndexRebuilder.hpp"
#include "storage/MemoryPageStore.hpp"
#include "storage/UserFile.hpp"

namespace tecflix::users {

using testing::TempFile;

namespace {

constexpr std::uint64_t kQuota = 5'000'000'000ULL;
constexpr std::uint64_t kUserCount = 40;  // más de una página de índice: obliga a crear nodos internos

User makeUser(std::uint64_t number) {
    const std::string suffix = std::to_string(number);
    return User{"id-" + suffix, "user" + suffix, "user" + suffix + "@tec.cr", "hash" + suffix, kQuota, 0};
}

// users.dat e users.idx reales en archivos temporales, con la lógica del repositorio.
class IndexRebuilderTest : public ::testing::Test {
protected:
    IndexRebuilderTest()
        : data_(name("dat")), index_(name("idx")), rebuilt_(name("idx.rebuild")) {}

    static std::string name(const std::string& extension) {
        return std::string{"rebuild_"} + ::testing::UnitTest::GetInstance()->current_test_info()->name() + "." + extension;
    }

    void fillWithUsers(std::uint64_t count) {
        UserFile file{data_.path()};
        FilePageStore pages{index_.path()};
        BTree tree{pages};
        FileUserRepository repository{file, tree};
        for (std::uint64_t i = 1; i <= count; ++i) {
            ASSERT_EQ(repository.add(makeUser(i)), AddResult::Added);
        }
    }

    // Todos los usuarios deben encontrarse por sus tres claves usando solo el índice reconstruido.
    void expectEveryUserFound(std::uint64_t first, std::uint64_t last) const {
        UserFile file{data_.path()};
        FilePageStore pages{index_.path()};
        BTree tree{pages};
        const FileUserRepository repository{file, tree};
        for (std::uint64_t i = first; i <= last; ++i) {
            const User expected = makeUser(i);
            ASSERT_TRUE(repository.findById(expected.id)) << expected.id;
            EXPECT_EQ(repository.findByUsername(expected.username)->id, expected.id);
            EXPECT_EQ(repository.findByEmail(expected.email)->id, expected.id);
        }
    }

    void damageByteOf(const std::filesystem::path& path, std::streamoff position) const {
        std::fstream file{path, std::ios::in | std::ios::out | std::ios::binary};
        file.seekg(position);
        char byte = 0;
        file.get(byte);
        byte = static_cast<char>(byte ^ 0x20);
        file.seekp(position);
        file.put(byte);
    }

    TempFile data_;
    TempFile index_;
    TempFile rebuilt_;
};

}  // namespace

TEST_F(IndexRebuilderTest, RebuildsFromScratchIntoAnEmptyStore) {
    fillWithUsers(kUserCount);
    const UserFile file{data_.path()};
    MemoryPageStore fresh;

    const RebuildReport report = rebuildUserIndex(file, fresh);

    EXPECT_EQ(report.recordsScanned, kUserCount);
    EXPECT_EQ(report.indexed, kUserCount);
    EXPECT_EQ(report.deletedSkipped, 0U);
    EXPECT_EQ(report.corruptSkipped, 0U);
    BTree tree{fresh};
    EXPECT_TRUE(userIndexIsConsistent(file, tree));
}

TEST_F(IndexRebuilderTest, DeletedUsersAreNotIndexedAgain) {
    fillWithUsers(10);
    {
        UserFile file{data_.path()};
        FilePageStore pages{index_.path()};
        BTree tree{pages};
        FileUserRepository repository{file, tree};
        ASSERT_TRUE(repository.remove("id-3"));
        ASSERT_TRUE(repository.remove("id-7"));
    }

    const RebuildReport report = rebuildUserIndexFile(data_.path(), index_.path());

    EXPECT_EQ(report.deletedSkipped, 2U);
    EXPECT_EQ(report.indexed, 8U);
    UserFile file{data_.path()};
    FilePageStore pages{index_.path()};
    BTree tree{pages};
    const FileUserRepository repository{file, tree};
    EXPECT_FALSE(repository.findById("id-3"));
    EXPECT_TRUE(repository.findById("id-4"));
}

TEST_F(IndexRebuilderTest, RebuildReplacesTheIndexFileAndLeavesNoTemporary) {
    fillWithUsers(kUserCount);
    damageByteOf(index_.path(), 5000);  // una página del índice queda dañada

    rebuildUserIndexFile(data_.path(), index_.path());

    EXPECT_FALSE(std::filesystem::exists(rebuilt_.path()));
    expectEveryUserFound(1, kUserCount);
}

TEST_F(IndexRebuilderTest, ACorruptRecordIsSkippedAndCounted) {
    fillWithUsers(5);
    // El registro 2 empieza en 64 + 1 * 384; se daña un byte de su contenido.
    damageByteOf(data_.path(), 64 + 384 + 10);

    const RebuildReport report = rebuildUserIndexFile(data_.path(), index_.path());

    EXPECT_EQ(report.corruptSkipped, 1U);
    EXPECT_EQ(report.indexed, 4U);
}

TEST_F(IndexRebuilderTest, TheFirstRecordWinsWhenTwoShareAUsername) {
    {
        UserFile file{data_.path()};
        file.append(makeUser(1));
        User clash = makeUser(2);
        clash.username = "user1";  // dos registros activos con el mismo username
        file.append(clash);
    }

    const RebuildReport report = rebuildUserIndexFile(data_.path(), index_.path());

    EXPECT_EQ(report.indexed, 1U);
    EXPECT_EQ(report.duplicatesSkipped, 1U);
}

TEST_F(IndexRebuilderTest, HealthyIndexIsLeftAlone) {
    fillWithUsers(kUserCount);

    EXPECT_FALSE(ensureUserIndex(data_.path(), index_.path()).has_value());
}

TEST_F(IndexRebuilderTest, MissingIndexIsRebuiltAtStartup) {
    fillWithUsers(kUserCount);
    std::filesystem::remove(index_.path());

    const auto report = ensureUserIndex(data_.path(), index_.path());

    ASSERT_TRUE(report.has_value());
    EXPECT_EQ(report->indexed, kUserCount);
    expectEveryUserFound(1, kUserCount);
}

TEST_F(IndexRebuilderTest, DamagedIndexPageIsRebuiltAtStartup) {
    fillWithUsers(kUserCount);
    damageByteOf(index_.path(), 4096 + 100);  // dentro de la página 1

    const auto report = ensureUserIndex(data_.path(), index_.path());

    ASSERT_TRUE(report.has_value());
    expectEveryUserFound(1, kUserCount);
}

TEST_F(IndexRebuilderTest, CrashBetweenWritingTheRecordAndIndexingItIsRecovered) {
    fillWithUsers(5);
    {
        UserFile file{data_.path()};
        file.append(makeUser(6));  // el registro llegó a disco, el índice no
    }

    const auto report = ensureUserIndex(data_.path(), index_.path());

    ASSERT_TRUE(report.has_value());
    EXPECT_EQ(report->indexed, 6U);
    expectEveryUserFound(1, 6);
}

TEST_F(IndexRebuilderTest, AnEmptyDatabaseNeedsNoRebuild) {
    EXPECT_FALSE(ensureUserIndex(data_.path(), index_.path()).has_value());
}

TEST_F(IndexRebuilderTest, UnreadableUsersFileIsReportedNotHidden) {
    {
        std::ofstream broken{data_.path(), std::ios::binary};
        broken << "esto no es un users.dat";
    }

    EXPECT_THROW(ensureUserIndex(data_.path(), index_.path()), std::runtime_error);
}

}  // namespace tecflix::users
