#include <httplib.h>

#include <chrono>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

#include "api/HealthController.hpp"
#include "api/InternalQuotaController.hpp"
#include "api/ServerSetup.hpp"
#include "api/UsersController.hpp"
#include "auth/Argon2PasswordHasher.hpp"
#include "auth/UuidV4Generator.hpp"
#include "config/Config.hpp"
#include "config/ProcessEnvironment.hpp"
#include "service/QuotaService.hpp"
#include "service/ReservationSweeper.hpp"
#include "service/UserService.hpp"
#include "storage/BTree.hpp"
#include "storage/FilePageStore.hpp"
#include "storage/FileReservationRepository.hpp"
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
constexpr const char* kReservationsFileName = "reservations.dat";
constexpr const char* kReservationsIndexFileName = "reservations.idx";
constexpr const char* kReadinessProbeId = "00000000-0000-0000-0000-000000000000";

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

        FilePageStore reservationPages{config.dataDirectory / kReservationsIndexFileName};
        BTree reservationIndex{reservationPages};
        FileReservationRepository reservations{config.dataDirectory / kReservationsFileName, reservationIndex};
        QuotaService quota{repository, reservations, clock, config.reservationLifetime};
        quota.expireDueReservations();  // al arrancar se recuperan las reservas que vencieron mientras estaba apagado
        const ReservationSweeper sweeper{quota, std::chrono::duration_cast<std::chrono::milliseconds>(config.reservationSweepInterval)};

        httplib::Server server;
        configureServer(server, ids);
        // "Listo" = el archivo de usuarios responde (una búsqueda de prueba no falla).
        const HealthController health{[&repository] {
            static_cast<void>(repository.findById(kReadinessProbeId));
            return true;
        }};
        health.registerRoutes(server);
        const UsersController users{service, verifier, ids, config.jwtLifetime};
        users.registerRoutes(server);
        const InternalQuotaController internalQuota{quota, config.serviceToken, ids};
        internalQuota.registerRoutes(server);

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
