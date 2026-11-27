#include "api/InternalQuotaController.hpp"

#include <algorithm>
#include <iterator>
#include <nlohmann/json.hpp>
#include <string_view>

#include "api/ApiResponses.hpp"
#include "api/BearerToken.hpp"
#include "api/HttpStatus.hpp"
#include "domain/User.hpp"
#include "tecflix/auth/ConstantTime.hpp"
#include "util/IsoTime.hpp"

namespace tecflix::users {

namespace {

using nlohmann::json;

constexpr std::string_view kBasePath = "/api/v1/internal/quota/reservations";
constexpr std::size_t kUuidLength = 36;
constexpr std::size_t kUuidDashPositions[] = {8, 13, 18, 23};

bool isDashPosition(std::size_t position) {
    return std::ranges::find(kUuidDashPositions, position) != std::end(kUuidDashPositions);
}

// UUID en texto: 36 caracteres hexadecimales con guiones en las posiciones de siempre.
bool isUuid(std::string_view text) {
    if (text.size() != kUuidLength) {
        return false;
    }
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        const bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        if (isDashPosition(i) ? c != '-' : !hex) {
            return false;
        }
    }
    return true;
}

std::string_view statusName(ReservationStatus status) {
    switch (status) {
        case ReservationStatus::Active:
            return "active";
        case ReservationStatus::Settled:
            return "settled";
        case ReservationStatus::Released:
            return "released";
        case ReservationStatus::Expired:
            return "expired";
    }
    return "active";
}

json reservationJson(const Reservation& reservation) {
    return json{{"reservationId", reservation.id},
                {"userId", reservation.userId},
                {"bytes", reservation.bytes},
                {"expiresAt", formatIsoUtc(reservation.expiresAt)},
                {"status", statusName(reservation.status)},
                {"createdAt", formatIsoUtc(reservation.createdAt)}};
}

void sendInvalid(httplib::Response& response, const std::string& traceId, std::string_view message, const char* field) {
    sendError(response, http_status::kBadRequest, "VALIDATION_ERROR", message, traceId, json{{"field", field}});
}

void sendUnknownReservation(httplib::Response& response, const std::string& traceId) {
    sendError(response, http_status::kNotFound, "NOT_FOUND", "No existe esa reserva", traceId);
}

void sendQuotaExceeded(httplib::Response& response, const std::string& traceId) {
    sendError(response, http_status::kForbidden, "QUOTA_EXCEEDED", "La cuota del usuario no alcanza", traceId);
}

void sendInvalidState(httplib::Response& response, const std::string& traceId) {
    sendError(response, http_status::kConflict, "INVALID_STATE", "La reserva no está en un estado que permita esta operación",
              traceId);
}

}  // namespace

InternalQuotaController::InternalQuotaController(QuotaService& quota, std::string serviceToken,
                                                 const IIdGenerator& traceIds)
    : quota_(quota), serviceToken_(std::move(serviceToken)), traceIds_(traceIds) {}

void InternalQuotaController::registerRoutes(TracedRouter& router) const {
    using Request = httplib::Request;
    using Response = httplib::Response;
    const std::string base{kBasePath};
    router.Post(base, [this](const Request& req, Response& res) { reserve(req, res); });
    router.Post(base + "/:reservationId/settle", [this](const Request& req, Response& res) { settle(req, res); });
    router.Post(base + "/:reservationId/release", [this](const Request& req, Response& res) { release(req, res); });
}

bool InternalQuotaController::authenticate(const httplib::Request& request, httplib::Response& response,
                                           const std::string& traceId) const {
    const std::string header = request.get_header_value("Authorization");
    const std::string_view token = bearerToken(header);
    if (token.empty()) {
        sendError(response, http_status::kUnauthorized, "MISSING_TOKEN", "Falta el token de servicio", traceId);
        return false;
    }
    if (!tecflix::auth::constantTimeEquals(token, serviceToken_)) {
        sendError(response, http_status::kUnauthorized, "INVALID_TOKEN", "El token de servicio no es válido", traceId);
        return false;
    }
    return true;
}

void InternalQuotaController::reserve(const httplib::Request& request, httplib::Response& response) const {
    const std::string traceId = traceIds_.next();
    if (!authenticate(request, response, traceId)) {
        return;
    }
    const auto body = readJsonBody(request, response, traceId);
    if (!body) {
        return;
    }

    const std::string reservationId = body->value("reservationId", "");
    const std::string userId = body->value("userId", "");
    if (!isUuid(reservationId)) {
        sendInvalid(response, traceId, "reservationId debe ser un UUID", "reservationId");
        return;
    }
    if (!isUuid(userId)) {
        sendInvalid(response, traceId, "userId debe ser un UUID", "userId");
        return;
    }
    const auto bytes = body->find("bytes");
    if (bytes == body->end() || !bytes->is_number_unsigned()) {
        sendInvalid(response, traceId, "bytes debe ser un entero mayor o igual a 0", "bytes");
        return;
    }
    std::optional<std::int64_t> expiresAt;
    if (const auto text = body->find("expiresAt"); text != body->end()) {
        expiresAt = text->is_string() ? parseIsoUtc(text->get<std::string>()) : std::nullopt;
        if (!expiresAt) {
            sendInvalid(response, traceId, "expiresAt debe tener el formato 2026-11-12T10:30:00Z", "expiresAt");
            return;
        }
    }

    const ReserveOutcome outcome = quota_.reserve({reservationId, userId, bytes->get<std::uint64_t>(), expiresAt});
    switch (outcome.status) {
        case ReserveStatus::Created:
        case ReserveStatus::AlreadyReserved:
            // users.yaml documenta solo 201 para esta operación: repetir la misma petición (idempotente)
            // devuelve la misma reserva con el mismo código.
            sendJson(response, http_status::kCreated, reservationJson(outcome.reservation));
            return;
        case ReserveStatus::UserNotFound:
            sendError(response, http_status::kNotFound, "NOT_FOUND", "No existe ese usuario", traceId);
            return;
        case ReserveStatus::QuotaExceeded:
            sendQuotaExceeded(response, traceId);
            return;
        case ReserveStatus::Conflict:
            sendError(response, http_status::kConflict, "CONFLICT",
                      "Ya existe una reserva con ese id y otros datos, o ya no está activa", traceId);
            return;
        case ReserveStatus::InvalidExpiry:
            sendInvalid(response, traceId, "expiresAt debe ser una fecha futura", "expiresAt");
            return;
    }
}

void InternalQuotaController::settle(const httplib::Request& request, httplib::Response& response) const {
    const std::string traceId = traceIds_.next();
    if (!authenticate(request, response, traceId)) {
        return;
    }
    const auto body = readJsonBody(request, response, traceId);
    if (!body) {
        return;
    }
    const auto finalBytes = body->find("finalBytes");
    if (finalBytes == body->end() || !finalBytes->is_number_unsigned()) {
        sendInvalid(response, traceId, "finalBytes debe ser un entero mayor o igual a 0", "finalBytes");
        return;
    }

    const SettleOutcome outcome = quota_.settle(request.path_params.at("reservationId"), finalBytes->get<std::uint64_t>());
    switch (outcome.status) {
        case SettleStatus::Settled:
        case SettleStatus::AlreadySettled:
            sendJson(response, http_status::kOk, reservationJson(outcome.reservation));
            return;
        case SettleStatus::NotFound:
            sendUnknownReservation(response, traceId);
            return;
        case SettleStatus::QuotaExceeded:
            sendQuotaExceeded(response, traceId);
            return;
        case SettleStatus::InvalidState:
            sendInvalidState(response, traceId);
            return;
    }
}

void InternalQuotaController::release(const httplib::Request& request, httplib::Response& response) const {
    const std::string traceId = traceIds_.next();
    if (!authenticate(request, response, traceId)) {
        return;
    }
    switch (quota_.release(request.path_params.at("reservationId"))) {
        case ReleaseStatus::Released:
        case ReleaseStatus::AlreadyReleased:
            response.status = http_status::kNoContent;
            return;
        case ReleaseStatus::NotFound:
            sendUnknownReservation(response, traceId);
            return;
    }
}

}  // namespace tecflix::users
