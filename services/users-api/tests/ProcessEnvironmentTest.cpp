#include <gtest/gtest.h>

#include <cstdlib>

#include "config/ProcessEnvironment.hpp"

namespace tecflix::users {

namespace {

void setVariable(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

}  // namespace

TEST(ProcessEnvironmentTest, ReadsAnExistingVariable) {
    setVariable("TECFLIX_PRUEBA_ENTORNO", "valor-de-prueba");

    const auto value = ProcessEnvironment{}.get("TECFLIX_PRUEBA_ENTORNO");

    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, "valor-de-prueba");
}

TEST(ProcessEnvironmentTest, MissingVariableIsEmpty) {
    EXPECT_FALSE(ProcessEnvironment{}.get("TECFLIX_VARIABLE_QUE_NO_EXISTE").has_value());
}

}  // namespace tecflix::users
