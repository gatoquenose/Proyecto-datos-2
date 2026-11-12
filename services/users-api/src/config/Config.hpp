#pragma once

#include <cstdint>
#include <string>

#include "config/IEnvironment.hpp"

namespace tecflix::users {

// Configuración del servicio. Se lee una sola vez al arrancar.
struct Config {
    std::uint16_t port;
    std::string logLevel;

    // Lanza std::invalid_argument si algún valor es inválido.
    static Config fromEnvironment(const IEnvironment& environment);
};

}  // namespace tecflix::users
