#pragma once

#include <string>

namespace tecflix::users {

// Genera identificadores únicos (UUID en texto) para los usuarios nuevos.
class IIdGenerator {
public:
    IIdGenerator() = default;
    IIdGenerator(const IIdGenerator&) = delete;
    IIdGenerator& operator=(const IIdGenerator&) = delete;
    IIdGenerator(IIdGenerator&&) = delete;
    IIdGenerator& operator=(IIdGenerator&&) = delete;
    virtual ~IIdGenerator() = default;

    [[nodiscard]] virtual std::string next() const = 0;
};

}  // namespace tecflix::users
