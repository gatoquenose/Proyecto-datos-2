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
    std::chrono::seconds jwtLifetime;
    std::uint64_t defaultQuotaBytes;
    std::filesystem::path dataDirectory;

    // Lanza std::invalid_argument si falta JWT_SECRET o algún valor es inválido.
    static Config fromEnvironment(const IEnvironment& environment);
};

}  // namespace tecflix::users
