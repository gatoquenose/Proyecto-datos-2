#pragma once

#include <chrono>
#include <string>

#include "tecflix/auth/IClock.hpp"

namespace tecflix::auth {

// Menor tamaño de secreto aceptado (HS256 recomienda al menos 256 bits).
inline constexpr std::size_t kMinSecretBytes = 32;

// Emite tokens JWT firmados con HS256. Solo lo usa la API de usuarios.
class JwtSigner {
public:
    // Lanza std::invalid_argument si el secreto es corto o la duración no es positiva.
    JwtSigner(std::string secret, const IClock& clock, std::chrono::seconds lifetime);

    [[nodiscard]] std::string issue(const std::string& subject, const std::string& username) const;
    [[nodiscard]] std::chrono::seconds lifetime() const;

private:
    std::string secret_;
    const IClock& clock_;
    std::chrono::seconds lifetime_;
};

}  // namespace tecflix::auth
