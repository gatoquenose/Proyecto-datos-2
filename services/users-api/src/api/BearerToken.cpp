#include "api/BearerToken.hpp"

#include <algorithm>
#include <cctype>

namespace tecflix::users {

namespace {

constexpr std::string_view kBearerPrefix = "bearer ";

}  // namespace

std::string_view bearerToken(std::string_view authorizationHeader) {
    const bool hasPrefix =
        authorizationHeader.size() > kBearerPrefix.size() &&
        std::ranges::equal(authorizationHeader.substr(0, kBearerPrefix.size()), kBearerPrefix,
                           [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == b; });
    return hasPrefix ? authorizationHeader.substr(kBearerPrefix.size()) : std::string_view{};
}

}  // namespace tecflix::users
