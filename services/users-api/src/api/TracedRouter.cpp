#include "api/TracedRouter.hpp"

#include <chrono>
#include <exception>
#include <iostream>
#include <nlohmann/json.hpp>

#include "api/ApiResponses.hpp"
#include "api/HttpStatus.hpp"
#include "tecflix/telemetry/ServerSpan.hpp"

namespace tecflix::users {

namespace {

// Si el endpoint solo puso contenido, httplib todavía no fijó el código (vale -1) y luego usa 200.
int finalStatus(const httplib::Response& response) {
    return response.status == -1 ? http_status::kOk : response.status;
}

// Registra el error con los identificadores que permiten encontrar su traza en el dashboard.
void logFailure(const std::string& serviceName, const tecflix::telemetry::ServerSpan& span, const std::string& reason) {
    std::cerr << nlohmann::json{{"level", "error"},
                                {"service", serviceName},
                                {"trace_id", span.traceId()},
                                {"span_id", span.spanId()},
                                {"message", reason}}
                     .dump()
              << '\n' << std::flush;
}

}  // namespace

TracedRouter::TracedRouter(httplib::Server& server, tecflix::telemetry::IHttpServerMetrics& metrics,
                           const IIdGenerator& traceIds, std::string serviceName)
    : server_(server), metrics_(metrics), traceIds_(traceIds), serviceName_(std::move(serviceName)) {}

httplib::Server::Handler TracedRouter::wrap(const std::string& method, const std::string& pattern,
                                            httplib::Server::Handler handler) const {
    return [this, method, pattern, handler = std::move(handler)](const httplib::Request& request, httplib::Response& response) {
        const auto start = std::chrono::steady_clock::now();
        tecflix::telemetry::ServerSpan span{
            serviceName_, method, pattern, request.path,
            [&request](std::string_view name) { return request.get_header_value(std::string{name}); }};
        try {
            handler(request, response);
        } catch (const std::exception& error) {
            logFailure(serviceName_, span, error.what());
            // El cliente recibe el id de la traza, pero no la causa interna.
            sendError(response, http_status::kInternalServerError, "INTERNAL_ERROR", "Error interno del servidor",
                      traceIds_.next());
        } catch (...) {
            logFailure(serviceName_, span, "error desconocido");
            sendError(response, http_status::kInternalServerError, "INTERNAL_ERROR", "Error interno del servidor",
                      traceIds_.next());
        }
        const int status = finalStatus(response);
        span.finish(status);
        metrics_.record(method, pattern, status,
                        std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());
    };
}

void TracedRouter::Get(const std::string& pattern, httplib::Server::Handler handler) {
    server_.Get(pattern, wrap("GET", pattern, std::move(handler)));
}

void TracedRouter::Post(const std::string& pattern, httplib::Server::Handler handler) {
    server_.Post(pattern, wrap("POST", pattern, std::move(handler)));
}

void TracedRouter::Patch(const std::string& pattern, httplib::Server::Handler handler) {
    server_.Patch(pattern, wrap("PATCH", pattern, std::move(handler)));
}

void TracedRouter::Delete(const std::string& pattern, httplib::Server::Handler handler) {
    server_.Delete(pattern, wrap("DELETE", pattern, std::move(handler)));
}

}  // namespace tecflix::users
