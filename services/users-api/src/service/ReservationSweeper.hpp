#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "service/QuotaService.hpp"

namespace tecflix::users {

// Hilo que cada cierto tiempo vence las reservas abandonadas (subidas que nunca terminaron),
// para que su espacio vuelva a la cuota del usuario. Se detiene solo al destruirse.
class ReservationSweeper {
public:
    ReservationSweeper(QuotaService& quota, std::chrono::milliseconds interval);

    ReservationSweeper(const ReservationSweeper&) = delete;
    ReservationSweeper& operator=(const ReservationSweeper&) = delete;
    ReservationSweeper(ReservationSweeper&&) = delete;
    ReservationSweeper& operator=(ReservationSweeper&&) = delete;
    ~ReservationSweeper() = default;  // std::jthread pide detenerse y espera al salir

private:
    void run(std::stop_token stop);

    QuotaService& quota_;
    std::chrono::milliseconds interval_;
    std::mutex mutex_;
    std::condition_variable_any wakeUp_;
    std::jthread thread_;  // último miembro: arranca cuando todo lo demás ya existe
};

}  // namespace tecflix::users
