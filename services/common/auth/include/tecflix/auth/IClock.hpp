#pragma once

#include <chrono>

namespace tecflix::auth {

// Reloj inyectable: permite probar la expiración de tokens sin esperar.
class IClock {
public:
    IClock() = default;
    IClock(const IClock&) = delete;
    IClock& operator=(const IClock&) = delete;
    IClock(IClock&&) = delete;
    IClock& operator=(IClock&&) = delete;
    virtual ~IClock() = default;

    [[nodiscard]] virtual std::chrono::system_clock::time_point now() const = 0;
};

}  // namespace tecflix::auth
