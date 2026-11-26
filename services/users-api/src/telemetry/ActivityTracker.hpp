#pragma once

#include <chrono>
#include <cstddef>
#include <map>
#include <mutex>
#include <string>
#include <string_view>

#include "tecflix/auth/IClock.hpp"

namespace tecflix::users {

// Lleva la cuenta de usuarios activos: los que tuvieron una solicitud autenticada exitosa
// (o iniciaron sesión) en los últimos 15 minutos.
class ActivityTracker {
public:
    static constexpr std::chrono::minutes kActiveWindow{15};

    explicit ActivityTracker(const tecflix::auth::IClock& clock);

    void touch(std::string_view userId);
    [[nodiscard]] std::size_t activeCount();

private:
    const tecflix::auth::IClock& clock_;
    std::mutex mutex_;
    std::map<std::string, std::chrono::system_clock::time_point, std::less<>> lastSeen_;
};

}  // namespace tecflix::users
