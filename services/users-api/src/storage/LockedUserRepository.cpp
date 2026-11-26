#include "storage/LockedUserRepository.hpp"

namespace tecflix::users {

LockedUserRepository::LockedUserRepository(IUserRepository& inner) : inner_(inner) {}

AddResult LockedUserRepository::add(const User& user) {
    const std::scoped_lock lock{mutex_};
    return inner_.add(user);
}

UpdateResult LockedUserRepository::update(const User& user) {
    const std::scoped_lock lock{mutex_};
    return inner_.update(user);
}

UpdateResult LockedUserRepository::modify(std::string_view id, const std::function<void(User&)>& change) {
    const std::scoped_lock lock{mutex_};
    return inner_.modify(id, change);
}

bool LockedUserRepository::remove(std::string_view id) {
    const std::scoped_lock lock{mutex_};
    return inner_.remove(id);
}

std::optional<User> LockedUserRepository::findById(std::string_view id) const {
    const std::scoped_lock lock{mutex_};
    return inner_.findById(id);
}

std::optional<User> LockedUserRepository::findByUsername(std::string_view username) const {
    const std::scoped_lock lock{mutex_};
    return inner_.findByUsername(username);
}

std::optional<User> LockedUserRepository::findByEmail(std::string_view email) const {
    const std::scoped_lock lock{mutex_};
    return inner_.findByEmail(email);
}

UsageTotals LockedUserRepository::usageTotals() const {
    const std::scoped_lock lock{mutex_};
    return inner_.usageTotals();
}

}  // namespace tecflix::users
