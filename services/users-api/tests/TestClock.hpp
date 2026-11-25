#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

#include "tecflix/auth/IClock.hpp"

namespace tecflix::users::testing {

// Reloj de pruebas: la hora solo cambia cuando la prueba lo pide. Es seguro entre hilos
// porque el servidor de las pruebas lo lee desde otro hilo.
class TestClock final : public tecflix::auth::IClock {
public:
    explicit TestClock(std::chrono::seconds sinceEpoch) : seconds_(sinceEpoch.count()) {}

    [[nodiscard]] std::chrono::system_clock::time_point now() const override {
        return std::chrono::system_clock::time_point{std::chrono::seconds{seconds_.load()}};
    }
    void advance(std::chrono::seconds amount) { seconds_ += amount.count(); }

private:
    std::atomic<std::int64_t> seconds_;
};

}  // namespace tecflix::users::testing
