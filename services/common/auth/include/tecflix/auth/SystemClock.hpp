#pragma once

#include "tecflix/auth/IClock.hpp"

namespace tecflix::auth {

class SystemClock final : public IClock {
public:
    [[nodiscard]] std::chrono::system_clock::time_point now() const override;
};

}  // namespace tecflix::auth
