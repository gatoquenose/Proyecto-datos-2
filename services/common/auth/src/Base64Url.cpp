#include "tecflix/auth/Base64Url.hpp"

#include <sodium.h>

#include <vector>

#include "tecflix/auth/CryptoRuntime.hpp"

namespace tecflix::auth {

namespace {

constexpr int kVariant = sodium_base64_VARIANT_URLSAFE_NO_PADDING;

}  // namespace

std::string base64UrlEncode(std::string_view bytes) {
    ensureCryptoReady();
    const std::size_t encodedSize = sodium_base64_encoded_len(bytes.size(), kVariant);  // incluye el '\0' final
    std::string encoded(encodedSize, '\0');
    sodium_bin2base64(encoded.data(), encodedSize, reinterpret_cast<const unsigned char*>(bytes.data()),
                      bytes.size(), kVariant);
    encoded.resize(encodedSize - 1);
    return encoded;
}

std::optional<std::string> base64UrlDecode(std::string_view text) {
    ensureCryptoReady();
    if (text.empty()) {
        return std::string{};
    }
    std::vector<unsigned char> decoded(text.size());
    std::size_t decodedSize = 0;
    const char* end = nullptr;
    const int result = sodium_base642bin(decoded.data(), decoded.size(), text.data(), text.size(), nullptr,
                                         &decodedSize, &end, kVariant);
    if (result != 0 || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return std::string{reinterpret_cast<const char*>(decoded.data()), decodedSize};
}

}  // namespace tecflix::auth
