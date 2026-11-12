#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace tecflix::users {

// Interfaz para leer variables de entorno. Existe para poder probar Config
// sin depender del entorno real de la máquina.
class IEnvironment {
public:
    IEnvironment() = default;
    IEnvironment(const IEnvironment&) = delete;
    IEnvironment& operator=(const IEnvironment&) = delete;
    IEnvironment(IEnvironment&&) = delete;
    IEnvironment& operator=(IEnvironment&&) = delete;
    virtual ~IEnvironment() = default;

    [[nodiscard]] virtual std::optional<std::string> get(std::string_view name) const = 0;
};

}  // namespace tecflix::users
