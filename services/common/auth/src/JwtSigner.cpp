#include "tecflix/auth/JwtSigner.hpp"

#include <nlohmann/json.hpp>
#include <stdexcept>
#include <utility>

#include "JwtCrypto.hpp"
#include "tecflix/auth/Base64Url.hpp"

namespace tecflix::auth {

JwtSigner::JwtSigner(std::string secret, const IClock& clock, std::chrono::seconds lifetime)
    : secret_(std::move(secret)), clock_(clock), lifetime_(lifetime) {
    if (secret_.size() < kMinSecretBytes) {
        throw std::invalid_argument("el secreto JWT debe tener al menos 32 bytes");
    }
    if (lifetime_ <= std::chrono::seconds::zero()) {
        throw std::invalid_argument("la duración del token debe ser positiva");
    }
}

std::chrono::seconds JwtSigner::lifetime() const {
    return lifetime_;
}

std::string JwtSigner::issue(const std::string& subject, const std::string& username) const {
    const auto issuedAt = std::chrono::duration_cast<std::chrono::seconds>(clock_.now().time_since_epoch());
    const nlohmann::json payload{{"sub", subject},
                                 {"username", username},
                                 {"iat", issuedAt.count()},
                                 {"exp", (issuedAt + lifetime_).count()}};

    const std::string signingInput = base64UrlEncode(kJwtHeaderJson) + "." + base64UrlEncode(payload.dump());
    return signingInput + "." + hs256Signature(secret_, signingInput);
}

}  // namespace tecflix::auth
