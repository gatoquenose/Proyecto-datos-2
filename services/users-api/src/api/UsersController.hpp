#pragma once

#include <httplib.h>

#include <chrono>
#include <optional>
#include <string>

#include "domain/IIdGenerator.hpp"
#include "service/UserService.hpp"
#include "tecflix/auth/JwtVerifier.hpp"

namespace tecflix::users {

// Endpoints REST de usuarios (ver docs/openapi/users.yaml). Solo traduce entre HTTP y
// UserService: las reglas de negocio viven en el servicio.
class UsersController {
public:
    UsersController(UserService& service, const tecflix::auth::JwtVerifier& verifier, const IIdGenerator& traceIds,
                    std::chrono::seconds tokenLifetime);

    void registerRoutes(httplib::Server& server) const;

private:
    void registerUser(const httplib::Request& request, httplib::Response& response) const;
    void login(const httplib::Request& request, httplib::Response& response) const;
    void getCurrentUser(const httplib::Request& request, httplib::Response& response) const;
    void updateCurrentUser(const httplib::Request& request, httplib::Response& response) const;
    void getQuota(const httplib::Request& request, httplib::Response& response) const;
    void searchUsers(const httplib::Request& request, httplib::Response& response) const;
    void getUser(const httplib::Request& request, httplib::Response& response) const;
    void updateUser(const httplib::Request& request, httplib::Response& response) const;
    void deleteUser(const httplib::Request& request, httplib::Response& response) const;

    // Devuelve los datos del token o responde 401 y devuelve std::nullopt.
    std::optional<tecflix::auth::JwtClaims> authenticate(const httplib::Request& request, httplib::Response& response,
                                                         const std::string& traceId) const;
    // Responde 403 si el id de la ruta no es el del usuario autenticado.
    bool requireSelf(const std::string& pathId, const tecflix::auth::JwtClaims& caller, httplib::Response& response,
                     const std::string& traceId) const;
    void changeEmailOf(const std::string& userId, const httplib::Request& request, httplib::Response& response,
                       const std::string& traceId) const;

    UserService& service_;
    const tecflix::auth::JwtVerifier& verifier_;
    const IIdGenerator& traceIds_;
    std::chrono::seconds tokenLifetime_;
};

}  // namespace tecflix::users
