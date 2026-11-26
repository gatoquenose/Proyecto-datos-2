#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

#include "storage/UserRecordCodec.hpp"

namespace tecflix::users {

namespace {

User sampleUser() {
    return User{"00000000-0000-0000-0000-000000000001", "Alice", "alice@tec.cr", "$argon2id$hash", 5000, 120};
}

}  // namespace

TEST(UserRecordCodecTest, UserSurvivesEncodeAndDecode) {
    RecordBuffer buffer{};

    UserRecordCodec::encode(sampleUser(), RecordStatus::Active, buffer);
    const DecodedRecord decoded = UserRecordCodec::decode(buffer);

    EXPECT_EQ(decoded.status, RecordStatus::Active);
    EXPECT_EQ(decoded.user.id, sampleUser().id);
    EXPECT_EQ(decoded.user.username, "Alice");
    EXPECT_EQ(decoded.user.email, "alice@tec.cr");
    EXPECT_EQ(decoded.user.passwordHash, "$argon2id$hash");
    EXPECT_EQ(decoded.user.quotaBytes, 5000U);
    EXPECT_EQ(decoded.user.usedBytes, 120U);
}

TEST(UserRecordCodecTest, FieldsAtTheirMaximumLengthFit) {
    User user = sampleUser();
    user.id = std::string(kMaxIdLength, 'a');
    user.username = std::string(kMaxUsernameLength, 'b');
    user.email = std::string(kMaxEmailLength, 'c');
    user.passwordHash = std::string(kMaxPasswordHashLength, 'd');
    RecordBuffer buffer{};

    UserRecordCodec::encode(user, RecordStatus::Active, buffer);
    const DecodedRecord decoded = UserRecordCodec::decode(buffer);

    EXPECT_EQ(decoded.user.email, user.email);
    EXPECT_EQ(decoded.user.passwordHash, user.passwordHash);
}

TEST(UserRecordCodecTest, RejectsFieldLongerThanItsLimit) {
    User user = sampleUser();
    user.username = std::string(kMaxUsernameLength + 1, 'x');
    RecordBuffer buffer{};

    EXPECT_THROW(UserRecordCodec::encode(user, RecordStatus::Active, buffer), std::invalid_argument);
}

TEST(UserRecordCodecTest, SetStatusMarksRecordAsDeleted) {
    RecordBuffer buffer{};
    UserRecordCodec::encode(sampleUser(), RecordStatus::Active, buffer);

    UserRecordCodec::setStatus(buffer, RecordStatus::Deleted);

    EXPECT_EQ(UserRecordCodec::decode(buffer).status, RecordStatus::Deleted);
}

TEST(UserRecordCodecTest, RejectsUnknownStatus) {
    const RecordBuffer buffer{};  // estado 0: no es un registro válido

    EXPECT_THROW(UserRecordCodec::decode(buffer), std::runtime_error);
}

}  // namespace tecflix::users
