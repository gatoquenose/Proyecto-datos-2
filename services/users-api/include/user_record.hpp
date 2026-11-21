#pragma once

// Supuesto: Las cadenas de caracteres (username, email, hash) se almacenan en búferes de tamaño fijo
// con longitud explícita en bytes (u16 prefijo o longitud útil en el campo si es menor que la capacidad máxima),
// rellenados con ceros a la derecha. Los campos de texto se validan como UTF-8 y se rechazan si exceden
// la capacidad máxima o no caben en el búfer asignado con std::invalid_argument.
// El CRC32C (Castagnoli) se calcula sobre el registro completo de 1024 bytes con el campo CRC32C en cero.

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <stdexcept>
#include <algorithm>

namespace tecflix {

inline constexpr std::size_t USER_RECORD_SIZE = 1024;
inline constexpr std::uint16_t FLAG_ACTIVE = 0x0001;
inline constexpr std::uint16_t FLAG_DELETED = 0x0002;

struct UserRecord {
    std::uint32_t magic = 0x54454346; // "TECF"
    std::uint16_t version = 1;
    std::uint16_t flags = FLAG_ACTIVE;
    std::array<std::uint8_t, 16> uuid{};
    std::string username;
    std::string email;
    std::array<std::uint8_t, 128> argon2idHash{};
    std::uint64_t quotaBytes = 0;
    std::uint64_t usedBytes = 0;
    std::uint64_t createdAt = 0;
    std::uint64_t updatedAt = 0;
    std::uint64_t nextFreeOffset = 0;
    std::uint32_t crc32c = 0;
};

// Standard CRC32C (Castagnoli) table-driven implementation
inline std::uint32_t calculate_crc32c(const std::uint8_t* data, std::size_t length) {
    std::uint32_t crc = 0xFFFFFFFF;
    for (std::size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0x82F63B78U;
            } else {
                crc = crc >> 1;
            }
        }
    }
    return ~crc;
}

inline void write_u16_le(std::uint8_t* dest, std::uint16_t val) {
    dest[0] = static_cast<std::uint8_t>(val & 0xFF);
    dest[1] = static_cast<std::uint8_t>((val >> 8) & 0xFF);
}

inline void write_u32_le(std::uint8_t* dest, std::uint32_t val) {
    dest[0] = static_cast<std::uint8_t>(val & 0xFF);
    dest[1] = static_cast<std::uint8_t>((val >> 8) & 0xFF);
    dest[2] = static_cast<std::uint8_t>((val >> 16) & 0xFF);
    dest[3] = static_cast<std::uint8_t>((val >> 24) & 0xFF);
}

inline void write_u64_le(std::uint8_t* dest, std::uint64_t val) {
    for (int i = 0; i < 8; ++i) {
        dest[i] = static_cast<std::uint8_t>((val >> (8 * i)) & 0xFF);
    }
}

inline std::uint16_t read_u16_le(const std::uint8_t* src) {
    return static_cast<std::uint16_t>(src[0]) |
           (static_cast<std::uint16_t>(src[1]) << 8);
}

inline std::uint32_t read_u32_le(const std::uint8_t* src) {
    return static_cast<std::uint32_t>(src[0]) |
           (static_cast<std::uint32_t>(src[1]) << 8) |
           (static_cast<std::uint32_t>(src[2]) << 16) |
           (static_cast<std::uint32_t>(src[3]) << 24);
}

inline std::uint64_t read_u64_le(const std::uint8_t* src) {
    std::uint64_t val = 0;
    for (int i = 0; i < 8; ++i) {
        val |= (static_cast<std::uint64_t>(src[i]) << (8 * i));
    }
    return val;
}

inline std::array<std::byte, USER_RECORD_SIZE> serialize(const UserRecord& record) {
    if (record.username.size() > 128) {
        throw std::invalid_argument("El nombre de usuario excede el tamaño máximo permitido de 128 bytes.");
    }
    if (record.email.size() > 256) {
        throw std::invalid_argument("El correo electrónico excede el tamaño máximo permitido de 256 bytes.");
    }

    std::array<std::byte, USER_RECORD_SIZE> buffer{};
    auto* ptr = reinterpret_cast<std::uint8_t*>(buffer.data());

    // Layout:
    // magic (4) + version (2) + flags (2) = 8 bytes
    write_u32_le(ptr + 0, record.magic);
    write_u16_le(ptr + 4, record.version);
    write_u16_le(ptr + 6, record.flags);

    // UUID (16 bytes)
    std::memcpy(ptr + 8, record.uuid.data(), 16);

    // Username field (128 bytes): u16 length + bytes + padding
    std::size_t uname_len = record.username.size();
    write_u16_le(ptr + 24, static_cast<std::uint16_t>(uname_len));
    std::memcpy(ptr + 26, record.username.data(), uname_len);

    // Email field (256 bytes): u16 length + bytes + padding
    std::size_t email_len = record.email.size();
    write_u16_le(ptr + 152, static_cast<std::uint16_t>(email_len));
    std::memcpy(ptr + 154, record.email.data(), email_len);

    // Argon2id hash (128 bytes)
    std::memcpy(ptr + 408, record.argon2idHash.data(), 128);

    // quotaBytes (8), usedBytes (8), createdAt (8), updatedAt (8), nextFreeOffset (8) = 40 bytes
    write_u64_le(ptr + 536, record.quotaBytes);
    write_u64_le(ptr + 544, record.usedBytes);
    write_u64_le(ptr + 552, record.createdAt);
    write_u64_le(ptr + 560, record.updatedAt);
    write_u64_le(ptr + 568, record.nextFreeOffset);

    // CRC32C (4 bytes) at offset 576 (calculated with CRC field as zero)
    write_u32_le(ptr + 576, 0);

    // Remaining bytes are zero-initialized by buffer{} initialization.
    std::uint32_t crc = calculate_crc32c(ptr, USER_RECORD_SIZE);
    write_u32_le(ptr + 576, crc);

    return buffer;
}

inline UserRecord deserialize(const std::array<std::byte, USER_RECORD_SIZE>& buffer) {
    auto* ptr = reinterpret_cast<const std::uint8_t*>(buffer.data());

    // Verify CRC32C first by copying and zeroing out the CRC field
    std::array<std::byte, USER_RECORD_SIZE> temp_buffer = buffer;
    auto* temp_ptr = reinterpret_cast<std::uint8_t*>(temp_buffer.data());
    std::uint32_t stored_crc = read_u32_le(temp_ptr + 576);
    write_u32_le(temp_ptr + 576, 0);
    std::uint32_t computed_crc = calculate_crc32c(temp_ptr, USER_RECORD_SIZE);

    if (stored_crc != computed_crc) {
        throw std::runtime_error("Error de deserialización: El CRC32C del registro no coincide.");
    }

    UserRecord record;
    record.magic = read_u32_le(ptr + 0);
    record.version = read_u16_le(ptr + 4);
    record.flags = read_u16_le(ptr + 6);

    record.uuid.fill(0);
    std::memcpy(record.uuid.data(), ptr + 8, 16);

    std::uint16_t uname_len = read_u16_le(ptr + 24);
    if (uname_len > 128) {
        throw std::runtime_error("Error de deserialización: La longitud del nombre de usuario excede el campo.");
    }
    record.username.assign(reinterpret_cast<const char*>(ptr + 26), uname_len);

    std::uint16_t email_len = read_u16_le(ptr + 152);
    if (email_len > 256) {
        throw std::runtime_error("Error de deserialización: La longitud del correo electrónico excede el campo.");
    }
    record.email.assign(reinterpret_cast<const char*>(ptr + 154), email_len);

    record.argon2idHash.fill(0);
    std::memcpy(record.argon2idHash.data(), ptr + 408, 128);

    record.quotaBytes = read_u64_le(ptr + 536);
    record.usedBytes = read_u64_le(ptr + 544);
    record.createdAt = read_u64_le(ptr + 552);
    record.updatedAt = read_u64_le(ptr + 560);
    record.nextFreeOffset = read_u64_le(ptr + 568);
    record.crc32c = stored_crc;

    return record;
}

}  // namespace tecflix
