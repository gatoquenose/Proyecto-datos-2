#pragma once

#include <memory>
#include <string>

#include "opentelemetry/metrics/meter.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "tecflix/telemetry/TelemetryConfig.hpp"

namespace tecflix::telemetry {

// Enciende OpenTelemetry para todo el proceso: crea los proveedores de trazas y métricas,
// los conecta al Collector por OTLP/HTTP y activa la propagación W3C (traceparent).
// Debe crearse antes que cualquier otra pieza que mida y destruirse al final: al destruirse
// envía lo pendiente y apaga los proveedores.
class TelemetrySetup {
public:
    explicit TelemetrySetup(const TelemetryConfig& config);
    ~TelemetrySetup();

    TelemetrySetup(const TelemetrySetup&) = delete;
    TelemetrySetup& operator=(const TelemetrySetup&) = delete;
    TelemetrySetup(TelemetrySetup&&) = delete;
    TelemetrySetup& operator=(TelemetrySetup&&) = delete;

    [[nodiscard]] bool enabled() const;

    // Medidor del servicio. Si la telemetría está desactivada devuelve uno que no hace nada.
    [[nodiscard]] opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter> meter() const;

private:
    struct Providers;  // tipos del SDK de OpenTelemetry, ocultos fuera del .cpp

    std::string serviceName_;
    std::string serviceVersion_;
    std::unique_ptr<Providers> providers_;
};

}  // namespace tecflix::telemetry
