#pragma once

#include <httplib.h>

#include <string>

#include "domain/IIdGenerator.hpp"
#include "service/QuotaService.hpp"

namespace tecflix::users {

// Endpoints /internal/quota/... que usan las otras APIs. Se protegen con un token de servicio
// distinto del JWT de usuario (un usuario normal nunca puede llamarlos).
class InternalQuotaController {
public:
    InternalQuotaController(QuotaService& quota, std::string serviceToken, const IIdGenerator& traceIds);

    void registerRoutes(httplib::Server& server) const;

private:
    void reserve(const httplib::Request& request, httplib::Response& response) const;
    void settle(const httplib::Request& request, httplib::Response& response) const;
    void release(const httplib::Request& request, httplib::Response& response) const;

    // Responde 401 y devuelve false si el token de servicio falta o no coincide.
    bool authenticate(const httplib::Request& request, httplib::Response& response, const std::string& traceId) const;

    QuotaService& quota_;
    std::string serviceToken_;
    const IIdGenerator& traceIds_;
};

}  // namespace tecflix::users
