#include "storage/BTreePage.hpp"

#include <stdexcept>
#include <utility>

#include "storage/BTreeLayout.hpp"
#include "storage/ByteCodec.hpp"

namespace tecflix::users {

namespace {

constexpr std::size_t kLeafFlagOffset = 0;
constexpr std::size_t kCountOffset = 2;
constexpr std::uint8_t kLeafMarker = 1;

}  // namespace

void BTreePage::encode(const BTreeNode& node, PageBuffer& buffer) {
    const std::size_t count = node.keys.size();
    const bool consistent = count <= kMaxKeys && node.values.size() == count &&
                            node.children.size() == (node.leaf ? 0 : count + 1);
    if (!consistent) {
        throw std::logic_error("nodo del árbol B inconsistente");
    }

    buffer.fill(0);
    buffer[kLeafFlagOffset] = node.leaf ? kLeafMarker : 0;
    putLittleEndian<std::uint16_t>(buffer, kCountOffset, static_cast<std::uint16_t>(count));

    for (std::size_t i = 0; i < count; ++i) {
        const std::string& key = node.keys[i];
        if (key.empty() || key.size() > kMaxKeyLength) {
            throw std::logic_error("clave con largo inválido");
        }
        putFixedString(buffer, kKeysOffset + i * kKeySlotBytes, kMaxKeyLength, key);
        putLittleEndian<std::uint64_t>(buffer, kValuesOffset + i * kValueBytes, node.values[i]);
    }
    for (std::size_t i = 0; i < node.children.size(); ++i) {
        putLittleEndian<PageId>(buffer, kChildrenOffset + i * kChildIdBytes, node.children[i]);
    }
}

BTreeNode BTreePage::decode(PageId id, const PageBuffer& buffer) {
    const std::size_t count = getLittleEndian<std::uint16_t>(buffer, kCountOffset);
    if (count > kMaxKeys) {
        throw std::runtime_error("página corrupta: demasiadas claves");
    }

    BTreeNode node;
    node.id = id;
    node.leaf = buffer[kLeafFlagOffset] == kLeafMarker;
    for (std::size_t i = 0; i < count; ++i) {
        std::string key = getFixedString(buffer, kKeysOffset + i * kKeySlotBytes, kMaxKeyLength);
        if (key.empty()) {
            throw std::runtime_error("página corrupta: clave vacía");
        }
        node.keys.push_back(std::move(key));
        node.values.push_back(getLittleEndian<std::uint64_t>(buffer, kValuesOffset + i * kValueBytes));
    }
    if (!node.leaf) {
        for (std::size_t i = 0; i <= count; ++i) {
            node.children.push_back(getLittleEndian<PageId>(buffer, kChildrenOffset + i * kChildIdBytes));
        }
    }
    return node;
}

}  // namespace tecflix::users
