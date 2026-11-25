#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "domain/Reservation.hpp"

namespace tecflix::users {

// Registro persistente de reservas de cuota. No es seguro entre hilos por sí mismo:
// QuotaService serializa todo acceso con su propio candado.
class IReservationRepository {
public:
    IReservationRepository() = default;
    IReservationRepository(const IReservationRepository&) = delete;
    IReservationRepository& operator=(const IReservationRepository&) = delete;
    IReservationRepository(IReservationRepository&&) = delete;
    IReservationRepository& operator=(IReservationRepository&&) = delete;
    virtual ~IReservationRepository() = default;

    // Devuelve false si ya existe una reserva con ese id.
    virtual bool add(const Reservation& reservation) = 0;

    // Reemplaza una reserva existente (lanza std::logic_error si no existe).
    virtual void update(const Reservation& reservation) = 0;

    [[nodiscard]] virtual std::optional<Reservation> find(std::string_view id) const = 0;

    // Reservas activas cuya fecha de expiración ya llegó (expiresAt <= now).
    [[nodiscard]] virtual std::vector<Reservation> findActiveExpiredBy(std::int64_t now) const = 0;
};

}  // namespace tecflix::users
