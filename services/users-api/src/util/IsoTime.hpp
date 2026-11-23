#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace tecflix::users {

// "2026-11-12T10:30:00Z" <-> segundos desde 1970. Solo se acepta ese formato exacto (UTC con "Z").
[[nodiscard]] std::string formatIsoUtc(std::int64_t epochSeconds);
[[nodiscard]] std::optional<std::int64_t> parseIsoUtc(std::string_view text);

}  // namespace tecflix::users
