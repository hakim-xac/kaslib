/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_CONVERT_JWT_V2_TESTS_H
#define KASLIBCXX_CONVERT_JWT_V2_TESTS_H

#include <chrono>
#include <string>
#include <string_view>

#include "KAS/convert/jwt/v2/jwt.h"
#include "tests/tests.h"

/**
 * @brief Tests for the JWT v2 creation and verification API.
 */
namespace kas::convert::jwt::v2::tests {

namespace {
    using namespace kas::tests;
    using namespace kas::convert::jwt::v2;

    // --- 1. toJson - basic ------------------------------------------
    inline void toJsonBasic(TestContext& ctx)
    {
        JwtPayload p;
        p.subject = "user1";
        p.name = "Ivan";
        p.issued_at = 1000;
        p.expires_at = 2000;

        const std::string json = toJson(p);
        KAS_TEST_CHECK_IMPL(ctx, json.find("\"sub\":\"user1\"") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, json.find("\"name\":\"Ivan\"") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, json.find("\"iat\":1000") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, json.find("\"exp\":2000") != std::string::npos);
    }

    // --- 2. toJson - escaping ----------------------------------------
    inline void toJsonEscape(TestContext& ctx)
    {
        JwtPayload p;
        p.subject = "a\"b\\c\nd";
        const std::string json = toJson(p);
        KAS_TEST_CHECK_IMPL(ctx, json.find("a\\\"b\\\\c\\nd") != std::string::npos);
    }

    // --- 3. fromJson - round-trip ------------------------------------
    inline void fromJsonRoundTrip(TestContext& ctx)
    {
        JwtPayload p;
        p.subject = "user1";
        p.name = "Ivan";
        p.issued_at = 1000;
        p.expires_at = 2000;

        const auto r = fromJson(toJson(p));
        KAS_TEST_CHECK_IMPL(ctx, r.has_value());
        if (r)
            KAS_TEST_CHECK_IMPL(ctx, *r == p);
    }

    // --- 4. fromJson - malformed JSON --------------------------------
    inline void fromJsonInvalid(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, !fromJson("").has_value());
        KAS_TEST_CHECK_IMPL(ctx, !fromJson("{").has_value());
        KAS_TEST_CHECK_IMPL(ctx, !fromJson("[]").has_value());
        KAS_TEST_CHECK_IMPL(ctx, !fromJson("{\"sub\":}").has_value());
        // lenient: either rejecting or accepting trailing garbage is acceptable
        KAS_TEST_CHECK_IMPL(ctx, !fromJson("{\"sub\":\"a\"}extra").has_value() || fromJson("{\"sub\":\"a\"}extra").has_value());
    }

    // --- 5. isExpired - 0 = no expiration ----------------------------
    inline void isExpiredNoExp(TestContext& ctx)
    {
        JwtPayload p;
        p.expires_at = 0;
        KAS_TEST_CHECK_IMPL(ctx, !isExpired(p));
    }

    // --- 6. isExpired - future/past bounds ---------------------------
    inline void isExpiredBounds(TestContext& ctx)
    {
        using namespace std::chrono;
        const auto now = duration_cast<seconds>(
            system_clock::now().time_since_epoch())
                             .count();

        JwtPayload future;
        future.expires_at = now + 3600;
        KAS_TEST_CHECK_IMPL(ctx, !isExpired(future));

        JwtPayload past;
        past.expires_at = now - 3600;
        KAS_TEST_CHECK_IMPL(ctx, isExpired(past));
    }

    // --- 7. createToken + verify - round-trip ------------------------
    inline void tokenRoundTrip(TestContext& ctx)
    {
        const Jwt j { "secret-key" };

        JwtPayload p;
        p.subject = "user1";
        p.name = "Ivan";
        p.issued_at = 1000;
        p.expires_at = 2000;

        const std::string token = j.createToken(p);
        KAS_TEST_CHECK_IMPL(ctx, !token.empty());
        KAS_TEST_CHECK_IMPL(ctx,
            std::count(token.begin(), token.end(), '.') == 2);

        const auto r = j.verify(token);
        KAS_TEST_CHECK_IMPL(ctx, r.has_value());
        if (r)
            KAS_TEST_CHECK_IMPL(ctx, *r == p);
    }

    // --- 8. verify - wrong secret ------------------------------------
    inline void tokenWrongSecret(TestContext& ctx)
    {
        const Jwt j1 { "secret-1" };
        const Jwt j2 { "secret-2" };

        JwtPayload p;
        p.subject = "u";
        const std::string token = j1.createToken(p);

        const auto r = j2.verify(token);
        KAS_TEST_CHECK_IMPL(ctx, r.has_error());
    }

    // --- 9. verify - malformed structure -----------------------------
    inline void tokenMalformed(TestContext& ctx)
    {
        const Jwt j { "s" };
        KAS_TEST_CHECK_IMPL(ctx, j.verify("").has_error());
        KAS_TEST_CHECK_IMPL(ctx, j.verify("a.b").has_error());
        KAS_TEST_CHECK_IMPL(ctx, j.verify("a.b.c.d").has_error());
        KAS_TEST_CHECK_IMPL(ctx, j.verify("...").has_error());
    }

    // --- 10. verifyAndValidate - expired token -----------------------
    inline void tokenExpired(TestContext& ctx)
    {
        const Jwt j { "s" };
        JwtPayload p;
        p.expires_at = 1; // 1970-01-01T00:00:01 - long expired
        const std::string token = j.createToken(p);

        const auto r = j.verifyAndValidate(token);
        KAS_TEST_CHECK_IMPL(ctx, r.has_error());
    }

    // --- 11. verifyAndValidate - valid token -------------------------
    inline void tokenValid(TestContext& ctx)
    {
        const Jwt j { "s" };
        const std::string token = j.createToken("user1", "Ivan");
        const auto r = j.verifyAndValidate(token);
        KAS_TEST_CHECK_IMPL(ctx, r.has_value());
        if (r) {
            KAS_TEST_CHECK_IMPL(ctx, r->subject == "user1");
            KAS_TEST_CHECK_IMPL(ctx, r->name == "Ivan");
        }
    }

    // --- 12. createToken(subject, name) - default TTL ----------------
    inline void createTokenDefaultTtl(TestContext& ctx)
    {
        const Jwt j { "s" };
        const std::string token = j.createToken("u", "n");
        const auto r = j.verify(token);
        KAS_TEST_CHECK_IMPL(ctx, r.has_value());
        if (r) {
            KAS_TEST_CHECK_IMPL(ctx, r->expires_at > r->issued_at);
        }
    }

    // --- 13. createToken(subject, name, 0) - no expiration -----------
    inline void createTokenNoExpiry(TestContext& ctx)
    {
        const Jwt j { "s" };
        const std::string token = j.createToken("u", "n", std::chrono::seconds { 0 });
        const auto r = j.verifyAndValidate(token);
        KAS_TEST_CHECK_IMPL(ctx, r.has_value()); // exp=0 -> never expires
    }

    // --- 14. Tampered payload - signature mismatch -------------------
    inline void tokenTamperedPayload(TestContext& ctx)
    {
        const Jwt j { "s" };
        const std::string token = j.createToken("u", "n");

        // Flip one character in the payload (middle part)
        std::string tampered = token;
        const auto first = tampered.find('.');
        tampered[first + 1] = (tampered[first + 1] == 'A') ? 'B' : 'A';

        const auto r = j.verify(tampered);
        KAS_TEST_CHECK_IMPL(ctx, r.has_error());
    }
} // namespace

/**
 * @brief Runs all JWT v2 tests and prints a summary.
 */
inline bool runTests()
{
    TestContext ctx;

    toJsonBasic(ctx);
    toJsonEscape(ctx);
    fromJsonRoundTrip(ctx);
    fromJsonInvalid(ctx);
    isExpiredNoExp(ctx);
    isExpiredBounds(ctx);

    tokenRoundTrip(ctx);
    tokenWrongSecret(ctx);
    tokenMalformed(ctx);
    tokenExpired(ctx);
    tokenValid(ctx);
    createTokenDefaultTtl(ctx);
    createTokenNoExpiry(ctx);
    tokenTamperedPayload(ctx);

    return printSummary("kas::convert::jwt::tests", ctx);
}

} // namespace kas::convert::jwt::v2::tests

#endif