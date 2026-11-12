#include "storage/BTree.hpp"

#include <algorithm>
#include <array>
#include <iterator>
#include <stdexcept>
#include <string_view>

#include "storage/BTreeLayout.hpp"
#include "storage/ByteCodec.hpp"

namespace tecflix::users {

namespace {

// Página 0: metadatos. Página 1: primera raíz (después puede cambiar).
constexpr PageId kMetaPage = 0;
constexpr PageId kFirstRootPage = 1;

constexpr std::string_view kMagic = "TFXIDX01";
constexpr std::uint32_t kFormatVersion = 1;
constexpr std::size_t kVersionOffset = 8;
constexpr std::size_t kPageSizeOffset = 12;
constexpr std::size_t kRootOffset = 16;

void validateKey(const std::string& key) {
    if (key.empty() || key.size() > kMaxKeyLength) {
        throw std::invalid_argument("la clave debe tener entre 1 y 127 bytes");
    }
}

std::size_t lowerBoundIndex(const std::vector<std::string>& keys, const std::string& key) {
    return static_cast<std::size_t>(std::distance(keys.begin(), std::ranges::lower_bound(keys, key)));
}

}  // namespace

BTree::BTree(IPageStore& store) : store_(store), root_(kFirstRootPage) {
    if (store_.pageCount() == 0) {
        initialize();
    } else {
        loadMeta();
    }
}

void BTree::initialize() {
    store_.allocate();  // página 0: metadatos
    BTreeNode root;
    root.id = store_.allocate();  // página 1: raíz vacía (hoja)
    root_ = root.id;
    saveNode(root);
    saveMeta();
}

void BTree::saveMeta() const {
    PageBuffer buffer{};
    std::ranges::copy(kMagic, buffer.begin());
    putLittleEndian<std::uint32_t>(buffer, kVersionOffset, kFormatVersion);
    putLittleEndian<std::uint32_t>(buffer, kPageSizeOffset, static_cast<std::uint32_t>(kPageSize));
    putLittleEndian<PageId>(buffer, kRootOffset, root_);
    store_.write(kMetaPage, buffer);
}

void BTree::loadMeta() {
    PageBuffer buffer{};
    store_.read(kMetaPage, buffer);
    const bool magicMatches = std::ranges::equal(kMagic, std::span{buffer}.first(kMagic.size()));
    const bool formatMatches = getLittleEndian<std::uint32_t>(buffer, kVersionOffset) == kFormatVersion &&
                               getLittleEndian<std::uint32_t>(buffer, kPageSizeOffset) == kPageSize;
    root_ = getLittleEndian<PageId>(buffer, kRootOffset);
    if (!magicMatches || !formatMatches || root_ == kMetaPage || root_ >= store_.pageCount()) {
        throw std::runtime_error("el archivo no es un índice de árbol B válido");
    }
}

BTreeNode BTree::loadNode(PageId id) const {
    PageBuffer buffer{};
    store_.read(id, buffer);
    return BTreePage::decode(id, buffer);
}

void BTree::saveNode(const BTreeNode& node) const {
    PageBuffer buffer{};
    BTreePage::encode(node, buffer);
    store_.write(node.id, buffer);
}

std::optional<std::uint64_t> BTree::find(const std::string& key) const {
    BTreeNode node = loadNode(root_);
    for (;;) {
        const std::size_t index = lowerBoundIndex(node.keys, key);
        if (index < node.keys.size() && node.keys[index] == key) {
            return node.values[index];
        }
        if (node.leaf) {
            return std::nullopt;
        }
        node = loadNode(node.children[index]);
    }
}

bool BTree::insert(const std::string& key, std::uint64_t value) {
    validateKey(key);
    if (find(key)) {
        return false;
    }

    BTreeNode root = loadNode(root_);
    if (root.keys.size() == kMaxKeys) {
        // La raíz está llena: el árbol crece un nivel hacia arriba.
        BTreeNode newRoot;
        newRoot.id = store_.allocate();
        newRoot.leaf = false;
        newRoot.children.push_back(root.id);
        splitChild(newRoot, 0);
        root_ = newRoot.id;
        saveMeta();
        insertIntoNonFull(newRoot, key, value);
    } else {
        insertIntoNonFull(root, key, value);
    }
    return true;
}

// Divide el hijo lleno parent.children[childIndex] en dos nodos y sube la clave
// del medio al padre.
void BTree::splitChild(BTreeNode& parent, std::size_t childIndex) {
    BTreeNode left = loadNode(parent.children[childIndex]);
    BTreeNode right;
    right.id = store_.allocate();
    right.leaf = left.leaf;

    const std::size_t median = kMinDegree - 1;
    std::string medianKey = left.keys[median];
    const std::uint64_t medianValue = left.values[median];

    right.keys.assign(left.keys.begin() + static_cast<std::ptrdiff_t>(median + 1), left.keys.end());
    right.values.assign(left.values.begin() + static_cast<std::ptrdiff_t>(median + 1), left.values.end());
    if (!left.leaf) {
        right.children.assign(left.children.begin() + static_cast<std::ptrdiff_t>(kMinDegree),
                              left.children.end());
        left.children.resize(kMinDegree);
    }
    left.keys.resize(median);
    left.values.resize(median);

    const auto position = static_cast<std::ptrdiff_t>(childIndex);
    parent.keys.insert(parent.keys.begin() + position, std::move(medianKey));
    parent.values.insert(parent.values.begin() + position, medianValue);
    parent.children.insert(parent.children.begin() + position + 1, right.id);

    saveNode(left);
    saveNode(right);
    saveNode(parent);
}

// Precondición: node no está lleno. Antes de bajar a un hijo lleno lo divide,
// así nunca hay que volver hacia arriba.
void BTree::insertIntoNonFull(BTreeNode& node, const std::string& key, std::uint64_t value) {
    std::size_t index = lowerBoundIndex(node.keys, key);
    if (node.leaf) {
        const auto position = static_cast<std::ptrdiff_t>(index);
        node.keys.insert(node.keys.begin() + position, key);
        node.values.insert(node.values.begin() + position, value);
        saveNode(node);
        return;
    }

    BTreeNode child = loadNode(node.children[index]);
    if (child.keys.size() == kMaxKeys) {
        splitChild(node, index);
        if (key > node.keys[index]) {
            ++index;
        }
        child = loadNode(node.children[index]);
    }
    insertIntoNonFull(child, key, value);
}

void BTree::forEachInOrder(const Visitor& visitor) const {
    visit(root_, visitor);
}

void BTree::visit(PageId id, const Visitor& visitor) const {
    const BTreeNode node = loadNode(id);
    for (std::size_t i = 0; i < node.keys.size(); ++i) {
        if (!node.leaf) {
            visit(node.children[i], visitor);
        }
        visitor(node.keys[i], node.values[i]);
    }
    if (!node.leaf) {
        visit(node.children[node.keys.size()], visitor);
    }
}

}  // namespace tecflix::users
