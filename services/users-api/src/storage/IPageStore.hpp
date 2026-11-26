#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace tecflix::users {

using PageId = std::uint32_t;

// Tamaño fijo de página: coincide con el tamaño de bloque típico de disco.
inline constexpr std::size_t kPageSize = 4096;

using PageBuffer = std::array<std::uint8_t, kPageSize>;

// Almacén de páginas de tamaño fijo. El árbol B solo conoce esta interfaz,
// así se puede probar en memoria y usar un archivo real en producción.
class IPageStore {
public:
    IPageStore() = default;
    IPageStore(const IPageStore&) = delete;
    IPageStore& operator=(const IPageStore&) = delete;
    IPageStore(IPageStore&&) = delete;
    IPageStore& operator=(IPageStore&&) = delete;
    virtual ~IPageStore() = default;

    // Cantidad de páginas que existen (la página 0 es la primera).
    [[nodiscard]] virtual PageId pageCount() const = 0;

    // Agrega una página nueva llena de ceros y devuelve su identificador.
    virtual PageId allocate() = 0;

    virtual void read(PageId id, PageBuffer& buffer) const = 0;
    virtual void write(PageId id, const PageBuffer& buffer) = 0;
};

}  // namespace tecflix::users
