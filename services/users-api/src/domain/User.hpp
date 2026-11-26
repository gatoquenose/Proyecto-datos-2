#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace tecflix::users {

// Límites de cada campo. Los usa el formato binario de users.dat; el email queda
// por debajo de kMaxKeyLength para que su clave del índice ("e:" + email) quepa.
inline constexpr std::size_t kMaxIdLength = 36;  // UUID en texto
inline constexpr std::size_t kMaxUsernameLength = 64;
inline constexpr std::size_t kMaxEmailLength = 120;
inline constexpr std::size_t kMaxPasswordHashLength = 128;  // hash Argon2id codificado

struct User {
    std::string id;
    std::string username;  // inmutable
    std::string email;
    std::string passwordHash;
    std::uint64_t quotaBytes = 0;  // inmutable
    std::uint64_t usedBytes = 0;
};

}  // namespace tecflix::users
