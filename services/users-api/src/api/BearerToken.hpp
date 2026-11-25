#pragma once

#include <string_view>

namespace tecflix::users {

// Extrae el token de "Authorization: Bearer <token>" (la palabra Bearer no distingue mayúsculas).
// Devuelve un texto vacío si el header no tiene ese formato.
[[nodiscard]] std::string_view bearerToken(std::string_view authorizationHeader);

}  // namespace tecflix::users
