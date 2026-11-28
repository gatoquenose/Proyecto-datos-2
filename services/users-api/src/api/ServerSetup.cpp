#include "api/ServerSetup.hpp"

#include <exception>
#include <iostream>
#include <string>

#include "api/ApiResponses.hpp"
#include "api/HttpStatus.hpp"

namespace tecflix::users {

namespace {

constexpr std::size_t kMaxBodyBytes = std::size_t{64} * 1024;

void describeUnhandled(const IIdGenerator& traceIds, httplib::Response& response, const std::exception_ptr& error) {
    const std::string traceId = traceIds.next();
    std::string reason = "error desconocido";
    try {
        std::rethrow_exception(error);
    } catch (const std::exception& exception) {
        reason = exception.what();
    } catch (...) {
        reason = "excepción que no es std::exception";
    }
    std::cerr << nlohmann::json{{"level", "error"}, {"service", "users-api"}, {"traceId", traceId}, {"message", reason}}.dump()
              << '\n' << std::flush;
    // Al cliente no se le dice la causa interna.
    sendError(response, http_status::kInternalServerError, "INTERNAL_ERROR", "Error interno del servidor",
              traceId);
}

std::string_view errorCodeFor(int status) {
    switch (status) {
        case http_status::kNotFound:
            return "NOT_FOUND";
        case http_status::kPayloadTooLarge:
            return "PAYLOAD_TOO_LARGE";
        case http_status::kUnsupportedMediaType:
            return "UNSUPPORTED_MEDIA_TYPE";
        default:
            return "VALIDATION_ERROR";
    }
}

}  // namespace

void configureServer(httplib::Server& server, const IIdGenerator& traceIds) {
    server.set_payload_max_length(kMaxBodyBytes);

    // Solo se completa la respuesta si ningún endpoint puso ya su propio error.
    server.set_error_handler([&traceIds](const httplib::Request&, httplib::Response& response) {
        if (response.body.empty()) {
            sendError(response, response.status, errorCodeFor(response.status), "La petición no se pudo atender",
                      traceIds.next());
        }
    });
    server.set_exception_handler([&traceIds](const httplib::Request&, httplib::Response& response, const std::exception_ptr& error) {
        describeUnhandled(traceIds, response, error);
    });
    server.set_logger([](const httplib::Request& request, const httplib::Response& response) {
        std::cout << nlohmann::json{{"level", "info"},
                                    {"service", "users-api"},
                                    {"method", request.method},
                                    {"path", request.path},
                                    {"status", response.status}}
                         .dump()
                  << '\n' << std::flush;
    });
}

}  // namespace tecflix::users
