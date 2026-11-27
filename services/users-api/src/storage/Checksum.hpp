#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace tecflix::users {

// Tamaño del campo donde se guarda el CRC32C dentro de una página o un registro.
inline constexpr std::size_t kChecksumBytes = sizeof(std::uint32_t);

// El CRC32C cubre todo el bloque salvo los 4 bytes donde se guarda a sí mismo.
// Se llama al final de encode(), cuando el contenido ya no cambia.
void sealChecksum(std::span<std::uint8_t> block, std::size_t checksumOffset);

[[nodiscard]] bool checksumMatches(std::span<const std::uint8_t> block, std::size_t checksumOffset);

// Lanza CorruptionError describiendo `what` si el checksum no coincide.
void requireChecksum(std::span<const std::uint8_t> block, std::size_t checksumOffset, const char* what);

}  // namespace tecflix::users
