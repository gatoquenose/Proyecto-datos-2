#pragma once

#include <httplib.h>

#include <functional>
#include <string>

#include "domain/IIdGenerator.hpp"

namespace tecflix::users {

// /health: el proceso está vivo. /ready: además puede atender (por ejemplo, sus archivos responden).
class HealthController {
public:
    using ReadinessCheck = std::function<bool()>;

    HealthController();
    explicit HealthController(ReadinessCheck readinessCheck);
    // Con generador de ids, el error 503 de /ready lleva traceId (regla 7 del equipo).
    HealthController(ReadinessCheck readinessCheck, const IIdGenerator& traceIds);

    void registerRoutes(httplib::Server& server) const;

    [[nodiscard]] static std::string healthBody();

private:
    ReadinessCheck readinessCheck_;
    const IIdGenerator* traceIds_ = nullptr;
};

}  // namespace tecflix::users
