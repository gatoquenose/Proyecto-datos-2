#pragma once

#include <cstdint>
#include <string>

namespace tecflix::auth {

// Contenido del token. Es el mismo en las tres APIs.
struct JwtClaims {
    std::string subject;   // "sub": id del usuario (UUID)
    std::string username;  // "username"
    std::int64_t issuedAt = 0;   // "iat": segundos desde 1970
    std::int64_t expiresAt = 0;  // "exp": segundos desde 1970
};

}  // namespace tecflix::auth
