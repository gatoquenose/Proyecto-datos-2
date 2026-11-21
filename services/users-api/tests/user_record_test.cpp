#include <gtest/gtest.h>
#include "user_record.hpp"

TEST(UserRecordTest, RoundTripSerialization) {
    tecflix::UserRecord original;
    original.magic = 0x54454346;
    original.version = 1;
    original.flags = tecflix::FLAG_ACTIVE;
    original.uuid = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                     0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
    original.username = "test_user";
    original.email = "test@example.com";
    original.argon2idHash.fill(0xAB);
    original.quotaBytes = 5368709120ULL;
    original.usedBytes = 104857600ULL;
    original.createdAt = 1718000000ULL;
    original.updatedAt = 1718001000ULL;
    original.nextFreeOffset = 4096ULL;

    auto serialized = tecflix::serialize(original);
    EXPECT_EQ(serialized.size(), 1024);

    auto deserialized = tecflix::deserialize(serialized);
    EXPECT_EQ(deserialized.magic, original.magic);
    EXPECT_EQ(deserialized.version, original.version);
    EXPECT_EQ(deserialized.flags, original.flags);
    EXPECT_EQ(deserialized.uuid, original.uuid);
    EXPECT_EQ(deserialized.username, original.username);
    EXPECT_EQ(deserialized.email, original.email);
    EXPECT_EQ(deserialized.argon2idHash, original.argon2idHash);
    EXPECT_EQ(deserialized.quotaBytes, original.quotaBytes);
    EXPECT_EQ(deserialized.usedBytes, original.usedBytes);
    EXPECT_EQ(deserialized.createdAt, original.createdAt);
    EXPECT_EQ(deserialized.updatedAt, original.updatedAt);
    EXPECT_EQ(deserialized.nextFreeOffset, original.nextFreeOffset);
}

TEST(UserRecordTest, ExactSize1024) {
    tecflix::UserRecord record;
    auto serialized = tecflix::serialize(record);
    EXPECT_EQ(serialized.size(), 1024);
}

TEST(UserRecordTest, FlagsActiveAndDeleted) {
    tecflix::UserRecord record;
    record.flags = tecflix::FLAG_ACTIVE;
    auto s1 = tecflix::serialize(record);
    EXPECT_EQ(tecflix::deserialize(s1).flags, tecflix::FLAG_ACTIVE);

    record.flags = tecflix::FLAG_DELETED;
    auto s2 = tecflix::serialize(record);
    EXPECT_EQ(tecflix::deserialize(s2).flags, tecflix::FLAG_DELETED);
}

TEST(UserRecordTest, LittleEndianQuotaBytes) {
    tecflix::UserRecord record;
    record.quotaBytes = 0x0102030405060708ULL;
    auto serialized = tecflix::serialize(record);
    // quotaBytes is located at offset 536
    const auto* ptr = reinterpret_cast<const std::uint8_t*>(serialized.data()) + 536;
    EXPECT_EQ(ptr[0], 0x08);
    EXPECT_EQ(ptr[1], 0x07);
    EXPECT_EQ(ptr[2], 0x06);
    EXPECT_EQ(ptr[3], 0x05);
    EXPECT_EQ(ptr[4], 0x04);
    EXPECT_EQ(ptr[5], 0x03);
    EXPECT_EQ(ptr[6], 0x02);
    EXPECT_EQ(ptr[7], 0x01);

    auto deserialized = tecflix::deserialize(serialized);
    EXPECT_EQ(deserialized.quotaBytes, 0x0102030405060708ULL);
}

TEST(UserRecordTest, Crc32cVectorStandard) {
    // Standard test vector for CRC32C (Castagnoli) with "123456789" = 0xE3069283
    std::string test_str = "123456789";
    std::uint32_t crc = tecflix::calculate_crc32c(
        reinterpret_cast<const std::uint8_t*>(test_str.data()), test_str.size());
    EXPECT_EQ(crc, 0xE3069283U);
}

TEST(UserRecordTest, Crc32cTamperedBitDetected) {
    tecflix::UserRecord record;
    record.username = "alice";
    auto serialized = tecflix::serialize(record);

    // Tamper with a byte in the serialized buffer
    serialized[100] ^= static_cast<std::byte>(0x01);

    EXPECT_THROW({
        tecflix::deserialize(serialized);
    }, std::runtime_error);
}

TEST(UserRecordTest, OversizedUsernameOrEmailRejected) {
    tecflix::UserRecord record;
    record.username = std::string(129, 'a');
    EXPECT_THROW({
        tecflix::serialize(record);
    }, std::invalid_argument);

    record.username = "valid";
    record.email = std::string(257, 'b');
    EXPECT_THROW({
        tecflix::serialize(record);
    }, std::invalid_argument);
}

TEST(UserRecordTest, Utf8AccentsRoundTrip) {
    tecflix::UserRecord record;
    record.username = "josé_ñora";
    record.email = "josé.ñora@ejemplo.es";

    auto serialized = tecflix::serialize(record);
    auto deserialized = tecflix::deserialize(serialized);

    EXPECT_EQ(deserialized.username, "josé_ñora");
    EXPECT_EQ(deserialized.email, "josé.ñora@ejemplo.es");
}
