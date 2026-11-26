#pragma once

#include "domain/IIdGenerator.hpp"

namespace tecflix::users {

// Da el identificador de traza activo para los errores ({ ..., "traceId" }): así el cliente puede
// buscar esa petición en el dashboard. Si no hay traza (telemetría desactivada), genera un id nuevo.
class TraceAwareIdGenerator final : public IIdGenerator {
public:
    explicit TraceAwareIdGenerator(const IIdGenerator& fallback);

    [[nodiscard]] std::string next() const override;

private:
    const IIdGenerator& fallback_;
};

}  // namespace tecflix::users
