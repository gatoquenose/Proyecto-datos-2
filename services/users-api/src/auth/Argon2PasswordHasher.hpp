#pragma once

#include <cstddef>

#include "domain/IPasswordHasher.hpp"

namespace tecflix::users {

// Costo del hash Argon2id. Más costo = más lento para quien intente adivinar contraseñas.
struct Argon2Params {
    unsigned long long opsLimit;  // pasadas sobre la memoria
    std::size_t memLimitBytes;    // memoria usada por cada hash

    // Valores recomendados por libsodium para inicios de sesión interactivos.
    static Argon2Params interactive();
    // Valores mínimos: solo para pruebas rápidas, nunca en producción.
    static Argon2Params minimumForTests();
};

class Argon2PasswordHasher final : public IPasswordHasher {
public:
    explicit Argon2PasswordHasher(Argon2Params params);

    // Devuelve el hash en formato PHC ("$argon2id$v=19$m=...,t=...,p=...$sal$hash"),
    // que ya incluye la sal aleatoria y los parámetros.
    [[nodiscard]] std::string hash(std::string_view password) const override;
    [[nodiscard]] bool verify(std::string_view password, std::string_view hash) const override;

private:
    Argon2Params params_;
};

}  // namespace tecflix::users
