#include "storage/MemoryWriteAheadLog.hpp"

#include <stdexcept>

namespace tecflix::users {

void MemoryWriteAheadLog::log(const WalEntry& entry) {
    if (slot_) {
        throw std::logic_error("hay una operación pendiente sin rehacer");
    }
    slot_ = entry;
}

void MemoryWriteAheadLog::clear() {
    slot_.reset();
}

std::optional<WalEntry> MemoryWriteAheadLog::pending() const {
    return slot_;
}

}  // namespace tecflix::users
