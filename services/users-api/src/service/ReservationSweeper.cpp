#include "service/ReservationSweeper.hpp"

#include <exception>
#include <iostream>
#include <nlohmann/json.hpp>

namespace tecflix::users {

ReservationSweeper::ReservationSweeper(QuotaService& quota, std::chrono::milliseconds interval)
    : quota_(quota), interval_(interval), thread_([this](std::stop_token stop) { run(stop); }) {}

void ReservationSweeper::run(std::stop_token stop) {
    std::unique_lock lock{mutex_};
    while (!stop.stop_requested()) {
        // Duerme hasta que pase el intervalo o se pida detener el hilo.
        wakeUp_.wait_for(lock, stop, interval_, [&stop] { return stop.stop_requested(); });
        if (stop.stop_requested()) {
            break;
        }
        try {
            quota_.expireDueReservations();
        } catch (const std::exception& error) {
            std::cerr << nlohmann::json{{"level", "error"}, {"service", "users-api"}, {"message", error.what()}}.dump()
                      << '\n' << std::flush;
        }
    }
}

}  // namespace tecflix::users
