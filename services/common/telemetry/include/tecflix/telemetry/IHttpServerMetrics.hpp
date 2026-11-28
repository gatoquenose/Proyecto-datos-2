#pragma once

#include <string_view>

namespace tecflix::telemetry {

// Métricas de las peticiones HTTP que atiende un servicio.
class IHttpServerMetrics {
public:
    IHttpServerMetrics() = default;
    IHttpServerMetrics(const IHttpServerMetrics&) = delete;
    IHttpServerMetrics& operator=(const IHttpServerMetrics&) = delete;
    IHttpServerMetrics(IHttpServerMetrics&&) = delete;
    IHttpServerMetrics& operator=(IHttpServerMetrics&&) = delete;
    virtual ~IHttpServerMetrics() = default;

    // 'route' es el patrón de la ruta ("/api/v1/users/:id"), no la ruta real: así hay pocas
    // combinaciones de etiquetas aunque haya miles de usuarios.
    virtual void record(std::string_view method, std::string_view route, int status, double durationSeconds) = 0;
};

// Implementación que no mide nada (telemetría desactivada y pruebas).
class NullHttpServerMetrics final : public IHttpServerMetrics {
public:
    void record(std::string_view /*method*/, std::string_view /*route*/, int /*status*/, double /*durationSeconds*/) override {}
};

}  // namespace tecflix::telemetry
