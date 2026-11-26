#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>

#include "config/IEnvironment.hpp"

namespace tecflix::users {

// Configuración del servicio. Se lee una sola vez al arrancar.
struct Config {
    std::uint16_t port;
    std::string logLevel;
    std::string jwtSecret;
    std::string serviceToken;
    std::chrono::seconds jwtLifetime;
    std::uint64_t defaultQuotaBytes;
    std::chrono::seconds reservationLifetime;
    std::chrono::seconds reservationSweepInterval;
    std::filesystem::path dataDirectory;
    std::string serviceName;
    std::string otlpEndpoint;  // vacío = telemetría desactivada
    std::chrono::milliseconds metricExportInterval;

    // Lanza std::invalid_argument si falta JWT_SECRET o SERVICE_TOKEN, o algún valor es inválido.
    static Config fromEnvironment(const IEnvironment& environment);
};

}  // namespace tecflix::users
