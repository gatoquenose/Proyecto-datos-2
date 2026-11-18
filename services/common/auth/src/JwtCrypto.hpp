#pragma once

#include <string>
#include <string_view>

namespace tecflix::auth {

// Cabecera fija de todos los tokens: solo se acepta HS256.
inline constexpr std::string_view kJwtHeaderJson = R"({"alg":"HS256","typ":"JWT"})";
inline constexpr std::string_view kJwtAlgorithm = "HS256";

// Firma HMAC-SHA256 del texto "cabecera.carga", codificada en base64url.
std::string hs256Signature(std::string_view secret, std::string_view signingInput);

// Comparación en tiempo constante: no revela en qué posición difieren los textos.
bool constantTimeEquals(std::string_view a, std::string_view b);

}  // namespace tecflix::auth
