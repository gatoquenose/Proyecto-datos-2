#pragma once

#include <filesystem>
#include <fstream>

#include "storage/IPageStore.hpp"

namespace tecflix::users {

// Almacén en un archivo binario: la página N vive en el offset N * kPageSize.
class FilePageStore final : public IPageStore {
public:
    // Abre el archivo (lo crea si no existe). Lanza std::runtime_error si no se puede
    // abrir o si su tamaño no es múltiplo de kPageSize (archivo corrupto).
    explicit FilePageStore(const std::filesystem::path& path);

    [[nodiscard]] PageId pageCount() const override;
    PageId allocate() override;
    void read(PageId id, PageBuffer& buffer) const override;
    void write(PageId id, const PageBuffer& buffer) override;

private:
    void checkRange(PageId id) const;

    // mutable: leer mueve el cursor del archivo, pero no cambia su contenido.
    mutable std::fstream file_;
    PageId pageCount_ = 0;
};

}  // namespace tecflix::users
