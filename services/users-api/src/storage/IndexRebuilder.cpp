#include "storage/IndexRebuilder.hpp"

#include <exception>

#include "storage/FilePageStore.hpp"
#include "storage/UserIndexKeys.hpp"

namespace tecflix::users {

namespace {

constexpr std::string_view kTemporarySuffix = ".rebuild";

struct KeyCounts {
    std::uint64_t ids = 0;
    std::uint64_t usernames = 0;
    std::uint64_t emails = 0;
    std::uint64_t unknown = 0;
};

KeyCounts countKeys(const BTree& index) {
    KeyCounts counts;
    index.forEachInOrder([&counts](const std::string& key, std::uint64_t) {
        const std::string_view prefix = std::string_view{key}.substr(0, kKeyPrefixLength);
        if (prefix == kIdKeyPrefix) {
            ++counts.ids;
        } else if (prefix == kUsernameKeyPrefix) {
            ++counts.usernames;
        } else if (prefix == kEmailKeyPrefix) {
            ++counts.emails;
        } else {
            ++counts.unknown;
        }
    });
    return counts;
}

bool alreadyIndexed(const BTree& index, const User& user) {
    return index.find(idKey(user.id)) || index.find(usernameKey(user.username)) ||
           index.find(emailKey(user.email));
}

}  // namespace

RebuildReport rebuildUserIndex(const UserFile& users, IPageStore& emptyStore) {
    BTree index{emptyStore};
    RebuildReport report;
    users.scan([&](const ScannedRecord& record) {
        ++report.recordsScanned;
        switch (record.state) {
            case RecordState::Deleted:
                ++report.deletedSkipped;
                return;
            case RecordState::Corrupt:
                ++report.corruptSkipped;
                return;
            case RecordState::Active:
                break;
        }
        const User& user = *record.user;
        if (alreadyIndexed(index, user)) {
            ++report.duplicatesSkipped;
            return;
        }
        index.insert(idKey(user.id), record.offset);
        index.insert(usernameKey(user.username), record.offset);
        index.insert(emailKey(user.email), record.offset);
        ++report.indexed;
    });
    return report;
}

RebuildReport rebuildUserIndexFile(const std::filesystem::path& dataFile, const std::filesystem::path& indexFile) {
    std::filesystem::path temporary = indexFile;
    temporary += kTemporarySuffix;
    std::filesystem::remove(temporary);  // resto de una reconstrucción interrumpida

    RebuildReport report;
    {
        const UserFile users{dataFile};
        FilePageStore store{temporary};
        report = rebuildUserIndex(users, store);
    }  // los archivos se cierran aquí, antes de renombrar (Windows no renombra archivos abiertos)
    std::filesystem::rename(temporary, indexFile);
    return report;
}

bool userIndexIsConsistent(const UserFile& users, const BTree& index) {
    std::uint64_t active = 0;
    users.forEachActive([&active](const User&) { ++active; });
    const KeyCounts counts = countKeys(index);
    return counts.unknown == 0 && counts.ids == active && counts.usernames == active && counts.emails == active;
}

std::optional<RebuildReport> ensureUserIndex(const std::filesystem::path& dataFile,
                                             const std::filesystem::path& indexFile) {
    bool healthy = false;
    try {
        const UserFile users{dataFile};
        FilePageStore store{indexFile};
        const BTree index{store};
        healthy = userIndexIsConsistent(users, index);
    } catch (const std::exception&) {
        healthy = false;  // índice ilegible: se reconstruye (si users.dat también falla, el error sale de aquí abajo)
    }
    if (healthy) {
        return std::nullopt;
    }
    return rebuildUserIndexFile(dataFile, indexFile);
}

}  // namespace tecflix::users
