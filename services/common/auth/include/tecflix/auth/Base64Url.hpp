#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace tecflix::auth {

// Base64 "seguro para URL" y sin relleno "=", el formato que usa JWT.
std::string base64UrlEncode(std::string_view bytes);

// Devuelve std::nullopt si el texto tiene caracteres inválidos.
std::optional<std::string> base64UrlDecode(std::string_view text);

}  // namespace tecflix::auth
