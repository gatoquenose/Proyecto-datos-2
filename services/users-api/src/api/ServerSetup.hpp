#pragma once

#include <httplib.h>

#include "domain/IIdGenerator.hpp"

namespace tecflix::users {

// Configuración común del servidor HTTP: errores en JSON (404, 405, 500...), límite de
// tamaño del cuerpo y registro de cada petición en una línea JSON.
void configureServer(httplib::Server& server, const IIdGenerator& traceIds);

}  // namespace tecflix::users
