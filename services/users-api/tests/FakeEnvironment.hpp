#pragma once

#include <functional>
#include <map>
#include <string>
#include <utility>

#include "config/IEnvironment.hpp"

namespace tecflix::users::testing {

// Entorno falso para pruebas: devuelve solo los valores que se le dan.
class FakeEnvironment final : public IEnvironment {
public:
    explicit FakeEnvironment(std::map<std::string, std::string, std::less<>> values = {})
        : values_(std::move(values)) {}

    [[nodiscard]] std::optional<std::string> get(std::string_view name) const override {
        const auto found = values_.find(name);
        if (found == values_.end()) {
            return std::nullopt;
        }
        return found->second;
    }

private:
    std::map<std::string, std::string, std::less<>> values_;
};

}  // namespace tecflix::users::testing
