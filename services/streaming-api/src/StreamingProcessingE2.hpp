#pragma once

// Tecflix E2: FFprobe + FFmpeg (argumentos separados, nunca comandos de shell).
// Cola y estado HTTP se gestionan en main.cpp.

#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <utility>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <process.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace tecflix::streaming {
namespace fs = std::filesystem;
using Json = nlohmann::json;

// _spawnvp en Windows concatena argv sin proteger espacios internos.
// Cada argumento debe codificarse siguiendo las reglas de Microsoft CRT.
inline int runProgram(const std::string& executable, const std::vector<std::string>& parameters) {
#ifdef _WIN32
    auto quoteForWindows = [](const std::string& value) {
        std::string quoted = "\"";
        std::size_t slashes = 0;
        for (char c : value) {
            if (c == '\\') {
                ++slashes;
            } else if (c == '"') {
                quoted.append(slashes * 2 + 1, '\\');
                quoted.push_back('"');
                slashes = 0;
            } else {
                quoted.append(slashes, '\\');
                quoted.push_back(c);
                slashes = 0;
            }
        }
        quoted.append(slashes * 2, '\\');
        quoted.push_back('"');
        return quoted;
    };
    std::vector<std::string> encoded;
    encoded.reserve(parameters.size() + 1);
    encoded.push_back(quoteForWindows(executable));
    for (const auto& p : parameters) encoded.push_back(quoteForWindows(p));
    std::vector<const char*> args;
    args.reserve(encoded.size() + 1);
    for (const auto& s : encoded) args.push_back(s.c_str());
    args.push_back(nullptr);
    const auto result = _spawnvp(_P_WAIT, executable.c_str(), args.data());
    return result < 0 ? -1 : static_cast<int>(result);
#else
    std::vector<const char*> args;
    args.reserve(parameters.size() + 2);
    args.push_back(executable.c_str());
    for (const auto& p : parameters) args.push_back(p.c_str());
    args.push_back(nullptr);
    const pid_t child = fork();
    if (child < 0) return -1;
    if (child == 0) {
        execvp(executable.c_str(), const_cast<char* const*>(args.data()));
        _exit(127);
    }
    int status = 0;
    if (waitpid(child, &status, 0) < 0 || !WIFEXITED(status)) return -1;
    return WEXITSTATUS(status);
#endif
}
struct ProcessResult {
    std::uint64_t finalBytes = 0;
    int durationSeconds = 0;
    int videoHeight = 0;
    int videoRenditions = 0;
    bool hasAudio = false;
};

inline std::uint64_t sumBytes(const fs::path& path) {
    std::uint64_t total = 0;
    if (!fs::exists(path)) return 0;
    if (fs::is_regular_file(path)) return static_cast<std::uint64_t>(fs::file_size(path));
    for (const auto& entry : fs::recursive_directory_iterator(path)) {
        if (entry.is_regular_file()) total += static_cast<std::uint64_t>(entry.file_size());
    }
    return total;
}

class FfmpegTranscoder {
public:
    FfmpegTranscoder(std::string ffmpeg, std::string ffprobe)
        : ffmpeg_(std::move(ffmpeg)), ffprobe_(std::move(ffprobe)) {}

    bool available() const {
        return runProgram(ffmpeg_, {"-version"}) == 0 && runProgram(ffprobe_, {"-version"}) == 0;
    }

    ProcessResult transcode(const fs::path& uploadPath) const {
        const fs::path absolute = fs::absolute(uploadPath);
        const fs::path original = absolute / "original.bin";
        const fs::path probeFile = absolute / "probe.json";
        const fs::path temporary = absolute / "dash.tmp";
        const fs::path dash = absolute / "dash";
        const fs::path thumbnailTmp = absolute / "thumbnail.tmp.jpg";
        const fs::path thumbnail = absolute / "thumbnail.jpg";
        if (!fs::is_regular_file(original)) throw std::runtime_error("No existe original.bin");
        fs::remove_all(temporary);
        fs::create_directories(temporary);

        // FFprobe guarda JSON directamente sin pasar por un shell ni parsear stdout.
        if (runProgram(ffprobe_, {"-v", "error", "-show_entries",
                "stream=index,codec_type,width,height:format=duration", "-of", "json",
                "-o", probeFile.string(), original.string()}) != 0) {
            throw std::runtime_error("FFprobe rechazo el archivo; no es un video valido");
        }
        std::ifstream in(probeFile, std::ios::binary);
        Json data = Json::parse(in, nullptr, false);
        in.close(); // Windows: cerrar antes de borrar probe.json.
        if (!data.is_object() || !data.contains("streams") || !data["streams"].is_array()) {
            throw std::runtime_error("FFprobe no devolvio pistas validas");
        }
        int height = 0;
        bool audio = false;
        for (const auto& stream : data["streams"]) {
            if (!stream.is_object()) continue;
            if (stream.value("codec_type", "") == "audio") audio = true;
            if (stream.value("codec_type", "") == "video" && height == 0) {
                height = stream.value("height", 0);
            }
        }
        if (height < 2) throw std::runtime_error("El archivo no contiene video utilizable");
        double duration = 0;
        try {
            const auto& value = data.at("format").at("duration");
            if (value.is_string()) duration = std::stod(value.get<std::string>());
            else if (value.is_number()) duration = value.get<double>();
        } catch (...) { /* Formatos sin duracion: rechazar mas abajo. */ }
        if (!std::isfinite(duration) || duration <= 0 || duration > 24.0 * 3600.0) {
            throw std::runtime_error("Duracion del video desconocida o fuera de rango");
        }

        // Solo generar alturas <= altura original. Si es inferior a 240p, usar su altura nativa.
        std::vector<int> heights;
        for (int h : {240, 360, 480, 720, 1080}) if (h <= height) heights.push_back(h);
        if (heights.empty()) heights.push_back(height);
        if (heights.size() > 5) throw std::runtime_error("Numero de rendiciones invalido");

        std::string graph = "[0:v:0]split=" + std::to_string(heights.size());
        for (std::size_t i = 0; i < heights.size(); ++i) graph += "[raw" + std::to_string(i) + "]";
        graph += ";";
        for (std::size_t i = 0; i < heights.size(); ++i) {
            if (i) graph += ";";
            graph += "[raw" + std::to_string(i) + "]scale=-2:" + std::to_string(heights[i]) +
                     "[v" + std::to_string(i) + "]";
        }
        std::vector<std::string> args = {
            "-hide_banner", "-nostdin", "-loglevel", "error", "-y", "-i", original.string(),
            "-filter_complex", graph
        };
        for (std::size_t i = 0; i < heights.size(); ++i) {
            args.push_back("-map"); args.push_back("[v" + std::to_string(i) + "]");
        }
        if (audio) { args.push_back("-map"); args.push_back("0:a:0"); }
        const std::vector<std::string> output = {
            "-c:v", "libx264", "-preset", "veryfast", "-crf", "24", "-pix_fmt", "yuv420p",
            "-sc_threshold", "0", "-force_key_frames", "expr:gte(t,n_forced*4)",
            "-c:a", "aac", "-b:a", "128k", "-seg_duration", "4",
            "-use_template", "1", "-use_timeline", "1", "-adaptation_sets",
            audio ? "id=0,streams=v id=1,streams=a" : "id=0,streams=v",
            "-init_seg_name", "init-stream$RepresentationID$.m4s",
            "-media_seg_name", "chunk-stream$RepresentationID$-$Number%05d$.m4s",
            "-f", "dash", (temporary / "manifest.mpd").generic_string()
        };
        args.insert(args.end(), output.begin(), output.end());
        if (runProgram(ffmpeg_, args) != 0) throw std::runtime_error("FFmpeg fallo al generar MPEG-DASH");
        // E2_VERIFICAR_SEGMENTOS_ANTES_DE_READY
        // El muxer DASH interpreta '/' al localizar la carpeta del manifiesto.
        // No basta con que exista manifest.mpd: verificar todos los tracks.
        for (int track = 0; track < static_cast<int>(heights.size()) + (audio ? 1 : 0); ++track) {
            const std::string trackId = std::to_string(track);
            const fs::path init = temporary / ("init-stream" + trackId + ".m4s");
            if (!fs::is_regular_file(init) || fs::file_size(init) == 0) {
                throw std::runtime_error("DASH sin inicializacion para pista " + trackId);
            }
            bool foundMedia = false;
            const std::string prefix = "chunk-stream" + trackId + "-";
            for (const auto& entry : fs::directory_iterator(temporary)) {
                if (entry.is_regular_file() && entry.path().extension() == ".m4s" &&
                    entry.path().filename().string().rfind(prefix, 0) == 0 &&
                    entry.file_size() > 0) {
                    foundMedia = true;
                    break;
                }
            }
            if (!foundMedia) {
                throw std::runtime_error("DASH sin fragmentos para pista " + trackId);
            }
        }
        const fs::path mpd = temporary / "manifest.mpd";
        if (!fs::is_regular_file(mpd)) throw std::runtime_error("FFmpeg no genero manifest.mpd");

        // La URL del MPD termina en /videos/{id}/manifest.mpd, mientras que los
        // fragmentos viven en /videos/{id}/dash/{archivo}. BaseURL corrige la resolucion.
        {
            std::ifstream source(mpd, std::ios::binary);
            const std::string content{std::istreambuf_iterator<char>(source), std::istreambuf_iterator<char>()};
            source.close(); // Evitar bloqueo de escritura del MPD en Windows.
            const auto at = content.find("<MPD ");
            const auto end = at == std::string::npos ? std::string::npos : content.find('>', at);
            if (end == std::string::npos) throw std::runtime_error("MPD sin elemento raiz valido");
            // DASH requiere ubicar BaseURL despues de ProgramInformation, si aparece.
            const auto infoEnd = content.find("</ProgramInformation>", end);
            const auto insert = infoEnd == std::string::npos ? end + 1 : infoEnd + std::string("</ProgramInformation>").size();
            const std::string fixed = content.substr(0, insert) + "\n  <BaseURL>dash/</BaseURL>" + content.substr(insert);
            std::ofstream dest(mpd, std::ios::binary | std::ios::trunc);
            dest << fixed;
            if (!dest.good()) throw std::runtime_error("No se pudo corregir BaseURL del MPD");
        }

        // Miniatura: primer frame (no falla por videos de menos de un segundo).
        if (runProgram(ffmpeg_, {"-hide_banner", "-nostdin", "-loglevel", "error", "-y",
              "-i", original.string(), "-frames:v", "1", "-vf", "scale=320:-2",
              thumbnailTmp.string()}) != 0 || !fs::is_regular_file(thumbnailTmp)) {
            throw std::runtime_error("FFmpeg no genero miniatura");
        }
        fs::remove_all(dash);
        fs::rename(temporary, dash);
        fs::remove(thumbnail);
        fs::rename(thumbnailTmp, thumbnail);
        fs::remove(probeFile);
        const std::uint64_t bytes = sumBytes(dash) + sumBytes(original) + sumBytes(thumbnail);
        return ProcessResult{bytes, static_cast<int>(std::ceil(duration)), height, static_cast<int>(heights.size()), audio};
    }
private:
    std::string ffmpeg_, ffprobe_;
};

} // namespace tecflix::streaming
