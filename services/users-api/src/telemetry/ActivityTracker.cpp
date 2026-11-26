#include "telemetry/ActivityTracker.hpp"

#include <algorithm>

namespace tecflix::users {

ActivityTracker::ActivityTracker(const tecflix::auth::IClock& clock) : clock_(clock) {}

void ActivityTracker::touch(std::string_view userId) {
    const std::scoped_lock lock{mutex_};
    lastSeen_.insert_or_assign(std::string{userId}, clock_.now());
}

std::size_t ActivityTracker::activeCount() {
    const std::scoped_lock lock{mutex_};
    const auto oldestAllowed = clock_.now() - kActiveWindow;
    // De paso se olvidan los usuarios inactivos, para que el mapa no crezca sin límite.
    std::erase_if(lastSeen_, [oldestAllowed](const auto& entry) { return entry.second < oldestAllowed; });
    return lastSeen_.size();
}

}  // namespace tecflix::users
