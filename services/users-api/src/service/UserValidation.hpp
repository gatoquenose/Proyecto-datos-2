#pragma once

#include <cstddef>
#include <string_view>

namespace tecflix::users {

inline constexpr std::size_t kMinUsernameLength = 3;
inline constexpr std::size_t kMinPasswordLength = 8;
inline constexpr std::size_t kMaxPasswordLength = 128;  // limita el costo de calcular el hash

// Mismas reglas que users.yaml: letras, números, '_', '.' y '-', de 3 a 64 caracteres.
[[nodiscard]] bool isValidUsername(std::string_view username);

// Comprobación sencilla: una sola '@', nada de espacios, texto antes y un dominio con punto.
[[nodiscard]] bool isValidEmail(std::string_view email);

[[nodiscard]] bool isValidPassword(std::string_view password);

}  // namespace tecflix::users
