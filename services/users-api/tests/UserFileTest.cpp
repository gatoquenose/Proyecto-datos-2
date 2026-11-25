#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>

#include "TempFile.hpp"
#include "storage/UserFile.hpp"
#include "storage/UserRecordCodec.hpp"

namespace tecflix::users {

using testing::TempFile;

namespace {

User userNumber(int number) {
    const std::string suffix = std::to_string(number);
    return User{"id-" + suffix, "user" + suffix, "user" + suffix + "@tec.cr", "hash", 1000, 0};
}

}  // namespace

TEST(UserFileTest, AppendedUserCanBeReadBack) {
    const TempFile temp{"userfile_basic.dat"};
    UserFile file{temp.path()};

    const auto offset = file.append(userNumber(1));

    EXPECT_EQ(file.read(offset).username, "user1");
    EXPECT_EQ(file.recordCount(), 1U);
}

TEST(UserFileTest, RecordsGetDifferentOffsets) {
    const TempFile temp{"userfile_offsets.dat"};
    UserFile file{temp.path()};

    const auto first = file.append(userNumber(1));
    const auto second = file.append(userNumber(2));

    EXPECT_EQ(second - first, kRecordBytes);
    EXPECT_EQ(file.read(first).username, "user1");
    EXPECT_EQ(file.read(second).username, "user2");
}

TEST(UserFileTest, DataSurvivesClosingAndReopeningTheFile) {
    const TempFile temp{"userfile_reopen.dat"};
    std::uint64_t offset = 0;
    {
        UserFile file{temp.path()};
        file.append(userNumber(1));
        offset = file.append(userNumber(2));
    }

    const UserFile reopened{temp.path()};

    EXPECT_EQ(reopened.recordCount(), 2U);
    EXPECT_EQ(reopened.read(offset).username, "user2");
}

TEST(UserFileTest, OverwriteReplacesTheRecordInPlace) {
    const TempFile temp{"userfile_overwrite.dat"};
    UserFile file{temp.path()};
    const auto offset = file.append(userNumber(1));
    User changed = userNumber(1);
    changed.usedBytes = 777;

    file.overwrite(offset, changed);

    EXPECT_EQ(file.read(offset).usedBytes, 777U);
    EXPECT_EQ(file.recordCount(), 1U);
}

TEST(UserFileTest, DeletedRecordCannotBeRead) {
    const TempFile temp{"userfile_deleted.dat"};
    UserFile file{temp.path()};
    const auto offset = file.append(userNumber(1));

    file.markDeleted(offset);

    EXPECT_THROW(static_cast<void>(file.read(offset)), std::runtime_error);
    EXPECT_THROW(file.overwrite(offset, userNumber(1)), std::runtime_error);
}

TEST(UserFileTest, RejectsOffsetsThatAreNotRecords) {
    const TempFile temp{"userfile_offset_check.dat"};
    UserFile file{temp.path()};
    const auto offset = file.append(userNumber(1));

    EXPECT_THROW(static_cast<void>(file.read(0)), std::out_of_range);          // cabecera
    EXPECT_THROW(static_cast<void>(file.read(offset + 1)), std::out_of_range);  // a mitad de registro
    EXPECT_THROW(static_cast<void>(file.read(offset + kRecordBytes)), std::out_of_range);  // más allá del final
}

TEST(UserFileTest, RefusesFileWithoutTheRightHeader) {
    const TempFile temp{"userfile_bad_header.dat"};
    {
        std::ofstream out{temp.path(), std::ios::binary};
        out << std::string(200, 'x');
    }

    EXPECT_THROW(UserFile{temp.path()}, std::runtime_error);
}

TEST(UserFileTest, RefusesFileWithIncompleteRecord) {
    const TempFile temp{"userfile_truncated.dat"};
    {
        UserFile file{temp.path()};
        file.append(userNumber(1));
    }
    std::filesystem::resize_file(temp.path(), std::filesystem::file_size(temp.path()) - 10);

    EXPECT_THROW(UserFile{temp.path()}, std::runtime_error);
}

}  // namespace tecflix::users
