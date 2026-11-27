#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>

#include "storage/BTree.hpp"
#include "storage/IPageStore.hpp"
#include "storage/UserFile.hpp"

namespace tecflix::users {

// Resultado de reconstruir users.idx a partir de users.dat.
struct RebuildReport {
    std::uint64_t recordsScanned = 0;
    std::uint64_t indexed = 0;            // usuarios que quedaron en el índice
    std::uint64_t deletedSkipped = 0;     // registros eliminados: no se indexan
    std::uint64_t corruptSkipped = 0;     // registros con checksum inválido: no se pueden indexar
    std::uint64_t duplicatesSkipped = 0;  // mismo id, username o email que uno anterior: gana el primero
};

// users.dat es la fuente de verdad; users.idx se puede volver a armar siempre desde él.
// Inserta en `emptyStore` (que debe estar vacío) las tres claves de cada usuario activo.
// También elimina de verdad las claves que el borrado lógico dejaba marcadas.
RebuildReport rebuildUserIndex(const UserFile& users, IPageStore& emptyStore);

// Reconstruye el archivo del índice. Escribe primero en "<índice>.rebuild" y solo al terminar
// lo pone en lugar del original, así una caída a mitad de la reconstrucción no deja un índice a medias.
RebuildReport rebuildUserIndexFile(const std::filesystem::path& dataFile, const std::filesystem::path& indexFile);

// El índice es legible, y tiene exactamente las tres claves de cada usuario activo.
// Recorre todos los nodos, así que también verifica el checksum de cada página.
[[nodiscard]] bool userIndexIsConsistent(const UserFile& users, const BTree& index);

// Se usa al arrancar: si el índice no se puede leer o no coincide con users.dat (archivo borrado,
// página dañada, caída entre escribir el registro y el índice), lo reconstruye.
// Devuelve el informe si hizo la reconstrucción, o nada si el índice estaba bien.
std::optional<RebuildReport> ensureUserIndex(const std::filesystem::path& dataFile,
                                             const std::filesystem::path& indexFile);

}  // namespace tecflix::users
