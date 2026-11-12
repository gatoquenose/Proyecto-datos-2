find_package(httplib CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(GTest REQUIRED)

if(MSVC)
  set(TECFLIX_WARNINGS /W4 /permissive-)
else()
  set(TECFLIX_WARNINGS -Wall -Wextra -Wpedantic)
endif()

# Biblioteca con toda la lógica: la usan el ejecutable y las pruebas.
add_library(users_core STATIC
    src/config/Config.cpp
    src/config/ProcessEnvironment.cpp
    src/api/HealthController.cpp
    src/storage/BTree.cpp
    src/storage/BTreePage.cpp
    src/storage/FilePageStore.cpp
    src/storage/MemoryPageStore.cpp
)
target_include_directories(users_core PUBLIC src)
target_link_libraries(users_core PUBLIC httplib::httplib nlohmann_json::nlohmann_json)
target_compile_options(users_core PRIVATE ${TECFLIX_WARNINGS})

add_executable(users-api src/main.cpp)
target_link_libraries(users-api PRIVATE users_core)
target_compile_options(users-api PRIVATE ${TECFLIX_WARNINGS})

add_executable(users_tests
    tests/ConfigTest.cpp
    tests/HealthControllerTest.cpp
    tests/BTreePageTest.cpp
    tests/BTreeTest.cpp
)
target_link_libraries(users_tests PRIVATE users_core GTest::gtest GTest::gtest_main)
target_compile_options(users_tests PRIVATE ${TECFLIX_WARNINGS})

include(GoogleTest)
gtest_discover_tests(users_tests)
