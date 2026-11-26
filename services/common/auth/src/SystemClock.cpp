#include "tecflix/auth/SystemClock.hpp"

namespace tecflix::auth {

std::chrono::system_clock::time_point SystemClock::now() const {
    return std::chrono::system_clock::now();
}

}  // namespace tecflix::auth
