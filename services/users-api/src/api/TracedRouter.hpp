#pragma once

#include <httplib.h>

#include <string>

#include "domain/IIdGenerator.hpp"
#include "tecflix/telemetry/IHttpServerMetrics.hpp"

namespace tecflix::users {

// Registra las rutas en el servidor envolviendo cada una con su traza y sus métricas HTTP.
// Cada petición abre una traza (continuando la del cliente si trae "traceparent"), mide su duración
// y, si el endpoint lanza una excepción, responde 500 con el id de la traza y la registra como error.
class TracedRouter {
public:
    TracedRouter(httplib::Server& server, tecflix::telemetry::IHttpServerMetrics& metrics, const IIdGenerator& traceIds,
                 std::string serviceName);

    void Get(const std::string& pattern, httplib::Server::Handler handler);
    void Post(const std::string& pattern, httplib::Server::Handler handler);
    void Patch(const std::string& pattern, httplib::Server::Handler handler);
    void Delete(const std::string& pattern, httplib::Server::Handler handler);

private:
    [[nodiscard]] httplib::Server::Handler wrap(const std::string& method, const std::string& pattern, httplib::Server::Handler handler) const;

    httplib::Server& server_;
    tecflix::telemetry::IHttpServerMetrics& metrics_;
    const IIdGenerator& traceIds_;
    std::string serviceName_;
};

}  // namespace tecflix::users
