#pragma once

#include "domain/IIdGenerator.hpp"

namespace tecflix::users {

// UUID versión 4 (aleatorio) con el generador criptográfico de libsodium.
class UuidV4Generator final : public IIdGenerator {
public:
    UuidV4Generator();

    [[nodiscard]] std::string next() const override;
};

}  // namespace tecflix::users
