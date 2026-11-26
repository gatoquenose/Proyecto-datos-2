#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

#include "domain/IReservationRepository.hpp"
#include "domain/IUserRepository.hpp"
#include "domain/Reservation.hpp"
#include "tecflix/auth/IClock.hpp"

namespace tecflix::users {

struct ReserveRequest {
    std::string reservationId;
    std::string userId;
    std::uint64_t bytes;
    std::optional<std::int64_t> expiresAt;  // segundos desde 1970; si falta se usa la duración por defecto
};

enum class ReserveStatus { Created, AlreadyReserved, UserNotFound, QuotaExceeded, Conflict, InvalidExpiry };

struct ReserveOutcome {
    ReserveStatus status;
    Reservation reservation;  // tiene valores en Created y AlreadyReserved
};

enum class SettleStatus { Settled, AlreadySettled, NotFound, QuotaExceeded, InvalidState };

struct SettleOutcome {
    SettleStatus status;
    Reservation reservation;  // tiene valores en Settled y AlreadySettled
};

enum class ReleaseStatus { Released, AlreadyReleased, NotFound };

// Reservas de cuota. usedBytes de un usuario = bytes de sus videos publicados + bytes apartados
// por reservas activas. Todas las operaciones son idempotentes y se ejecutan de una en una.
class QuotaService {
public:
    QuotaService(IUserRepository& users, IReservationRepository& reservations, const tecflix::auth::IClock& clock,
                 std::chrono::seconds defaultLifetime);

    // Aparta espacio. Repetir la misma petición devuelve la reserva existente (AlreadyReserved).
    [[nodiscard]] ReserveOutcome reserve(const ReserveRequest& request);

    // Ajusta una reserva activa al tamaño final (devuelve la diferencia o cobra el extra).
    [[nodiscard]] SettleOutcome settle(std::string_view reservationId, std::uint64_t finalBytes);

    // Devuelve el espacio de una reserva activa o publicada. Liberar de nuevo no hace nada.
    [[nodiscard]] ReleaseStatus release(std::string_view reservationId);

    // Vence las reservas activas que ya pasaron su fecha y devuelve cuántas fueron.
    std::size_t expireDueReservations();

private:
    [[nodiscard]] std::int64_t nowSeconds() const;
    // Si la reserva está activa y ya venció, la marca como vencida y devuelve su espacio.
    void expireIfDue(Reservation& reservation);
    void addUsed(const std::string& userId, std::uint64_t bytes);
    void subtractUsed(const std::string& userId, std::uint64_t bytes);

    IUserRepository& users_;
    IReservationRepository& reservations_;
    const tecflix::auth::IClock& clock_;
    std::chrono::seconds defaultLifetime_;
    std::mutex mutex_;
};

}  // namespace tecflix::users
