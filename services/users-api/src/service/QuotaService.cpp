#include "service/QuotaService.hpp"

#include <stdexcept>

namespace tecflix::users {

namespace {

std::uint64_t remainingOf(const User& user) {
    return user.quotaBytes > user.usedBytes ? user.quotaBytes - user.usedBytes : 0;
}

}  // namespace

QuotaService::QuotaService(IUserRepository& users, IReservationRepository& reservations,
                           const tecflix::auth::IClock& clock, std::chrono::seconds defaultLifetime)
    : users_(users), reservations_(reservations), clock_(clock), defaultLifetime_(defaultLifetime) {}

std::int64_t QuotaService::nowSeconds() const {
    return std::chrono::duration_cast<std::chrono::seconds>(clock_.now().time_since_epoch()).count();
}

void QuotaService::addUsed(const std::string& userId, std::uint64_t bytes) {
    users_.modify(userId, [bytes](User& user) { user.usedBytes += bytes; });
}

void QuotaService::subtractUsed(const std::string& userId, std::uint64_t bytes) {
    users_.modify(userId, [bytes](User& user) { user.usedBytes = user.usedBytes > bytes ? user.usedBytes - bytes : 0; });
}

void QuotaService::expireIfDue(Reservation& reservation) {
    if (reservation.status != ReservationStatus::Active || reservation.expiresAt > nowSeconds()) {
        return;
    }
    reservation.status = ReservationStatus::Expired;
    reservations_.update(reservation);
    subtractUsed(reservation.userId, reservation.bytes);
}

ReserveOutcome QuotaService::reserve(const ReserveRequest& request) {
    const std::scoped_lock lock{mutex_};
    const std::int64_t now = nowSeconds();
    const std::int64_t expiresAt = request.expiresAt.value_or(now + defaultLifetime_.count());
    if (expiresAt <= now) {
        return {ReserveStatus::InvalidExpiry, Reservation{}};
    }

    if (auto existing = reservations_.find(request.reservationId)) {
        expireIfDue(*existing);
        const bool sameRequest = existing->userId == request.userId && existing->bytes == request.bytes &&
                                 existing->status == ReservationStatus::Active;
        return {sameRequest ? ReserveStatus::AlreadyReserved : ReserveStatus::Conflict,
                sameRequest ? *existing : Reservation{}};
    }

    const auto user = users_.findById(request.userId);
    if (!user) {
        return {ReserveStatus::UserNotFound, Reservation{}};
    }
    if (request.bytes > remainingOf(*user)) {
        return {ReserveStatus::QuotaExceeded, Reservation{}};
    }

    const Reservation reservation{request.reservationId, user->id, request.bytes, ReservationStatus::Active, now,
                                  expiresAt};
    if (!reservations_.add(reservation)) {
        throw std::logic_error("la reserva apareció durante su creación");
    }
    addUsed(user->id, request.bytes);
    return {ReserveStatus::Created, reservation};
}

SettleOutcome QuotaService::settle(std::string_view reservationId, std::uint64_t finalBytes) {
    const std::scoped_lock lock{mutex_};
    auto reservation = reservations_.find(reservationId);
    if (!reservation) {
        return {SettleStatus::NotFound, Reservation{}};
    }
    expireIfDue(*reservation);

    if (reservation->status == ReservationStatus::Settled) {
        const bool sameSize = reservation->bytes == finalBytes;
        return {sameSize ? SettleStatus::AlreadySettled : SettleStatus::InvalidState,
                sameSize ? *reservation : Reservation{}};
    }
    if (reservation->status != ReservationStatus::Active) {
        return {SettleStatus::InvalidState, Reservation{}};
    }

    if (finalBytes > reservation->bytes) {
        const std::uint64_t extra = finalBytes - reservation->bytes;
        const auto user = users_.findById(reservation->userId);
        if (!user) {
            return {SettleStatus::NotFound, Reservation{}};
        }
        if (extra > remainingOf(*user)) {
            return {SettleStatus::QuotaExceeded, Reservation{}};
        }
        addUsed(reservation->userId, extra);
    } else {
        subtractUsed(reservation->userId, reservation->bytes - finalBytes);
    }
    reservation->bytes = finalBytes;
    reservation->status = ReservationStatus::Settled;
    reservations_.update(*reservation);
    return {SettleStatus::Settled, *reservation};
}

ReleaseStatus QuotaService::release(std::string_view reservationId) {
    const std::scoped_lock lock{mutex_};
    auto reservation = reservations_.find(reservationId);
    if (!reservation) {
        return ReleaseStatus::NotFound;
    }
    expireIfDue(*reservation);

    const bool holdsSpace =
        reservation->status == ReservationStatus::Active || reservation->status == ReservationStatus::Settled;
    if (!holdsSpace) {
        return ReleaseStatus::AlreadyReleased;  // ya liberada o vencida: el espacio ya volvió
    }
    subtractUsed(reservation->userId, reservation->bytes);
    reservation->status = ReservationStatus::Released;
    reservations_.update(*reservation);
    return ReleaseStatus::Released;
}

std::size_t QuotaService::expireDueReservations() {
    const std::scoped_lock lock{mutex_};
    const std::vector<Reservation> due = reservations_.findActiveExpiredBy(nowSeconds());
    for (Reservation reservation : due) {
        expireIfDue(reservation);
    }
    return due.size();
}

}  // namespace tecflix::users
