#pragma once

namespace tecflix::auth {

// Inicializa la librería criptográfica (libsodium). Se puede llamar muchas veces.
// Lanza std::runtime_error si no se puede inicializar.
void ensureCryptoReady();

}  // namespace tecflix::auth
