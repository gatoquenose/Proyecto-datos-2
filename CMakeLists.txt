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

find_package(Boost CONFIG REQUIRED COMPONENTS system)

add_library(tecflix_common INTERFACE)
target_include_directories(tecflix_common INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/services/common/include)
target_link_libraries(tecflix_common INTERFACE Boost::system)

function(add_tecflix_service name)
  add_executable(${name} services/${name}/src/main.cpp)
  target_link_libraries(${name} PRIVATE tecflix_common)
  if(MSVC)
    target_compile_options(${name} PRIVATE /W4 /permissive-)
  else()
    target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()

add_tecflix_service(users-api)
add_tecflix_service(metadata-api)
add_tecflix_service(streaming-api)
