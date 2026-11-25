#pragma once

#include <mutex>

#include "domain/IUserRepository.hpp"

namespace tecflix::users {

// Decorador que deja pasar una operación a la vez. El archivo y el árbol B no admiten
// accesos simultáneos (ni siquiera las lecturas mueven el cursor del archivo), y el servidor
// atiende varias peticiones en hilos distintos.
// Mejora posible para la V2: mutex compartido (lecturas en paralelo) con un archivo por lector.
class LockedUserRepository final : public IUserRepository {
public:
    explicit LockedUserRepository(IUserRepository& inner);

    AddResult add(const User& user) override;
    UpdateResult update(const User& user) override;
    UpdateResult modify(std::string_view id, const std::function<void(User&)>& change) override;
    bool remove(std::string_view id) override;

    [[nodiscard]] std::optional<User> findById(std::string_view id) const override;
    [[nodiscard]] std::optional<User> findByUsername(std::string_view username) const override;
    [[nodiscard]] std::optional<User> findByEmail(std::string_view email) const override;

private:
    IUserRepository& inner_;
    mutable std::mutex mutex_;
};

}  // namespace tecflix::users
