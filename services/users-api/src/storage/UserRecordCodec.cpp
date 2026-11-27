#include "storage/UserRecordCodec.hpp"

#include <stdexcept>

#include "storage/ByteCodec.hpp"
#include "storage/Checksum.hpp"
#include "storage/CorruptionError.hpp"

namespace tecflix::users {

namespace {

constexpr std::size_t kLengthByte = 1;
constexpr std::size_t kNumberBytes = sizeof(std::uint64_t);

constexpr std::size_t kStatusOffset = 0;
constexpr std::size_t kIdOffset = 1;
constexpr std::size_t kUsernameOffset = kIdOffset + kLengthByte + kMaxIdLength;
constexpr std::size_t kEmailOffset = kUsernameOffset + kLengthByte + kMaxUsernameLength;
constexpr std::size_t kHashOffset = kEmailOffset + kLengthByte + kMaxEmailLength;
constexpr std::size_t kQuotaOffset = kHashOffset + kLengthByte + kMaxPasswordHashLength;
constexpr std::size_t kUsedOffset = kQuotaOffset + kNumberBytes;
constexpr std::size_t kChecksumOffset = kUsedOffset + kNumberBytes;

static_assert(kChecksumOffset + kChecksumBytes <= kRecordBytes, "el registro debe caber en kRecordBytes");

bool isKnownStatus(std::uint8_t value) {
    return value == static_cast<std::uint8_t>(RecordStatus::Active) ||
           value == static_cast<std::uint8_t>(RecordStatus::Deleted);
}

}  // namespace

void UserRecordCodec::encode(const User& user, RecordStatus status, RecordBuffer& buffer) {
    buffer.fill(0);
    buffer[kStatusOffset] = static_cast<std::uint8_t>(status);
    putFixedString(buffer, kIdOffset, kMaxIdLength, user.id);
    putFixedString(buffer, kUsernameOffset, kMaxUsernameLength, user.username);
    putFixedString(buffer, kEmailOffset, kMaxEmailLength, user.email);
    putFixedString(buffer, kHashOffset, kMaxPasswordHashLength, user.passwordHash);
    putLittleEndian<std::uint64_t>(buffer, kQuotaOffset, user.quotaBytes);
    putLittleEndian<std::uint64_t>(buffer, kUsedOffset, user.usedBytes);
    sealChecksum(buffer, kChecksumOffset);
}

DecodedRecord UserRecordCodec::decode(const RecordBuffer& buffer) {
    requireChecksum(buffer, kChecksumOffset, "registro de usuario");
    if (!isKnownStatus(buffer[kStatusOffset])) {
        throw CorruptionError("registro corrupto: estado desconocido");
    }
    DecodedRecord decoded;
    decoded.status = static_cast<RecordStatus>(buffer[kStatusOffset]);
    decoded.user.id = getFixedString(buffer, kIdOffset, kMaxIdLength);
    decoded.user.username = getFixedString(buffer, kUsernameOffset, kMaxUsernameLength);
    decoded.user.email = getFixedString(buffer, kEmailOffset, kMaxEmailLength);
    decoded.user.passwordHash = getFixedString(buffer, kHashOffset, kMaxPasswordHashLength);
    decoded.user.quotaBytes = getLittleEndian<std::uint64_t>(buffer, kQuotaOffset);
    decoded.user.usedBytes = getLittleEndian<std::uint64_t>(buffer, kUsedOffset);
    return decoded;
}

void UserRecordCodec::setStatus(RecordBuffer& buffer, RecordStatus status) {
    // Se verifica antes de volver a sellar: si no, un registro dañado quedaría "legalizado" con un CRC nuevo.
    requireChecksum(buffer, kChecksumOffset, "registro de usuario");
    buffer[kStatusOffset] = static_cast<std::uint8_t>(status);
    sealChecksum(buffer, kChecksumOffset);  // el estado está dentro de la zona protegida
}

}  // namespace tecflix::users
