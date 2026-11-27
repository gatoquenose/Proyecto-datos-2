#include <gtest/gtest.h>

#include <fstream>
#include <stdexcept>

#include "TempFile.hpp"
#include "storage/FileWriteAheadLog.hpp"
#include "storage/MemoryWriteAheadLog.hpp"

namespace tecflix::users {

using testing::TempFile;

namespace {

WalEntry sampleEntry(WalOp op = WalOp::Add, std::uint64_t offset = 448) {
    WalEntry entry;
    entry.op = op;
    entry.offset = offset;
    entry.before.fill(0x11);
    entry.after.fill(0x22);
    return entry;
}

void damageByte(const std::filesystem::path& path, std::streamoff position) {
    std::fstream file{path, std::ios::in | std::ios::out | std::ios::binary};
    file.seekg(position);
    char byte = 0;
    file.get(byte);
    byte = static_cast<char>(byte ^ 0x40);
    file.seekp(position);
    file.put(byte);
}

constexpr std::streamoff kFirstEntry = 64;  // después de la cabecera de FixedRecordFile

class FileWriteAheadLogTest : public ::testing::Test {
protected:
    FileWriteAheadLogTest()
        : temp_(std::string{"wal_"} + ::testing::UnitTest::GetInstance()->current_test_info()->name() + ".wal") {}

    TempFile temp_;
};

}  // namespace

TEST_F(FileWriteAheadLogTest, StartsEmpty) {
    const FileWriteAheadLog wal{temp_.path()};

    EXPECT_FALSE(wal.pending().has_value());
}

TEST_F(FileWriteAheadLogTest, LoggedEntryIsPendingUntilCleared) {
    FileWriteAheadLog wal{temp_.path()};
    wal.log(sampleEntry(WalOp::Update, 832));

    const auto pending = wal.pending();
    ASSERT_TRUE(pending.has_value());
    EXPECT_EQ(pending->op, WalOp::Update);
    EXPECT_EQ(pending->offset, 832U);
    EXPECT_EQ(pending->before, sampleEntry().before);
    EXPECT_EQ(pending->after, sampleEntry().after);

    wal.clear();
    EXPECT_FALSE(wal.pending().has_value());
}

TEST_F(FileWriteAheadLogTest, PendingEntrySurvivesARestart) {
    {
        FileWriteAheadLog wal{temp_.path()};
        wal.log(sampleEntry(WalOp::Remove, 64));
    }  // el proceso "cae" sin borrar la anotación

    const FileWriteAheadLog reopened{temp_.path()};

    ASSERT_TRUE(reopened.pending().has_value());
    EXPECT_EQ(reopened.pending()->op, WalOp::Remove);
}

TEST_F(FileWriteAheadLogTest, ClearedEntryStaysClearedAfterARestart) {
    {
        FileWriteAheadLog wal{temp_.path()};
        wal.log(sampleEntry());
        wal.clear();
    }

    const FileWriteAheadLog reopened{temp_.path()};

    EXPECT_FALSE(reopened.pending().has_value());
}

TEST_F(FileWriteAheadLogTest, RefusesToLogWhileAnotherOperationIsPending) {
    FileWriteAheadLog wal{temp_.path()};
    wal.log(sampleEntry());

    EXPECT_THROW(wal.log(sampleEntry(WalOp::Remove)), std::logic_error);
    EXPECT_EQ(wal.pending()->op, WalOp::Add);  // la primera no se perdió
}

TEST_F(FileWriteAheadLogTest, RefusesToLogAfterARestartWithAPendingOperation) {
    {
        FileWriteAheadLog wal{temp_.path()};
        wal.log(sampleEntry());
    }
    FileWriteAheadLog reopened{temp_.path()};

    EXPECT_THROW(reopened.log(sampleEntry(WalOp::Update)), std::logic_error);
}

TEST_F(FileWriteAheadLogTest, CanLogAgainAfterClearing) {
    FileWriteAheadLog wal{temp_.path()};
    wal.log(sampleEntry(WalOp::Add));
    wal.clear();

    wal.log(sampleEntry(WalOp::Remove));

    EXPECT_EQ(wal.pending()->op, WalOp::Remove);
}

TEST_F(FileWriteAheadLogTest, AnEntryCutInHalfIsIgnored) {
    {
        FileWriteAheadLog wal{temp_.path()};
        wal.log(sampleEntry());
    }
    damageByte(temp_.path(), kFirstEntry + 100);  // el checksum ya no coincide: la escritura no terminó

    FileWriteAheadLog reopened{temp_.path()};

    EXPECT_FALSE(reopened.pending().has_value());
    EXPECT_NO_THROW(reopened.log(sampleEntry()));  // y no bloquea nuevas operaciones
}

TEST_F(FileWriteAheadLogTest, AnUnknownOperationCodeIsIgnored) {
    {
        FileWriteAheadLog wal{temp_.path()};
        wal.log(sampleEntry());
    }
    damageByte(temp_.path(), kFirstEntry);  // cambia el código de operación

    const FileWriteAheadLog reopened{temp_.path()};

    EXPECT_FALSE(reopened.pending().has_value());
}

TEST(MemoryWriteAheadLogTest, BehavesLikeTheFileOne) {
    MemoryWriteAheadLog wal;
    EXPECT_FALSE(wal.pending().has_value());

    wal.log(sampleEntry());
    EXPECT_TRUE(wal.pending().has_value());
    EXPECT_THROW(wal.log(sampleEntry()), std::logic_error);

    wal.clear();
    EXPECT_FALSE(wal.pending().has_value());
}

}  // namespace tecflix::users
