#include "config/ProcessEnvironment.hpp"

#include <cstdlib>
#include <memory>

namespace tecflix::users {

std::optional<std::string> ProcessEnvironment::get(std::string_view name) const {
    const std::string key{name};
#ifdef _WIN32
    // getenv está marcada como insegura en MSVC; _dupenv_s es la alternativa.
    char* raw = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&raw, &length, key.c_str()) != 0 || raw == nullptr) {
        return std::nullopt;
    }
    const std::unique_ptr<char, decltype(&std::free)> owner{raw, &std::free};
    return std::string{raw};
#else
    const char* value = std::getenv(key.c_str());
    if (value == nullptr) {
        return std::nullopt;
    }
    return std::string{value};
#endif
}

}  // namespace tecflix::users
