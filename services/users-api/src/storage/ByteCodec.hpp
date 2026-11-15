#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace tecflix::users {

// Lectura y escritura de enteros en little-endian, byte por byte.
// Así el formato del archivo es el mismo sin importar la máquina.
template <typename T>
void putLittleEndian(std::span<std::uint8_t> bytes, std::size_t offset, T value) {
    constexpr unsigned kBitsPerByte = 8;
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        bytes[offset + i] = static_cast<std::uint8_t>(value >> (kBitsPerByte * i));
    }
}

template <typename T>
T getLittleEndian(std::span<const std::uint8_t> bytes, std::size_t offset) {
    constexpr unsigned kBitsPerByte = 8;
    T value = 0;
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        value = static_cast<T>(value | (static_cast<T>(bytes[offset + i]) << (kBitsPerByte * i)));
    }
    return value;
}

// Texto de largo variable dentro de un campo de tamaño fijo:
// 1 byte con el largo y después los caracteres. maxLength debe ser <= 255.
inline void putFixedString(std::span<std::uint8_t> bytes, std::size_t offset, std::size_t maxLength,
                           std::string_view text) {
    if (text.size() > maxLength) {
        throw std::invalid_argument("texto demasiado largo para su campo");
    }
    bytes[offset] = static_cast<std::uint8_t>(text.size());
    std::ranges::copy(text, bytes.begin() + static_cast<std::ptrdiff_t>(offset + 1));
}

inline std::string getFixedString(std::span<const std::uint8_t> bytes, std::size_t offset,
                                  std::size_t maxLength) {
    const std::size_t length = bytes[offset];
    if (length > maxLength) {
        throw std::runtime_error("campo corrupto: largo inválido");
    }
    const auto first = bytes.begin() + static_cast<std::ptrdiff_t>(offset + 1);
    return std::string{first, first + static_cast<std::ptrdiff_t>(length)};
}

}  // namespace tecflix::users
