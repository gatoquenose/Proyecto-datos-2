#pragma once

#include <optional>

#include "storage/IWriteAheadLog.hpp"

namespace tecflix::users {

// Registro en memoria: para pruebas y para usar el repositorio sin recuperación ante caídas.
class MemoryWriteAheadLog final : public IWriteAheadLog {
public:
    void log(const WalEntry& entry) override;
    void clear() override;
    [[nodiscard]] std::optional<WalEntry> pending() const override;

private:
    std::optional<WalEntry> slot_;
};

}  // namespace tecflix::users
