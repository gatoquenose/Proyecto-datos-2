#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace tecflix::telemetry {

struct TelemetryConfig {
    std::string serviceName;
    std::string serviceVersion;
    // Dirección base del OpenTelemetry Collector por OTLP/HTTP, por ejemplo http://otel-collector:4318.
    // Si está vacía, la telemetría queda desactivada (las llamadas no hacen nada).
    std::string otlpEndpoint;
    std::chrono::milliseconds metricExportInterval{15000};
    // Nombres de los histogramas medidos en segundos: usan límites de cubeta adecuados
    // (milisegundos a segundos) en lugar de los predeterminados de OpenTelemetry.
    std::vector<std::string> durationHistograms;
};

}  // namespace tecflix::telemetry
