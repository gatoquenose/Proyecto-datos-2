#include "auth/Argon2PasswordHasher.hpp"

#include <sodium.h>

#include <array>
#include <stdexcept>

#include "domain/User.hpp"
#include "tecflix/auth/CryptoRuntime.hpp"

namespace tecflix::users {

// El hash más largo que produce libsodium debe caber en el campo del registro.
static_assert(crypto_pwhash_STRBYTES - 1 <= kMaxPasswordHashLength, "el hash debe caber en users.dat");

Argon2Params Argon2Params::interactive() {
    return {crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE};
}

Argon2Params Argon2Params::minimumForTests() {
    return {crypto_pwhash_OPSLIMIT_MIN, crypto_pwhash_MEMLIMIT_MIN};
}

Argon2PasswordHasher::Argon2PasswordHasher(Argon2Params params) : params_(params) {
    tecflix::auth::ensureCryptoReady();
}

std::string Argon2PasswordHasher::hash(std::string_view password) const {
    std::array<char, crypto_pwhash_STRBYTES> output{};
    const int result = crypto_pwhash_str_alg(output.data(), password.data(), password.size(), params_.opsLimit,
                                             params_.memLimitBytes, crypto_pwhash_ALG_ARGON2ID13);
    if (result != 0) {
        throw std::runtime_error("no se pudo calcular el hash de la contraseña");
    }
    return std::string{output.data()};
}

bool Argon2PasswordHasher::verify(std::string_view password, std::string_view hash) const {
    if (hash.size() >= crypto_pwhash_STRBYTES) {
        return false;
    }
    const std::string terminated{hash};  // copia con '\0' final, como exige libsodium
    return crypto_pwhash_str_verify(terminated.c_str(), password.data(), password.size()) == 0;
}

}  // namespace tecflix::users
