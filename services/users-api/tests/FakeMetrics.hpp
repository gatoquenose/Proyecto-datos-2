#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "telemetry/IUsersMetrics.hpp"
#include "tecflix/telemetry/IHttpServerMetrics.hpp"

namespace tecflix::users::testing {

// Mediciones falsas: guardan lo que se les informa para que las pruebas lo revisen.
class FakeUsersMetrics final : public IUsersMetrics {
public:
    void pageRead() override { ++pagesRead; }
    void pageWritten() override { ++pagesWritten; }
    void searchFinished(std::string_view lookup, double durationSeconds) override {
        searches.emplace_back(lookup);
        if (durationSeconds < 0.0) {
            negativeDurations = true;
        }
    }
    void userCreated() override { ++created; }
    void userDeleted() override { ++deleted; }

    int pagesRead = 0;
    int pagesWritten = 0;
    int created = 0;
    int deleted = 0;
    bool negativeDurations = false;
    std::vector<std::string> searches;
};

struct HttpRecord {
    std::string method;
    std::string route;
    int status;
    double seconds;
};

class FakeHttpMetrics final : public tecflix::telemetry::IHttpServerMetrics {
public:
    void record(std::string_view method, std::string_view route, int status, double durationSeconds) override {
        records.push_back({std::string{method}, std::string{route}, status, durationSeconds});
    }

    std::vector<HttpRecord> records;
};

}  // namespace tecflix::users::testing
