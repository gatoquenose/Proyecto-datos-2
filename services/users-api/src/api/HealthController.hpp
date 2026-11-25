#pragma once

#include <httplib.h>

#include <functional>
#include <string>

namespace tecflix::users {

// /health: el proceso está vivo. /ready: además puede atender (por ejemplo, sus archivos responden).
class HealthController {
public:
    using ReadinessCheck = std::function<bool()>;

    HealthController();
    explicit HealthController(ReadinessCheck readinessCheck);

    void registerRoutes(httplib::Server& server) const;

    [[nodiscard]] static std::string healthBody();

private:
    ReadinessCheck readinessCheck_;
};

}  // namespace tecflix::users
