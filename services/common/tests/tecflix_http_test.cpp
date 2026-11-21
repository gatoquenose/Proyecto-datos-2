#include <gtest/gtest.h>
#include "tecflix_http.hpp"
#include <set>
#include <string>

namespace tecflix {

class FakeTraceIdGenerator : public ITraceIdGenerator {
 public:
  std::string generate() override { return "trace-fijo"; }
};

http::request<http::string_body> make_request(http::verb verb, const std::string& target) {
  http::request<http::string_body> req{verb, target, 11};
  return req;
}

TEST(TecflixHttpTest, GetHealthReturnsOk) {
  FakeTraceIdGenerator gen;
  auto req = make_request(http::verb::get, HEALTH_ROUTE);
  auto res = response_for(req, gen);
  EXPECT_EQ(res.result(), http::status::ok);
  EXPECT_EQ(res.body(), R"({"status":"ok"})");
  EXPECT_EQ(res[http::field::content_type], "application/json");
}

TEST(TecflixHttpTest, GetReadyReturnsOk) {
  FakeTraceIdGenerator gen;
  auto req = make_request(http::verb::get, READY_ROUTE);
  auto res = response_for(req, gen);
  EXPECT_EQ(res.result(), http::status::ok);
  EXPECT_EQ(res.body(), R"({"status":"ok"})");
}

TEST(TecflixHttpTest, GetHealthMissingPrefixReturnsNotFound) {
  FakeTraceIdGenerator gen;
  auto req = make_request(http::verb::get, "/health");
  auto res = response_for(req, gen);
  EXPECT_EQ(res.result(), http::status::not_found);
  EXPECT_EQ(res.body(), R"({"code":"NOT_FOUND","message":"Recurso no encontrado.","details":null,"traceId":"trace-fijo"})");
}

TEST(TecflixHttpTest, GetReadyMissingPrefixReturnsNotFound) {
  FakeTraceIdGenerator gen;
  auto req = make_request(http::verb::get, "/ready");
  auto res = response_for(req, gen);
  EXPECT_EQ(res.result(), http::status::not_found);
}

TEST(TecflixHttpTest, GetUnknownPathReturnsNotFound) {
  FakeTraceIdGenerator gen;
  auto req = make_request(http::verb::get, "/api/v1/otra");
  auto res = response_for(req, gen);
  EXPECT_EQ(res.result(), http::status::not_found);
}

TEST(RandomTraceIdGeneratorTest, GenerateReturns32Chars) {
  RandomTraceIdGenerator gen;
  std::string id = gen.generate();
  EXPECT_EQ(id.length(), 32);
}

TEST(RandomTraceIdGeneratorTest, GenerateReturnsLowercaseHex) {
  RandomTraceIdGenerator gen;
  std::string id = gen.generate();
  for (char c : id) {
    EXPECT_TRUE(std::isxdigit(c));
    if (std::isalpha(c)) {
      EXPECT_EQ(c, std::tolower(c));
    }
  }
}

TEST(RandomTraceIdGeneratorTest, GenerateProducesUniqueValues) {
  RandomTraceIdGenerator gen;
  std::set<std::string> ids;
  for (int i = 0; i < 100; ++i) {
    ids.insert(gen.generate());
  }
  EXPECT_EQ(ids.size(), 100);
}

} // namespace tecflix
