#include "storage/FileUserRepository.hpp"

#include <algorithm>
#include <stdexcept>

#include "storage/BTreeLayout.hpp"

namespace tecflix::users {

namespace {

constexpr std::string_view kIdPrefix = "i:";
constexpr std::string_view kUsernamePrefix = "u:";
constexpr std::string_view kEmailPrefix = "e:";
constexpr std::size_t kPrefixLength = 2;
constexpr char kCaseOffset = 'a' - 'A';

static_assert(kMaxEmailLength + kPrefixLength <= kMaxKeyLength, "la clave del email debe caber en el índice");
static_assert(kMaxUsernameLength + kPrefixLength <= kMaxKeyLength, "la clave del username debe caber en el índice");

std::string lowerAscii(std::string_view text) {
    std::string result{text};
    std::ranges::transform(result, result.begin(), [](char c) {
        return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + kCaseOffset) : c;
    });
    return result;
}

std::string idKey(std::string_view id) {
    return std::string{kIdPrefix} + lowerAscii(id);
}

std::string usernameKey(std::string_view username) {
    return std::string{kUsernamePrefix} + lowerAscii(username);
}

std::string emailKey(std::string_view email) {
    return std::string{kEmailPrefix} + lowerAscii(email);
}

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

FileUserRepository::FileUserRepository(UserFile& file, BTree& index) : file_(file), index_(index) {}

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

    const std::uint64_t offset = file_.append(user);
    index_.insert(idIndexKey, offset);
    index_.insert(usernameIndexKey, offset);
    index_.insert(emailIndexKey, offset);
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

    file_.overwrite(*offset, updated);
    if (emailKeyChanged) {
        index_.erase(oldEmailKey);
        index_.insert(newEmailKey, *offset);
    }
    return UpdateResult::Updated;
}

bool FileUserRepository::remove(std::string_view id) {
    const auto offset = index_.find(idKey(id));
    if (!offset) {
        return false;
    }
    const User user = file_.read(*offset);
    index_.erase(idKey(user.id));
    index_.erase(usernameKey(user.username));
    index_.erase(emailKey(user.email));
    file_.markDeleted(*offset);
    return true;
}

}  // namespace tecflix::users
