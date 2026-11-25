#pragma once

#include <httplib.h>

#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>

namespace tecflix::users {

void sendJson(httplib::Response& response, int status, const nlohmann::json& body);

// Formato de error común a todas las APIs: { code, message, details, traceId }.
void sendError(httplib::Response& response, int status, std::string_view code, std::string_view message,
               const std::string& traceId, nlohmann::json details = nullptr);

// Lee el cuerpo como un objeto JSON. Si no es válido, responde el error (415 o 400)
// y devuelve std::nullopt.
std::optional<nlohmann::json> readJsonBody(const httplib::Request& request, httplib::Response& response,
                                           const std::string& traceId);

}  // namespace tecflix::users
