#include "tecflix_http.hpp"
#include "tecflix_config.hpp"
#include <iostream>

int main() {
  try {
    tecflix::ProcessEnvironment env;
    auto config = tecflix::load_service_config(env);
    return tecflix::run_http_service("streaming-api", config.port);
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }
}
