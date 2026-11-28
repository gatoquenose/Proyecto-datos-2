#include "api/Cors.hpp"

#include "api/HttpStatus.hpp"

namespace tecflix::users {

void configureCors(httplib::Server& server, const std::string& allowedOrigin) {
    server.set_pre_routing_handler(
        [allowedOrigin](const httplib::Request& req, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", allowedOrigin);
            res.set_header("Vary", "Origin");
            res.set_header("Access-Control-Expose-Headers", "Content-Range, Accept-Ranges");
            if (req.method == "OPTIONS") {
                res.set_header("Access-Control-Allow-Methods", "GET, POST, PATCH, DELETE, OPTIONS");
                res.set_header("Access-Control-Allow-Headers", "Authorization, Content-Type, traceparent, tracestate");
                res.set_header("Access-Control-Max-Age", "600");
                res.status = http_status::kNoContent;
                return httplib::Server::HandlerResponse::Handled;
            }
            return httplib::Server::HandlerResponse::Unhandled;
        });
}

}  // namespace tecflix::users