#pragma once

#include <cstddef>
#include <cstdint>

#include "storage/IPageStore.hpp"

namespace tecflix::users {

// Formato de una página del árbol B (4096 bytes):
//   [0, 8)      cabecera: hoja (1 byte), reservado (1), cantidad de claves (2), reservado (4)
//   [8, ...)    claves: kMaxKeys espacios de kKeySlotBytes (1 byte de largo + texto)
//   [...]       valores: kMaxKeys enteros de 8 bytes (offset del registro en users.dat)
//   [...]       hijos: kMaxKeys + 1 identificadores de página de 4 bytes
inline constexpr std::size_t kPageHeaderBytes = 8;
inline constexpr std::size_t kKeySlotBytes = 128;
inline constexpr std::size_t kMaxKeyLength = kKeySlotBytes - 1;
inline constexpr std::size_t kValueBytes = sizeof(std::uint64_t);
inline constexpr std::size_t kChildIdBytes = sizeof(PageId);

namespace detail {

// Cuántas claves caben: cada clave aporta clave + valor + un hijo, y sobra un hijo.
inline constexpr std::size_t kRawMaxKeys =
    (kPageSize - kPageHeaderBytes - kChildIdBytes) / (kKeySlotBytes + kValueBytes + kChildIdBytes);

}  // namespace detail

// El máximo de claves debe ser impar (2t - 1) para poder dividir un nodo lleno por la mitad.
inline constexpr std::size_t kMaxKeys =
    detail::kRawMaxKeys % 2 == 1 ? detail::kRawMaxKeys : detail::kRawMaxKeys - 1;

// Grado mínimo t del árbol B: todo nodo (salvo la raíz) tiene al menos t - 1 claves.
inline constexpr std::size_t kMinDegree = (kMaxKeys + 1) / 2;

inline constexpr std::size_t kKeysOffset = kPageHeaderBytes;
inline constexpr std::size_t kValuesOffset = kKeysOffset + kMaxKeys * kKeySlotBytes;
inline constexpr std::size_t kChildrenOffset = kValuesOffset + kMaxKeys * kValueBytes;

static_assert(kChildrenOffset + (kMaxKeys + 1) * kChildIdBytes <= kPageSize,
              "un nodo lleno debe caber en una página");

}  // namespace tecflix::users
