#include "util/IsoTime.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <format>

namespace tecflix::users {

namespace {

using std::chrono::days;
using std::chrono::seconds;
using std::chrono::sys_days;

constexpr std::size_t kLength = 20;  // YYYY-MM-DDTHH:MM:SSZ
constexpr std::int64_t kSecondsPerMinute = 60;
constexpr std::int64_t kSecondsPerHour = 3600;
constexpr int kHoursPerDay = 24;
constexpr int kMinutesPerHour = 60;
constexpr int kSecondsPerMinuteInt = 60;

struct Separator {
    std::size_t position;
    char character;
};
constexpr std::array<Separator, 6> kSeparators{
    {{4, '-'}, {7, '-'}, {10, 'T'}, {13, ':'}, {16, ':'}, {19, 'Z'}}};

// Lee 'count' dígitos desde 'start'; devuelve -1 si hay algo que no es un dígito.
int digits(std::string_view text, std::size_t start, std::size_t count) {
    int value = 0;
    for (std::size_t i = start; i < start + count; ++i) {
        if (text[i] < '0' || text[i] > '9') {
            return -1;
        }
        value = value * 10 + (text[i] - '0');
    }
    return value;
}

bool hasValidShape(std::string_view text) {
    if (text.size() != kLength) {
        return false;
    }
    return std::ranges::all_of(kSeparators, [text](const Separator& s) { return text[s.position] == s.character; });
}

}  // namespace

std::string formatIsoUtc(std::int64_t epochSeconds) {
    const std::chrono::sys_seconds moment{seconds{epochSeconds}};
    const auto day = std::chrono::floor<days>(moment);
    const std::chrono::year_month_day date{day};
    const std::chrono::hh_mm_ss time{moment - day};
    return std::format("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}Z", static_cast<int>(date.year()),
                       static_cast<unsigned>(date.month()), static_cast<unsigned>(date.day()), time.hours().count(),
                       time.minutes().count(), time.seconds().count());
}

std::optional<std::int64_t> parseIsoUtc(std::string_view text) {
    if (!hasValidShape(text)) {
        return std::nullopt;
    }
    const int year = digits(text, 0, 4);
    const int month = digits(text, 5, 2);
    const int day = digits(text, 8, 2);
    const int hour = digits(text, 11, 2);
    const int minute = digits(text, 14, 2);
    const int second = digits(text, 17, 2);
    const bool digitsOk = std::ranges::all_of(std::array{year, month, day, hour, minute, second},
                                              [](int value) { return value >= 0; });
    if (!digitsOk || hour >= kHoursPerDay || minute >= kMinutesPerHour || second >= kSecondsPerMinuteInt) {
        return std::nullopt;
    }

    const std::chrono::year_month_day date{std::chrono::year{year}, std::chrono::month{static_cast<unsigned>(month)},
                                           std::chrono::day{static_cast<unsigned>(day)}};
    if (!date.ok()) {
        return std::nullopt;
    }
    const std::int64_t daySeconds = std::chrono::duration_cast<seconds>(sys_days{date}.time_since_epoch()).count();
    return daySeconds + hour * kSecondsPerHour + minute * kSecondsPerMinute + second;
}

}  // namespace tecflix::users
