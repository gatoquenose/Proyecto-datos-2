#include <gtest/gtest.h>

#include <string>

#include "tecflix/auth/Base64Url.hpp"

namespace tecflix::auth {

TEST(Base64UrlTest, EncodesKnownValueWithoutPadding) {
    EXPECT_EQ(base64UrlEncode("Man"), "TWFu");
    EXPECT_EQ(base64UrlEncode("Ma"), "TWE");
    EXPECT_EQ(base64UrlEncode("M"), "TQ");
    EXPECT_EQ(base64UrlEncode(""), "");
}

TEST(Base64UrlTest, UsesUrlSafeAlphabet) {
    const std::string bytes = "\xfb\xff\xfe";

    const std::string encoded = base64UrlEncode(bytes);

    EXPECT_EQ(encoded.find('+'), std::string::npos);
    EXPECT_EQ(encoded.find('/'), std::string::npos);
    EXPECT_EQ(encoded, "-__-");
}

TEST(Base64UrlTest, DecodeUndoesEncodeForAnyBytes) {
    std::string bytes;
    for (int value = 0; value < 256; ++value) {
        bytes.push_back(static_cast<char>(value));
    }

    EXPECT_EQ(base64UrlDecode(base64UrlEncode(bytes)), bytes);
}

TEST(Base64UrlTest, RejectsInvalidCharacters) {
    EXPECT_FALSE(base64UrlDecode("ab$d").has_value());
    EXPECT_FALSE(base64UrlDecode("ab cd").has_value());
    EXPECT_FALSE(base64UrlDecode("ab+/").has_value());
}

}  // namespace tecflix::auth
