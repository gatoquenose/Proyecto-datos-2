#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "domain/IUserRepository.hpp"
#include "storage/BTree.hpp"
#include "storage/UserFile.hpp"

namespace tecflix::users {

// Repositorio de usuarios sobre users.dat (los datos) y un único árbol B (el índice).
// El índice tiene tres tipos de clave que apuntan al offset del registro:
//   "u:<username>"  "e:<email>"  "i:<id>"
// Username y email se guardan en minúsculas, así la búsqueda no distingue mayúsculas.
class FileUserRepository final : public IUserRepository {
public:
    FileUserRepository(UserFile& file, BTree& index);

    AddResult add(const User& user) override;
    UpdateResult update(const User& user) override;
    bool remove(std::string_view id) override;

    [[nodiscard]] std::optional<User> findById(std::string_view id) const override;
    [[nodiscard]] std::optional<User> findByUsername(std::string_view username) const override;
    [[nodiscard]] std::optional<User> findByEmail(std::string_view email) const override;

private:
    [[nodiscard]] std::optional<User> readByKey(const std::string& key) const;

    UserFile& file_;
    BTree& index_;
};

}  // namespace tecflix::users
