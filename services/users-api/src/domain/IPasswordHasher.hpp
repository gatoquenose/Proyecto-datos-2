#pragma once

#include <string>
#include <string_view>

namespace tecflix::users {

// Convierte contraseñas en hashes seguros y comprueba contraseñas contra un hash.
class IPasswordHasher {
public:
    IPasswordHasher() = default;
    IPasswordHasher(const IPasswordHasher&) = delete;
    IPasswordHasher& operator=(const IPasswordHasher&) = delete;
    IPasswordHasher(IPasswordHasher&&) = delete;
    IPasswordHasher& operator=(IPasswordHasher&&) = delete;
    virtual ~IPasswordHasher() = default;

    [[nodiscard]] virtual std::string hash(std::string_view password) const = 0;
    [[nodiscard]] virtual bool verify(std::string_view password, std::string_view hash) const = 0;
};

}  // namespace tecflix::users
