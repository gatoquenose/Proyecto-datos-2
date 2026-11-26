#pragma once

namespace tecflix::users::http_status {

// Códigos HTTP usados por la API (propios, para no depender de la versión de httplib).
inline constexpr int kOk = 200;
inline constexpr int kCreated = 201;
inline constexpr int kNoContent = 204;
inline constexpr int kBadRequest = 400;
inline constexpr int kUnauthorized = 401;
inline constexpr int kForbidden = 403;
inline constexpr int kNotFound = 404;
inline constexpr int kConflict = 409;
inline constexpr int kPayloadTooLarge = 413;
inline constexpr int kUnsupportedMediaType = 415;
inline constexpr int kInternalServerError = 500;
inline constexpr int kServiceUnavailable = 503;

}  // namespace tecflix::users::http_status
