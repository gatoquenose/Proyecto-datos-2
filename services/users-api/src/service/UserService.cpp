#include "service/UserService.hpp"

#include <stdexcept>

#include "service/UserValidation.hpp"

namespace tecflix::users {

namespace {

constexpr std::string_view kPlaceholderPassword = "contrasena-de-relleno";

RegisterOutcome rejected(RegisterStatus status) {
    return {status, User{}};
}

}  // namespace

UserService::UserService(IUserRepository& repository, const IPasswordHasher& hasher, const IIdGenerator& ids,
                         const tecflix::auth::JwtSigner& signer, std::uint64_t defaultQuotaBytes)
    : repository_(repository),
      hasher_(hasher),
      ids_(ids),
      signer_(signer),
      defaultQuotaBytes_(defaultQuotaBytes),
      unknownUserHash_(hasher.hash(kPlaceholderPassword)) {}

RegisterOutcome UserService::registerUser(const RegisterRequest& request) {
    if (!isValidUsername(request.username)) {
        return rejected(RegisterStatus::InvalidUsername);
    }
    if (!isValidEmail(request.email)) {
        return rejected(RegisterStatus::InvalidEmail);
    }
    if (!isValidPassword(request.password)) {
        return rejected(RegisterStatus::InvalidPassword);
    }

    // La cuota se asigna aquí y no cambia nunca más.
    User user{ids_.next(), request.username, request.email, hasher_.hash(request.password), defaultQuotaBytes_, 0};
    switch (repository_.add(user)) {
        case AddResult::Added:
            return {RegisterStatus::Created, std::move(user)};
        case AddResult::UsernameTaken:
            return rejected(RegisterStatus::UsernameTaken);
        case AddResult::EmailTaken:
            return rejected(RegisterStatus::EmailTaken);
        case AddResult::IdTaken:
            break;
    }
    throw std::runtime_error("el generador de ids produjo un id repetido");
}

LoginOutcome UserService::login(std::string_view username, std::string_view password) const {
    const auto user = repository_.findByUsername(username);
    // Se calcula un hash aunque el usuario no exista, para que no se note la diferencia por el tiempo.
    const bool passwordMatches = hasher_.verify(password, user ? user->passwordHash : unknownUserHash_);
    if (!user || !passwordMatches) {
        return {};
    }
    return {true, signer_.issue(user->id, user->username), *user};
}

std::optional<User> UserService::findById(std::string_view id) const {
    return repository_.findById(id);
}

std::optional<User> UserService::findByUsername(std::string_view username) const {
    return repository_.findByUsername(username);
}

std::optional<User> UserService::findByEmail(std::string_view email) const {
    return repository_.findByEmail(email);
}

ChangeEmailOutcome UserService::changeEmail(std::string_view userId, const std::string& newEmail) {
    if (!isValidEmail(newEmail)) {
        return {ChangeEmailStatus::InvalidEmail, User{}};
    }
    switch (repository_.modify(userId, [&newEmail](User& user) { user.email = newEmail; })) {
        case UpdateResult::Updated:
            break;
        case UpdateResult::NotFound:
            return {ChangeEmailStatus::NotFound, User{}};
        case UpdateResult::EmailTaken:
            return {ChangeEmailStatus::EmailTaken, User{}};
        case UpdateResult::ImmutableFieldChanged:
            throw std::logic_error("el cambio de email no debe tocar campos inmutables");
    }
    auto updated = repository_.findById(userId);
    if (!updated) {
        return {ChangeEmailStatus::NotFound, User{}};  // lo eliminaron justo después de actualizarlo
    }
    return {ChangeEmailStatus::Changed, std::move(*updated)};
}

bool UserService::removeUser(std::string_view userId) {
    return repository_.remove(userId);
}

}  // namespace tecflix::users
