#pragma once
// Coordinacion de borrado fisico Tecflix.
// Endpoint interno: POST /api/v1/internal/videos/{videoId}/streaming-deleted
// Usa la misma proteccion SERVICE_TOKEN que los callbacks E2.
#include "MetadataProcessingCallbacks.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/types.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace tecflix::metadata::deletion_callbacks {
namespace b = bsoncxx::builder::basic;
using Request = processing_callbacks::Request;
using Response = processing_callbacks::Response;

inline int port_from_env(const char* name, int fallback) {
    const char* raw = std::getenv(name);
    if (!raw || !*raw) return fallback;
    try {
        const int p = std::stoi(raw);
        return p >= 1 && p <= 65535 ? p : fallback;
    } catch (...) { return fallback; }
}

// Solo llamadas a los servicios locales configurados, nunca a URLs del usuario.
// Nullopt indica error de red/timeout; un codigo HTTP representa respuesta real.
inline std::optional<unsigned> internal_http(
        const std::string& host, int port, boost::beast::http::verb method,
        const std::string& path, const std::string& token) {
    if (token.size() < 32) return std::nullopt;
    try {
        namespace asio = boost::asio;
        namespace beast = boost::beast;
        namespace http = boost::beast::http;
        asio::io_context io;
        asio::ip::tcp::resolver resolver(io);
        beast::tcp_stream stream(io);
        stream.expires_after(std::chrono::seconds(5));
        stream.connect(resolver.resolve(host, std::to_string(port)));
        http::request<http::string_body> req(method, path, 11);
        req.set(http::field::host, host);
        req.set(http::field::authorization, "Bearer " + token);
        req.keep_alive(false);
        req.prepare_payload();
        http::write(stream, req);
        stream.expires_after(std::chrono::seconds(8));
        beast::flat_buffer buffer;
        http::response<http::string_body> response;
        http::read(stream, buffer, response);
        beast::error_code ignored;
        stream.socket().shutdown(asio::ip::tcp::socket::shutdown_both, ignored);
        return response.result_int();
    } catch (const std::exception& ex) {
        std::cerr << "Peticion interna fallida: " << ex.what() << '\n';
        return std::nullopt;
    }
}

inline std::optional<unsigned> request_streaming_delete(const std::string& id) {
    const char* token = std::getenv("SERVICE_TOKEN");
    const char* host = std::getenv("STREAMING_API_HOST");
    return internal_http(host && *host ? host : "127.0.0.1",
                         port_from_env("STREAMING_API_PORT", 8083),
                         boost::beast::http::verb::delete_,
                         "/api/v1/internal/videos/" + id,
                         token ? token : "");
}

inline std::optional<unsigned> release_video_quota(const std::string& id) {
    const char* token = std::getenv("SERVICE_TOKEN");
    const char* host = std::getenv("USERS_API_HOST");
    return internal_http(host && *host ? host : "127.0.0.1",
                         port_from_env("USERS_API_PORT", 8080),
                         boost::beast::http::verb::post,
                         "/api/v1/internal/quota/reservations/" + id + "/release",
                         token ? token : "");
}

inline std::optional<Response> handle(const Request& req, mongocxx::database& db) {
    const std::string target(req.target().data(), req.target().size());
    constexpr std::string_view prefix = "/api/v1/internal/videos/";
    constexpr std::string_view suffix = "/streaming-deleted";
    if (!std::string_view(target).starts_with(prefix) ||
        !std::string_view(target).ends_with(suffix)) return std::nullopt;
    if (req.method() != boost::beast::http::verb::post)
        return processing_callbacks::error(req, boost::beast::http::status::method_not_allowed,
                                           "METHOD_NOT_ALLOWED", "Se requiere POST");
    if (!processing_callbacks::service_token_valid(req))
        return processing_callbacks::error(req, boost::beast::http::status::unauthorized,
                                           "INVALID_TOKEN", "Token de servicio invalido");
    const std::string id = target.substr(prefix.size(), target.size()-prefix.size()-suffix.size());
    if (!processing_callbacks::valid_uuid(id))
        return processing_callbacks::error(req, boost::beast::http::status::bad_request,
                                           "VALIDATION_ERROR", "ID invalido");
    try {
        auto videos = db["videos"];
        auto filter = b::make_document(b::kvp("_id", id));
        auto doc = videos.find_one(filter.view());
        // Un callback repetido tras borrado completo es idempotente.
        if (!doc) return processing_callbacks::result(req, boost::beast::http::status::no_content);
        const auto status = processing_callbacks::str(doc->view(), "deletionStatus");
        if (status != "pending_streaming_deletion")
            return processing_callbacks::error(req, boost::beast::http::status::conflict,
                                              "INVALID_STATE", "Borrado no solicitado por el propietario");

        // La cuota solo se libera DESPUES de que streaming confirma que borro los archivos.
        const auto released = release_video_quota(id);
        if (!released || (*released != 204 && *released != 404))
            return processing_callbacks::error(req, boost::beast::http::status::bad_gateway,
                                               "DEPENDENCY_ERROR", "Usuarios no confirmo liberacion de cuota");
        // 404 significa que nunca existio reserva: video creado sin subida.
        if (*released == 404) std::cerr << "Video sin reserva de cuota: " << id << '\n';
        auto pending = b::make_document(b::kvp("_id", id),
                                        b::kvp("deletionStatus", "pending_streaming_deletion"));
        videos.delete_one(pending.view());
        return processing_callbacks::result(req, boost::beast::http::status::no_content);
    } catch (const std::exception& ex) {
        std::cerr << "Callback streaming-deleted: " << ex.what() << '\n';
        return processing_callbacks::error(req, boost::beast::http::status::internal_server_error,
                                           "INTERNAL_ERROR", "No se pudo finalizar el borrado");
    }
}
}  // namespace tecflix::metadata::deletion_callbacks
