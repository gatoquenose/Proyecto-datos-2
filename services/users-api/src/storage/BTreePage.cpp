#include "storage/BTreePage.hpp"

#include <algorithm>
#include <stdexcept>

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
        const std::size_t slot = kKeysOffset + i * kKeySlotBytes;
        buffer[slot] = static_cast<std::uint8_t>(key.size());
        std::copy(key.begin(), key.end(), buffer.begin() + static_cast<std::ptrdiff_t>(slot + 1));
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
        const std::size_t slot = kKeysOffset + i * kKeySlotBytes;
        const std::size_t length = buffer[slot];
        if (length == 0 || length > kMaxKeyLength) {
            throw std::runtime_error("página corrupta: largo de clave inválido");
        }
        const auto first = buffer.begin() + static_cast<std::ptrdiff_t>(slot + 1);
        node.keys.emplace_back(first, first + static_cast<std::ptrdiff_t>(length));
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
