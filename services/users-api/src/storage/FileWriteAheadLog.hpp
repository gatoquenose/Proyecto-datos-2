#pragma once

#include <filesystem>
#include <optional>

#include "storage/FixedRecordFile.hpp"
#include "storage/IWriteAheadLog.hpp"

namespace tecflix::users {

// users.wal: una sola ranura de tamaño fijo con la operación en curso.
//   [0]      operación (0 = ninguna)
//   [1, 9)   offset del registro
//   registro antes (384) | registro después (384)
//   final    CRC32C de todo lo anterior
// Como las escrituras están serializadas, una ranura basta: es más simple que un log que crece
// y no necesita compactarse. Límite conocido: se vacía al sistema operativo (flush) pero no se
// fuerza a disco (fsync), que std::fstream no ofrece; sobrevive a que el proceso caiga,
// no a un corte de energía en el instante exacto.
class FileWriteAheadLog final : public IWriteAheadLog {
public:
    // Crea el archivo si no existe. Lanza std::runtime_error si la cabecera no es válida.
    explicit FileWriteAheadLog(const std::filesystem::path& path);

    void log(const WalEntry& entry) override;
    void clear() override;
    [[nodiscard]] std::optional<WalEntry> pending() const override;

private:
    void store(const WalEntry& entry);

    FixedRecordFile file_;
    bool armed_ = false;  // hay una operación anotada sin terminar
};

}  // namespace tecflix::users
