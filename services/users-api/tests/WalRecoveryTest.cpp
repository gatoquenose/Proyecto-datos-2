#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

#include "TempFile.hpp"
#include "storage/BTree.hpp"
#include "storage/FilePageStore.hpp"
#include "storage/FileUserRepository.hpp"
#include "storage/FileWriteAheadLog.hpp"
#include "storage/IndexRebuilder.hpp"
#include "storage/MemoryPageStore.hpp"
#include "storage/MemoryWriteAheadLog.hpp"
#include "storage/UserFile.hpp"
#include "storage/UserIndexKeys.hpp"

namespace tecflix::users {

using testing::TempFile;

namespace {

constexpr std::uint64_t kQuota = 5'000'000'000ULL;

User makeUser(int number) {
    const std::string suffix = std::to_string(number);
    return User{"id-" + suffix, "user" + suffix, "user" + suffix + "@tec.cr", "hash" + suffix, kQuota, 0};
}

RecordBuffer imageOf(const User& user, RecordStatus status = RecordStatus::Active) {
    RecordBuffer image{};
    UserRecordCodec::encode(user, status, image);
    return image;
}

// Simula caídas: se anota la operación en el WAL y se aplican a mano solo algunos de sus pasos,
// como habría quedado el disco si el proceso hubiera muerto en ese punto.
class WalRecoveryTest : public ::testing::Test {
protected:
    WalRecoveryTest()
        : temp_(std::string{"walrec_"} + ::testing::UnitTest::GetInstance()->current_test_info()->name() + ".dat"),
          file_(temp_.path()),
          index_(pages_),
          repository_(file_, index_, wal_) {}

    void addNormally(int number) { ASSERT_EQ(repository_.add(makeUser(number)), AddResult::Added); }

    std::uint64_t offsetOf(int number) const { return *index_.find(idKey(makeUser(number).id)); }

    void expectFullyIndexed(int number) const {
        const User user = makeUser(number);
        ASSERT_TRUE(repository_.findById(user.id)) << user.id;
        EXPECT_EQ(repository_.findByUsername(user.username)->id, user.id);
        EXPECT_EQ(repository_.findByEmail(user.email)->id, user.id);
    }

    void expectGone(int number) const {
        const User user = makeUser(number);
        EXPECT_FALSE(repository_.findById(user.id));
        EXPECT_FALSE(repository_.findByUsername(user.username));
        EXPECT_FALSE(repository_.findByEmail(user.email));
    }

    TempFile temp_;
    UserFile file_;
    MemoryPageStore pages_;
    BTree index_;
    MemoryWriteAheadLog wal_;
    FileUserRepository repository_;
};

}  // namespace

TEST_F(WalRecoveryTest, NormalOperationsLeaveNothingPending) {
    addNormally(1);
    ASSERT_EQ(repository_.update(User{"id-1", "user1", "nuevo@tec.cr", "hash1", kQuota, 7}), UpdateResult::Updated);
    ASSERT_TRUE(repository_.remove("id-1"));

    EXPECT_FALSE(wal_.pending().has_value());
    EXPECT_FALSE(repository_.recoverPending());
}

// ---- Add ----

TEST_F(WalRecoveryTest, AddLoggedButNothingApplied) {
    const User user = makeUser(1);
    wal_.log({WalOp::Add, file_.nextOffset(), RecordBuffer{}, imageOf(user)});

    EXPECT_TRUE(repository_.recoverPending());

    expectFullyIndexed(1);
    EXPECT_EQ(file_.recordCount(), 1U);
    EXPECT_FALSE(wal_.pending().has_value());
}

TEST_F(WalRecoveryTest, AddWithTheRecordWrittenButNotIndexed) {
    addNormally(1);
    const User user = makeUser(2);
    const std::uint64_t offset = file_.nextOffset();
    wal_.log({WalOp::Add, offset, RecordBuffer{}, imageOf(user)});
    file_.appendImage(imageOf(user));  // se llegó a escribir el registro

    EXPECT_TRUE(repository_.recoverPending());

    expectFullyIndexed(2);
    EXPECT_EQ(file_.recordCount(), 2U) << "no debe duplicar el registro";
}

TEST_F(WalRecoveryTest, AddWithOnlySomeKeysIndexed) {
    const User user = makeUser(1);
    const std::uint64_t offset = file_.nextOffset();
    wal_.log({WalOp::Add, offset, RecordBuffer{}, imageOf(user)});
    file_.appendImage(imageOf(user));
    index_.insert(idKey(user.id), offset);  // cayó antes de indexar username y email

    EXPECT_TRUE(repository_.recoverPending());

    expectFullyIndexed(1);
    EXPECT_EQ(file_.recordCount(), 1U);
}

TEST_F(WalRecoveryTest, AddThatHadAlreadyFinishedIsHarmlessToRedo) {
    addNormally(1);
    const std::uint64_t offset = offsetOf(1);
    wal_.log({WalOp::Add, offset, RecordBuffer{}, imageOf(makeUser(1))});  // cayó justo antes de borrar la anotación

    EXPECT_TRUE(repository_.recoverPending());

    expectFullyIndexed(1);
    EXPECT_EQ(file_.recordCount(), 1U);
}

// ---- Update ----

TEST_F(WalRecoveryTest, UpdateLoggedButNothingApplied) {
    addNormally(1);
    const std::uint64_t offset = offsetOf(1);
    User changed = makeUser(1);
    changed.email = "otro@tec.cr";
    changed.usedBytes = 99;
    wal_.log({WalOp::Update, offset, file_.readImage(offset), imageOf(changed)});

    EXPECT_TRUE(repository_.recoverPending());

    EXPECT_EQ(repository_.findByEmail("otro@tec.cr")->usedBytes, 99U);
    EXPECT_FALSE(repository_.findByEmail("user1@tec.cr"));
    EXPECT_EQ(repository_.findById("id-1")->email, "otro@tec.cr");
}

TEST_F(WalRecoveryTest, UpdateWithTheRecordWrittenButTheEmailKeyNotMoved) {
    addNormally(1);
    const std::uint64_t offset = offsetOf(1);
    User changed = makeUser(1);
    changed.email = "otro@tec.cr";
    wal_.log({WalOp::Update, offset, file_.readImage(offset), imageOf(changed)});
    file_.writeImage(offset, imageOf(changed));

    EXPECT_TRUE(repository_.recoverPending());

    EXPECT_EQ(repository_.findByEmail("otro@tec.cr")->id, "id-1");
    EXPECT_FALSE(repository_.findByEmail("user1@tec.cr"));
}

TEST_F(WalRecoveryTest, UpdateWithTheOldKeyDroppedButTheNewOneMissing) {
    addNormally(1);
    const std::uint64_t offset = offsetOf(1);
    User changed = makeUser(1);
    changed.email = "otro@tec.cr";
    wal_.log({WalOp::Update, offset, file_.readImage(offset), imageOf(changed)});
    file_.writeImage(offset, imageOf(changed));
    index_.erase(emailKey("user1@tec.cr"));  // el usuario quedaría sin búsqueda por email

    EXPECT_TRUE(repository_.recoverPending());

    EXPECT_EQ(repository_.findByEmail("otro@tec.cr")->id, "id-1");
}

// ---- Remove ----

TEST_F(WalRecoveryTest, RemoveLoggedButNothingApplied) {
    addNormally(1);
    addNormally(2);
    const std::uint64_t offset = offsetOf(1);
    RecordBuffer deleted = file_.readImage(offset);
    UserRecordCodec::setStatus(deleted, RecordStatus::Deleted);
    wal_.log({WalOp::Remove, offset, file_.readImage(offset), deleted});

    EXPECT_TRUE(repository_.recoverPending());

    expectGone(1);
    expectFullyIndexed(2);
}

TEST_F(WalRecoveryTest, RemoveWithTheKeysErasedButTheRecordStillActive) {
    // El caso que sin WAL "resucitaba" al usuario al reconstruir el índice.
    addNormally(1);
    const std::uint64_t offset = offsetOf(1);
    RecordBuffer deleted = file_.readImage(offset);
    UserRecordCodec::setStatus(deleted, RecordStatus::Deleted);
    wal_.log({WalOp::Remove, offset, file_.readImage(offset), deleted});
    index_.erase(idKey("id-1"));
    index_.erase(usernameKey("user1"));
    index_.erase(emailKey("user1@tec.cr"));

    EXPECT_TRUE(repository_.recoverPending());

    expectGone(1);
    int activeRecords = 0;
    file_.forEachActive([&activeRecords](const User&) { ++activeRecords; });
    EXPECT_EQ(activeRecords, 0);
}

TEST_F(WalRecoveryTest, RemovedUsernameCanBeRegisteredAgainAfterRecovery) {
    addNormally(1);
    const std::uint64_t offset = offsetOf(1);
    RecordBuffer deleted = file_.readImage(offset);
    UserRecordCodec::setStatus(deleted, RecordStatus::Deleted);
    wal_.log({WalOp::Remove, offset, file_.readImage(offset), deleted});
    ASSERT_TRUE(repository_.recoverPending());

    User again = makeUser(1);
    again.id = "id-nuevo";

    EXPECT_EQ(repository_.add(again), AddResult::Added);
}

// ---- Reglas del WAL ----

TEST_F(WalRecoveryTest, RefusesNewWritesWhileAnOperationIsPending) {
    wal_.log({WalOp::Add, file_.nextOffset(), RecordBuffer{}, imageOf(makeUser(1))});

    EXPECT_THROW(repository_.add(makeUser(2)), std::logic_error);
    EXPECT_FALSE(repository_.findById("id-2"));
}

TEST_F(WalRecoveryTest, RecoveryAcceptsNewWritesAgain) {
    wal_.log({WalOp::Add, file_.nextOffset(), RecordBuffer{}, imageOf(makeUser(1))});
    ASSERT_TRUE(repository_.recoverPending());

    EXPECT_EQ(repository_.add(makeUser(2)), AddResult::Added);
}

TEST_F(WalRecoveryTest, AnEntryFromAnUnknownOperationIsRejected) {
    WalEntry broken;
    broken.op = static_cast<WalOp>(9);
    MemoryWriteAheadLog log;
    log.log(broken);
    FileUserRepository repository{file_, index_, log};

    EXPECT_THROW(repository.recoverPending(), std::logic_error);
}

// ---- Con archivos reales y reinicio completo ----

TEST(WalRestartTest, CrashedProcessIsRecoveredByTheNextOne) {
    TempFile data{"walrestart.dat"};
    TempFile idx{"walrestart.idx"};
    TempFile wal{"walrestart.wal"};
    const User user = makeUser(1);
    {
        UserFile file{data.path()};
        FilePageStore pages{idx.path()};
        BTree index{pages};
        FileWriteAheadLog log{wal.path()};
        FileUserRepository repository{file, index, log};
        ASSERT_EQ(repository.add(makeUser(9)), AddResult::Added);
        log.log({WalOp::Add, file.nextOffset(), RecordBuffer{}, imageOf(user)});
        file.appendImage(imageOf(user));  // el proceso muere aquí: registro escrito, índice sin tocar
    }

    UserFile file{data.path()};
    FilePageStore pages{idx.path()};
    BTree index{pages};
    FileWriteAheadLog log{wal.path()};
    FileUserRepository repository{file, index, log};

    EXPECT_TRUE(repository.recoverPending());
    EXPECT_EQ(repository.findByUsername("user1")->id, "id-1");
    EXPECT_EQ(repository.findByUsername("user9")->id, "id-9");
    EXPECT_FALSE(repository.recoverPending());
}

TEST(WalRestartTest, IndexRebuildFollowedByRecoveryGivesTheSameResult) {
    TempFile data{"walrebuild.dat"};
    TempFile idx{"walrebuild.idx"};
    TempFile wal{"walrebuild.wal"};
    const User user = makeUser(1);
    {
        UserFile file{data.path()};
        FilePageStore pages{idx.path()};
        BTree index{pages};
        FileWriteAheadLog log{wal.path()};
        log.log({WalOp::Add, file.nextOffset(), RecordBuffer{}, imageOf(user)});
        file.appendImage(imageOf(user));
    }

    EXPECT_TRUE(ensureUserIndex(data.path(), idx.path()).has_value());  // el arranque reconstruye primero

    UserFile file{data.path()};
    FilePageStore pages{idx.path()};
    BTree index{pages};
    FileWriteAheadLog log{wal.path()};
    FileUserRepository repository{file, index, log};
    EXPECT_TRUE(repository.recoverPending());
    EXPECT_EQ(file.recordCount(), 1U);
    EXPECT_EQ(repository.findById("id-1")->username, "user1");
}

}  // namespace tecflix::users
