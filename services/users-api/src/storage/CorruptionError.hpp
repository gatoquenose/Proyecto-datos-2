#pragma once

#include <stdexcept>

namespace tecflix::users {

// Los bytes leídos del disco no son lo que se escribió (checksum que no coincide, campos imposibles).
// Distinguirlo de otros errores permite reaccionar distinto: reconstruir el índice, avisar, etc.
class CorruptionError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

}  // namespace tecflix::users
