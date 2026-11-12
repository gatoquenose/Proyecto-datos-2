#pragma once

#include "config/IEnvironment.hpp"

namespace tecflix::users {

// Implementación real: lee las variables del proceso.
class ProcessEnvironment final : public IEnvironment {
public:
    [[nodiscard]] std::optional<std::string> get(std::string_view name) const override;
};

}  // namespace tecflix::users
