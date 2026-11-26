#include <gtest/gtest.h>

#include <chrono>
#include <string>

#include "FakeClock.hpp"
#include "JwtCrypto.hpp"
#include "tecflix/auth/Base64Url.hpp"
#include "tecflix/auth/JwtSigner.hpp"
#include "tecflix/auth/JwtVerifier.hpp"

namespace tecflix::auth {

using testing::FakeClock;
using std::chrono::seconds;

namespace {

const std::string kSecret = "un-secreto-de-prueba-con-mas-de-32-bytes!";
constexpr seconds kStart{1'800'000'000};
constexpr seconds kLifetime{3600};

class JwtTest : public ::testing::Test {
protected:
    FakeClock clock_{kStart};
    JwtSigner signer_{kSecret, clock_, kLifetime};
    JwtVerifier verifier_{kSecret, clock_};

    // Reemplaza una parte del token (0 = cabecera, 1 = carga, 2 = firma).
    static std::string withPart(const std::string& token, int index, const std::string& replacement) {
        std::string parts[3];
        std::size_t start = 0;
        for (int i = 0; i < 3; ++i) {
            const std::size_t end = (i == 2) ? token.size() : token.find('.', start);
            parts[i] = token.substr(start, end - start);
            start = end + 1;
        }
        parts[index] = replacement;
        return parts[0] + "." + parts[1] + "." + parts[2];
    }
};

}  // namespace

TEST_F(JwtTest, ValidTokenCarriesItsClaims) {
    const std::string token = signer_.issue("id-123", "alice");

    const VerifiedToken result = verifier_.verify(token);

    ASSERT_EQ(result.error, TokenError::None);
    EXPECT_EQ(result.claims.subject, "id-123");
    EXPECT_EQ(result.claims.username, "alice");
    EXPECT_EQ(result.claims.issuedAt, kStart.count());
    EXPECT_EQ(result.claims.expiresAt, (kStart + kLifetime).count());
}

TEST_F(JwtTest, TokenIsAcceptedUntilItsLastSecondAndThenExpires) {
    const std::string token = signer_.issue("id-123", "alice");

    clock_.advance(kLifetime - seconds{1});
    EXPECT_EQ(verifier_.verify(token).error, TokenError::None);

    clock_.advance(seconds{1});
    EXPECT_EQ(verifier_.verify(token).error, TokenError::Expired);
}

TEST_F(JwtTest, TokenSignedWithAnotherSecretIsInvalid) {
    const JwtSigner other{"otro-secreto-distinto-de-mas-de-32-bytes!!", clock_, kLifetime};

    EXPECT_EQ(verifier_.verify(other.issue("id-123", "alice")).error, TokenError::Invalid);
}

TEST_F(JwtTest, ModifiedPayloadIsInvalid) {
    const std::string token = signer_.issue("id-123", "alice");
    const std::string forgedPayload =
        base64UrlEncode(R"({"sub":"id-999","username":"admin","iat":1800000000,"exp":1900000000})");

    EXPECT_EQ(verifier_.verify(withPart(token, 1, forgedPayload)).error, TokenError::Invalid);
}

TEST_F(JwtTest, ModifiedSignatureIsInvalid) {
    const std::string token = signer_.issue("id-123", "alice");

    EXPECT_EQ(verifier_.verify(withPart(token, 2, "AAAA")).error, TokenError::Invalid);
}

TEST_F(JwtTest, TokenWithAlgorithmNoneIsInvalid) {
    const std::string token = signer_.issue("id-123", "alice");
    const std::string noneHeader = base64UrlEncode(R"({"alg":"none","typ":"JWT"})");

    EXPECT_EQ(verifier_.verify(withPart(token, 0, noneHeader)).error, TokenError::Invalid);
    EXPECT_EQ(verifier_.verify(withPart(withPart(token, 0, noneHeader), 2, "x")).error, TokenError::Invalid);
}

TEST_F(JwtTest, MalformedTokensAreInvalid) {
    EXPECT_EQ(verifier_.verify("").error, TokenError::Invalid);
    EXPECT_EQ(verifier_.verify("abc").error, TokenError::Invalid);
    EXPECT_EQ(verifier_.verify("a.b").error, TokenError::Invalid);
    EXPECT_EQ(verifier_.verify("a.b.c.d").error, TokenError::Invalid);
    EXPECT_EQ(verifier_.verify("..").error, TokenError::Invalid);
    EXPECT_EQ(verifier_.verify("$$$.$$$.$$$").error, TokenError::Invalid);
}

TEST_F(JwtTest, AuthenticTokenWithMissingClaimsIsInvalid) {
    // Firmado con el secreto correcto, pero la carga no trae username, iat ni exp.
    const std::string signingInput =
        base64UrlEncode(kJwtHeaderJson) + "." + base64UrlEncode(R"({"sub":"id-123"})");
    const std::string token = signingInput + "." + hs256Signature(kSecret, signingInput);

    EXPECT_EQ(verifier_.verify(token).error, TokenError::Invalid);
}

TEST_F(JwtTest, AuthenticTokenWithWrongClaimTypesIsInvalid) {
    const std::string signingInput =
        base64UrlEncode(kJwtHeaderJson) + "." +
        base64UrlEncode(R"({"sub":"id-123","username":"alice","iat":"ayer","exp":"mañana"})");
    const std::string token = signingInput + "." + hs256Signature(kSecret, signingInput);

    EXPECT_EQ(verifier_.verify(token).error, TokenError::Invalid);
}

TEST(JwtSignerTest, RejectsShortSecretAndNonPositiveLifetime) {
    const FakeClock clock{kStart};

    EXPECT_THROW(JwtSigner("corto", clock, kLifetime), std::invalid_argument);
    EXPECT_THROW(JwtSigner(kSecret, clock, seconds{0}), std::invalid_argument);
}

}  // namespace tecflix::auth
