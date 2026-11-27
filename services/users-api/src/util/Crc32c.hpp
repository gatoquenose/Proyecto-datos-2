#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tecflix::users {

// CRC32C (polinomio de Castagnoli), el mismo que usan iSCSI y varios sistemas de archivos.
// Se puede alimentar por partes con update() para sumar zonas separadas de un mismo bloque.
class Crc32c {
public:
    void update(std::span<const std::uint8_t> bytes);

    [[nodiscard]] std::uint32_t value() const;

    [[nodiscard]] static std::uint32_t of(std::span<const std::uint8_t> bytes);

private:
    static constexpr std::uint32_t kInitial = 0xFFFFFFFFU;

    std::uint32_t state_ = kInitial;
};

}  // namespace tecflix::users
