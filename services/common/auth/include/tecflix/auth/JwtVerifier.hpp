#pragma once

#include <string>
#include <string_view>

#include "tecflix/auth/IClock.hpp"
#include "tecflix/auth/JwtClaims.hpp"

namespace tecflix::auth {

enum class TokenError { None, Invalid, Expired };

struct VerifiedToken {
    TokenError error = TokenError::Invalid;
    JwtClaims claims;  // solo tiene valores cuando error == None
};

// Comprueba tokens JWT HS256. Lo usan las tres APIs.
class JwtVerifier {
public:
    JwtVerifier(std::string secret, const IClock& clock);

    // Invalid: mal formado, firma incorrecta o algoritmo distinto de HS256.
    // Expired: firma correcta pero la fecha de expiración ya pasó.
    [[nodiscard]] VerifiedToken verify(std::string_view token) const;

private:
    std::string secret_;
    const IClock& clock_;
};

}  // namespace tecflix::auth
