#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "storage/BTreePage.hpp"
#include "storage/IPageStore.hpp"

namespace tecflix::users {

// Árbol B en disco: guarda pares clave -> offset del registro en users.dat.
// Cada nodo ocupa exactamente una página del IPageStore. La página 0 guarda los
// metadatos (formato y raíz) y la raíz puede cambiar de página cuando el árbol crece.
class BTree {
public:
    using Visitor = std::function<void(const std::string& key, std::uint64_t value)>;

    // Si el almacén está vacío crea un árbol nuevo; si no, valida y carga el existente
    // (lanza std::runtime_error si el formato no coincide).
    explicit BTree(IPageStore& store);

    // Devuelve false si la clave ya existía. Lanza std::invalid_argument si la clave
    // está vacía o supera kMaxKeyLength bytes.
    bool insert(const std::string& key, std::uint64_t value);

    [[nodiscard]] std::optional<std::uint64_t> find(const std::string& key) const;

    // Recorre todas las claves en orden ascendente.
    void forEachInOrder(const Visitor& visitor) const;

private:
    void initialize();
    void loadMeta();
    void saveMeta() const;

    [[nodiscard]] BTreeNode loadNode(PageId id) const;
    void saveNode(const BTreeNode& node) const;

    void splitChild(BTreeNode& parent, std::size_t childIndex);
    void insertIntoNonFull(BTreeNode& node, const std::string& key, std::uint64_t value);
    void visit(PageId id, const Visitor& visitor) const;

    IPageStore& store_;
    PageId root_;
};

}  // namespace tecflix::users
