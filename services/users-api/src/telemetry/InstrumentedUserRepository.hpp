#pragma once

#include "domain/IUserRepository.hpp"
#include "telemetry/IUsersMetrics.hpp"

namespace tecflix::users {

// Decorador que mide lo que hace el repositorio: cuánto tardan las búsquedas (índice + lectura del
// registro) y cuántos usuarios se crean y se eliminan.
class InstrumentedUserRepository final : public IUserRepository {
public:
    InstrumentedUserRepository(IUserRepository& inner, IUsersMetrics& metrics);

    AddResult add(const User& user) override;
    UpdateResult update(const User& user) override;
    UpdateResult modify(std::string_view id, const std::function<void(User&)>& change) override;
    bool remove(std::string_view id) override;

    [[nodiscard]] std::optional<User> findById(std::string_view id) const override;
    [[nodiscard]] std::optional<User> findByUsername(std::string_view username) const override;
    [[nodiscard]] std::optional<User> findByEmail(std::string_view email) const override;
    [[nodiscard]] UsageTotals usageTotals() const override;

private:
    IUserRepository& inner_;
    IUsersMetrics& metrics_;
};

}  // namespace tecflix::users
