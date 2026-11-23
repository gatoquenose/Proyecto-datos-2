#pragma once

#include <cstdint>
#include <string>

namespace tecflix::users {

enum class ReservationStatus : std::uint8_t {
    Active = 1,    // espacio apartado mientras se sube y se procesa el video
    Settled = 2,   // ajustada al tamaño final: el video ya está publicado
    Released = 3,  // el espacio volvió a la cuota (subida cancelada o video eliminado)
    Expired = 4    // venció sin completarse; el espacio volvió a la cuota
};

struct Reservation {
    std::string id;      // = id del video
    std::string userId;
    std::uint64_t bytes = 0;
    ReservationStatus status = ReservationStatus::Active;
    std::int64_t createdAt = 0;  // segundos desde 1970
    std::int64_t expiresAt = 0;
};

}  // namespace tecflix::users
