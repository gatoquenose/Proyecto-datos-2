#pragma once

#include <string_view>

namespace tecflix::users {

// Lo que la API de usuarios mide, expresado sin depender de OpenTelemetry.
class IUsersMetrics {
public:
    IUsersMetrics() = default;
    IUsersMetrics(const IUsersMetrics&) = delete;
    IUsersMetrics& operator=(const IUsersMetrics&) = delete;
    IUsersMetrics(IUsersMetrics&&) = delete;
    IUsersMetrics& operator=(IUsersMetrics&&) = delete;
    virtual ~IUsersMetrics() = default;

    virtual void pageRead() = 0;
    virtual void pageWritten() = 0;
    // 'lookup' dice por qué se buscó: "id", "username" o "email".
    virtual void searchFinished(std::string_view lookup, double durationSeconds) = 0;
    virtual void userCreated() = 0;
    virtual void userDeleted() = 0;
};

// No mide nada (telemetría desactivada y pruebas).
class NullUsersMetrics final : public IUsersMetrics {
public:
    void pageRead() override {}
    void pageWritten() override {}
    void searchFinished(std::string_view, double) override {}
    void userCreated() override {}
    void userDeleted() override {}
};

}  // namespace tecflix::users
