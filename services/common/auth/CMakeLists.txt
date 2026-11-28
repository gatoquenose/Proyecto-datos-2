find_package(unofficial-sodium CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(GTest REQUIRED)

if(MSVC)
  set(TECFLIX_WARNINGS /W4 /permissive-)
else()
  set(TECFLIX_WARNINGS -Wall -Wextra -Wpedantic)
endif()

# Tokens JWT (HS256) compartidos por las tres APIs: users-api los emite y las tres los verifican.
add_library(tecflix_auth STATIC
    src/Base64Url.cpp
    src/CryptoRuntime.cpp
    src/JwtCrypto.cpp
    src/JwtSigner.cpp
    src/JwtVerifier.cpp
    src/SystemClock.cpp
)
target_include_directories(tecflix_auth PUBLIC include)
target_link_libraries(tecflix_auth PUBLIC nlohmann_json::nlohmann_json unofficial-sodium::sodium)
target_compile_options(tecflix_auth PRIVATE ${TECFLIX_WARNINGS})

add_executable(tecflix_auth_tests
    tests/Base64UrlTest.cpp
    tests/JwtTest.cpp
    tests/SystemClockTest.cpp
)
target_include_directories(tecflix_auth_tests PRIVATE src)  # para probar JwtCrypto.hpp (interno)
target_link_libraries(tecflix_auth_tests PRIVATE tecflix_auth GTest::gtest GTest::gtest_main)
target_compile_options(tecflix_auth_tests PRIVATE ${TECFLIX_WARNINGS})

include(GoogleTest)
gtest_discover_tests(tecflix_auth_tests)
