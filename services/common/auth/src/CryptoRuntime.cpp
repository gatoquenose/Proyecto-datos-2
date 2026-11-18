#include "tecflix/auth/CryptoRuntime.hpp"

#include <sodium.h>

#include <stdexcept>

namespace tecflix::auth {

void ensureCryptoReady() {
    // sodium_init es seguro entre hilos y devuelve 1 si ya estaba inicializada.
    if (sodium_init() < 0) {
        throw std::runtime_error("no se pudo inicializar libsodium");
    }
}

}  // namespace tecflix::auth
