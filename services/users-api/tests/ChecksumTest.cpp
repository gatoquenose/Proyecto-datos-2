#include <gtest/gtest.h>

#include <stdexcept>

#include "domain/Reservation.hpp"
#include "storage/BTreeLayout.hpp"
#include "storage/BTreePage.hpp"
#include "storage/Checksum.hpp"
#include "storage/CorruptionError.hpp"
#include "storage/ReservationRecordCodec.hpp"
#include "storage/UserRecordCodec.hpp"

namespace tecflix::users {

namespace {

constexpr std::size_t kPageChecksumOffset = 4;

User sampleUser() {
    return User{"00000000-0000-0000-0000-000000000001", "Alice", "alice@tec.cr", "$argon2id$hash", 5000, 120};
}

PageBuffer samplePage() {
    BTreeNode node;
    node.leaf = true;
    node.keys = {"u:alice", "u:bob"};
    node.values = {100, 200};
    PageBuffer buffer{};
    BTreePage::encode(node, buffer);
    return buffer;
}

}  // namespace

TEST(ChecksumTest, SealedBlockVerifiesAndAnyChangeBreaksIt) {
    std::array<std::uint8_t, 32> block{};
    block[0] = 7;
    sealChecksum(block, 8);
    EXPECT_TRUE(checksumMatches(block, 8));

    block[20] ^= 0x10U;

    EXPECT_FALSE(checksumMatches(block, 8));
    EXPECT_THROW(requireChecksum(block, 8, "bloque"), CorruptionError);
}

TEST(ChecksumTest, ChecksumFieldItselfIsNotPartOfTheCheckedBytes) {
    std::array<std::uint8_t, 16> block{};
    sealChecksum(block, 4);
    const auto sealed = block;

    sealChecksum(block, 4);  // sellar dos veces da lo mismo: el campo no se cuenta a sí mismo

    EXPECT_EQ(block, sealed);
}

TEST(ChecksumTest, CorruptedErrorIsStillARuntimeError) {
    std::array<std::uint8_t, 16> block{};
    block[0] = 1;  // sin sellar: el checksum guardado (0) no coincide

    EXPECT_THROW(requireChecksum(block, 4, "bloque"), std::runtime_error);
}

TEST(BTreePageChecksumTest, FlippingAnyByteOfAPageIsDetected) {
    const PageBuffer good = samplePage();
    EXPECT_NO_THROW(BTreePage::decode(1, good));

    for (const std::size_t position : {std::size_t{0}, std::size_t{2}, kKeysOffset + 3, kValuesOffset + 1,
                                       kPageSize - 1}) {
        PageBuffer damaged = good;
        damaged[position] ^= 0x01U;
        EXPECT_THROW(BTreePage::decode(1, damaged), CorruptionError) << "byte " << position;
    }
}

TEST(BTreePageChecksumTest, AnUnwrittenZeroPageIsNotAValidNode) {
    const PageBuffer zeros{};

    EXPECT_THROW(BTreePage::decode(1, zeros), CorruptionError);
}

TEST(BTreePageChecksumTest, DamagedChecksumFieldIsDetected) {
    PageBuffer damaged = samplePage();
    damaged[kPageChecksumOffset] ^= 0xFFU;

    EXPECT_THROW(BTreePage::decode(1, damaged), CorruptionError);
}

TEST(UserRecordChecksumTest, DamagedRecordIsDetected) {
    RecordBuffer buffer{};
    UserRecordCodec::encode(sampleUser(), RecordStatus::Active, buffer);
    buffer[40] ^= 0x04U;

    EXPECT_THROW(UserRecordCodec::decode(buffer), CorruptionError);
}

TEST(UserRecordChecksumTest, SetStatusKeepsTheRecordValid) {
    RecordBuffer buffer{};
    UserRecordCodec::encode(sampleUser(), RecordStatus::Active, buffer);

    UserRecordCodec::setStatus(buffer, RecordStatus::Deleted);

    EXPECT_NO_THROW(UserRecordCodec::decode(buffer));
}

TEST(UserRecordChecksumTest, SetStatusRefusesToHideExistingDamage) {
    RecordBuffer buffer{};
    UserRecordCodec::encode(sampleUser(), RecordStatus::Active, buffer);
    buffer[40] ^= 0x04U;

    EXPECT_THROW(UserRecordCodec::setStatus(buffer, RecordStatus::Deleted), CorruptionError);
}

TEST(ReservationRecordChecksumTest, DamagedRecordIsDetected) {
    Reservation reservation;
    reservation.id = "00000000-0000-0000-0000-0000000000a1";
    reservation.userId = "00000000-0000-0000-0000-000000000001";
    reservation.bytes = 10;
    reservation.status = ReservationStatus::Active;
    ReservationBuffer buffer{};
    ReservationRecordCodec::encode(reservation, buffer);
    EXPECT_NO_THROW(ReservationRecordCodec::decode(buffer));

    buffer[60] ^= 0x01U;

    EXPECT_THROW(ReservationRecordCodec::decode(buffer), CorruptionError);
}

}  // namespace tecflix::users
