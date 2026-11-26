#include "telemetry/InstrumentedUserRepository.hpp"

#include <chrono>

namespace tecflix::users {

namespace {

// Ejecuta una búsqueda y anota cuánto tardó, haya encontrado algo o no.
template <typename Search>
std::optional<User> timed(IUsersMetrics& metrics, std::string_view lookup, Search&& search) {
    const auto start = std::chrono::steady_clock::now();
    std::optional<User> found = search();
    metrics.searchFinished(lookup, std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());
    return found;
}

}  // namespace

InstrumentedUserRepository::InstrumentedUserRepository(IUserRepository& inner, IUsersMetrics& metrics)
    : inner_(inner), metrics_(metrics) {}

AddResult InstrumentedUserRepository::add(const User& user) {
    const AddResult result = inner_.add(user);
    if (result == AddResult::Added) {
        metrics_.userCreated();
    }
    return result;
}

UpdateResult InstrumentedUserRepository::update(const User& user) {
    return inner_.update(user);
}

UpdateResult InstrumentedUserRepository::modify(std::string_view id, const std::function<void(User&)>& change) {
    return inner_.modify(id, change);
}

bool InstrumentedUserRepository::remove(std::string_view id) {
    const bool removed = inner_.remove(id);
    if (removed) {
        metrics_.userDeleted();
    }
    return removed;
}

std::optional<User> InstrumentedUserRepository::findById(std::string_view id) const {
    return timed(metrics_, "id", [&] { return inner_.findById(id); });
}

std::optional<User> InstrumentedUserRepository::findByUsername(std::string_view username) const {
    return timed(metrics_, "username", [&] { return inner_.findByUsername(username); });
}

std::optional<User> InstrumentedUserRepository::findByEmail(std::string_view email) const {
    return timed(metrics_, "email", [&] { return inner_.findByEmail(email); });
}

UsageTotals InstrumentedUserRepository::usageTotals() const {
    return inner_.usageTotals();
}

}  // namespace tecflix::users
