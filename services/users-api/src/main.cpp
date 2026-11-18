#include <httplib.h>

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

#include "api/HealthController.hpp"
#include "api/ServerSetup.hpp"
#include "api/UsersController.hpp"
#include "auth/Argon2PasswordHasher.hpp"
#include "auth/UuidV4Generator.hpp"
#include "config/Config.hpp"
#include "config/ProcessEnvironment.hpp"
#include "service/UserService.hpp"
#include "storage/BTree.hpp"
#include "storage/FilePageStore.hpp"
#include "storage/FileUserRepository.hpp"
#include "storage/LockedUserRepository.hpp"
#include "storage/UserFile.hpp"
#include "tecflix/auth/JwtSigner.hpp"
#include "tecflix/auth/JwtVerifier.hpp"
#include "tecflix/auth/SystemClock.hpp"

namespace {

constexpr const char* kBindAddress = "0.0.0.0";
constexpr const char* kUsersFileName = "users.dat";
constexpr const char* kIndexFileName = "users.idx";

// Log estructurado en JSON, una línea por evento.
void logEvent(std::ostream& stream, const std::string& level, const std::string& message) {
    stream << nlohmann::json{{"level", level}, {"service", "users-api"}, {"message", message}}.dump() << '\n' << std::flush;
}

}  // namespace

// Aquí se arma todo el servicio: cada pieza recibe lo que necesita por el constructor.
int main() {
    using namespace tecflix::users;
    try {
        const ProcessEnvironment environment;
        const Config config = Config::fromEnvironment(environment);
        std::filesystem::create_directories(config.dataDirectory);

        const tecflix::auth::SystemClock clock;
        const UuidV4Generator ids;
        const Argon2PasswordHasher hasher{Argon2Params::interactive()};

        UserFile usersFile{config.dataDirectory / kUsersFileName};
        FilePageStore indexPages{config.dataDirectory / kIndexFileName};
        BTree index{indexPages};
        FileUserRepository fileRepository{usersFile, index};
        LockedUserRepository repository{fileRepository};

        const tecflix::auth::JwtSigner signer{config.jwtSecret, clock, config.jwtLifetime};
        const tecflix::auth::JwtVerifier verifier{config.jwtSecret, clock};
        UserService service{repository, hasher, ids, signer, config.defaultQuotaBytes};

        httplib::Server server;
        configureServer(server, ids);
        const HealthController health;
        health.registerRoutes(server);
        const UsersController users{service, verifier, ids, config.jwtLifetime};
        users.registerRoutes(server);

        logEvent(std::cout, "info", "users-api escuchando en el puerto " + std::to_string(config.port));
        if (!server.listen(kBindAddress, config.port)) {
            logEvent(std::cerr, "error", "no se pudo abrir el puerto " + std::to_string(config.port));
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        logEvent(std::cerr, "error", error.what());
        return EXIT_FAILURE;
    }
}
