#include "tecflix/auth/JwtVerifier.hpp"

#include <chrono>
#include <nlohmann/json.hpp>
#include <optional>
#include <utility>

#include "JwtCrypto.hpp"
#include "tecflix/auth/Base64Url.hpp"

namespace tecflix::auth {

namespace {

struct TokenParts {
    std::string_view header;
    std::string_view payload;
    std::string_view signature;
    std::string_view signingInput;  // "cabecera.carga"
};

std::optional<TokenParts> split(std::string_view token) {
    const std::size_t firstDot = token.find('.');
    if (firstDot == std::string_view::npos) {
        return std::nullopt;
    }
    const std::size_t secondDot = token.find('.', firstDot + 1);
    if (secondDot == std::string_view::npos || token.find('.', secondDot + 1) != std::string_view::npos) {
        return std::nullopt;
    }
    TokenParts parts{token.substr(0, firstDot), token.substr(firstDot + 1, secondDot - firstDot - 1),
                     token.substr(secondDot + 1), token.substr(0, secondDot)};
    if (parts.header.empty() || parts.payload.empty() || parts.signature.empty()) {
        return std::nullopt;
    }
    return parts;
}

std::optional<nlohmann::json> parseObject(std::string_view encoded) {
    const auto decoded = base64UrlDecode(encoded);
    if (!decoded) {
        return std::nullopt;
    }
    nlohmann::json json = nlohmann::json::parse(*decoded, nullptr, false);
    if (json.is_discarded() || !json.is_object()) {
        return std::nullopt;
    }
    return json;
}

bool hasAlgorithm(const nlohmann::json& header, std::string_view expected) {
    const auto algorithm = header.find("alg");
    return algorithm != header.end() && algorithm->is_string() && algorithm->get<std::string>() == expected;
}

std::optional<JwtClaims> readClaims(const nlohmann::json& payload) {
    const bool wellFormed = payload.contains("sub") && payload["sub"].is_string() &&
                            payload.contains("username") && payload["username"].is_string() &&
                            payload.contains("iat") && payload["iat"].is_number_integer() &&
                            payload.contains("exp") && payload["exp"].is_number_integer();
    if (!wellFormed) {
        return std::nullopt;
    }
    return JwtClaims{payload["sub"].get<std::string>(), payload["username"].get<std::string>(),
                     payload["iat"].get<std::int64_t>(), payload["exp"].get<std::int64_t>()};
}

}  // namespace

JwtVerifier::JwtVerifier(std::string secret, const IClock& clock) : secret_(std::move(secret)), clock_(clock) {}

VerifiedToken JwtVerifier::verify(std::string_view token) const {
    const auto parts = split(token);
    if (!parts) {
        return {};
    }

    // 1) Solo se acepta HS256 (así no sirve un token que diga "alg": "none").
    const auto header = parseObject(parts->header);
    if (!header || !hasAlgorithm(*header, kJwtAlgorithm)) {
        return {};
    }

    // 2) La firma debe coincidir antes de creer en cualquier dato del token.
    if (!constantTimeEquals(parts->signature, hs256Signature(secret_, parts->signingInput))) {
        return {};
    }

    const auto payload = parseObject(parts->payload);
    const auto claims = payload ? readClaims(*payload) : std::nullopt;
    if (!claims) {
        return {};
    }

    const auto now = std::chrono::duration_cast<std::chrono::seconds>(clock_.now().time_since_epoch()).count();
    if (claims->expiresAt <= now) {
        return {TokenError::Expired, *claims};
    }
    return {TokenError::None, *claims};
}

}  // namespace tecflix::auth
