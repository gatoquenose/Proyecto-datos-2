#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "domain/IUserRepository.hpp"
#include "storage/BTree.hpp"
#include "storage/IWriteAheadLog.hpp"
#include "storage/MemoryWriteAheadLog.hpp"
#include "storage/UserFile.hpp"

namespace tecflix::users {

// Repositorio de usuarios sobre users.dat (los datos) y un único árbol B (el índice).
// El índice tiene tres tipos de clave que apuntan al offset del registro:
//   "u:<username>"  "e:<email>"  "i:<id>"
// Username y email se guardan en minúsculas, así la búsqueda no distingue mayúsculas.
//
// Toda escritura pasa por el WAL: se anota la operación, se aplica a users.dat y al índice, y se borra
// la anotación. Si el proceso cae a la mitad, recoverPending() la rehace al arrancar. Aplicar una
// operación es idempotente (repetirla deja el mismo resultado), por eso rehacer es seguro.
class FileUserRepository final : public IUserRepository {
public:
    // Con un WAL en memoria: no sobrevive a una caída. Para pruebas y herramientas sin recuperación.
    FileUserRepository(UserFile& file, BTree& index);

    // Con el WAL que se indique (en producción, el archivo users.wal).
    FileUserRepository(UserFile& file, BTree& index, IWriteAheadLog& log);

    // Rehace la operación que una caída dejó a medias. Devuelve true si había una.
    // Llamar al arrancar, antes de atender peticiones.
    bool recoverPending();

    AddResult add(const User& user) override;
    UpdateResult update(const User& user) override;
    UpdateResult modify(std::string_view id, const std::function<void(User&)>& change) override;
    bool remove(std::string_view id) override;

    [[nodiscard]] std::optional<User> findById(std::string_view id) const override;
    [[nodiscard]] std::optional<User> findByUsername(std::string_view username) const override;
    [[nodiscard]] std::optional<User> findByEmail(std::string_view email) const override;
    [[nodiscard]] UsageTotals usageTotals() const override;

private:
    [[nodiscard]] std::optional<User> readByKey(const std::string& key) const;

    void commit(const WalEntry& entry);  // anotar, aplicar, borrar la anotación
    void apply(const WalEntry& entry);
    void applyAdd(const WalEntry& entry);
    void applyUpdate(const WalEntry& entry);
    void applyRemove(const WalEntry& entry);
    void ensureKey(const std::string& key, std::uint64_t offset);
    void dropKey(const std::string& key, std::uint64_t offset);

    UserFile& file_;
    BTree& index_;
    std::unique_ptr<MemoryWriteAheadLog> ownedLog_;  // solo en el constructor de 2 argumentos
    IWriteAheadLog& log_;
};

}  // namespace tecflix::users
