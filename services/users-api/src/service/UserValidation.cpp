#include "service/UserValidation.hpp"

#include <algorithm>

#include "domain/User.hpp"

namespace tecflix::users {

namespace {

bool isAsciiLetterOrDigit(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

bool isUsernameCharacter(char c) {
    return isAsciiLetterOrDigit(c) || c == '_' || c == '.' || c == '-';
}

bool isEmailCharacter(char c) {
    // Cualquier carácter visible de ASCII; se descartan espacios y controles.
    return c > ' ' && c < '\x7f';
}

}  // namespace

bool isValidUsername(std::string_view username) {
    return username.size() >= kMinUsernameLength && username.size() <= kMaxUsernameLength &&
           std::ranges::all_of(username, isUsernameCharacter);
}

bool isValidEmail(std::string_view email) {
    if (email.empty() || email.size() > kMaxEmailLength || !std::ranges::all_of(email, isEmailCharacter)) {
        return false;
    }
    const std::size_t at = email.find('@');
    if (at == std::string_view::npos || at == 0 || email.find('@', at + 1) != std::string_view::npos) {
        return false;
    }
    const std::string_view domain = email.substr(at + 1);
    const std::size_t dot = domain.find('.');
    return dot != std::string_view::npos && dot != 0 && domain.back() != '.';
}

bool isValidPassword(std::string_view password) {
    return password.size() >= kMinPasswordLength && password.size() <= kMaxPasswordLength;
}

}  // namespace tecflix::users
