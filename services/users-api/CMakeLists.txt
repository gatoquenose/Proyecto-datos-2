find_package(httplib CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(GTest REQUIRED)
find_package(unofficial-sodium CONFIG REQUIRED)

# Tokens JWT, compartidos con las otras APIs (se agrega una sola vez).
if(NOT TARGET tecflix_auth)
  add_subdirectory(${CMAKE_SOURCE_DIR}/services/common/auth ${CMAKE_BINARY_DIR}/services/common/auth)
endif()

if(MSVC)
  set(TECFLIX_WARNINGS /W4 /permissive-)
else()
  set(TECFLIX_WARNINGS -Wall -Wextra -Wpedantic)
endif()

# Biblioteca con toda la lógica: la usan el ejecutable y las pruebas.
add_library(users_core STATIC
    src/config/Config.cpp
    src/config/ProcessEnvironment.cpp
    src/api/ApiResponses.cpp
    src/api/HealthController.cpp
    src/api/ServerSetup.cpp
    src/api/UsersController.cpp
    src/auth/Argon2PasswordHasher.cpp
    src/auth/UuidV4Generator.cpp
    src/service/UserService.cpp
    src/service/UserValidation.cpp
    src/storage/BinaryFile.cpp
    src/storage/BTree.cpp
    src/storage/BTreePage.cpp
    src/storage/FilePageStore.cpp
    src/storage/MemoryPageStore.cpp
    src/storage/UserFile.cpp
    src/storage/UserRecordCodec.cpp
    src/storage/FileUserRepository.cpp
    src/storage/LockedUserRepository.cpp
)
target_include_directories(users_core PUBLIC src)
target_link_libraries(users_core PUBLIC httplib::httplib nlohmann_json::nlohmann_json tecflix_auth unofficial-sodium::sodium)
target_compile_options(users_core PRIVATE ${TECFLIX_WARNINGS})

add_executable(users-api src/main.cpp)
target_link_libraries(users-api PRIVATE users_core)
target_compile_options(users-api PRIVATE ${TECFLIX_WARNINGS})

add_executable(users_tests
    tests/ConfigTest.cpp
    tests/HealthControllerTest.cpp
    tests/BTreePageTest.cpp
    tests/BTreeTest.cpp
    tests/UserRecordCodecTest.cpp
    tests/UserFileTest.cpp
    tests/FileUserRepositoryTest.cpp
    tests/AuthTest.cpp
    tests/UserValidationTest.cpp
    tests/UserServiceTest.cpp
    tests/LockedUserRepositoryTest.cpp
    tests/UsersControllerTest.cpp
)
target_link_libraries(users_tests PRIVATE users_core GTest::gtest GTest::gtest_main)
target_compile_options(users_tests PRIVATE ${TECFLIX_WARNINGS})

include(GoogleTest)
gtest_discover_tests(users_tests)
