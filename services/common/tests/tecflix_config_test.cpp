#include <gtest/gtest.h>
#include "tecflix_config.hpp"
#include <map>
#include <optional>
#include <string>

namespace tecflix {

class FakeEnvironment : public IEnvironment {
 public:
  std::map<std::string, std::string> vars;

  std::optional<std::string> get(const std::string& name) const override {
    auto it = vars.find(name);
    if (it != vars.end()) {
      return it->second;
    }
    return std::nullopt;
  }
};

TEST(TecflixConfigTest, DefaultPortWhenVariableNotSet) {
  FakeEnvironment env;
  ServiceConfig config = load_service_config(env);
  EXPECT_EQ(config.port, DEFAULT_PORT);
}

TEST(TecflixConfigTest, ValidCustomPort9090) {
  FakeEnvironment env;
  env.vars[PORT_ENV_VAR] = "9090";
  ServiceConfig config = load_service_config(env);
  EXPECT_EQ(config.port, 9090);
}

TEST(TecflixConfigTest, ValidBoundaryPorts) {
  FakeEnvironment env;

  env.vars[PORT_ENV_VAR] = "1";
  EXPECT_EQ(load_service_config(env).port, 1);

  env.vars[PORT_ENV_VAR] = "65535";
  EXPECT_EQ(load_service_config(env).port, 65535);
}

TEST(TecflixConfigTest, InvalidPortsThrowException) {
  FakeEnvironment env;

  const std::string invalid_values[] = {
      "0", "65536", "-1", "abc", "80abc", " 80", ""
  };

  for (const auto& val : invalid_values) {
    env.vars[PORT_ENV_VAR] = val;
    EXPECT_THROW({
      load_service_config(env);
    }, std::invalid_argument);
  }
}

}  // namespace tecflix
