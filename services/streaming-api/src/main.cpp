// Tecflix streaming-api — Fase E1: subida reanudable y cuotas.
// Contratos: docs/openapi/streaming.yaml y docs/openapi/users.yaml.
// Pendiente en E2: FFmpeg, MPEG-DASH, miniaturas, confirmaciones internas de metadata.
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <sodium.h>
#include "tecflix/auth/CryptoRuntime.hpp"
#include "tecflix/auth/JwtVerifier.hpp"
#include "tecflix/auth/SystemClock.hpp"
#include "StreamingProcessingE2.hpp"

#include <algorithm>
#include <array>
#include <condition_variable>
#include <deque>
#include <thread>
#include <vector>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <iostream>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace fs = std::filesystem;
using Json = nlohmann::json;
namespace auth = tecflix::auth;

namespace {
constexpr std::size_t kChunkBytes = 4 * 1024 * 1024;
constexpr std::uint64_t kMaxVideoBytes = 5ULL * 1024 * 1024 * 1024;
constexpr std::size_t kMaxBodyBytes = kChunkBytes + 1024;

std::string envOr(const char* key, std::string fallback) {
    if (const char* value = std::getenv(key); value && *value) return value;
    return fallback;
}
std::string requiredEnv(const char* key) {
    const auto value = envOr(key, "");
    if (value.empty()) throw std::runtime_error(std::string("Falta ") + key);
    return value;
}
bool uuid(const std::string& id) {
    if (id.size() != 36) return false;
    for (std::size_t i = 0; i < id.size(); ++i) {
        const char c = id[i];
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (c != '-') return false;
        } else if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
    }
    return true;
}
std::string randomUuid() {
    std::array<unsigned char, 16> data{};
    randombytes_buf(data.data(), data.size());
    data[6] = static_cast<unsigned char>((data[6] & 0x0f) | 0x40);
    data[8] = static_cast<unsigned char>((data[8] & 0x3f) | 0x80);
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < data.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) out << '-';
        out << std::setw(2) << static_cast<int>(data[i]);
    }
    return out.str();
}
std::string digest(std::string_view content) {
    std::array<unsigned char, crypto_hash_sha256_BYTES> hash{};
    crypto_hash_sha256(hash.data(), reinterpret_cast<const unsigned char*>(content.data()), content.size());
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (const auto b : hash) out << std::setw(2) << static_cast<int>(b);
    return out.str();
}
bool checksum(const std::string& s) {
    return s.size() == 64 && std::all_of(s.begin(), s.end(), [](char c) {
        return std::isxdigit(static_cast<unsigned char>(c)) != 0;
    });
}
std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}
void sendJson(httplib::Response& res, int status, const Json& obj) {
    res.status = status;
    res.set_content(obj.dump(), "application/json");
}
void fail(httplib::Response& res, int status, const char* code, const std::string& msg) {
    sendJson(res, status, Json{{"code", code}, {"message", msg}, {"details", Json::object()}, {"traceId", ""}});
}
std::optional<Json> parseBody(const httplib::Request& req) {
    if (req.get_header_value("Content-Type").find("application/json") != 0 || req.body.size() > 65536) return std::nullopt;
    Json j = Json::parse(req.body, nullptr, false);
    if (!j.is_object()) return std::nullopt;
    return j;
}
struct Upload {
    std::string uploadId, videoId, ownerId, contentType, fileChecksum, status = "active", error;
    std::uint64_t sizeBytes = 0, receivedBytes = 0, reservedBytes = 0;
    int durationSeconds = 0, videoRenditions = 0;
    bool quotaSettled = false, metadataSynced = false;
    unsigned totalChunks = 0;
    std::set<unsigned> receivedChunks;
};
Json asJson(const Upload& u) {
    return Json{{"uploadId", u.uploadId}, {"videoId", u.videoId}, {"ownerId", u.ownerId},
                {"sizeBytes", u.sizeBytes}, {"chunkSizeBytes", kChunkBytes},
                {"totalChunks", u.totalChunks}, {"receivedChunks", u.receivedChunks},
                {"receivedBytes", u.receivedBytes}, {"status", u.status},
                {"error", u.error.empty() ? Json(nullptr) : Json(u.error)},
                {"durationSeconds", u.durationSeconds}, {"videoRenditions", u.videoRenditions},
                {"quotaSettled", u.quotaSettled},
                {"metadataSynced", u.metadataSynced}, {"reservedBytes", u.reservedBytes}};
}
void save(const fs::path& root, const Upload& u) {
    fs::create_directories(root / "sessions");
    const auto path = root / "sessions" / (u.uploadId + ".json");
    auto temp = path;
    temp += ".tmp";
    Json j = asJson(u);
    j["contentType"] = u.contentType;
    j["fileChecksum"] = u.fileChecksum;
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) throw std::runtime_error("No fue posible guardar el estado de subida");
        out << j.dump(2);
        if (!out.good()) throw std::runtime_error("Error al escribir el estado de subida");
    }
    std::error_code ec;
    fs::remove(path, ec);  // Windows no permite renombrar sobre archivo existente.
    fs::rename(temp, path);
}
std::map<std::string, Upload> load(const fs::path& root) {
    std::map<std::string, Upload> result;
    const auto dir = root / "sessions";
    if (!fs::exists(dir)) return result;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
        try {
            std::ifstream in(entry.path(), std::ios::binary);
            Json j; in >> j;
            Upload u;
            u.uploadId = j.at("uploadId").get<std::string>();
            u.videoId = j.at("videoId").get<std::string>();
            u.ownerId = j.at("ownerId").get<std::string>();
            u.contentType = j.at("contentType").get<std::string>();
            u.fileChecksum = j.at("fileChecksum").get<std::string>();
            u.status = j.at("status").get<std::string>();
            u.sizeBytes = j.at("sizeBytes").get<std::uint64_t>();
            u.receivedBytes = j.at("receivedBytes").get<std::uint64_t>();
            u.totalChunks = j.at("totalChunks").get<unsigned>();
            u.reservedBytes = j.value("reservedBytes", u.sizeBytes);
            u.durationSeconds = j.value("durationSeconds", 0);
            u.videoRenditions = j.value("videoRenditions", 0);
            u.quotaSettled = j.value("quotaSettled", false);
            u.metadataSynced = j.value("metadataSynced", false);
            if (j.contains("error") && j["error"].is_string()) u.error = j["error"].get<std::string>();
            for (const auto& c : j.at("receivedChunks")) u.receivedChunks.insert(c.get<unsigned>());
            if (uuid(u.uploadId) && uuid(u.videoId) && uuid(u.ownerId)) result.emplace(u.uploadId, std::move(u));
        } catch (const std::exception& ex) {
            std::cerr << "Estado invalido " << entry.path().filename().string() << ": " << ex.what() << '\n';
        }
    }
    return result;
}

class StreamingApi {
public:
    StreamingApi(std::string secret, std::string serviceToken, fs::path root,
                 std::string usersHost, int usersPort, std::string metadataHost, int metadataPort)
        : clock_(), verifier_(std::move(secret), clock_), token_(std::move(serviceToken)), root_(std::move(root)),
          usersHost_(std::move(usersHost)), usersPort_(usersPort), metadataHost_(std::move(metadataHost)),
          metadataPort_(metadataPort), transcoder_(envOr("FFMPEG_BIN", "ffmpeg"), envOr("FFPROBE_BIN", "ffprobe")),
          frontendOrigin_(envOr("FRONTEND_ORIGIN", "http://localhost:5173")) {
        fs::create_directories(root_ / "sessions");
        fs::create_directories(root_ / "uploads");
        uploads_ = load(root_);
        // En reinicios se recuperan trabajos que ya tenian original.bin y notificaciones pendientes.
        for (const auto& [id, u] : uploads_) {
            if ((u.status == "processing" && fs::exists(root_ / "uploads" / id / "original.bin")) ||
                (u.status == "ready" && !u.metadataSynced)) jobs_.push_back(id);
        }
        toolsAvailable_ = transcoder_.available();
        worker_ = std::thread([this] { workLoop(); });
        cv_.notify_one();
    }
    ~StreamingApi() {
        { std::lock_guard lock(mutex_); stopping_ = true; }
        cv_.notify_all();
        if (worker_.joinable()) worker_.join();
    }
    void configure(httplib::Server& server) {
        server.Get("/api/v1/health", [](const auto&, auto& res) { sendJson(res, 200, {{"status", "ok"}}); });
        server.Get("/health", [](const auto&, auto& res) { sendJson(res, 200, {{"status", "ok"}}); });
        server.Get("/api/v1/ready", [this](const auto&, auto& res) {
            const bool ready = toolsAvailable_ && fs::exists(root_);
            sendJson(res, ready ? 200 : 503, {{"status", ready ? "ready" : "unavailable"}});
        });
        server.Get("/ready", [this](const auto&, auto& res) {
            const bool ready = toolsAvailable_ && fs::exists(root_);
            sendJson(res, ready ? 200 : 503, {{"status", ready ? "ready" : "unavailable"}});
        });
        server.Post(R"(/api/v1/videos/([0-9A-Fa-f-]{36})/upload)", [this](const auto& req, auto& res) { start(req, res); });
        server.Put(R"(/api/v1/uploads/([0-9A-Fa-f-]{36})/chunks/([0-9]+))", [this](const auto& req, auto& res) { chunk(req, res); });
        server.Get(R"(/api/v1/uploads/([0-9A-Fa-f-]{36}))", [this](const auto& req, auto& res) { get(req, res); });
        server.Delete(R"(/api/v1/uploads/([0-9A-Fa-f-]{36}))", [this](const auto& req, auto& res) { cancel(req, res); });
        server.Delete(R"(/api/v1/internal/videos/([0-9A-Fa-f-]{36}))", [this](const auto& req, auto& res) { requestPhysicalDeletion(req, res); });
        server.Post(R"(/api/v1/uploads/([0-9A-Fa-f-]{36})/complete)", [this](const auto& req, auto& res) { complete(req, res); });
        server.Get(R"(/api/v1/videos/([0-9A-Fa-f-]{36})/processing)", [this](const auto& req, auto& res) { processing(req, res); });
        server.Get(R"(/api/v1/videos/([0-9A-Fa-f-]{36})/manifest\.mpd)", [this](const auto& req, auto& res) { media(req, res, "manifest.mpd"); });
        server.Get(R"(/api/v1/videos/([0-9A-Fa-f-]{36})/thumbnail)", [this](const auto& req, auto& res) { media(req, res, "thumbnail.jpg"); });
        server.Get(R"(/api/v1/videos/([0-9A-Fa-f-]{36})/dash/([A-Za-z0-9_.-]+))", [this](const auto& req, auto& res) {
            const std::string filename = req.matches[2].str();
            if (filename == "." || filename == ".." || filename.find("..") != std::string::npos) {
                fail(res, 400, "VALIDATION_ERROR", "Archivo invalido"); return;
            }
            media(req, res, filename);
        });
        server.Options(R"(/api/v1/videos/([0-9A-Fa-f-]{36})/(manifest\.mpd|thumbnail|dash/[A-Za-z0-9_.-]+))", [this](const auto& req, auto& res) {
            cors(req, res); res.status = req.get_header_value("Origin") == frontendOrigin_ ? 204 : 403;
        });
    }
private:
    auth::SystemClock clock_;
    auth::JwtVerifier verifier_;
    std::string token_;
    fs::path root_;
    std::string usersHost_, metadataHost_;
    int usersPort_, metadataPort_;
    std::mutex mutex_;
    std::map<std::string, Upload> uploads_;
    tecflix::streaming::FfmpegTranscoder transcoder_;
    std::string frontendOrigin_;
    bool toolsAvailable_ = false;
    std::condition_variable cv_;
    std::deque<std::string> jobs_;
    std::deque<std::string> deletionJobs_;
    std::set<std::string> deletingVideos_;
    std::thread worker_;
    bool stopping_ = false;

    void cors(const httplib::Request& req, httplib::Response& res) const {
        if (req.get_header_value("Origin") == frontendOrigin_) {
            res.set_header("Access-Control-Allow-Origin", frontendOrigin_);
            res.set_header("Vary", "Origin");
            res.set_header("Access-Control-Allow-Headers", "Authorization, Range, Content-Type");
            res.set_header("Access-Control-Allow-Methods", "GET, OPTIONS");
            res.set_header("Access-Control-Expose-Headers", "Accept-Ranges, Content-Range, Content-Length");
        }
    }
    bool notifyMetadata(const Upload& u, bool success, int duration, std::uint64_t bytes,
                        const std::string& reason = "") {
        httplib::Client client(metadataHost_, metadataPort_);
        client.set_connection_timeout(3);
        client.set_read_timeout(5);
        const httplib::Headers h{{"Authorization", "Bearer " + token_}};
        const std::string url = "/api/v1/internal/videos/" + u.videoId +
            (success ? "/processing-complete" : "/processing-failed");
        const Json body = success ? Json{{"durationSeconds", duration}, {"finalSizeBytes", bytes}}
                                  : Json{{"reason", reason.substr(0, 900)}};
        auto r = client.Post(url, h, body.dump(), "application/json");
        if (!r || r->status != 204) {
            std::cerr << "Metadata no confirmo " << url << " ("
                      << (r ? std::to_string(r->status) : "sin respuesta") << ")\n";
            return false;
        }
        return true;
    }
    bool quotaSettle(const Upload& u, std::uint64_t bytes) {
        httplib::Client client(usersHost_, usersPort_);
        client.set_connection_timeout(3); client.set_read_timeout(5);
        const httplib::Headers h{{"Authorization", "Bearer " + token_}};
        const Json body{{"finalBytes", bytes}};
        auto r = client.Post("/api/v1/internal/quota/reservations/" + u.videoId + "/settle", h,
                             body.dump(), "application/json");
        if (!r || r->status != 200) {
            std::cerr << "No se pudo ajustar la cuota de " << u.videoId << " ("
                      << (r ? std::to_string(r->status) : "sin respuesta") << ")\n";
            return false;
        }
        return true;
    }
    void workLoop() {
        for (;;) {
            std::string id;
            std::string deletionId;
            Upload snapshot;
            {
                std::unique_lock lock(mutex_);
                cv_.wait(lock, [this] { return stopping_ || !jobs_.empty() || !deletionJobs_.empty(); });
                if (stopping_) return;
                if (!deletionJobs_.empty()) {
                    deletionId = std::move(deletionJobs_.front());
                    deletionJobs_.pop_front();
                } else {
                    id = std::move(jobs_.front()); jobs_.pop_front();
                    const auto it = uploads_.find(id);
                    if (it == uploads_.end()) continue;
                    snapshot = it->second;
                }
            }
            if (!deletionId.empty()) {
                performPhysicalDeletion(deletionId);
                continue;
            }
            if (snapshot.status == "ready" && !snapshot.metadataSynced) {
                const bool synced = notifyMetadata(snapshot, true, snapshot.durationSeconds,
                    tecflix::streaming::sumBytes(root_ / "uploads" / id / "original.bin") +
                    tecflix::streaming::sumBytes(root_ / "uploads" / id / "dash") +
                    tecflix::streaming::sumBytes(root_ / "uploads" / id / "thumbnail.jpg"));
                if (synced) {
                    std::lock_guard lock(mutex_);
                    auto it = uploads_.find(id);
                    if (it != uploads_.end()) { it->second.metadataSynced = true; save(root_, it->second); }
                }
                continue;
            }
            if (snapshot.status != "processing") continue;
            try {
                if (!toolsAvailable_) throw std::runtime_error("FFmpeg/FFprobe no estan disponibles");
                const auto result = transcoder_.transcode(root_ / "uploads" / id);
                if (!quotaSettle(snapshot, result.finalBytes)) {
                    throw std::runtime_error("Usuarios rechazo el ajuste de cuota");
                }
                {
                    std::lock_guard lock(mutex_);
                    auto it = uploads_.find(id);
                    if (it == uploads_.end()) continue;
                    it->second.status = "ready";
                    it->second.durationSeconds = result.durationSeconds;
                    it->second.videoRenditions = result.videoRenditions;
                    it->second.quotaSettled = true;
                    it->second.error.clear();
                    save(root_, it->second);
                }
                const bool synced = notifyMetadata(snapshot, true, result.durationSeconds, result.finalBytes);
                {
                    std::lock_guard lock(mutex_);
                    auto it = uploads_.find(id);
                    if (it != uploads_.end()) { it->second.metadataSynced = synced; save(root_, it->second); }
                }
                std::cout << "DASH listo: " << snapshot.videoId << " (" << result.durationSeconds << " s)\n";
            } catch (const std::exception& ex) {
                std::string reason = ex.what();
                std::cerr << "Procesamiento fallido " << snapshot.videoId << ": " << reason << '\n';
                std::error_code ec;
                fs::remove_all(root_ / "uploads" / id / "dash.tmp", ec);
                fs::remove_all(root_ / "uploads" / id / "dash", ec);
                fs::remove(root_ / "uploads" / id / "thumbnail.tmp.jpg", ec);
                fs::remove(root_ / "uploads" / id / "thumbnail.jpg", ec);
                httplib::Response ignored;
                const bool released = quotaRelease(snapshot, ignored);
                const bool synced = notifyMetadata(snapshot, false, 0, 0, reason);
                std::lock_guard lock(mutex_);
                auto it = uploads_.find(id);
                if (it != uploads_.end()) {
                    it->second.status = "failed";
                    it->second.error = reason.substr(0, 900) + (released ? "" : " (cuota pendiente de liberar)");
                    it->second.metadataSynced = synced;
                    save(root_, it->second);
                }
            }
        }
    }
    void media(const httplib::Request& req, httplib::Response& res, const std::string& filename) {
        cors(req, res);
        const auto claims = user(req, res);
        if (!claims) return;
        const auto vid = req.matches[1].str();
        if (!uuid(vid)) { fail(res, 400, "VALIDATION_ERROR", "videoId invalido"); return; }
        std::string id;
        int videoRenditions = 0;
        {
            std::lock_guard lock(mutex_);
            for (const auto& [uploadId, u] : uploads_) {
                if (u.videoId == vid && u.status == "ready") {
                    id = uploadId; videoRenditions = u.videoRenditions; break;
                }
            }
        }
        if (id.empty()) { fail(res, 404, "NOT_FOUND", "Video aun no reproducible"); return; }
        // Metadata aplica las reglas de visibilidad. Una respuesta 200 solo es suficiente
        // si pertenece al usuario o si la visibilidad es public.
        httplib::Client client(metadataHost_, metadataPort_);
        client.set_connection_timeout(3); client.set_read_timeout(5);
        const httplib::Headers h{{"Authorization", req.get_header_value("Authorization")}};
        auto result = client.Get("/api/v1/videos/" + vid, h);
        if (!result) { fail(res, 502, "DEPENDENCY_ERROR", "Metadata no responde"); return; }
        if (result->status != 200) { fail(res, 404, "NOT_FOUND", "Video no accesible"); return; }
        Json item = Json::parse(result->body, nullptr, false);
        if (!item.is_object() || (item.value("ownerId", "") != claims->subject &&
                                  item.value("visibility", "") != "public")) {
            fail(res, 403, "FORBIDDEN", "Video privado"); return;
        }
        const bool manifest = filename == "manifest.mpd";
        const bool thumbnail = filename == "thumbnail.jpg";
        const fs::path path = root_ / "uploads" / id /
            (manifest ? fs::path("dash") / filename : (thumbnail ? fs::path(filename) : fs::path("dash") / filename));
        if (!fs::is_regular_file(path)) { fail(res, 404, "NOT_FOUND", "Archivo no disponible"); return; }
        res.set_header("Accept-Ranges", "bytes");
        res.set_header("Cache-Control", manifest ? "no-cache" : "private, max-age=3600");
        std::string mime = manifest ? "application/dash+xml" : (thumbnail ? "image/jpeg" : "application/octet-stream");
        std::smatch match;
        const static std::regex segmentRegex(R"((?:init|chunk)-stream([0-9]{1,2})(?:-[0-9]+)?\.m4s)");
        if (!manifest && !thumbnail && std::regex_match(filename, match, segmentRegex)) {
            const int rendition = std::stoi(match[1].str());
            mime = rendition >= videoRenditions && videoRenditions > 0 ? "audio/mp4" : "video/mp4";
        }
        res.set_file_content(fs::absolute(path).string(), mime);
    }

    std::optional<auth::JwtClaims> user(const httplib::Request& req, httplib::Response& res) {
        const std::string header = req.get_header_value("Authorization");
        if (header.rfind("Bearer ", 0) != 0) {
            fail(res, 401, "MISSING_TOKEN", "Falta token Bearer"); return std::nullopt;
        }
        const auto result = verifier_.verify(std::string_view(header).substr(7));
        if (result.error != auth::TokenError::None) {
            fail(res, 401, "INVALID_TOKEN", "JWT invalido o expirado"); return std::nullopt;
        }
        return result.claims;
    }
    bool metadata(const httplib::Request& req, httplib::Response& res,
                  const std::string& videoId, const std::string& ownerId) {
        httplib::Client client(metadataHost_, metadataPort_);
        client.set_connection_timeout(3);
        client.set_read_timeout(5);
        const httplib::Headers headers{{"Authorization", req.get_header_value("Authorization")}};
        auto result = client.Get("/api/v1/videos/" + videoId, headers);
        if (!result) { fail(res, 502, "DEPENDENCY_ERROR", "Metadata no responde"); return false; }
        if (result->status == 404 || result->status == 403) {
            fail(res, 404, "NOT_FOUND", "Video no encontrado"); return false;
        }
        if (result->status != 200) { fail(res, 502, "DEPENDENCY_ERROR", "Metadata no disponible"); return false; }
        const Json item = Json::parse(result->body, nullptr, false);
        if (!item.is_object() || item.value("ownerId", "") != ownerId) {
            fail(res, 403, "FORBIDDEN", "Video ajeno"); return false;
        }
        if (item.value("status", "") != "processing") {
            fail(res, 409, "INVALID_STATE", "Video no esta en processing"); return false;
        }
        return true;
    }
    bool quotaReserve(const Upload& u, httplib::Response& res) {
        httplib::Client client(usersHost_, usersPort_);
        client.set_connection_timeout(3);
        client.set_read_timeout(5);
        const httplib::Headers headers{{"Authorization", "Bearer " + token_}};
        const Json body{{"reservationId", u.videoId}, {"userId", u.ownerId}, {"bytes", u.reservedBytes}};
        auto result = client.Post("/api/v1/internal/quota/reservations", headers, body.dump(), "application/json");
        if (!result) { fail(res, 502, "DEPENDENCY_ERROR", "Usuarios no responde"); return false; }
        if (result->status == 403) { fail(res, 413, "QUOTA_EXCEEDED", "Cuota insuficiente"); return false; }
        if (result->status != 200 && result->status != 201) {
            fail(res, 502, "DEPENDENCY_ERROR", "No se pudo reservar cuota; revisar usuarios"); return false;
        }
        return true;
    }
    bool quotaRelease(const Upload& u, httplib::Response& res) {
        httplib::Client client(usersHost_, usersPort_);
        client.set_connection_timeout(3);
        client.set_read_timeout(5);
        const httplib::Headers headers{{"Authorization", "Bearer " + token_}};
        auto result = client.Post("/api/v1/internal/quota/reservations/" + u.videoId + "/release", headers, "", "application/json");
        if (!result || result->status != 204) {
            fail(res, 502, "DEPENDENCY_ERROR", "No fue posible liberar la reserva de cuota"); return false;
        }
        return true;
    }
    void start(const httplib::Request& req, httplib::Response& res) {
        const auto claims = user(req, res);
        if (!claims) return;
        const std::string videoId = req.matches[1].str();
        if (!uuid(videoId)) { fail(res, 400, "VALIDATION_ERROR", "videoId invalido"); return; }
        const auto body = parseBody(req);
        if (!body || !body->contains("sizeBytes") || !(*body)["sizeBytes"].is_number_unsigned() ||
            !body->contains("totalChunks") || !(*body)["totalChunks"].is_number_unsigned() ||
            !body->contains("fileChecksum") || !(*body)["fileChecksum"].is_string() ||
            !body->contains("contentType") || !(*body)["contentType"].is_string()) {
            fail(res, 400, "VALIDATION_ERROR", "Solicitud incompleta"); return;
        }
        Upload u;
        u.videoId = videoId;
        u.ownerId = claims->subject;
        u.uploadId = randomUuid();
        u.fileChecksum = lower((*body)["fileChecksum"].get<std::string>());
        u.contentType = (*body)["contentType"].get<std::string>();
        u.sizeBytes = (*body)["sizeBytes"].get<std::uint64_t>();
        const auto chunkCount = (*body)["totalChunks"].get<std::uint64_t>();
        const auto expectedCount = (u.sizeBytes + kChunkBytes - 1) / kChunkBytes;
        const bool supported = u.contentType == "video/mp4" || u.contentType == "video/webm" ||
                               u.contentType == "video/quicktime" || u.contentType == "video/x-matroska";
        if (!supported) { fail(res, 415, "UNSUPPORTED_MEDIA_TYPE", "Formato no admitido"); return; }
        if (!checksum(u.fileChecksum) || u.sizeBytes == 0 || u.sizeBytes > kMaxVideoBytes ||
            chunkCount == 0 || chunkCount != expectedCount || chunkCount > std::numeric_limits<unsigned>::max()) {
            fail(res, 400, "VALIDATION_ERROR", "Tamano, checksum o cantidad de fragmentos invalidos"); return;
        }
        u.totalChunks = static_cast<unsigned>(chunkCount);
        // Reservar espacio para original + las rendiciones DASH + miniatura.
        const auto multiplier = std::clamp(std::stoi(envOr("STREAMING_QUOTA_MULTIPLIER", "8")), 2, 16);
        if (u.sizeBytes > std::numeric_limits<std::uint64_t>::max() / static_cast<unsigned>(multiplier)) {
            fail(res, 400, "VALIDATION_ERROR", "Archivo demasiado grande"); return;
        }
        u.reservedBytes = u.sizeBytes * static_cast<unsigned>(multiplier);
        std::lock_guard lock(mutex_);
        for (const auto& [_, other] : uploads_) {
            if (other.videoId == videoId && other.status != "cancelled" && other.status != "failed") {
                fail(res, 409, "INVALID_STATE", "Este video ya tiene una subida activa"); return;
            }
        }
        if (!metadata(req, res, videoId, u.ownerId)) return;
        if (!quotaReserve(u, res)) return;
        try {
            fs::create_directories(root_ / "uploads" / u.uploadId / "parts");
            save(root_, u);
            uploads_.emplace(u.uploadId, u);
        } catch (const std::exception&) {
            httplib::Response ignored;
            quotaRelease(u, ignored);
            fail(res, 500, "INTERNAL_ERROR", "No fue posible iniciar la subida"); return;
        }
        sendJson(res, 201, asJson(u));
    }
    void chunk(const httplib::Request& req, httplib::Response& res) {
        const auto claims = user(req, res);
        if (!claims) return;
        const std::string id = req.matches[1].str();
        if (!uuid(id)) { fail(res, 400, "VALIDATION_ERROR", "uploadId invalido"); return; }
        unsigned n = 0;
        try {
            const auto value = std::stoull(req.matches[2].str());
            if (value > std::numeric_limits<unsigned>::max()) throw std::out_of_range("n");
            n = static_cast<unsigned>(value);
        } catch (const std::exception&) { fail(res, 400, "VALIDATION_ERROR", "Numero invalido"); return; }
        std::lock_guard lock(mutex_);
        const auto it = uploads_.find(id);
        if (it == uploads_.end()) { fail(res, 404, "NOT_FOUND", "Subida desconocida"); return; }
        Upload& u = it->second;
        if (u.ownerId != claims->subject) { fail(res, 403, "FORBIDDEN", "Subida ajena"); return; }
        if (u.status != "active") { fail(res, 409, "INVALID_STATE", "Subida no activa"); return; }
        if (n >= u.totalChunks) { fail(res, 400, "VALIDATION_ERROR", "Fragmento fuera de rango"); return; }
        const std::uint64_t start = static_cast<std::uint64_t>(n) * kChunkBytes;
        const std::uint64_t expected = std::min<std::uint64_t>(kChunkBytes, u.sizeBytes - start);
        if (req.body.size() != expected || req.get_header_value("Content-Type").find("application/octet-stream") != 0) {
            fail(res, 400, "VALIDATION_ERROR", "Tamano o Content-Type invalido"); return;
        }
        const std::string expectedRange = "bytes " + std::to_string(start) + "-" +
                                          std::to_string(start + expected - 1) + "/" + std::to_string(u.sizeBytes);
        if (req.get_header_value("Content-Range") != expectedRange) {
            fail(res, 400, "VALIDATION_ERROR", "Content-Range incorrecto"); return;
        }
        const auto sentHash = lower(req.get_header_value("X-Chunk-Checksum"));
        if (!checksum(sentHash) || digest(req.body) != sentHash) {
            fail(res, 400, "VALIDATION_ERROR", "SHA-256 de fragmento incorrecto"); return;
        }
        const auto part = root_ / "uploads" / id / "parts" / (std::to_string(n) + ".part");
        if (u.receivedChunks.count(n) != 0) {
            // Reenvio del mismo fragmento es idempotente; no se reescribe el archivo.
            std::ifstream in(part, std::ios::binary);
            const std::string bytes{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
            if (!in || bytes != req.body) {
                fail(res, 409, "INVALID_STATE", "Fragmento existente con contenido distinto"); return;
            }
            res.status = 204; return;
        }
        try {
            std::ofstream out(part, std::ios::binary | std::ios::trunc);
            if (!out) throw std::runtime_error("No se puede abrir archivo de fragmento");
            out.write(req.body.data(), static_cast<std::streamsize>(req.body.size()));
            if (!out.good()) throw std::runtime_error("Error al escribir fragmento");
            out.close();
            Upload updated = u;
            updated.receivedChunks.insert(n);
            updated.receivedBytes += expected;
            save(root_, updated);
            u = std::move(updated);
            res.status = 204;
        } catch (const std::exception&) {
            fail(res, 500, "INTERNAL_ERROR", "No se pudo escribir fragmento");
        }
    }
    void get(const httplib::Request& req, httplib::Response& res) {
        const auto claims = user(req, res);
        if (!claims) return;
        std::lock_guard lock(mutex_);
        const auto it = uploads_.find(req.matches[1].str());
        if (it == uploads_.end()) { fail(res, 404, "NOT_FOUND", "Subida desconocida"); return; }
        if (it->second.ownerId != claims->subject) { fail(res, 403, "FORBIDDEN", "Subida ajena"); return; }
        sendJson(res, 200, asJson(it->second));
    }
    void cancel(const httplib::Request& req, httplib::Response& res) {
        const auto claims = user(req, res);
        if (!claims) return;
        std::lock_guard lock(mutex_);
        const auto it = uploads_.find(req.matches[1].str());
        if (it == uploads_.end()) { fail(res, 404, "NOT_FOUND", "Subida desconocida"); return; }
        if (it->second.ownerId != claims->subject) { fail(res, 403, "FORBIDDEN", "Subida ajena"); return; }
        if (it->second.status != "active") { fail(res, 409, "INVALID_STATE", "No puede cancelarse en ese estado"); return; }
        if (!quotaRelease(it->second, res)) return;
        try {
            fs::remove_all(root_ / "uploads" / it->first);
            fs::remove(root_ / "sessions" / (it->first + ".json"));
            uploads_.erase(it);
            res.status = 204;
        } catch (const std::exception&) { fail(res, 500, "INTERNAL_ERROR", "Error al limpiar archivos"); }
    }
    void complete(const httplib::Request& req, httplib::Response& res) {
        const auto claims = user(req, res);
        if (!claims) return;
        const auto body = parseBody(req);
        if (!body || !body->contains("fileChecksum") || !(*body)["fileChecksum"].is_string()) {
            fail(res, 400, "VALIDATION_ERROR", "Falta checksum final"); return;
        }
        std::lock_guard lock(mutex_);
        const auto it = uploads_.find(req.matches[1].str());
        if (it == uploads_.end()) { fail(res, 404, "NOT_FOUND", "Subida desconocida"); return; }
        Upload& u = it->second;
        if (u.ownerId != claims->subject) { fail(res, 403, "FORBIDDEN", "Subida ajena"); return; }
        if (u.status != "active" || u.receivedChunks.size() != u.totalChunks ||
            lower((*body)["fileChecksum"].get<std::string>()) != u.fileChecksum) {
            fail(res, 409, "INVALID_STATE", "Faltan fragmentos o checksum no coincide"); return;
        }
        const auto destination = root_ / "uploads" / u.uploadId / "original.bin";
        const auto temp = root_ / "uploads" / u.uploadId / "original.tmp";
        try {
            crypto_hash_sha256_state state;
            crypto_hash_sha256_init(&state);
            std::ofstream out(temp, std::ios::binary | std::ios::trunc);
            if (!out) throw std::runtime_error("No se pudo crear video original");
            std::array<char, 64 * 1024> buffer{};
            std::uint64_t total = 0;
            for (unsigned i = 0; i < u.totalChunks; ++i) {
                std::ifstream in(root_ / "uploads" / u.uploadId / "parts" / (std::to_string(i) + ".part"), std::ios::binary);
                if (!in) throw std::runtime_error("Fragmento no disponible");
                while (in) {
                    in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
                    const auto count = in.gcount();
                    if (count <= 0) break;
                    out.write(buffer.data(), count);
                    if (!out) throw std::runtime_error("Error al unir fragmentos");
                    crypto_hash_sha256_update(&state, reinterpret_cast<const unsigned char*>(buffer.data()), static_cast<unsigned long long>(count));
                    total += static_cast<std::uint64_t>(count);
                }
            }
            out.close();
            std::array<unsigned char, crypto_hash_sha256_BYTES> hash{};
            crypto_hash_sha256_final(&state, hash.data());
            std::ostringstream hex; hex << std::hex << std::setfill('0');
            for (const auto b : hash) hex << std::setw(2) << static_cast<int>(b);
            if (hex.str() != u.fileChecksum || total != u.sizeBytes) {
                fs::remove(temp);
                fail(res, 400, "VALIDATION_ERROR", "SHA-256 o tamano final no coincide"); return;
            }
            fs::rename(temp, destination);
            Upload updated = u;
            updated.status = "processing";
            save(root_, updated);
            u = std::move(updated);
            jobs_.push_back(u.uploadId);
            cv_.notify_one();
            sendJson(res, 202, {{"videoId", u.videoId}, {"status", "processing"}});
        } catch (const std::exception& ex) {
            fail(res, 500, "INTERNAL_ERROR", std::string("Error al completar: ") + ex.what());
        }
    }
    // Solo metadata puede ordenar un borrado mediante el token entre servicios.
    void requestPhysicalDeletion(const httplib::Request& req, httplib::Response& res) {
        const std::string header = req.get_header_value("Authorization");
        constexpr std::string_view prefix = "Bearer ";
        if (header.size() != prefix.size() + token_.size() ||
            header.compare(0, prefix.size(), prefix) != 0 ||
            sodium_memcmp(header.data() + prefix.size(), token_.data(), token_.size()) != 0) {
            fail(res, 401, "INVALID_TOKEN", "Token de servicio invalido"); return;
        }
        const std::string videoId = req.matches[1].str();
        if (!uuid(videoId)) { fail(res, 400, "VALIDATION_ERROR", "videoId invalido"); return; }
        {
            std::lock_guard lock(mutex_);
            for (const auto& [_, u] : uploads_) {
                if (u.videoId == videoId && u.status == "active") {
                    fail(res, 409, "INVALID_STATE", "Cancela la subida activa primero"); return;
                }
            }
            if (deletingVideos_.insert(videoId).second) {
                deletionJobs_.push_back(videoId);
                cv_.notify_one();
            }
        }
        res.status = 202;
    }

    // El mismo worker que transcodifica procesa el borrado; nunca elimina
    // un directorio mientras FFmpeg esta trabajando sobre ese video.
    void performPhysicalDeletion(const std::string& videoId) {
        try {
            std::vector<std::string> ids;
            {
                std::lock_guard lock(mutex_);
                for (const auto& [uploadId, u] : uploads_)
                    if (u.videoId == videoId) ids.push_back(uploadId);
            }
            for (const auto& id : ids) {
                // Si Windows no permite eliminar algun archivo, la excepcion
                // impide notificar la liberacion de cuota; permite reintento.
                fs::remove_all(root_ / "uploads" / id);
                fs::remove(root_ / "sessions" / (id + ".json"));
                std::lock_guard lock(mutex_);
                uploads_.erase(id);
            }
            httplib::Client client(metadataHost_, metadataPort_);
            client.set_connection_timeout(3);
            client.set_read_timeout(8);
            const httplib::Headers h{{"Authorization", "Bearer " + token_}};
            const auto endpoint = "/api/v1/internal/videos/" + videoId + "/streaming-deleted";
            auto reply = client.Post(endpoint, h, "", "application/json");
            if (!reply || reply->status != 204) {
                std::cerr << "Pendiente callback streaming-deleted " << videoId
                          << " (" << (reply ? std::to_string(reply->status) : "sin respuesta")
                          << "); reintentar DELETE metadata\n";
            } else {
                std::cout << "Video borrado y cuota confirmada: " << videoId << '\n';
            }
        } catch (const std::exception& ex) {
            std::cerr << "No se pudo borrar video " << videoId << ": " << ex.what()
                      << "; reintentar DELETE metadata\n";
        }
        std::lock_guard lock(mutex_);
        deletingVideos_.erase(videoId);
    }

    void processing(const httplib::Request& req, httplib::Response& res) {
        const auto claims = user(req, res);
        if (!claims) return;
        const auto vid = req.matches[1].str();
        std::lock_guard lock(mutex_);
        for (const auto& [_, u] : uploads_) {
            if (u.videoId == vid) {
                if (u.ownerId != claims->subject) {
                    fail(res, 403, "FORBIDDEN", "Video ajeno"); return;
                }
                sendJson(res, 200, {{"videoId", vid}, {"status", u.status},
                    {"durationSeconds", u.durationSeconds},
                    {"error", u.error.empty() ? Json(nullptr) : Json(u.error)},
                    {"metadataSynced", u.metadataSynced}});
                return;
            }
        }
        fail(res, 404, "NOT_FOUND", "Video sin subida registrada");
    }
};
} // namespace

int main() {
    try {
        tecflix::auth::ensureCryptoReady();
        const std::string secret = requiredEnv("JWT_SECRET");
        const std::string token = requiredEnv("SERVICE_TOKEN");
        if (token.size() < 32) throw std::runtime_error("SERVICE_TOKEN requiere al menos 32 caracteres");
        const fs::path storage = envOr("STREAMING_DATA_DIR", "build-v143/streaming-data");
        StreamingApi app(secret, token, storage,
                         envOr("USERS_API_HOST", "127.0.0.1"), std::stoi(envOr("USERS_API_PORT", "8080")),
                         envOr("METADATA_API_HOST", "127.0.0.1"), std::stoi(envOr("METADATA_API_PORT", "8082")));
        httplib::Server server;
        server.set_payload_max_length(kMaxBodyBytes);
        app.configure(server);
        const int port = std::stoi(envOr("STREAMING_API_PORT", "8083"));
        if (port < 1 || port > 65535) throw std::runtime_error("Puerto invalido");
        std::cout << "streaming-api (E2: subida + FFmpeg + DASH) escuchando en " << port << '\n';
        if (!server.listen("127.0.0.1", port)) throw std::runtime_error("No se pudo abrir el puerto");
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "No se pudo iniciar streaming-api: " << ex.what() << '\n';
        return 1;
    }
}
