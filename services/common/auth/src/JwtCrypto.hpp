#pragma once

#include <string>
#include <string_view>

#include "tecflix/auth/ConstantTime.hpp"

namespace tecflix::auth {

// Cabecera fija de todos los tokens: solo se acepta HS256.
inline constexpr std::string_view kJwtHeaderJson = R"({"alg":"HS256","typ":"JWT"})";
inline constexpr std::string_view kJwtAlgorithm = "HS256";

// Firma HMAC-SHA256 del texto "cabecera.carga", codificada en base64url.
std::string hs256Signature(std::string_view secret, std::string_view signingInput);

}  // namespace tecflix::auth
