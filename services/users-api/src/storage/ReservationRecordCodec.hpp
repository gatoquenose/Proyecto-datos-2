#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "domain/Reservation.hpp"

namespace tecflix::users {

inline constexpr std::size_t kReservationRecordBytes = 128;

using ReservationBuffer = std::array<std::uint8_t, kReservationRecordBytes>;

// Registro de reserva (128 bytes):
//   estado (1) | id (1+36) | userId (1+36) | bytes (8) | createdAt (8) | expiresAt (8) | CRC32C reservado (4)
class ReservationRecordCodec {
public:
    // Lanza std::invalid_argument si algún texto supera su límite.
    static void encode(const Reservation& reservation, ReservationBuffer& buffer);

    // Lanza std::runtime_error si los bytes no son un registro válido.
    static Reservation decode(const ReservationBuffer& buffer);
};

}  // namespace tecflix::users
