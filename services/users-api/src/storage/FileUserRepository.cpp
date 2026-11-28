#include "storage/FileUserRepository.hpp"

#include <algorithm>
#include <stdexcept>

#include "storage/BTreeLayout.hpp"
#include "storage/UserIndexKeys.hpp"
#include "storage/UserRecordCodec.hpp"

namespace tecflix::users {

namespace {

static_assert(kMaxEmailLength + kKeyPrefixLength <= kMaxKeyLength, "la clave del email debe caber en el índice");
static_assert(kMaxUsernameLength + kKeyPrefixLength <= kMaxKeyLength, "la clave del username debe caber en el índice");

void validate(const User& user) {
    if (user.id.empty() || user.username.empty() || user.email.empty() || user.passwordHash.empty()) {
        throw std::invalid_argument("id, username, email y hash de contraseña son obligatorios");
    }
    const bool withinLimits = user.id.size() <= kMaxIdLength && user.username.size() <= kMaxUsernameLength &&
                              user.email.size() <= kMaxEmailLength &&
                              user.passwordHash.size() <= kMaxPasswordHashLength;
    if (!withinLimits) {
        throw std::invalid_argument("algún campo del usuario supera su largo máximo");
    }
}

}  // namespace

FileUserRepository::FileUserRepository(UserFile& file, BTree& index)
    : file_(file), index_(index), ownedLog_(std::make_unique<MemoryWriteAheadLog>()), log_(*ownedLog_) {}

FileUserRepository::FileUserRepository(UserFile& file, BTree& index, IWriteAheadLog& log)
    : file_(file), index_(index), log_(log) {}

bool FileUserRepository::recoverPending() {
    const auto entry = log_.pending();
    if (!entry) {
        return false;
    }
    apply(*entry);
    log_.clear();
    return true;
}

void FileUserRepository::commit(const WalEntry& entry) {
    log_.log(entry);
    apply(entry);
    log_.clear();
}

void FileUserRepository::apply(const WalEntry& entry) {
    switch (entry.op) {
        case WalOp::Add:
            applyAdd(entry);
            return;
        case WalOp::Update:
            applyUpdate(entry);
            return;
        case WalOp::Remove:
            applyRemove(entry);
            return;
        case WalOp::None:
            break;
    }
    throw std::logic_error("operación del WAL desconocida");
}

// Pone la clave apuntando a 'offset', exista o no. Repetirlo no cambia nada.
void FileUserRepository::ensureKey(const std::string& key, std::uint64_t offset) {
    const auto current = index_.find(key);
    if (!current) {
        index_.insert(key, offset);
    } else if (*current != offset) {
        index_.update(key, offset);
    }
}

// Quita la clave solo si todavía apunta a este registro. Si ya no está, no hay nada que hacer.
void FileUserRepository::dropKey(const std::string& key, std::uint64_t offset) {
    const auto current = index_.find(key);
    if (current && *current == offset) {
        index_.erase(key);
    }
}

void FileUserRepository::applyAdd(const WalEntry& entry) {
    const User user = UserRecordCodec::decode(entry.after).user;
    if (file_.nextOffset() == entry.offset) {
        file_.appendImage(entry.after);
    } else {
        file_.writeImage(entry.offset, entry.after);  // ya se había agregado antes de la caída
    }
    ensureKey(idKey(user.id), entry.offset);
    ensureKey(usernameKey(user.username), entry.offset);
    ensureKey(emailKey(user.email), entry.offset);
}

void FileUserRepository::applyUpdate(const WalEntry& entry) {
    const User before = UserRecordCodec::decode(entry.before).user;
    const User after = UserRecordCodec::decode(entry.after).user;
    file_.writeImage(entry.offset, entry.after);
    const std::string oldEmailKey = emailKey(before.email);
    const std::string newEmailKey = emailKey(after.email);
    if (oldEmailKey != newEmailKey) {
        dropKey(oldEmailKey, entry.offset);
        ensureKey(newEmailKey, entry.offset);
    }
}

void FileUserRepository::applyRemove(const WalEntry& entry) {
    const User user = UserRecordCodec::decode(entry.before).user;
    dropKey(idKey(user.id), entry.offset);
    dropKey(usernameKey(user.username), entry.offset);
    dropKey(emailKey(user.email), entry.offset);
    file_.writeImage(entry.offset, entry.after);
}

std::optional<User> FileUserRepository::readByKey(const std::string& key) const {
    const auto offset = index_.find(key);
    if (!offset) {
        return std::nullopt;
    }
    return file_.read(*offset);
}

std::optional<User> FileUserRepository::findById(std::string_view id) const {
    return readByKey(idKey(id));
}

std::optional<User> FileUserRepository::findByUsername(std::string_view username) const {
    return readByKey(usernameKey(username));
}

std::optional<User> FileUserRepository::findByEmail(std::string_view email) const {
    return readByKey(emailKey(email));
}

UsageTotals FileUserRepository::usageTotals() const {
    UsageTotals totals;
    file_.forEachActive([&totals](const User& user) {
        ++totals.users;
        totals.usedBytes += user.usedBytes;
        totals.quotaBytes += user.quotaBytes;
    });
    return totals;
}

AddResult FileUserRepository::add(const User& user) {
    validate(user);
    const std::string idIndexKey = idKey(user.id);
    const std::string usernameIndexKey = usernameKey(user.username);
    const std::string emailIndexKey = emailKey(user.email);
    if (index_.find(idIndexKey)) {
        return AddResult::IdTaken;
    }
    if (index_.find(usernameIndexKey)) {
        return AddResult::UsernameTaken;
    }
    if (index_.find(emailIndexKey)) {
        return AddResult::EmailTaken;
    }

    RecordBuffer image{};
    UserRecordCodec::encode(user, RecordStatus::Active, image);
    commit({WalOp::Add, file_.nextOffset(), RecordBuffer{}, image});
    return AddResult::Added;
}

UpdateResult FileUserRepository::update(const User& user) {
    return modify(user.id, [&user](User& current) { current = user; });
}

UpdateResult FileUserRepository::modify(std::string_view id, const std::function<void(User&)>& change) {
    const auto offset = index_.find(idKey(id));
    if (!offset) {
        return UpdateResult::NotFound;
    }
    const User current = file_.read(*offset);
    User updated = current;
    change(updated);
    validate(updated);
    const bool immutableChanged = updated.id != current.id || updated.username != current.username ||
                                  updated.quotaBytes != current.quotaBytes;
    if (immutableChanged) {
        return UpdateResult::ImmutableFieldChanged;
    }

    const std::string oldEmailKey = emailKey(current.email);
    const std::string newEmailKey = emailKey(updated.email);
    const bool emailKeyChanged = oldEmailKey != newEmailKey;
    if (emailKeyChanged && index_.find(newEmailKey)) {
        return UpdateResult::EmailTaken;
    }

    RecordBuffer after{};
    UserRecordCodec::encode(updated, RecordStatus::Active, after);
    commit({WalOp::Update, *offset, file_.readImage(*offset), after});
    return UpdateResult::Updated;
}

bool FileUserRepository::remove(std::string_view id) {
    const auto offset = index_.find(idKey(id));
    if (!offset) {
        return false;
    }
    const RecordBuffer before = file_.readImage(*offset);
    RecordBuffer after = before;
    UserRecordCodec::setStatus(after, RecordStatus::Deleted);
    commit({WalOp::Remove, *offset, before, after});
    return true;
}

}  // namespace tecflix::users
