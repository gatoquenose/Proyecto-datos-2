cmake_minimum_required(VERSION 3.24)

if(DEFINED ENV{VCPKG_ROOT} AND EXISTS "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      CACHE FILEPATH "vcpkg toolchain file" FORCE)
endif()

project(tecflix VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

enable_testing()

find_package(Boost CONFIG REQUIRED)

add_library(tecflix_common INTERFACE)
target_include_directories(tecflix_common INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/services/common/include)
target_link_libraries(tecflix_common INTERFACE Boost::headers)

function(add_tecflix_service name)
  add_executable(${name} services/${name}/src/main.cpp)
  target_link_libraries(${name} PRIVATE tecflix_common)
  if(MSVC)
    target_compile_options(${name} PRIVATE /W4 /permissive-)
  else()
    target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()

# users-api tiene su propio CMakeLists (biblioteca, ejecutable y pruebas).
add_subdirectory(services/users-api)

# Shared authentication is already registered by users-api in some branches.
# Register it here only if users-api did not create that target.
if(NOT TARGET tecflix_auth)
  add_subdirectory(services/common/auth)
endif()

# Temporal: metadata-api y streaming-api siguen usando el helper compartido
# hasta que sus responsables construyan su version.
add_tecflix_service(metadata-api)
add_tecflix_service(streaming-api)
find_package(bsoncxx CONFIG REQUIRED)
find_package(mongocxx CONFIG REQUIRED)

target_sources(metadata-api PRIVATE
    services/metadata-api/src/VideoRepository.cpp
)

# CAMBIO: en Linux (triplete estatico) existen los targets _static;
# en Windows (dinamico) existen los _shared. Esto funciona en ambos.
target_link_libraries(metadata-api PRIVATE
    $<IF:$<TARGET_EXISTS:mongo::bsoncxx_static>,mongo::bsoncxx_static,mongo::bsoncxx_shared>
    $<IF:$<TARGET_EXISTS:mongo::mongocxx_static>,mongo::mongocxx_static,mongo::mongocxx_shared>
)

target_link_libraries(metadata-api PRIVATE tecflix_auth tecflix_telemetry)

# Streaming API: JWT, HTTP and JSON
find_package(httplib CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(unofficial-sodium CONFIG REQUIRED)

if(NOT TARGET tecflix_telemetry)
    add_subdirectory(services/common/telemetry)
endif()

target_link_libraries(streaming-api PRIVATE
    tecflix_auth
    httplib::httplib
    nlohmann_json::nlohmann_json
    unofficial-sodium::sodium
    tecflix_telemetry
)
