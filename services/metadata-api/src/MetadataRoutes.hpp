#pragma once
#include "MetadataProcessingCallbacks.hpp"
#include "MetadataDeletionCallbacks.hpp"

// Tecflix metadata: POST, GET listado + cursor, GET por ID, PATCH y DELETE logico.
// Usa el JWT compartido de la API de usuarios y MongoDB C++ Driver 4.x.
// Integrar al comienzo del handler HTTP existente (antes de las rutas anteriores).

#include "tecflix_http.hpp"
#include "tecflix/auth/Base64Url.hpp"
#include "tecflix/auth/CryptoRuntime.hpp"
#include "tecflix/auth/JwtVerifier.hpp"
#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/options/find.hpp>
#include <mongocxx/options/index.hpp>
#include <nlohmann/json.hpp>
#include <sodium.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <map>
#include <limits>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tecflix::metadata {

namespace detail {

namespace b = bsoncxx::builder::basic;
using Json = nlohmann::json;
using Request = http::request<http::string_body>;
using Response = http::response<http::string_body>;

inline Response respond(const Request& request, http::status status, Json body) {
    Response r{status, request.version()};
    r.set(http::field::content_type, "application/json");
    r.keep_alive(request.keep_alive());
    if (status != http::status::no_content) r.body() = body.dump();
    r.prepare_payload();
    return r;
}

inline Response error(const Request& request, http::status status,
                    const std::string& code, const std::string& message) {
    return respond(request, status, Json{{"code", code}, {"message", message},
                                        {"details", Json::object()}, {"traceId", ""}});
}

inline std::string str(bsoncxx::document::view doc, const char* name,
                    const std::string& fallback = "") {
    auto e = doc[name];
    if (e && e.type() == bsoncxx::type::k_string) {
        auto v = e.get_string().value;
        return {v.data(), v.size()};
    }
    return fallback;
}

inline std::int64_t number(bsoncxx::document::view doc, const char* name) {
    auto e = doc[name];
    if (!e) return 0;
    if (e.type() == bsoncxx::type::k_int32) return e.get_int32().value;
    if (e.type() == bsoncxx::type::k_int64) return e.get_int64().value;
    return 0;
}

inline Json video_json(bsoncxx::document::view doc) {
    Json tags = Json::array();
    auto e = doc["tags"];
    if (e && e.type() == bsoncxx::type::k_array) {
        for (auto item : e.get_array().value) {
            if (item.type() == bsoncxx::type::k_string) {
                auto v = item.get_string().value;
                tags.push_back(std::string{v.data(), v.size()});
            }
        }
    }
    return Json{
        {"id", str(doc, "_id")},
        {"ownerId", str(doc, "ownerId")},
        {"ownerUsername", str(doc, "ownerUsername")},
        {"title", str(doc, "title")},
        {"description", str(doc, "description")},
        {"author", str(doc, "author")},
        {"topic", str(doc, "topic")},
        {"tags", std::move(tags)},
        {"visibility", str(doc, "visibility", "private")},
        {"status", str(doc, "status", "processing")},
        {"sizeBytes", number(doc, "sizeBytes")},
        {"durationSeconds", number(doc, "durationSeconds")},
        {"uploadedAt", str(doc, "uploadedAt")},
        {"createdAt", str(doc, "createdAt")},
        {"updatedAt", str(doc, "updatedAt")}
    };
}

inline bool hex_char(char ch) {
    return std::isxdigit(static_cast<unsigned char>(ch)) != 0;
}

// Acepta IDs hexadecimales antiguos de 24 caracteres y UUID de 36 (contrato nuevo).
inline bool valid_id(std::string_view id) {
    if (id.size() == 24) return std::all_of(id.begin(), id.end(), hex_char);
    if (id.size() != 36) return false;
    for (std::size_t i = 0; i < id.size(); ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (id[i] != '-') return false;
        } else if (!hex_char(id[i])) {
            return false;
        }
    }
    return true;
}

inline std::optional<std::string> percent_decode(std::string_view v) {
    std::string result;
    result.reserve(v.size());
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (v[i] == '+') result.push_back(' ');
        else if (v[i] == '%') {
            if (i + 2 >= v.size() || !hex_char(v[i + 1]) || !hex_char(v[i + 2]))
                return std::nullopt;
            auto digit = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                return c - 'A' + 10;
            };
            result.push_back(static_cast<char>(digit(v[i + 1]) * 16 + digit(v[i + 2])));
            i += 2;
        } else result.push_back(v[i]);
    }
    return result;
}

inline std::optional<std::map<std::string, std::string>> parse_query(std::string_view query) {
    std::map<std::string, std::string> result;
    while (!query.empty()) {
        const auto pos = query.find('&');
        const auto part = query.substr(0, pos);
        const auto eq = part.find('=');
        if (part.empty() || eq == std::string_view::npos) return std::nullopt;
        auto key = percent_decode(part.substr(0, eq));
        auto val = percent_decode(part.substr(eq + 1));
        if (!key || !val || !result.emplace(*key, *val).second) return std::nullopt;
        if (pos == std::string_view::npos) break;
        query.remove_prefix(pos + 1);
    }
    return result;
}

inline std::string now_utc() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t raw = std::chrono::system_clock::to_time_t(now);
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

inline bool valid_iso_date(const std::string& s) {
    static const std::regex re(
        R"(^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}([.][0-9]+)?(Z|[+-][0-9]{2}:[0-9]{2})$)");
    return std::regex_match(s, re);
}

inline bool valid_patch(const Json& j, bool visibility_only) {
    if (!j.is_object() || j.empty()) return false;
    if (visibility_only) {
        return j.size() == 1 && j.contains("visibility") &&
            j["visibility"].is_string() &&
            (j["visibility"] == "public" || j["visibility"] == "private");
    }
    for (auto it = j.begin(); it != j.end(); ++it) {
        const std::string name = it.key();
        const auto& value = it.value();
        if (name == "tags") {
            if (!value.is_array() || value.size() > 30) return false;
            for (const auto& tag : value) {
                if (!tag.is_string() || tag.get<std::string>().size() > 50) return false;
            }
            continue;
        }
        std::size_t max_size = 0;
        if (name == "title" || name == "author") max_size = 200;
        else if (name == "description") max_size = 5000;
        else if (name == "topic") max_size = 100;
        else if (name == "uploadedAt") max_size = 40;
        else return false;
        if (!value.is_string()) return false;
        const auto str_value = value.get<std::string>();
        if (str_value.size() > max_size) return false;
        if (name == "title" && str_value.empty()) return false;
        if (name == "uploadedAt" && !valid_iso_date(str_value)) return false;
    }
    return true;
}

inline bool valid_create(const Json& j) {
    if (!j.is_object() || j.size() < 7 || j.size() > 8) return false;
    for (const auto& required : {"title", "description", "author", "topic", "tags", "visibility", "sizeBytes"}) {
        if (!j.contains(required)) return false;
    }
    for (auto it = j.begin(); it != j.end(); ++it) {
        const auto key = it.key();
        if (key != "title" && key != "description" && key != "author" &&
            key != "topic" && key != "tags" && key != "visibility" &&
            key != "uploadedAt" && key != "sizeBytes") return false;
    }
    if (!j["visibility"].is_string() ||
        (j["visibility"] != "public" && j["visibility"] != "private")) return false;
    if (!j["sizeBytes"].is_number_integer() || j["sizeBytes"] <= 0 ||
        (j["sizeBytes"].is_number_unsigned() &&
        j["sizeBytes"].get<std::uint64_t>() >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))) return false;
    if (!j["title"].is_string() || j["title"].get<std::string>().empty() ||
        j["title"].get<std::string>().size() > 200) return false;
    if (!j["description"].is_string() || j["description"].get<std::string>().size() > 5000) return false;
    if (!j["author"].is_string() || j["author"].get<std::string>().size() > 200) return false;
    if (!j["topic"].is_string() || j["topic"].get<std::string>().size() > 100) return false;
    if (!j["tags"].is_array() || j["tags"].size() > 30) return false;
    for (const auto& tag : j["tags"]) {
        if (!tag.is_string() || tag.get<std::string>().size() > 50) return false;
    }
    if (j.contains("uploadedAt") &&
        (!j["uploadedAt"].is_string() ||
        j["uploadedAt"].get<std::string>().size() > 40 ||
        !valid_iso_date(j["uploadedAt"].get<std::string>()))) return false;
    return true;
}

inline std::string uuid_v4() {
    auth::ensureCryptoReady();
    std::array<unsigned char, 16> bytes{};
    randombytes_buf(bytes.data(), bytes.size());
    bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0F) | 0x40);
    bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3F) | 0x80);
    constexpr char hex[] = "0123456789abcdef";
    std::string id;
    id.reserve(36);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) id.push_back('-');
        id.push_back(hex[bytes[i] >> 4]);
        id.push_back(hex[bytes[i] & 0x0F]);
    }
    return id;
}

inline Response create_video(const Request& request, mongocxx::database& db,
                            const auth::JwtClaims& claims) {
    const auto h = request[http::field::content_type];
    if (std::string_view(h.data(), h.size()).find("application/json") != 0)
        return error(request, http::status::unsupported_media_type,
                    "UNSUPPORTED_MEDIA_TYPE", "Se requiere application/json");
    if (request.body().size() > 65536)
        return error(request, http::status::payload_too_large,
                    "PAYLOAD_TOO_LARGE", "JSON demasiado grande");
    auto body = Json::parse(request.body(), nullptr, false);
    if (!valid_create(body))
        return error(request, http::status::bad_request,
                    "VALIDATION_ERROR", "Campos requeridos o formato invalidos");

    const auto now = now_utc();
    body["_id"] = uuid_v4();
    body["ownerId"] = claims.subject;
    body["ownerUsername"] = claims.username;
    body["status"] = "processing";
    body["durationSeconds"] = 0;
    if (!body.contains("uploadedAt")) body["uploadedAt"] = now;
    body["createdAt"] = now;
    body["updatedAt"] = now;
    // deletedAt ausente equivale a null en el filtro MongoDB de videos activos.
    auto doc = bsoncxx::from_json(body.dump());
    db["videos"].insert_one(doc.view());
    return respond(request, http::status::created, video_json(doc.view()));
}

inline std::optional<auth::JwtClaims> authenticate(
        const Request& request, const auth::JwtVerifier& verifier) {
    const auto h = request[http::field::authorization];
    const std::string_view value{h.data(), h.size()};
    if (!value.starts_with("Bearer ") || value.size() == 7) return std::nullopt;
    const auto verified = verifier.verify(value.substr(7));
    if (verified.error != auth::TokenError::None) return std::nullopt;
    return verified.claims;
}

inline std::optional<Json> decode_cursor(std::string_view value) {
    if (value.size() > 4096) return std::nullopt;
    const auto decoded = auth::base64UrlDecode(value);
    if (!decoded) return std::nullopt;
    Json j = Json::parse(*decoded, nullptr, false);
    if (!j.is_object()) return std::nullopt;
    if (!j.contains("v") || !j["v"].is_number_integer() || j["v"] != 1 ||
        !j.contains("sortBy") || !j["sortBy"].is_string() ||
        !j.contains("key") || !j["key"].is_string() ||
        !j.contains("id") || !j["id"].is_string() ||
        !j.contains("owner") || !j["owner"].is_string() ||
        !j.contains("subject") || !j["subject"].is_string() ||
        !j.contains("q") || !j["q"].is_string() ||
        !j.contains("order") || !j["order"].is_string()) return std::nullopt;
    return j;
}

inline Response list_videos(const Request& request, mongocxx::database& db,
                            const auth::JwtClaims& claims, std::string_view query) {
    auto params = parse_query(query);
    if (!params) return error(request, http::status::bad_request, "VALIDATION_ERROR", "Consulta invalida");
    for (const auto& [key, val] : *params) {
        if (key != "sortBy" && key != "order" && key != "cursor" &&
            key != "pageSize" && key != "q" && key != "owner") {
            return error(request, http::status::bad_request, "VALIDATION_ERROR", "Filtro no permitido");
        }
    }
    auto get = [&](const std::string& key, const std::string& fallback = "") {
        auto it = params->find(key);
        return it == params->end() ? fallback : it->second;
    };

    const std::string sort_by = get("sortBy", "uploadedAt");
    const std::string order = get("order", "desc");
    const std::string owner = get("owner");
    const std::string search = get("q");
    if ((sort_by != "title" && sort_by != "author" && sort_by != "uploadedAt") ||
        (order != "asc" && order != "desc") ||
        (!owner.empty() && owner != "me") || search.size() > 200)
        return error(request, http::status::bad_request, "VALIDATION_ERROR", "Parametros invalidos");

    int page_size = 20;
    auto size_str = get("pageSize");
    if (!size_str.empty()) {
        const auto [end, ec] = std::from_chars(size_str.data(), size_str.data() + size_str.size(), page_size);
        if (ec != std::errc{} || end != size_str.data() + size_str.size() ||
            page_size < 1 || page_size > 50)
            return error(request, http::status::bad_request, "VALIDATION_ERROR", "pageSize debe ser 1..50");
    } else if (params->contains("pageSize")) {
        return error(request, http::status::bad_request, "VALIDATION_ERROR", "pageSize invalido");
    }
    const int direction = order == "asc" ? 1 : -1;
    const std::string field = sort_by == "author" ? "ownerUsername" : sort_by;

    b::array conditions;
    conditions.append(b::make_document(b::kvp("deletedAt", bsoncxx::types::b_null{})));
    if (owner == "me") {
        conditions.append(b::make_document(b::kvp("ownerId", claims.subject)));
    } else {
        b::array allowed;
        allowed.append(b::make_document(b::kvp("ownerId", claims.subject)));
        allowed.append(b::make_document(b::kvp("visibility", "public")));
        conditions.append(b::make_document(b::kvp("$or", allowed.extract())));
    }
    if (!search.empty()) {
        // Busca texto sin depender de un indice construido durante el arranque.
        // Escapamos los caracteres especiales para tratar la consulta literalmente.
        std::string safe_pattern;
        safe_pattern.reserve(search.size() * 2);
        for (char ch : search) {
            if (std::string_view{R"(\.^$|?*+()[]{} )"}.find(ch) != std::string_view::npos)
                safe_pattern.push_back('\\');
            safe_pattern.push_back(ch);
        }
        b::array fields;
        for (const char* name : {"title", "tags", "topic"}) {
            fields.append(b::make_document(b::kvp(std::string{name}, b::make_document(
            b::kvp("$regex", safe_pattern), b::kvp("$options", "i")))));

        }
        conditions.append(b::make_document(b::kvp("$or", fields.extract())));
    }

    const std::string cursor_value = get("cursor");
    if (!cursor_value.empty()) {
        const auto decoded = decode_cursor(cursor_value);
        if (!decoded || decoded->value("sortBy", "") != sort_by ||
            decoded->value("order", "") != order ||
            decoded->value("owner", "") != owner ||
            decoded->value("subject", "") != claims.subject ||
            decoded->value("q", "") != search ||
            !valid_id((*decoded)["id"].get<std::string>()))
            return error(request, http::status::bad_request, "INVALID_CURSOR", "Cursor invalido");

        const std::string last_key = (*decoded)["key"].get<std::string>();
        const std::string last_id = (*decoded)["id"].get<std::string>();
        const std::string cmp = direction == 1 ? "$gt" : "$lt";
        b::array next;
        next.append(b::make_document(
            b::kvp(field, b::make_document(b::kvp(cmp, last_key)))));
        next.append(b::make_document(
            b::kvp(field, last_key),
            b::kvp("_id", b::make_document(b::kvp(cmp, last_id)))));
        conditions.append(b::make_document(b::kvp("$or", next.extract())));
    } else if (params->contains("cursor")) {
        return error(request, http::status::bad_request, "INVALID_CURSOR", "Cursor vacio");
    }

    b::document filter;
    filter.append(b::kvp("$and", conditions.extract()));
    mongocxx::options::find options;
    options.sort(b::make_document(b::kvp(field, direction), b::kvp("_id", direction)));
    options.limit(static_cast<std::int64_t>(page_size) + 1);
    options.collation(b::make_document(b::kvp("locale", "en"), b::kvp("strength", 2)));

    Json items = Json::array();
    bool has_more = false;
    std::string last_key, last_id;
    auto results = db["videos"].find(filter.view(), options);
    for (auto&& doc : results) {
        if (items.size() >= static_cast<std::size_t>(page_size)) {
            has_more = true;
            break;
        }
        last_key = str(doc, field.c_str());
        last_id = str(doc, "_id");
        items.push_back(video_json(doc));
    }
    Json next_cursor = nullptr;
    if (has_more) {
        const Json token = {{"v", 1}, {"sortBy", sort_by}, {"order", order},
                            {"owner", owner}, {"subject", claims.subject},
                            {"q", search}, {"key", last_key}, {"id", last_id}};
        next_cursor = auth::base64UrlEncode(token.dump());
    }
    return respond(request, http::status::ok,
                Json{{"items", std::move(items)}, {"nextCursor", std::move(next_cursor)}});
}

inline Response get_video(const Request& request, mongocxx::database& db,
                        const auth::JwtClaims& claims, const std::string& id) {
    if (!valid_id(id)) return error(request, http::status::bad_request, "VALIDATION_ERROR", "ID invalido");
    auto doc = db["videos"].find_one(b::make_document(b::kvp("_id", id),
        b::kvp("deletedAt", bsoncxx::types::b_null{})));
    if (!doc) return error(request, http::status::not_found, "NOT_FOUND", "Video no encontrado");
    const auto view = doc->view();
    if (str(view, "ownerId") != claims.subject && str(view, "visibility") != "public")
        return error(request, http::status::not_found, "NOT_FOUND", "Video no encontrado");
    return respond(request, http::status::ok, video_json(view));
}

inline Response patch_video(const Request& request, mongocxx::database& db,
                            const auth::JwtClaims& claims, const std::string& id,
                            bool visibility_only) {
    if (!valid_id(id)) return error(request, http::status::bad_request, "VALIDATION_ERROR", "ID invalido");
    auto doc = db["videos"].find_one(b::make_document(b::kvp("_id", id),
        b::kvp("deletedAt", bsoncxx::types::b_null{})));
    if (!doc) return error(request, http::status::not_found, "NOT_FOUND", "Video no encontrado");
    if (str(doc->view(), "ownerId") != claims.subject)
        return error(request, http::status::forbidden, "FORBIDDEN", "Solo el propietario puede editar");

    const auto content_type = request[http::field::content_type];
    if (std::string_view(content_type.data(), content_type.size()).find("application/json") != 0)
        return error(request, http::status::unsupported_media_type, "UNSUPPORTED_MEDIA_TYPE", "Se requiere application/json");
    if (request.body().size() > 65536)
        return error(request, http::status::payload_too_large, "PAYLOAD_TOO_LARGE", "JSON demasiado grande");
    Json update = Json::parse(request.body(), nullptr, false);
    if (!valid_patch(update, visibility_only))
        return error(request, http::status::bad_request, "VALIDATION_ERROR", "Campos invalidos o no editables");

    update["updatedAt"] = now_utc();
    const auto changes = bsoncxx::from_json(update.dump());
    auto filter = b::make_document(b::kvp("_id", id), b::kvp("ownerId", claims.subject),
                                b::kvp("deletedAt", bsoncxx::types::b_null{}));
    auto set_doc = b::make_document(b::kvp("$set", changes.view()));
    auto result = db["videos"].update_one(filter.view(), set_doc.view());
    if (!result || result->matched_count() == 0)
        return error(request, http::status::not_found, "NOT_FOUND", "Video no encontrado");
    auto current = db["videos"].find_one(filter.view());
    if (!current) return error(request, http::status::not_found, "NOT_FOUND", "Video no encontrado");
    return respond(request, http::status::ok, video_json(current->view()));
}

// DELETE coordinado con streaming; la cuota se libera en streaming-deleted.
inline Response delete_video(const Request& request, mongocxx::database& db,
                             const auth::JwtClaims& claims, const std::string& id) {
    if (!valid_id(id)) return error(request, http::status::bad_request, "VALIDATION_ERROR", "ID invalido");
    auto filter = b::make_document(b::kvp("_id", id));
    auto doc = db["videos"].find_one(filter.view());
    if (!doc) return error(request, http::status::not_found, "NOT_FOUND", "Video no encontrado");
    if (str(doc->view(), "ownerId") != claims.subject)
        return error(request, http::status::forbidden, "FORBIDDEN", "Solo el propietario puede eliminar");

    const std::string deletionStatus = str(doc->view(), "deletionStatus");
    if (!deletionStatus.empty() && deletionStatus != "pending_streaming_deletion")
        return error(request, http::status::conflict, "INVALID_STATE", "Eliminacion incompatible");
    const bool wasPending = deletionStatus == "pending_streaming_deletion";
    const auto now = now_utc();
    if (!wasPending) {
        auto liveFilter = b::make_document(b::kvp("_id", id), b::kvp("ownerId", claims.subject),
                                        b::kvp("deletedAt", bsoncxx::types::b_null{}));
        auto update = b::make_document(b::kvp("$set", b::make_document(
            b::kvp("deletedAt", now), b::kvp("deletionStatus", "pending_streaming_deletion"),
            b::kvp("updatedAt", now))));
        auto changed = db["videos"].update_one(liveFilter.view(), update.view());
        if (!changed || changed->matched_count() == 0)
            return error(request, http::status::conflict, "INVALID_STATE", "Video en eliminacion");
    }

    const auto code = deletion_callbacks::request_streaming_delete(id);
    if (code && *code == 202) return respond(request, http::status::no_content, Json::object());

    // Si streaming RECHAZO el borrado (por ejemplo, subida activa), es seguro
    // deshacer el tombstone. Si hubo timeout, el resultado es ambiguo: mantener
    // el pendiente para evitar revivir un video cuyos archivos ya se borran.
    if (code && !wasPending) {
        auto pending = b::make_document(b::kvp("_id", id), b::kvp("ownerId", claims.subject),
                                        b::kvp("deletionStatus", "pending_streaming_deletion"));
        auto undo = b::make_document(
            b::kvp("$unset", b::make_document(b::kvp("deletedAt", ""), b::kvp("deletionStatus", ""))),
            b::kvp("$set", b::make_document(b::kvp("updatedAt", now_utc()))));
        db["videos"].update_one(pending.view(), undo.view());
    }
    if (code && *code == 409)
        return error(request, http::status::conflict, "INVALID_STATE",
                    "Hay una subida activa; cancelala antes de eliminar el video");
    return error(request, http::status::bad_gateway, "DEPENDENCY_ERROR",
                "Streaming no confirmo el borrado; reintenta DELETE con el mismo videoId");
}

}  // namespace detail

// Ejecutar al iniciar main(), luego de confirmar conexion a MongoDB.
// MongoDB crea los indices solo si no existen; los siguientes arranques reutilizan su definicion.
inline void ensure_indexes(mongocxx::database& db) {
    namespace b = bsoncxx::builder::basic;

    b::array indexes;

    for (const std::string scope : {"ownerId", "visibility"}) {
        for (const std::string field : {
            "title", "ownerUsername", "uploadedAt"
        }) {
            indexes.append(b::make_document(
                b::kvp("key", b::make_document(
                    b::kvp(scope, 1),
                    b::kvp(field, 1),
                    b::kvp("_id", 1)
                )),
                b::kvp("name", "idx_" + scope + "_" + field),
                b::kvp("collation", b::make_document(
                    b::kvp("locale", "en"),
                    b::kvp("strength", 2)
                ))
            ));
        }
    }

    auto command = b::make_document(
        b::kvp("createIndexes", "videos"),
        b::kvp("indexes", indexes.extract())
    );

    db.run_command(command.view());
}
    // La creacion de indices NO debe ejecutarse mientras diagnosticamos
    // el abort() nativo que surgio al incorporar el inicio de metadata.
    // Las rutas siguen funcionando sin indices, aunque las consultas
    // pueden ser mas lentas con colecciones grandes.
    // Cuando se confirme el arranque, migraremos los indices por separado.

// Devuelve nullopt para rutas que NO atiende este modulo (ej. /health).
// Agregar esta llamada al comienzo del handler HTTP existente para evitar colisiones.
inline std::optional<detail::Response> handle_extra_routes(
        const detail::Request& request, mongocxx::database& db,
        const auth::JwtVerifier& verifier) {
    if (auto deleted = deletion_callbacks::handle(request, db)) return std::move(*deleted);
    if (auto cb = processing_callbacks::handle(request, db)) return std::move(*cb);
    const std::string target{request.target().data(), request.target().size()};
    const auto question = target.find('?');
    const std::string path = target.substr(0, question);
    const std::string_view query = question == std::string::npos
        ? std::string_view{} : std::string_view{target}.substr(question + 1);
    const std::string prefix = "/api/v1/videos/";
    const bool is_list = path == "/api/v1/videos" && request.method() == http::verb::get;
    const bool is_create = path == "/api/v1/videos" && request.method() == http::verb::post;
    const bool is_detail = path.starts_with(prefix) &&
        (request.method() == http::verb::get || request.method() == http::verb::patch ||
        request.method() == http::verb::delete_);
    if (!is_list && !is_create && !is_detail) return std::nullopt;

    const auto claims = detail::authenticate(request, verifier);
    if (!claims)
        return detail::error(request, http::status::unauthorized, "INVALID_TOKEN",
                            "Falta token Bearer valido o ha expirado");

    try {
        if (is_list) return detail::list_videos(request, db, *claims, query);
        if (is_create) {
            if (!query.empty()) return detail::error(request, http::status::bad_request,
                "VALIDATION_ERROR", "No se admiten parametros aqui");
            return detail::create_video(request, db, *claims);
        }
        if (!query.empty())
            return detail::error(request, http::status::bad_request, "VALIDATION_ERROR", "No se admiten parametros aqui");
        std::string id = path.substr(prefix.size());
        const std::string vis_suffix = "/visibility";
        const bool is_visibility = request.method() == http::verb::patch &&
                                id.ends_with(vis_suffix);
        if (is_visibility) id.resize(id.size() - vis_suffix.size());
        if (id.find('/') != std::string::npos)
            return detail::error(request, http::status::bad_request, "VALIDATION_ERROR", "Ruta invalida");

        if (request.method() == http::verb::get) return detail::get_video(request, db, *claims, id);
        if (request.method() == http::verb::patch)
            return detail::patch_video(request, db, *claims, id, is_visibility);
        return detail::delete_video(request, db, *claims, id);
    } catch (const std::exception&) {
        return detail::error(request, http::status::internal_server_error, "INTERNAL_ERROR",
                            "Error interno de metadata");
    }
}

}  // namespace tecflix::metadata
