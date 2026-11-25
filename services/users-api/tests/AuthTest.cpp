#include <gtest/gtest.h>

#include <set>
#include <string>

#include "auth/Argon2PasswordHasher.hpp"
#include "auth/UuidV4Generator.hpp"
#include "domain/User.hpp"

namespace tecflix::users {

namespace {

const Argon2PasswordHasher kHasher{Argon2Params::minimumForTests()};

}  // namespace

TEST(Argon2PasswordHasherTest, HashIsArgon2idAndDoesNotContainThePassword) {
    const std::string hash = kHasher.hash("mi-contrasena-segura");

    EXPECT_TRUE(hash.starts_with("$argon2id$"));
    EXPECT_EQ(hash.find("mi-contrasena-segura"), std::string::npos);
    EXPECT_LE(hash.size(), kMaxPasswordHashLength);
}

TEST(Argon2PasswordHasherTest, VerifiesTheRightPasswordOnly) {
    const std::string hash = kHasher.hash("mi-contrasena-segura");

    EXPECT_TRUE(kHasher.verify("mi-contrasena-segura", hash));
    EXPECT_FALSE(kHasher.verify("otra-contrasena", hash));
    EXPECT_FALSE(kHasher.verify("", hash));
}

TEST(Argon2PasswordHasherTest, SamePasswordGetsADifferentHashEachTime) {
    EXPECT_NE(kHasher.hash("misma-contrasena"), kHasher.hash("misma-contrasena"));
}

TEST(Argon2PasswordHasherTest, RejectsGarbageHashes) {
    EXPECT_FALSE(kHasher.verify("x", "esto-no-es-un-hash"));
    EXPECT_FALSE(kHasher.verify("x", ""));
    EXPECT_FALSE(kHasher.verify("x", std::string(500, 'a')));
}

TEST(Argon2PasswordHasherTest, HashVerifiesEvenWithProductionCost) {
    const Argon2PasswordHasher production{Argon2Params::interactive()};

    EXPECT_TRUE(production.verify("clave-de-produccion", production.hash("clave-de-produccion")));
}

TEST(UuidV4GeneratorTest, ProducesWellFormedVersion4Uuids) {
    const UuidV4Generator generator;

    const std::string id = generator.next();

    ASSERT_EQ(id.size(), kMaxIdLength);
    EXPECT_EQ(id[8], '-');
    EXPECT_EQ(id[13], '-');
    EXPECT_EQ(id[18], '-');
    EXPECT_EQ(id[23], '-');
    EXPECT_EQ(id[14], '4');
    EXPECT_NE(std::string{"89ab"}.find(id[19]), std::string::npos);
}

TEST(UuidV4GeneratorTest, ProducesDifferentIds) {
    const UuidV4Generator generator;
    std::set<std::string> ids;

    for (int i = 0; i < 1000; ++i) {
        ids.insert(generator.next());
    }

    EXPECT_EQ(ids.size(), 1000U);
}

}  // namespace tecflix::users
