#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "storage/IPageStore.hpp"

namespace tecflix::users {

// Un nodo del árbol B ya leído a memoria.
struct BTreeNode {
    PageId id = 0;
    bool leaf = true;
    std::vector<std::string> keys;
    std::vector<std::uint64_t> values;
    std::vector<PageId> children;  // vacío en las hojas; keys.size() + 1 en nodos internos
};

// Convierte un nodo en los bytes de una página y viceversa.
class BTreePage {
public:
    // Lanza std::logic_error si el nodo no cabe o es inconsistente.
    static void encode(const BTreeNode& node, PageBuffer& buffer);

    // Lanza std::runtime_error si los bytes no son una página válida.
    static BTreeNode decode(PageId id, const PageBuffer& buffer);
};

}  // namespace tecflix::users
