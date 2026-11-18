#include "api/UsersController.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <string_view>

#include "api/ApiResponses.hpp"
#include "api/HttpStatus.hpp"

namespace tecflix::users {

namespace {

using nlohmann::json;

constexpr std::string_view kApiPrefix = "/api/v1";
constexpr std::string_view kBearerPrefix = "bearer ";
constexpr int kMinPageSize = 1;
constexpr int kMaxPageSize = 50;

json publicJson(const User& user) {
    return json{{"id", user.id}, {"username", user.username}};
}

json meJson(const User& user) {
    return json{{"id", user.id},
                {"username", user.username},
                {"email", user.email},
                {"quotaBytes", user.quotaBytes},
                {"usedBytes", user.usedBytes}};
}

json quotaJson(const User& user) {
    const std::uint64_t remaining = user.quotaBytes > user.usedBytes ? user.quotaBytes - user.usedBytes : 0;
    return json{{"quotaBytes", user.quotaBytes}, {"usedBytes", user.usedBytes}, {"remainingBytes", remaining}};
}

std::optional<std::string> stringField(const json& body, const char* key) {
    const auto found = body.find(key);
    if (found == body.end() || !found->is_string()) {
        return std::nullopt;
    }
    return found->get<std::string>();
}

// Extrae el token de "Authorization: Bearer <token>" (la palabra Bearer no distingue mayúsculas).
std::string_view bearerToken(std::string_view header) {
    if (header.size() <= kBearerPrefix.size() ||
        !std::ranges::equal(header.substr(0, kBearerPrefix.size()), kBearerPrefix,
                            [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == b; })) {
        return {};
    }
    return header.substr(kBearerPrefix.size());
}

void sendMissingFields(httplib::Response& response, const std::string& traceId, const json& required) {
    sendError(response, http_status::kBadRequest, "VALIDATION_ERROR", "Faltan campos obligatorios o no son texto", traceId,
              json{{"required", required}});
}

void sendNoAccount(httplib::Response& response, const std::string& traceId) {
    sendError(response, http_status::kUnauthorized, "INVALID_TOKEN", "La cuenta del token ya no existe", traceId);
}

std::string path(std::string_view suffix) {
    return std::string{kApiPrefix} + std::string{suffix};
}

}  // namespace

UsersController::UsersController(UserService& service, const tecflix::auth::JwtVerifier& verifier,
                                 const IIdGenerator& traceIds, std::chrono::seconds tokenLifetime)
    : service_(service), verifier_(verifier), traceIds_(traceIds), tokenLifetime_(tokenLifetime) {}

void UsersController::registerRoutes(httplib::Server& server) const {
    using Request = httplib::Request;
    using Response = httplib::Response;
    server.Post(path("/users"), [this](const Request& req, Response& res) { registerUser(req, res); });
    server.Post(path("/auth/login"), [this](const Request& req, Response& res) { login(req, res); });
    // /users/me y /users/me/quota se registran antes que /users/:id para que "me" no se tome como un id.
    server.Get(path("/users/me"), [this](const Request& req, Response& res) { getCurrentUser(req, res); });
    server.Patch(path("/users/me"), [this](const Request& req, Response& res) { updateCurrentUser(req, res); });
    server.Get(path("/users/me/quota"), [this](const Request& req, Response& res) { getQuota(req, res); });
    server.Get(path("/users"), [this](const Request& req, Response& res) { searchUsers(req, res); });
    server.Get(path("/users/:id"), [this](const Request& req, Response& res) { getUser(req, res); });
    server.Patch(path("/users/:id"), [this](const Request& req, Response& res) { updateUser(req, res); });
    server.Delete(path("/users/:id"), [this](const Request& req, Response& res) { deleteUser(req, res); });
}

std::optional<tecflix::auth::JwtClaims> UsersController::authenticate(const httplib::Request& request,
                                                                      httplib::Response& response,
                                                                      const std::string& traceId) const {
    const std::string header = request.get_header_value("Authorization");
    const std::string_view token = bearerToken(header);
    if (token.empty()) {
        sendError(response, http_status::kUnauthorized, "MISSING_TOKEN", "Falta el token en el header Authorization",
                  traceId);
        return std::nullopt;
    }
    const tecflix::auth::VerifiedToken verified = verifier_.verify(token);
    if (verified.error == tecflix::auth::TokenError::Expired) {
        sendError(response, http_status::kUnauthorized, "TOKEN_EXPIRED", "El token expiró; inicia sesión de nuevo", traceId);
        return std::nullopt;
    }
    if (verified.error != tecflix::auth::TokenError::None) {
        sendError(response, http_status::kUnauthorized, "INVALID_TOKEN", "El token no es válido", traceId);
        return std::nullopt;
    }
    return verified.claims;
}

bool UsersController::requireSelf(const std::string& pathId, const tecflix::auth::JwtClaims& caller,
                                  httplib::Response& response, const std::string& traceId) const {
    if (pathId == caller.subject) {
        return true;
    }
    sendError(response, http_status::kForbidden, "FORBIDDEN", "Solo puedes modificar tu propia cuenta", traceId);
    return false;
}

void UsersController::registerUser(const httplib::Request& request, httplib::Response& response) const {
    const std::string traceId = traceIds_.next();
    const auto body = readJsonBody(request, response, traceId);
    if (!body) {
        return;
    }
    const auto username = stringField(*body, "username");
    const auto email = stringField(*body, "email");
    const auto password = stringField(*body, "password");
    if (!username || !email || !password) {
        sendMissingFields(response, traceId, json::array({"username", "email", "password"}));
        return;
    }

    const RegisterOutcome outcome = service_.registerUser({*username, *email, *password});
    switch (outcome.status) {
        case RegisterStatus::Created:
            sendJson(response, http_status::kCreated, publicJson(outcome.user));
            return;
        case RegisterStatus::InvalidUsername:
            sendError(response, http_status::kBadRequest, "VALIDATION_ERROR",
                      "El username debe tener de 3 a 64 caracteres: letras, números, '_', '.' o '-'", traceId,
                      json{{"field", "username"}});
            return;
        case RegisterStatus::InvalidEmail:
            sendError(response, http_status::kBadRequest, "VALIDATION_ERROR", "El email no es válido (máximo 120 caracteres)",
                      traceId, json{{"field", "email"}});
            return;
        case RegisterStatus::InvalidPassword:
            sendError(response, http_status::kBadRequest, "VALIDATION_ERROR",
                      "La contraseña debe tener entre 8 y 128 caracteres", traceId, json{{"field", "password"}});
            return;
        case RegisterStatus::UsernameTaken:
            sendError(response, http_status::kConflict, "USERNAME_EXISTS", "Ese username ya está en uso", traceId);
            return;
        case RegisterStatus::EmailTaken:
            sendError(response, http_status::kConflict, "EMAIL_EXISTS", "Ese email ya está registrado", traceId);
            return;
    }
}

void UsersController::login(const httplib::Request& request, httplib::Response& response) const {
    const std::string traceId = traceIds_.next();
    const auto body = readJsonBody(request, response, traceId);
    if (!body) {
        return;
    }
    const auto username = stringField(*body, "username");
    const auto password = stringField(*body, "password");
    if (!username || !password) {
        sendMissingFields(response, traceId, json::array({"username", "password"}));
        return;
    }

    const LoginOutcome outcome = service_.login(*username, *password);
    if (!outcome.success) {
        sendError(response, http_status::kUnauthorized, "INVALID_CREDENTIALS", "Usuario o contraseña incorrectos", traceId);
        return;
    }
    sendJson(response, http_status::kOk,
             json{{"accessToken", outcome.token},
                  {"tokenType", "Bearer"},
                  {"expiresIn", tokenLifetime_.count()},
                  {"user", meJson(outcome.user)}});
}

void UsersController::getCurrentUser(const httplib::Request& request, httplib::Response& response) const {
    const std::string traceId = traceIds_.next();
    const auto caller = authenticate(request, response, traceId);
    if (!caller) {
        return;
    }
    const auto user = service_.findById(caller->subject);
    if (!user) {
        sendNoAccount(response, traceId);
        return;
    }
    sendJson(response, http_status::kOk, meJson(*user));
}

void UsersController::getQuota(const httplib::Request& request, httplib::Response& response) const {
    const std::string traceId = traceIds_.next();
    const auto caller = authenticate(request, response, traceId);
    if (!caller) {
        return;
    }
    const auto user = service_.findById(caller->subject);
    if (!user) {
        sendNoAccount(response, traceId);
        return;
    }
    sendJson(response, http_status::kOk, quotaJson(*user));
}

void UsersController::changeEmailOf(const std::string& userId, const httplib::Request& request,
                                    httplib::Response& response, const std::string& traceId) const {
    const auto body = readJsonBody(request, response, traceId);
    if (!body) {
        return;
    }
    const auto email = stringField(*body, "email");
    if (!email || body->size() != 1) {
        sendError(response, http_status::kBadRequest, "VALIDATION_ERROR", "Solo se puede modificar el campo email", traceId,
                  json{{"editable", json::array({"email"})}});
        return;
    }

    const ChangeEmailOutcome outcome = service_.changeEmail(userId, *email);
    switch (outcome.status) {
        case ChangeEmailStatus::Changed:
            sendJson(response, http_status::kOk, meJson(outcome.user));
            return;
        case ChangeEmailStatus::InvalidEmail:
            sendError(response, http_status::kBadRequest, "VALIDATION_ERROR", "El email no es válido (máximo 120 caracteres)",
                      traceId, json{{"field", "email"}});
            return;
        case ChangeEmailStatus::NotFound:
            sendNoAccount(response, traceId);
            return;
        case ChangeEmailStatus::EmailTaken:
            sendError(response, http_status::kConflict, "EMAIL_EXISTS", "Ese email ya está registrado", traceId);
            return;
    }
}

void UsersController::updateCurrentUser(const httplib::Request& request, httplib::Response& response) const {
    const std::string traceId = traceIds_.next();
    const auto caller = authenticate(request, response, traceId);
    if (caller) {
        changeEmailOf(caller->subject, request, response, traceId);
    }
}

void UsersController::updateUser(const httplib::Request& request, httplib::Response& response) const {
    const std::string traceId = traceIds_.next();
    const auto caller = authenticate(request, response, traceId);
    if (caller && requireSelf(request.path_params.at("id"), *caller, response, traceId)) {
        changeEmailOf(caller->subject, request, response, traceId);
    }
}

void UsersController::getUser(const httplib::Request& request, httplib::Response& response) const {
    const std::string traceId = traceIds_.next();
    const auto caller = authenticate(request, response, traceId);
    if (!caller) {
        return;
    }
    const auto user = service_.findById(request.path_params.at("id"));
    if (!user) {
        sendError(response, http_status::kNotFound, "NOT_FOUND", "No existe ese usuario", traceId);
        return;
    }
    // El email y la cuota solo los ve el propio usuario.
    sendJson(response, http_status::kOk, user->id == caller->subject ? meJson(*user) : publicJson(*user));
}

void UsersController::deleteUser(const httplib::Request& request, httplib::Response& response) const {
    const std::string traceId = traceIds_.next();
    const auto caller = authenticate(request, response, traceId);
    if (!caller || !requireSelf(request.path_params.at("id"), *caller, response, traceId)) {
        return;
    }
    if (!service_.removeUser(caller->subject)) {
        sendError(response, http_status::kNotFound, "NOT_FOUND", "No existe ese usuario", traceId);
        return;
    }
    response.status = http_status::kNoContent;
}

void UsersController::searchUsers(const httplib::Request& request, httplib::Response& response) const {
    const std::string traceId = traceIds_.next();
    const auto caller = authenticate(request, response, traceId);
    if (!caller) {
        return;
    }

    if (request.has_param("pageSize")) {
        const std::string text = request.get_param_value("pageSize");
        int pageSize = 0;
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), pageSize);
        if (error != std::errc{} || end != text.data() + text.size() || pageSize < kMinPageSize || pageSize > kMaxPageSize) {
            sendError(response, http_status::kBadRequest, "VALIDATION_ERROR", "pageSize debe estar entre 1 y 50", traceId,
                      json{{"field", "pageSize"}});
            return;
        }
    }
    const bool hasUsername = request.has_param("username");
    const bool hasEmail = request.has_param("email");
    if (!hasUsername && !hasEmail) {
        sendError(response, http_status::kBadRequest, "VALIDATION_ERROR", "Indica username o email para buscar", traceId,
                  json{{"anyOf", json::array({"username", "email"})}});
        return;
    }

    std::optional<User> byUsername;
    if (hasUsername) {
        byUsername = service_.findByUsername(request.get_param_value("username"));
    }
    std::optional<User> byEmail;
    if (hasEmail) {
        byEmail = service_.findByEmail(request.get_param_value("email"));
        if (byEmail && byEmail->id != caller->subject) {
            byEmail.reset();  // el email de otras personas nunca se revela
        }
    }

    std::optional<User> match;
    if (hasUsername && hasEmail) {
        if (byUsername && byEmail && byUsername->id == byEmail->id) {
            match = byUsername;
        }
    } else {
        match = hasUsername ? byUsername : byEmail;
    }

    json items = json::array();
    if (match) {
        items.push_back(publicJson(*match));
    }
    // Búsqueda exacta: siempre cabe en una página, por eso nextCursor es null.
    sendJson(response, http_status::kOk, json{{"items", items}, {"nextCursor", nullptr}});
}

}  // namespace tecflix::users
