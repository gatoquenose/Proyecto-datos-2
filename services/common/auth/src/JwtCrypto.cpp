#include "JwtCrypto.hpp"

#include <sodium.h>

#include <array>

#include "tecflix/auth/Base64Url.hpp"
#include "tecflix/auth/CryptoRuntime.hpp"

namespace tecflix::auth {

std::string hs256Signature(std::string_view secret, std::string_view signingInput) {
    ensureCryptoReady();
    crypto_auth_hmacsha256_state state;
    crypto_auth_hmacsha256_init(&state, reinterpret_cast<const unsigned char*>(secret.data()), secret.size());
    crypto_auth_hmacsha256_update(&state, reinterpret_cast<const unsigned char*>(signingInput.data()),
                                  signingInput.size());
    std::array<unsigned char, crypto_auth_hmacsha256_BYTES> mac{};
    crypto_auth_hmacsha256_final(&state, mac.data());
    return base64UrlEncode(std::string_view{reinterpret_cast<const char*>(mac.data()), mac.size()});
}

bool constantTimeEquals(std::string_view a, std::string_view b) {
    ensureCryptoReady();
    return a.size() == b.size() && sodium_memcmp(a.data(), b.data(), a.size()) == 0;
}

}  // namespace tecflix::auth
