#pragma once

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <cstdlib>
#include <iostream>
#include <string>

namespace tecflix {
namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;

inline http::response<http::string_body> response_for(const http::request<http::string_body>& request) {
  http::response<http::string_body> response{http::status::not_found, request.version()};
  response.set(http::field::server, "tecflix-phase-a");
  response.set(http::field::content_type, "application/json");
  response.keep_alive(request.keep_alive());
  if (request.target() == "/health" || request.target() == "/ready") {
    response.result(http::status::ok);
    response.body() = R"({"status":"ok"})";
  } else {
    response.body() = R"({"error":"not found"})";
  }
  response.prepare_payload();
  return response;
}

inline int run_http_service(const char* service_name, unsigned short port = 8080) {
  try {
    asio::io_context context;
    tcp::acceptor acceptor{context, {tcp::v4(), port}};
    std::cout << service_name << " listening on " << port << '\n';
    for (;;) {
      tcp::socket socket{context};
      acceptor.accept(socket);
      beast::flat_buffer buffer;
      http::request<http::string_body> request;
      http::read(socket, buffer, request);
      auto response = response_for(request);
      http::write(socket, response);
      beast::error_code ignored;
      socket.shutdown(tcp::socket::shutdown_send, ignored);
    }
  } catch (const std::exception& error) {
    std::cerr << service_name << ": " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
}  // namespace tecflix
