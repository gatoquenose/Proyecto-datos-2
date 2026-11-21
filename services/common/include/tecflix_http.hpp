#pragma once

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>

namespace tecflix {
namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;

inline constexpr const char* HEALTH_ROUTE = "/api/v1/health";
inline constexpr const char* READY_ROUTE = "/api/v1/ready";
inline constexpr const char* NOT_FOUND_CODE = "NOT_FOUND";
inline constexpr const char* NOT_FOUND_MESSAGE = "Recurso no encontrado.";

class ITraceIdGenerator {
 public:
  virtual ~ITraceIdGenerator() = default;
  virtual std::string generate() = 0;
};

class RandomTraceIdGenerator : public ITraceIdGenerator {
 public:
  RandomTraceIdGenerator() : rd_{}, engine_{rd_()} {}

  std::string generate() override {
    std::uniform_int_distribution<uint64_t> dist;
    uint64_t part1 = dist(engine_);
    uint64_t part2 = dist(engine_);

    std::ostringstream ss;
    ss << std::hex << std::setfill('0')
       << std::setw(16) << part1
       << std::setw(16) << part2;
    return ss.str();
  }

 private:
  std::random_device rd_;
  std::mt19937_64 engine_;
};

inline std::string make_error_body(const std::string& code, const std::string& message, const std::string& trace_id) {
  std::ostringstream ss;
  ss << R"({"code":")" << code
     << R"(","message":")" << message
     << R"(","details":null,"traceId":")" << trace_id
     << R"("})";
  return ss.str();
}

inline http::response<http::string_body> response_for(
    const http::request<http::string_body>& request,
    ITraceIdGenerator& generator) {
  http::response<http::string_body> response{http::status::not_found, request.version()};
  response.set(http::field::server, "tecflix-phase-a");
  response.set(http::field::content_type, "application/json");
  response.keep_alive(request.keep_alive());

  if (request.target() == HEALTH_ROUTE || request.target() == READY_ROUTE) {
    response.result(http::status::ok);
    response.body() = R"({"status":"ok"})";
  } else {
    response.body() = make_error_body(NOT_FOUND_CODE, NOT_FOUND_MESSAGE, generator.generate());
  }
  response.prepare_payload();
  return response;
}

inline int run_http_service(const char* service_name, unsigned short port = 8080) {
  try {
    asio::io_context context;
    tcp::acceptor acceptor{context, {tcp::v4(), port}};
    std::cout << service_name << " listening on " << port << '\n';
    RandomTraceIdGenerator trace_id_generator;
    for (;;) {
      try {
        tcp::socket socket{context};
        acceptor.accept(socket);
        beast::flat_buffer buffer;
        http::request<http::string_body> request;
        http::read(socket, buffer, request);
        auto response = response_for(request, trace_id_generator);
        http::write(socket, response);
        beast::error_code ignored;
        socket.shutdown(tcp::socket::shutdown_send, ignored);
      } catch (const std::exception& error) {
        std::cerr << service_name << " connection error: " << error.what() << '\n';
      }
    }
  } catch (const std::exception& error) {
    std::cerr << service_name << ": " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
}  // namespace tecflix
