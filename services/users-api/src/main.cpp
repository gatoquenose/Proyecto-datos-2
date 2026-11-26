#include <httplib.h>

#include <chrono>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

#include "api/HealthController.hpp"
#include "api/InternalQuotaController.hpp"
#include "api/ServerSetup.hpp"
#include "api/TracedRouter.hpp"
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
#include "tecflix/telemetry/GaugeRegistry.hpp"
#include "tecflix/telemetry/OtelHttpServerMetrics.hpp"
#include "tecflix/telemetry/ProcessGauges.hpp"
#include "tecflix/telemetry/TelemetrySetup.hpp"
#include "telemetry/ActivityTracker.hpp"
#include "telemetry/InstrumentedPageStore.hpp"
#include "telemetry/InstrumentedUserRepository.hpp"
#include "telemetry/OtelUsersMetrics.hpp"
#include "telemetry/TraceAwareIdGenerator.hpp"

namespace {

constexpr const char* kBindAddress = "0.0.0.0";
constexpr const char* kServiceVersion = "1.0.0";
constexpr const char* kUsersFileName = "users.dat";
constexpr const char* kIndexFileName = "users.idx";
constexpr const char* kReservationsFileName = "reservations.dat";
constexpr const char* kReservationsIndexFileName = "reservations.idx";
constexpr const char* kReadinessProbeId = "00000000-0000-0000-0000-000000000000";

// Log estructurado en JSON, una línea por evento.
void logEvent(std::ostream& stream, const std::string& level, const std::string& message) {
    stream << nlohmann::json{{"level", level}, {"service", "users-api"}, {"message", message}}.dump() << '\n' << std::flush;
}

// Medidores que se consultan periódicamente: los comunes (CPU, memoria, disco), el almacenamiento
// total de los usuarios y los usuarios activos.
struct Gauges {
    explicit Gauges(const opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter>& meter) : registry(meter) {}

    tecflix::telemetry::GaugeRegistry registry;
    std::unique_ptr<tecflix::telemetry::ProcessSampler> process;
};

std::unique_ptr<Gauges> registerGauges(const opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter>& meter,
                                       const std::filesystem::path& dataDirectory,
                                       const tecflix::users::IUserRepository& users,
                                       tecflix::users::ActivityTracker& activity) {
    auto gauges = std::make_unique<Gauges>(meter);
    gauges->process = tecflix::telemetry::registerProcessGauges(gauges->registry, dataDirectory);
    gauges->registry.addInteger("storage_used_bytes", "Bytes de cuota usados por todos los usuarios", "By",
                                [&users] { return static_cast<std::int64_t>(users.usageTotals().usedBytes); });
    gauges->registry.addInteger("storage_quota_bytes", "Bytes de cuota asignados a todos los usuarios", "By",
                                [&users] { return static_cast<std::int64_t>(users.usageTotals().quotaBytes); });
    gauges->registry.addInteger("active_users", "Usuarios con actividad en los últimos 15 minutos", "{user}",
                                [&activity] { return static_cast<std::int64_t>(activity.activeCount()); });
    return gauges;
}

}  // namespace

// Aquí se arma todo el servicio: cada pieza recibe lo que necesita por el constructor.
int main() {
    using namespace tecflix::users;
    try {
        const ProcessEnvironment environment;
        const Config config = Config::fromEnvironment(environment);
        std::filesystem::create_directories(config.dataDirectory);

        // La telemetría se enciende primero y se apaga al final (envía lo que quede pendiente).
        const tecflix::telemetry::TelemetrySetup telemetry{{config.serviceName, kServiceVersion, config.otlpEndpoint,
                                                            config.metricExportInterval,
                                                            {"http_request_duration_seconds",
                                                             "users_btree_search_duration_seconds"}}};
        const auto meter = telemetry.meter();
        tecflix::telemetry::OtelHttpServerMetrics httpMetrics{meter};
        OtelUsersMetrics usersMetrics{meter};

        const tecflix::auth::SystemClock clock;
        const UuidV4Generator uuids;
        const TraceAwareIdGenerator traceIds{uuids};  // para los errores: el id de la traza activa
        const Argon2PasswordHasher hasher{Argon2Params::interactive()};

        UserFile usersFile{config.dataDirectory / kUsersFileName};
        FilePageStore userPages{config.dataDirectory / kIndexFileName};
        InstrumentedPageStore measuredUserPages{userPages, usersMetrics};
        BTree index{measuredUserPages};
        FileUserRepository fileRepository{usersFile, index};
        LockedUserRepository lockedRepository{fileRepository};
        InstrumentedUserRepository repository{lockedRepository, usersMetrics};

        const tecflix::auth::JwtSigner signer{config.jwtSecret, clock, config.jwtLifetime};
        const tecflix::auth::JwtVerifier verifier{config.jwtSecret, clock};
        UserService service{repository, hasher, uuids, signer, config.defaultQuotaBytes};

        FilePageStore reservationPages{config.dataDirectory / kReservationsIndexFileName};
        BTree reservationIndex{reservationPages};
        FileReservationRepository reservations{config.dataDirectory / kReservationsFileName, reservationIndex};
        QuotaService quota{repository, reservations, clock, config.reservationLifetime};
        quota.expireDueReservations();  // al arrancar se recuperan las reservas que vencieron mientras estaba apagado
        const ReservationSweeper sweeper{quota, std::chrono::duration_cast<std::chrono::milliseconds>(config.reservationSweepInterval)};

        ActivityTracker activity{clock};
        const auto gauges = registerGauges(meter, config.dataDirectory, repository, activity);

        httplib::Server server;
        configureServer(server, traceIds);
        // "Listo" = el archivo de usuarios responde (una búsqueda de prueba no falla).
        const HealthController health{[&repository] {
            static_cast<void>(repository.findById(kReadinessProbeId));
            return true;
        }};
        health.registerRoutes(server);
        TracedRouter router{server, httpMetrics, traceIds, config.serviceName};
        const UsersController users{service, verifier, traceIds, activity, config.jwtLifetime};
        users.registerRoutes(router);
        const InternalQuotaController internalQuota{quota, config.serviceToken, traceIds};
        internalQuota.registerRoutes(router);

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
