#pragma once

#include <functional>
#include <optional>
#include <string_view>

#include "domain/User.hpp"

namespace tecflix::users {

enum class AddResult { Added, IdTaken, UsernameTaken, EmailTaken };

enum class UpdateResult { Updated, NotFound, EmailTaken, ImmutableFieldChanged };

// Persistencia de usuarios. Username y email se comparan sin distinguir mayúsculas.
class IUserRepository {
public:
    IUserRepository() = default;
    IUserRepository(const IUserRepository&) = delete;
    IUserRepository& operator=(const IUserRepository&) = delete;
    IUserRepository(IUserRepository&&) = delete;
    IUserRepository& operator=(IUserRepository&&) = delete;
    virtual ~IUserRepository() = default;

    // Lanza std::invalid_argument si algún campo está vacío o supera su límite.
    virtual AddResult add(const User& user) = 0;

    // Identifica al usuario por user.id. Rechaza cambios de id, username o quotaBytes.
    virtual UpdateResult update(const User& user) = 0;

    // Lee al usuario, aplica 'change' y lo guarda, todo como una sola operación: evita que dos
    // peticiones simultáneas pisen los cambios de la otra. Mismas reglas que update.
    virtual UpdateResult modify(std::string_view id, const std::function<void(User&)>& change) = 0;

    // Borrado lógico. Devuelve false si el usuario no existe.
    virtual bool remove(std::string_view id) = 0;

    [[nodiscard]] virtual std::optional<User> findById(std::string_view id) const = 0;
    [[nodiscard]] virtual std::optional<User> findByUsername(std::string_view username) const = 0;
    [[nodiscard]] virtual std::optional<User> findByEmail(std::string_view email) const = 0;
};

}  // namespace tecflix::users
