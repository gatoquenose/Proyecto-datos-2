#pragma once

#include <algorithm>
#include <string>
#include <string_view>

namespace tecflix::users {

// Claves del índice users.idx. Un mismo registro tiene tres: por id, por username y por email.
// Username y email se guardan en minúsculas, así la búsqueda no distingue mayúsculas.
// Las usan el repositorio (al escribir) y la reconstrucción del índice: deben ser idénticas.
inline constexpr std::string_view kIdKeyPrefix = "i:";
inline constexpr std::string_view kUsernameKeyPrefix = "u:";
inline constexpr std::string_view kEmailKeyPrefix = "e:";
inline constexpr std::size_t kKeyPrefixLength = 2;

inline std::string lowerAscii(std::string_view text) {
    constexpr char kCaseOffset = 'a' - 'A';
    std::string result{text};
    std::ranges::transform(result, result.begin(), [](char c) {
        return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + kCaseOffset) : c;
    });
    return result;
}

inline std::string idKey(std::string_view id) {
    return std::string{kIdKeyPrefix} + lowerAscii(id);
}

inline std::string usernameKey(std::string_view username) {
    return std::string{kUsernameKeyPrefix} + lowerAscii(username);
}

inline std::string emailKey(std::string_view email) {
    return std::string{kEmailKeyPrefix} + lowerAscii(email);
}

}  // namespace tecflix::users
