#pragma once

#include <httplib.h>

#include <string>

namespace tecflix::users {

// Endpoint /health: indica que el proceso está vivo.
class HealthController {
public:
    void registerRoutes(httplib::Server& server) const;

    [[nodiscard]] static std::string healthBody();
};

}  // namespace tecflix::users
