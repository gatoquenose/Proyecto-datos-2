find_package(opentelemetry-cpp CONFIG REQUIRED)
find_package(GTest REQUIRED)

if(MSVC)
  set(TECFLIX_WARNINGS /W4 /permissive-)
else()
  set(TECFLIX_WARNINGS -Wall -Wextra -Wpedantic)
endif()

# Medición común a las tres APIs: trazas, métricas HTTP y medidores de proceso.
# Solo usa la API de OpenTelemetry fuera de TelemetrySetup, que es la única pieza que conoce el SDK.
add_library(tecflix_telemetry STATIC
    src/GaugeRegistry.cpp
    src/OtelHttpServerMetrics.cpp
    src/ProcessGauges.cpp
    src/ProcessSampler.cpp
    src/ServerSpan.cpp
    src/TelemetrySetup.cpp
)
target_include_directories(tecflix_telemetry PUBLIC include ${OPENTELEMETRY_CPP_INCLUDE_DIRS})
target_link_libraries(tecflix_telemetry PUBLIC ${OPENTELEMETRY_CPP_LIBRARIES})
target_compile_options(tecflix_telemetry PRIVATE ${TECFLIX_WARNINGS})
if(WIN32)
  target_link_libraries(tecflix_telemetry PRIVATE psapi)
  # Los encabezados de Windows (los trae OpenTelemetry) definen macros min y max que rompen std::min y std::max.
  target_compile_definitions(tecflix_telemetry PUBLIC NOMINMAX)
endif()

add_executable(tecflix_telemetry_tests
    tests/ProcessSamplerTest.cpp
    tests/PropagationTest.cpp
)
target_link_libraries(tecflix_telemetry_tests PRIVATE tecflix_telemetry GTest::gtest GTest::gtest_main)
target_compile_options(tecflix_telemetry_tests PRIVATE ${TECFLIX_WARNINGS})

include(GoogleTest)
gtest_discover_tests(tecflix_telemetry_tests)
