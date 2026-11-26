#pragma once
// Tecflix: callbacks internos de procesado de streaming hacia metadata.
// Rutas: POST /api/v1/internal/videos/{id}/processing-complete|processing-failed
// Autorizacion separada: SERVICE_TOKEN en Authorization: Bearer <token>.
// Este archivo debe incluirse desde MetadataRoutes.hpp antes de handle_extra_routes.

#include "tecflix_http.hpp"
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <nlohmann/json.hpp>
#include <sodium.h>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace tecflix::metadata::processing_callbacks {
namespace b = bsoncxx::builder::basic;
using Json = nlohmann::json;
using Request = http::request<http::string_body>;
using Response = http::response<http::string_body>;

inline Response result(const Request& req, http::status code, const Json& data = Json::object()) {
    Response res{code, req.version()};
    res.keep_alive(req.keep_alive());
    if (code != http::status::no_content) {
        res.set(http::field::content_type, "application/json");
        res.body() = data.dump();
    }
    res.prepare_payload();
    return res;
}
inline Response error(const Request& req, http::status code,
                      std::string_view name, std::string_view message) {
    return result(req, code, Json{{"code", std::string(name)}, {"message", std::string(message)},
                                  {"details", Json::object()}, {"traceId", ""}});
}
inline bool valid_uuid(std::string_view id) {
    if (id.size() != 36) return false;
    for (std::size_t i = 0; i < id.size(); ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (id[i] != '-') return false;
        } else if (!std::isxdigit(static_cast<unsigned char>(id[i]))) return false;
    }
    return true;
}
inline std::string str(bsoncxx::document::view doc, const char* name) {
    auto field = doc[name];
    if (!field || field.type() != bsoncxx::type::k_string) return "";
    auto s = field.get_string().value;
    return {s.data(), s.size()};
}
inline std::optional<std::int64_t> num(bsoncxx::document::view doc, const char* name) {
    auto field = doc[name];
    if (!field) return std::nullopt;
    if (field.type() == bsoncxx::type::k_int32) return field.get_int32().value;
    if (field.type() == bsoncxx::type::k_int64) return field.get_int64().value;
    return std::nullopt;
}
inline std::string utc_now() {
    auto now = std::chrono::system_clock::now();
    auto raw = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &raw);
#else
    gmtime_r(&raw, &utc);
#endif
    std::ostringstream os;
    os << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return os.str();
}
inline bool service_token_valid(const Request& req) {
    const char* configured = std::getenv("SERVICE_TOKEN");
    if (!configured) return false;
    const std::string_view secret(configured);
    if (secret.size() < 32) return false;
    auto value = req[http::field::authorization];
    const std::string_view header{value.data(), value.size()};
    constexpr std::string_view prefix = "Bearer ";
    if (!header.starts_with(prefix) || header.size() != prefix.size() + secret.size()) return false;
    // Ya se inicializa libsodium para autenticar JWT en este servicio.
    // Hacer comparacion constante en tiempo para el token entre servicios.
    return sodium_memcmp(header.data() + prefix.size(), secret.data(), secret.size()) == 0;
}
inline bool positive_integer(const Json& j, const char* key, std::int64_t& out) {
    if (!j.contains(key) || !j[key].is_number_integer()) return false;
    const Json& value = j[key];
    if (value.is_number_unsigned()) {
        const auto n = value.get<std::uint64_t>();
        if (n > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) return false;
        out = static_cast<std::int64_t>(n);
    } else {
        out = value.get<std::int64_t>();
    }
    return out >= 0;
}
inline bool same_state(bsoncxx::document::view doc, bool complete,
                       std::int64_t duration, std::optional<std::int64_t> bytes,
                       const std::string& reason) {
    const auto status = str(doc, "status");
    if (complete) {
        if (status != "ready" || num(doc, "durationSeconds") != duration) return false;
        return !bytes || num(doc, "sizeBytes") == bytes;
    }
    return status == "failed" && str(doc, "processingError") == reason;
}

inline std::optional<Response> handle(const Request& req, mongocxx::database& db) {
    const std::string target{req.target().data(), req.target().size()};
    constexpr std::string_view prefix = "/api/v1/internal/videos/";
    constexpr std::string_view suffix_ok = "/processing-complete";
    constexpr std::string_view suffix_fail = "/processing-failed";
    if (!std::string_view(target).starts_with(prefix)) return std::nullopt;
    const std::string_view path(target);
    const bool complete = path.ends_with(suffix_ok);
    const bool failed = path.ends_with(suffix_fail);
    if (!complete && !failed) return std::nullopt;  // otras rutas internas: posterior integracion

    if (req.method() != http::verb::post)
        return error(req, http::status::method_not_allowed, "METHOD_NOT_ALLOWED", "Se requiere POST");
    if (!service_token_valid(req))
        return error(req, http::status::unauthorized, "INVALID_TOKEN", "Token de servicio invalido");
    const auto suffix = complete ? suffix_ok : suffix_fail;
    const std::string id = target.substr(prefix.size(), target.size() - prefix.size() - suffix.size());
    if (!valid_uuid(id))
        return error(req, http::status::bad_request, "VALIDATION_ERROR", "ID de video invalido");
    const auto content_type = req[http::field::content_type];
    if (std::string_view(content_type.data(), content_type.size()).find("application/json") != 0)
        return error(req, http::status::unsupported_media_type, "UNSUPPORTED_MEDIA_TYPE", "Se requiere JSON");
    if (req.body().size() > 65536)
        return error(req, http::status::payload_too_large, "PAYLOAD_TOO_LARGE", "JSON demasiado grande");
    Json json = Json::parse(req.body(), nullptr, false);
    if (!json.is_object())
        return error(req, http::status::bad_request, "VALIDATION_ERROR", "JSON invalido");

    std::int64_t duration = 0;
    std::optional<std::int64_t> final_bytes;
    std::string reason;
    if (complete) {
        if ((json.size() != 1 && json.size() != 2) ||
            (json.size() == 2 && !json.contains("finalSizeBytes")) ||
            !positive_integer(json, "durationSeconds", duration))
            return error(req, http::status::bad_request, "VALIDATION_ERROR", "durationSeconds invalido");
        if (json.contains("finalSizeBytes")) {
            std::int64_t n = 0;
            if (!positive_integer(json, "finalSizeBytes", n))
                return error(req, http::status::bad_request, "VALIDATION_ERROR", "finalSizeBytes invalido");
            final_bytes = n;
        }
    } else {
        if (json.size() != 1 || !json.contains("reason") || !json["reason"].is_string())
            return error(req, http::status::bad_request, "VALIDATION_ERROR", "reason invalido");
        reason = json["reason"].get<std::string>();
        if (reason.empty() || reason.size() > 1000)
            return error(req, http::status::bad_request, "VALIDATION_ERROR", "reason debe tener 1..1000 caracteres");
    }

    try {
        auto videos = db["videos"];
        auto live = b::make_document(b::kvp("_id", id),
                                    b::kvp("deletedAt", bsoncxx::types::b_null{}));
        auto doc = videos.find_one(live.view());
        if (!doc) return error(req, http::status::not_found, "NOT_FOUND", "Video no encontrado");
        if (same_state(doc->view(), complete, duration, final_bytes, reason))
            return result(req, http::status::no_content);
        if (str(doc->view(), "status") != "processing")
            return error(req, http::status::conflict, "INVALID_STATE", "Estado de video incompatible");

        Json changes = {{"status", complete ? "ready" : "failed"}, {"updatedAt", utc_now()}};
        if (complete) {
            changes["durationSeconds"] = duration;
            if (final_bytes) changes["sizeBytes"] = *final_bytes;
            changes["processingError"] = "";
        } else {
            changes["processingError"] = reason;
        }
        auto changes_doc = bsoncxx::from_json(changes.dump());
        auto filter = b::make_document(b::kvp("_id", id),
                                      b::kvp("deletedAt", bsoncxx::types::b_null{}),
                                      b::kvp("status", "processing"));
        auto update = b::make_document(b::kvp("$set", changes_doc.view()));
        auto updated = videos.update_one(filter.view(), update.view());
        if (updated && updated->matched_count() == 1) return result(req, http::status::no_content);

        // Si otro callback cambio el estado simultaneamente, verificar idempotencia.
        doc = videos.find_one(live.view());
        if (!doc) return error(req, http::status::not_found, "NOT_FOUND", "Video no encontrado");
        if (same_state(doc->view(), complete, duration, final_bytes, reason))
            return result(req, http::status::no_content);
        return error(req, http::status::conflict, "INVALID_STATE", "Estado de video incompatible");
    } catch (const std::exception&) {
        return error(req, http::status::internal_server_error, "INTERNAL_ERROR", "Error interno en callback");
    }
}
} // namespace tecflix::metadata::processing_callbacks
