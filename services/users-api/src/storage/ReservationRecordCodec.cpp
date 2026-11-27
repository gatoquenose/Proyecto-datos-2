#include "storage/ReservationRecordCodec.hpp"

#include <stdexcept>

#include "domain/User.hpp"
#include "storage/ByteCodec.hpp"
#include "storage/Checksum.hpp"
#include "storage/CorruptionError.hpp"

namespace tecflix::users {

namespace {

constexpr std::size_t kLengthByte = 1;
constexpr std::size_t kNumberBytes = sizeof(std::uint64_t);

constexpr std::size_t kStatusOffset = 0;
constexpr std::size_t kIdOffset = 1;
constexpr std::size_t kUserIdOffset = kIdOffset + kLengthByte + kMaxIdLength;
constexpr std::size_t kBytesOffset = kUserIdOffset + kLengthByte + kMaxIdLength;
constexpr std::size_t kCreatedOffset = kBytesOffset + kNumberBytes;
constexpr std::size_t kExpiresOffset = kCreatedOffset + kNumberBytes;
constexpr std::size_t kChecksumOffset = kExpiresOffset + kNumberBytes;

static_assert(kChecksumOffset + kChecksumBytes <= kReservationRecordBytes, "la reserva debe caber en su registro");

bool isKnownStatus(std::uint8_t value) {
    return value >= static_cast<std::uint8_t>(ReservationStatus::Active) &&
           value <= static_cast<std::uint8_t>(ReservationStatus::Expired);
}

}  // namespace

void ReservationRecordCodec::encode(const Reservation& reservation, ReservationBuffer& buffer) {
    buffer.fill(0);
    buffer[kStatusOffset] = static_cast<std::uint8_t>(reservation.status);
    putFixedString(buffer, kIdOffset, kMaxIdLength, reservation.id);
    putFixedString(buffer, kUserIdOffset, kMaxIdLength, reservation.userId);
    putLittleEndian<std::uint64_t>(buffer, kBytesOffset, reservation.bytes);
    putLittleEndian<std::uint64_t>(buffer, kCreatedOffset, static_cast<std::uint64_t>(reservation.createdAt));
    putLittleEndian<std::uint64_t>(buffer, kExpiresOffset, static_cast<std::uint64_t>(reservation.expiresAt));
    sealChecksum(buffer, kChecksumOffset);
}

Reservation ReservationRecordCodec::decode(const ReservationBuffer& buffer) {
    requireChecksum(buffer, kChecksumOffset, "registro de reserva");
    if (!isKnownStatus(buffer[kStatusOffset])) {
        throw CorruptionError("reserva corrupta: estado desconocido");
    }
    Reservation reservation;
    reservation.status = static_cast<ReservationStatus>(buffer[kStatusOffset]);
    reservation.id = getFixedString(buffer, kIdOffset, kMaxIdLength);
    reservation.userId = getFixedString(buffer, kUserIdOffset, kMaxIdLength);
    reservation.bytes = getLittleEndian<std::uint64_t>(buffer, kBytesOffset);
    reservation.createdAt = static_cast<std::int64_t>(getLittleEndian<std::uint64_t>(buffer, kCreatedOffset));
    reservation.expiresAt = static_cast<std::int64_t>(getLittleEndian<std::uint64_t>(buffer, kExpiresOffset));
    return reservation;
}

}  // namespace tecflix::users
