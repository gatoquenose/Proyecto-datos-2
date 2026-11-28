#pragma once

#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>

#include "storage/BTreePage.hpp"
#include "storage/IPageStore.hpp"

namespace tecflix::users {

// Valor reservado que marca una clave como eliminada (borrado lógico).
// No se puede usar como valor de una clave viva.
inline constexpr std::uint64_t kTombstone = std::numeric_limits<std::uint64_t>::max();

// Árbol B en disco: guarda pares clave -> offset del registro en users.dat.
// Cada nodo ocupa exactamente una página del IPageStore. La página 0 guarda los
// metadatos (formato y raíz) y la raíz puede cambiar de página cuando el árbol crece.
//
// El borrado es lógico: la clave queda en el árbol con el valor kTombstone y deja de
// verse. Una inserción posterior con la misma clave la reactiva. La reconstrucción del
// índice (rebuild-index) es la que elimina físicamente estas claves.
class BTree {
public:
    using Visitor = std::function<void(const std::string& key, std::uint64_t value)>;

    // Si el almacén está vacío crea un árbol nuevo; si no, valida y carga el existente
    // (lanza std::runtime_error si el formato no coincide).
    explicit BTree(IPageStore& store);

    // Devuelve false si la clave ya existe (y no está eliminada). Lanza
    // std::invalid_argument si la clave está vacía o supera kMaxKeyLength bytes, o si
    // el valor es kTombstone.
    bool insert(const std::string& key, std::uint64_t value);

    // Cambia el valor de una clave existente. Devuelve false si no existe.
    bool update(const std::string& key, std::uint64_t value);

    // Borrado lógico. Devuelve false si la clave no existe.
    bool erase(const std::string& key);

    [[nodiscard]] std::optional<std::uint64_t> find(const std::string& key) const;

    // Recorre las claves vivas en orden ascendente.
    void forEachInOrder(const Visitor& visitor) const;

private:
    // Posición de una clave dentro de un nodo ya leído.
    struct Location {
        BTreeNode node;
        std::size_t index{};
    };

    void initialize();
    void loadMeta();
    void saveMeta() const;

    [[nodiscard]] BTreeNode loadNode(PageId id) const;
    void saveNode(const BTreeNode& node) const;

    // Busca la clave, incluso si está eliminada.
    [[nodiscard]] std::optional<Location> locate(const std::string& key) const;
    void setValue(Location& location, std::uint64_t value) const;

    void insertNew(const std::string& key, std::uint64_t value);
    void splitChild(BTreeNode& parent, std::size_t childIndex);
    void insertIntoNonFull(BTreeNode& node, const std::string& key, std::uint64_t value);
    void visit(PageId id, const Visitor& visitor) const;

    IPageStore& store_;
    PageId root_;
};

}  // namespace tecflix::users
