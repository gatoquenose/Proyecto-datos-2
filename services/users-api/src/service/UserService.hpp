#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "domain/IIdGenerator.hpp"
#include "domain/IPasswordHasher.hpp"
#include "domain/IUserRepository.hpp"
#include "domain/User.hpp"
#include "tecflix/auth/JwtSigner.hpp"

namespace tecflix::users {

struct RegisterRequest {
    std::string username;
    std::string email;
    std::string password;
};

enum class RegisterStatus {
    Created,
    InvalidUsername,
    InvalidEmail,
    InvalidPassword,
    UsernameTaken,
    EmailTaken
};

struct RegisterOutcome {
    RegisterStatus status{};
    User user;  // solo tiene valores cuando status == Created
};

struct LoginOutcome {
    bool success = false;
    std::string token;  // solo cuando success es true
    User user;          // solo cuando success es true
};

enum class ChangeEmailStatus { Changed, InvalidEmail, NotFound, EmailTaken };

struct ChangeEmailOutcome {
    ChangeEmailStatus status{};
    User user;  // usuario ya actualizado; solo tiene valores cuando status == Changed
};

// Reglas de negocio de la API de usuarios: registro e inicio de sesión.
class UserService {
public:
    UserService(IUserRepository& repository, const IPasswordHasher& hasher, const IIdGenerator& ids,
                const tecflix::auth::JwtSigner& signer, std::uint64_t defaultQuotaBytes);

    [[nodiscard]] RegisterOutcome registerUser(const RegisterRequest& request);

    // Mismo resultado ("credenciales inválidas") si el usuario no existe o la contraseña falla.
    [[nodiscard]] LoginOutcome login(std::string_view username, std::string_view password) const;

    [[nodiscard]] std::optional<User> findById(std::string_view id) const;
    [[nodiscard]] std::optional<User> findByUsername(std::string_view username) const;
    [[nodiscard]] std::optional<User> findByEmail(std::string_view email) const;

    [[nodiscard]] ChangeEmailOutcome changeEmail(std::string_view userId, const std::string& newEmail);

    // Borrado lógico. Devuelve false si el usuario no existe.
    bool removeUser(std::string_view userId);

private:
    IUserRepository& repository_;
    const IPasswordHasher& hasher_;
    const IIdGenerator& ids_;
    const tecflix::auth::JwtSigner& signer_;
    std::uint64_t defaultQuotaBytes_;
    std::string unknownUserHash_;  // hash falso para gastar el mismo tiempo cuando el usuario no existe
};

}  // namespace tecflix::users
