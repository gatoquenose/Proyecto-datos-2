#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "domain/User.hpp"

namespace tecflix::users {

// Cada usuario ocupa exactamente kRecordBytes en users.dat, así el offset del
// registro número N se calcula sin buscar.
inline constexpr std::size_t kRecordBytes = 384;

using RecordBuffer = std::array<std::uint8_t, kRecordBytes>;

enum class RecordStatus : std::uint8_t { Active = 1, Deleted = 2 };

struct DecodedRecord {
    RecordStatus status;
    User user;
};

// Convierte un usuario en los bytes de un registro y viceversa.
//   [0]        estado (1 = activo, 2 = eliminado)
//   campos     id, username, email y hash: 1 byte de largo + texto (tamaño máximo fijo)
//   números    quotaBytes y usedBytes, 8 bytes cada uno
//   final      4 bytes reservados para el CRC32C (se agrega con la recuperación ante fallos)
class UserRecordCodec {
public:
    // Lanza std::invalid_argument si algún campo supera su límite.
    static void encode(const User& user, RecordStatus status, RecordBuffer& buffer);

    // Lanza std::runtime_error si los bytes no son un registro válido.
    static DecodedRecord decode(const RecordBuffer& buffer);

    static void setStatus(RecordBuffer& buffer, RecordStatus status);
};

}  // namespace tecflix::users
