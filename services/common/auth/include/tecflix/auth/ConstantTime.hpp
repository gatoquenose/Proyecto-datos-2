#pragma once

#include <string_view>

namespace tecflix::auth {

// Comparación en tiempo constante: no revela en qué posición difieren los textos.
// Sirve para comparar secretos (firmas, tokens de servicio).
bool constantTimeEquals(std::string_view a, std::string_view b);

}  // namespace tecflix::auth
