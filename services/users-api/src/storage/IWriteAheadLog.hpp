#pragma once

#include <cstdint>
#include <optional>

#include "storage/UserRecordCodec.hpp"

namespace tecflix::users {

enum class WalOp : std::uint8_t { None = 0, Add = 1, Update = 2, Remove = 3 };

// Una operación sobre users.dat + users.idx, descrita con las imágenes completas del registro
// antes y después. Con eso se puede rehacer sin depender del estado en que quedaron los archivos.
//   Add:    before vacío; after = registro nuevo; offset = donde se agrega
//   Update: before y after; offset = donde está el registro
//   Remove: before = registro activo; after = el mismo registro marcado como eliminado
struct WalEntry {
    WalOp op = WalOp::None;
    std::uint64_t offset = 0;
    RecordBuffer before{};
    RecordBuffer after{};
};

// Registro de intención (write-ahead log): la operación se anota ANTES de tocar los archivos
// de datos y se borra al terminar. Si el proceso cae a la mitad, al arrancar queda anotada
// y se rehace. Las operaciones de escritura están serializadas, así que hay a lo sumo una pendiente.
class IWriteAheadLog {
public:
    IWriteAheadLog() = default;
    IWriteAheadLog(const IWriteAheadLog&) = delete;
    IWriteAheadLog& operator=(const IWriteAheadLog&) = delete;
    IWriteAheadLog(IWriteAheadLog&&) = delete;
    IWriteAheadLog& operator=(IWriteAheadLog&&) = delete;
    virtual ~IWriteAheadLog() = default;

    // Anota la operación. Lanza std::logic_error si ya hay una pendiente sin rehacer:
    // anotar otra la perdería.
    virtual void log(const WalEntry& entry) = 0;

    // La operación ya se aplicó por completo.
    virtual void clear() = 0;

    // La operación anotada y sin terminar, si hay una. Una anotación a medias (checksum
    // inválido) no cuenta: la operación nunca llegó a empezar.
    [[nodiscard]] virtual std::optional<WalEntry> pending() const = 0;
};

}  // namespace tecflix::users
