#pragma once

#include <charconv>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>

namespace tecflix {

inline constexpr const char* PORT_ENV_VAR = "TECFLIX_PORT";
inline constexpr unsigned short DEFAULT_PORT = 8080;
inline constexpr unsigned short MIN_PORT = 1;
inline constexpr unsigned short MAX_PORT = 65535;

class IEnvironment {
 public:
  virtual ~IEnvironment() = default;
  virtual std::optional<std::string> get(const std::string& name) const = 0;
};

class ProcessEnvironment : public IEnvironment {
 public:
  std::optional<std::string> get(const std::string& name) const override {
    const char* value = std::getenv(name.c_str());
    if (value == nullptr) {
      return std::nullopt;
    }
    return std::string(value);
  }
};

struct ServiceConfig {
  unsigned short port;
};

inline ServiceConfig load_service_config(const IEnvironment& env) {
  ServiceConfig config{DEFAULT_PORT};
  auto port_str_opt = env.get(PORT_ENV_VAR);

  if (port_str_opt) {
    const std::string& port_str = *port_str_opt;
    if (port_str.empty()) {
      throw std::invalid_argument(std::string("La variable ") + PORT_ENV_VAR + " no puede estar vacía.");
    }

    unsigned int value = 0;
    auto [ptr, ec] = std::from_chars(port_str.data(), port_str.data() + port_str.size(), value);

    if (ec != std::errc() || ptr != port_str.data() + port_str.size()) {
      throw std::invalid_argument(std::string("La variable ") + PORT_ENV_VAR + " tiene un formato inválido: " + port_str);
    }

    if (value < MIN_PORT || value > MAX_PORT) {
      throw std::invalid_argument(std::string("La variable ") + PORT_ENV_VAR + " está fuera del rango permitido: " + port_str);
    }

    config.port = static_cast<unsigned short>(value);
  }

  return config;
}

}  // namespace tecflix
