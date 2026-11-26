#include <gtest/gtest.h>

#include <string>

#include "domain/User.hpp"
#include "service/UserValidation.hpp"

namespace tecflix::users {

TEST(UserValidationTest, AcceptsGoodUsernames) {
    EXPECT_TRUE(isValidUsername("abc"));
    EXPECT_TRUE(isValidUsername("Alice_01.x-y"));
    EXPECT_TRUE(isValidUsername(std::string(kMaxUsernameLength, 'a')));
}

TEST(UserValidationTest, RejectsBadUsernames) {
    EXPECT_FALSE(isValidUsername(""));
    EXPECT_FALSE(isValidUsername("ab"));
    EXPECT_FALSE(isValidUsername("con espacio"));
    EXPECT_FALSE(isValidUsername("tilde-á"));
    EXPECT_FALSE(isValidUsername("a@b"));
    EXPECT_FALSE(isValidUsername(std::string(kMaxUsernameLength + 1, 'a')));
}

TEST(UserValidationTest, AcceptsGoodEmails) {
    EXPECT_TRUE(isValidEmail("alice@tec.cr"));
    EXPECT_TRUE(isValidEmail("a.b+c@sub.dominio.com"));
    EXPECT_TRUE(isValidEmail(std::string(kMaxEmailLength - 7, 'a') + "@tec.cr"));
}

TEST(UserValidationTest, RejectsBadEmails) {
    EXPECT_FALSE(isValidEmail(""));
    EXPECT_FALSE(isValidEmail("sin-arroba"));
    EXPECT_FALSE(isValidEmail("@tec.cr"));
    EXPECT_FALSE(isValidEmail("a@b"));
    EXPECT_FALSE(isValidEmail("a@.cr"));
    EXPECT_FALSE(isValidEmail("a@tec."));
    EXPECT_FALSE(isValidEmail("a@@tec.cr"));
    EXPECT_FALSE(isValidEmail("a b@tec.cr"));
    EXPECT_FALSE(isValidEmail(std::string(kMaxEmailLength, 'a') + "@tec.cr"));
}

TEST(UserValidationTest, PasswordLengthLimits) {
    EXPECT_FALSE(isValidPassword("corta12"));
    EXPECT_TRUE(isValidPassword("larga123"));
    EXPECT_TRUE(isValidPassword(std::string(kMaxPasswordLength, 'x')));
    EXPECT_FALSE(isValidPassword(std::string(kMaxPasswordLength + 1, 'x')));
}

}  // namespace tecflix::users
