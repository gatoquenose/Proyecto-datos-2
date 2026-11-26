#pragma once

#include <chrono>

#include "tecflix/auth/IClock.hpp"

namespace tecflix::auth::testing {

// Reloj de pruebas: la hora solo cambia cuando la prueba lo pide.
class FakeClock final : public IClock {
public:
    explicit FakeClock(std::chrono::seconds sinceEpoch) : now_(sinceEpoch) {}

    [[nodiscard]] std::chrono::system_clock::time_point now() const override {
        return std::chrono::system_clock::time_point{now_};
    }
    void advance(std::chrono::seconds amount) { now_ += amount; }

private:
    std::chrono::seconds now_;
};

}  // namespace tecflix::auth::testing
