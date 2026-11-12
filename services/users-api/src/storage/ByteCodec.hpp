#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

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

}  // namespace tecflix::users
