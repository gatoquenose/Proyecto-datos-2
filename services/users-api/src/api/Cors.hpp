#pragma once
#include <string>
#include <httplib.h>

namespace tecflix::users {
void configureCors(httplib::Server& server, const std::string& allowedOrigin);
}