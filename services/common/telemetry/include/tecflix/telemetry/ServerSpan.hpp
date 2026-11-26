#pragma once

#include <functional>
#include <string>
#include <string_view>

#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/trace/scope.h"
#include "opentelemetry/trace/span.h"

namespace tecflix::telemetry {

// Devuelve el valor de un header de la petición (o un texto vacío si no existe).
using HeaderLookup = std::function<std::string(std::string_view name)>;

// Traza de una petición recibida. Si la petición trae el header "traceparent" (W3C), continúa
// esa traza; si no, empieza una nueva. Mientras exista, es la traza activa del hilo: cualquier
// span creado dentro quedará como hijo. Al destruirse la termina.
class ServerSpan {
public:
    ServerSpan(std::string_view instrumentationName, std::string_view method, std::string_view route,
               std::string_view path, const HeaderLookup& header);
    ~ServerSpan();

    ServerSpan(const ServerSpan&) = delete;
    ServerSpan& operator=(const ServerSpan&) = delete;
    ServerSpan(ServerSpan&&) = delete;
    ServerSpan& operator=(ServerSpan&&) = delete;

    // Anota el código HTTP de la respuesta. Los 5xx marcan la traza con error.
    void finish(int httpStatus);

    // Identificadores en hexadecimal; vacíos si la telemetría está desactivada.
    [[nodiscard]] std::string traceId() const;
    [[nodiscard]] std::string spanId() const;

private:
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span_;
    opentelemetry::trace::Scope scope_;
};

// Identificador de la traza activa en este hilo, o texto vacío si no hay ninguna.
[[nodiscard]] std::string currentTraceId();

}  // namespace tecflix::telemetry
