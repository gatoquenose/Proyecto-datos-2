#pragma once

#include "tecflix_config.hpp"
#include "tecflix_http.hpp"
#include <iostream>

namespace tecflix {

inline int run_service(const char* service_name) {
  try {
    tecflix::ProcessEnvironment env;
    auto config = tecflix::load_service_config(env);
    return tecflix::run_http_service(service_name, config.port);
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return EXIT_FAILURE;
  }
}

}  // namespace tecflix
