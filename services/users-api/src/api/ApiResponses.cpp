#include "api/ApiResponses.hpp"
#include "api/HttpStatus.hpp"

#include <algorithm>
#include <cctype>

namespace tecflix::users {

namespace {

constexpr std::string_view kJsonContentType = "application/json";

bool isJsonContentType(std::string_view contentType) {
    if (contentType.size() < kJsonContentType.size()) {
        return false;
    }
    return std::ranges::equal(contentType.substr(0, kJsonContentType.size()), kJsonContentType,
                              [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == b; });
}

}  // namespace

void sendJson(httplib::Response& response, int status, const nlohmann::json& body) {
    response.status = status;
    response.set_content(body.dump(), std::string{kJsonContentType});
}

void sendError(httplib::Response& response, int status, std::string_view code, std::string_view message,
               const std::string& traceId, nlohmann::json details) {
    sendJson(response, status,
             nlohmann::json{{"code", code}, {"message", message}, {"details", std::move(details)}, {"traceId", traceId}});
}

std::optional<nlohmann::json> readJsonBody(const httplib::Request& request, httplib::Response& response,
                                           const std::string& traceId) {
    if (!isJsonContentType(request.get_header_value("Content-Type"))) {
        sendError(response, http_status::kUnsupportedMediaType, "UNSUPPORTED_MEDIA_TYPE",
                  "El contenido debe enviarse como application/json", traceId);
        return std::nullopt;
    }
    nlohmann::json body = nlohmann::json::parse(request.body, nullptr, false);
    if (body.is_discarded() || !body.is_object()) {
        sendError(response, http_status::kBadRequest, "VALIDATION_ERROR",
                  "El cuerpo debe ser un objeto JSON válido", traceId);
        return std::nullopt;
    }
    return body;
}

}  // namespace tecflix::users
