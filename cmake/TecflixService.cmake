function(tecflix_add_service name)
  file(GLOB_RECURSE service_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
  )
  list(FILTER service_sources EXCLUDE REGEX "main\\.cpp$")

  if(service_sources)
    add_library(${name}_lib STATIC ${service_sources})
    target_include_directories(${name}_lib PUBLIC
      "${CMAKE_CURRENT_SOURCE_DIR}/include"
      "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(${name}_lib PUBLIC tecflix_common)
  else()
    add_library(${name}_lib INTERFACE)
    target_include_directories(${name}_lib INTERFACE
      "${CMAKE_CURRENT_SOURCE_DIR}/include"
      "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(${name}_lib INTERFACE tecflix_common)
  endif()

  add_executable(${name} "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
  target_link_libraries(${name} PRIVATE ${name}_lib)
  if(MSVC)
    target_compile_options(${name} PRIVATE /W4 /permissive-)
    if(TARGET ${name}_lib AND NOT "${service_sources}" STREQUAL "")
      target_compile_options(${name}_lib PRIVATE /W4 /permissive-)
    endif()
  else()
    target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic)
    if(TARGET ${name}_lib AND NOT "${service_sources}" STREQUAL "")
      target_compile_options(${name}_lib PRIVATE -Wall -Wextra -Wpedantic)
    endif()
  endif()

  if(BUILD_TESTING)
    file(GLOB_RECURSE service_tests CONFIGURE_DEPENDS
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/*_test.cpp"
    )
    if(service_tests)
      add_executable(${name}_tests ${service_tests})
      target_link_libraries(${name}_tests PRIVATE ${name}_lib GTest::gtest_main)
      if(MSVC)
        target_compile_options(${name}_tests PRIVATE /W4 /permissive-)
      else()
        target_compile_options(${name}_tests PRIVATE -Wall -Wextra -Wpedantic)
      endif()
      gtest_discover_tests(${name}_tests)
    endif()
  endif()
endfunction()
