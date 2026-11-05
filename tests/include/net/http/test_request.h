/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_NET_HTTP_REQUEST_TESTS_H
#define KASLIBCXX_NET_HTTP_REQUEST_TESTS_H

#include "KAS/net/http/Request.h"
#include "tests/tests.h"

#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace kas::net::http::test_request::tests {

namespace {
    using namespace kas::tests;
    using kas::net::http::CaseInsensitiveHash;
    using kas::net::http::HttpVersion;
    using kas::net::http::Request;
    using kas::net::http::TransparentStringHash;

    // ------------------------------------------------------------
    // Constructors / default state
    // ------------------------------------------------------------

    /// Default-constructed request: empty fields, HTTP/1.1, keep-alive enabled.
    inline void testDefaultConstructor(TestContext& ctx)
    {
        Request req;
        KAS_TEST_CHECK_IMPL(ctx, req.getMethod().empty());
        KAS_TEST_CHECK_IMPL(ctx, req.getTarget().empty());
        KAS_TEST_CHECK_IMPL(ctx, req.getPath().empty());
        KAS_TEST_CHECK_IMPL(ctx, req.getQueryString().empty());
        KAS_TEST_CHECK_IMPL(ctx, req.getQueryParams().empty());
        KAS_TEST_CHECK_IMPL(ctx, req.getHeaders().empty());
        KAS_TEST_CHECK_IMPL(ctx, req.getBody().empty());
        KAS_TEST_CHECK_IMPL(ctx, req.getBodySize() == 0);
        KAS_TEST_CHECK_IMPL(ctx, req.getRemoteAddress().empty());
        KAS_TEST_CHECK_IMPL(ctx, req.getVersion() == HttpVersion::HTTP_1_1);
        KAS_TEST_CHECK_IMPL(ctx, req.isKeepAlive() == true);
    }

    /// Parameterized constructor: parses method, target, path, and query.
    inline void testParameterizedConstructor(TestContext& ctx)
    {
        Request req("GET", "/api/users", HttpVersion::HTTP_1_1);
        KAS_TEST_CHECK_IMPL(ctx, req.getMethod() == "GET");
        KAS_TEST_CHECK_IMPL(ctx, req.getTarget() == "/api/users");
        KAS_TEST_CHECK_IMPL(ctx, req.getPath() == "/api/users");
        KAS_TEST_CHECK_IMPL(ctx, req.getQueryString().empty());
        KAS_TEST_CHECK_IMPL(ctx, req.getVersion() == HttpVersion::HTTP_1_1);

        Request req2("POST", "/x?y=1", HttpVersion::HTTP_1_0);
        KAS_TEST_CHECK_IMPL(ctx, req2.getMethod() == "POST");
        KAS_TEST_CHECK_IMPL(ctx, req2.getPath() == "/x");
        KAS_TEST_CHECK_IMPL(ctx, req2.getQueryString() == "y=1");
        KAS_TEST_CHECK_IMPL(ctx, req2.getVersion() == HttpVersion::HTTP_1_0);
    }

    // ------------------------------------------------------------
    // method
    // ------------------------------------------------------------

    /// setMethod/getMethod including setting an empty string.
    inline void testMethod(TestContext& ctx)
    {
        Request req;
        KAS_TEST_CHECK_IMPL(ctx, req.getMethod().empty());

        req.setMethod("GET");
        KAS_TEST_CHECK_IMPL(ctx, req.getMethod() == "GET");

        req.setMethod("POST");
        KAS_TEST_CHECK_IMPL(ctx, req.getMethod() == "POST");

        // Empty string.
        req.setMethod("");
        KAS_TEST_CHECK_IMPL(ctx, req.getMethod().empty());
    }

    /// Lvalue and rvalue fluent chains for setMethod.
    inline void testMethodFluent(TestContext& ctx)
    {
        // Lvalue chain.
        Request req;
        Request& ref = req.setMethod("PUT").setMethod("DELETE");
        KAS_TEST_CHECK_IMPL(ctx, &ref == &req);
        KAS_TEST_CHECK_IMPL(ctx, req.getMethod() == "DELETE");

        // Rvalue chain (through a temporary object).
        Request req2 = Request { }.setMethod("PATCH");
        KAS_TEST_CHECK_IMPL(ctx, req2.getMethod() == "PATCH");
    }

    // ------------------------------------------------------------
    // target / path / query
    // ------------------------------------------------------------

    /// Target parsing: with/without query, empty path, trailing '?', re-setting.
    inline void testTargetParsing(TestContext& ctx)
    {
        // No query.
        {
            Request req;
            req.setTarget("/api/users");
            KAS_TEST_CHECK_IMPL(ctx, req.getTarget() == "/api/users");
            KAS_TEST_CHECK_IMPL(ctx, req.getPath() == "/api/users");
            KAS_TEST_CHECK_IMPL(ctx, req.getQueryString().empty());
            KAS_TEST_CHECK_IMPL(ctx, req.getQueryParams().empty());
        }

        // With query.
        {
            Request req;
            req.setTarget("/api/users?page=2&limit=10");
            KAS_TEST_CHECK_IMPL(ctx, req.getPath() == "/api/users");
            KAS_TEST_CHECK_IMPL(ctx, req.getQueryString() == "page=2&limit=10");
            KAS_TEST_CHECK_IMPL(ctx, req.getQueryParams().size() == 2);
        }

        // Empty path, only query.
        {
            Request req;
            req.setTarget("?x=1");
            KAS_TEST_CHECK_IMPL(ctx, req.getPath().empty());
            KAS_TEST_CHECK_IMPL(ctx, req.getQueryString() == "x=1");
        }

        // Path + empty query ("/x?").
        {
            Request req;
            req.setTarget("/x?");
            KAS_TEST_CHECK_IMPL(ctx, req.getPath() == "/x");
            KAS_TEST_CHECK_IMPL(ctx, req.getQueryString().empty());
            KAS_TEST_CHECK_IMPL(ctx, req.getQueryParams().empty());
        }

        // Re-setting the target clears old parameters.
        {
            Request req;
            req.setTarget("/a?x=1&y=2");
            KAS_TEST_CHECK_IMPL(ctx, req.getQueryParams().size() == 2);
            req.setTarget("/b?z=3");
            KAS_TEST_CHECK_IMPL(ctx, req.getPath() == "/b");
            KAS_TEST_CHECK_IMPL(ctx, req.getQueryString() == "z=3");
            KAS_TEST_CHECK_IMPL(ctx, req.getQueryParams().size() == 1);
            KAS_TEST_CHECK_IMPL(ctx, req.getQueryParam("x") == std::nullopt);
            auto z = req.getQueryParam("z");
            KAS_TEST_CHECK_IMPL(ctx, z.has_value() && *z == "3");
        }
    }

    /// Query parsing: values, empty values, flag without '=', missing key,
    /// transparent lookup, and case-sensitive keys.
    inline void testQueryParams(TestContext& ctx)
    {
        Request req("GET", "/search?q=hello&lang=en&empty=&flag");
        KAS_TEST_CHECK_IMPL(ctx, req.getQueryParams().size() == 4);

        {
            auto v = req.getQueryParam("q");
            KAS_TEST_CHECK_IMPL(ctx, v.has_value() && *v == "hello");
        }
        {
            auto v = req.getQueryParam("lang");
            KAS_TEST_CHECK_IMPL(ctx, v.has_value() && *v == "en");
        }
        {
            // Key with an empty value ("empty=").
            auto v = req.getQueryParam("empty");
            KAS_TEST_CHECK_IMPL(ctx, v.has_value() && v->empty());
        }
        {
            // Key without '=' ("flag").
            auto v = req.getQueryParam("flag");
            KAS_TEST_CHECK_IMPL(ctx, v.has_value() && v->empty());
        }
        {
            // Missing key.
            auto v = req.getQueryParam("missing");
            KAS_TEST_CHECK_IMPL(ctx, !v.has_value());
        }

        // Transparent lookup by string_view.
        std::string_view key = "lang";
        auto v = req.getQueryParam(key);
        KAS_TEST_CHECK_IMPL(ctx, v.has_value() && *v == "en");

        // Query keys are case-sensitive.
        Request req2("GET", "/x?User=1&user=2");
        KAS_TEST_CHECK_IMPL(ctx, req2.getQueryParams().size() == 2);
        {
            auto u = req2.getQueryParam("User");
            KAS_TEST_CHECK_IMPL(ctx, u.has_value() && *u == "1");
        }
        {
            auto u = req2.getQueryParam("user");
            KAS_TEST_CHECK_IMPL(ctx, u.has_value() && *u == "2");
        }
    }

    /// Duplicate query keys: the first occurrence is kept (emplace semantics).
    inline void testQueryParamsDuplicates(TestContext& ctx)
    {
        Request req("GET", "/x?a=1&a=2&a=3");
        KAS_TEST_CHECK_IMPL(ctx, req.getQueryParams().size() == 1);
        auto v = req.getQueryParam("a");
        KAS_TEST_CHECK_IMPL(ctx, v.has_value() && *v == "1");
    }

    /// Query edge cases: empty segments, '=' in values, empty key "=v".
    inline void testQueryParamsEdgeCases(TestContext& ctx)
    {
        // Empty segments.
        Request req("GET", "/x?&&a=1&&b=2&");
        KAS_TEST_CHECK_IMPL(ctx, req.getQueryParams().size() == 2);
        {
            auto a = req.getQueryParam("a");
            KAS_TEST_CHECK_IMPL(ctx, a.has_value() && *a == "1");
        }
        {
            auto b = req.getQueryParam("b");
            KAS_TEST_CHECK_IMPL(ctx, b.has_value() && *b == "2");
        }

        // '=' in the value (the separator is the first '=').
        Request req2("GET", "/x?k=a=b");
        {
            auto k = req2.getQueryParam("k");
            KAS_TEST_CHECK_IMPL(ctx, k.has_value() && *k == "a=b");
        }

        // Empty key "=v" is stored with an empty key.
        Request req3("GET", "/x?=v");
        KAS_TEST_CHECK_IMPL(ctx, req3.getQueryParams().size() == 1);
        {
            // And can be retrieved by the empty key.
            auto v = req3.getQueryParam("");
            KAS_TEST_CHECK_IMPL(ctx, v.has_value() && *v == "v");
        }
    }

    // Additional query edge cases:
    //  - '?' inside a value (splitTarget looks for the FIRST '?')
    //  - URL decoding is NOT performed (pins the current contract)
    //  - '+' is NOT converted to a space

    /// Extended query edge cases: '?' in value, no percent-decoding, no '+' handling.
    inline void testQueryEdgeCasesExtended(TestContext& ctx)
    {
        // '?' in value: the separator is the first '?', the rest is part of the query.
        {
            Request req("GET", "/x?a=b?c");
            KAS_TEST_CHECK_IMPL(ctx, req.getPath() == "/x");
            KAS_TEST_CHECK_IMPL(ctx, req.getQueryString() == "a=b?c");
            auto a = req.getQueryParam("a");
            KAS_TEST_CHECK_IMPL(ctx, a.has_value() && *a == "b?c");
        }

        // '%20' and '+' are NOT decoded - pins the current behavior.
        // If URL decoding is added later, this test must be updated.
        {
            Request req("GET", "/x?q=hello%20world&a=b+c");
            auto q = req.getQueryParam("q");
            KAS_TEST_CHECK_IMPL(ctx, q.has_value() && *q == "hello%20world");
            auto a = req.getQueryParam("a");
            KAS_TEST_CHECK_IMPL(ctx, a.has_value() && *a == "b+c");
        }

        // Empty query after '?'.
        {
            Request req("GET", "/x?");
            KAS_TEST_CHECK_IMPL(ctx, req.getQueryParams().empty());
            KAS_TEST_CHECK_IMPL(ctx, !req.getQueryParam("").has_value());
        }
    }

    // ------------------------------------------------------------
    // version
    // ------------------------------------------------------------

    /// Version getters/setters and versionToString.
    inline void testVersion(TestContext& ctx)
    {
        Request req;
        KAS_TEST_CHECK_IMPL(ctx, req.getVersion() == HttpVersion::HTTP_1_1);
        KAS_TEST_CHECK_IMPL(ctx, req.getVersionString() == "HTTP/1.1");

        req.setVersion(HttpVersion::HTTP_1_0);
        KAS_TEST_CHECK_IMPL(ctx, req.getVersion() == HttpVersion::HTTP_1_0);
        KAS_TEST_CHECK_IMPL(ctx, req.getVersionString() == "HTTP/1.0");

        req.setVersion(HttpVersion::HTTP_2_0);
        KAS_TEST_CHECK_IMPL(ctx, req.getVersionString() == "HTTP/2.0");

        // static
        KAS_TEST_CHECK_IMPL(ctx, Request::versionToString(HttpVersion::HTTP_1_0) == "HTTP/1.0");
        KAS_TEST_CHECK_IMPL(ctx, Request::versionToString(HttpVersion::HTTP_1_1) == "HTTP/1.1");
        KAS_TEST_CHECK_IMPL(ctx, Request::versionToString(HttpVersion::HTTP_2_0) == "HTTP/2.0");
    }

    /// Invalid enum value: pins the fallback contract ("HTTP/1.1").
    inline void testVersionEdgeCases(TestContext& ctx)
    {
        const auto bogus = static_cast<HttpVersion>(999);
        KAS_TEST_CHECK_IMPL(ctx, Request::versionToString(bogus) == "HTTP/1.1");

        Request req;
        req.setVersion(bogus);
        KAS_TEST_CHECK_IMPL(ctx, req.getVersionString() == "HTTP/1.1");
    }

    // ------------------------------------------------------------
    // headers
    // ------------------------------------------------------------

    /// Header set/get/has/overwrite, case-insensitivity, transparent lookup, empty values.
    inline void testHeaders(TestContext& ctx)
    {
        Request req;
        KAS_TEST_CHECK_IMPL(ctx, req.getHeaders().empty());
        KAS_TEST_CHECK_IMPL(ctx, !req.hasHeader("Content-Type"));

        req.setHeader("Content-Type", "application/json");
        KAS_TEST_CHECK_IMPL(ctx, req.hasHeader("Content-Type"));
        KAS_TEST_CHECK_IMPL(ctx, req.hasHeader("content-type"));
        KAS_TEST_CHECK_IMPL(ctx, req.hasHeader("CONTENT-TYPE"));

        {
            auto v = req.getHeader("Content-Type");
            KAS_TEST_CHECK_IMPL(ctx, v.has_value() && *v == "application/json");
        }
        {
            auto v = req.getHeader("content-type");
            KAS_TEST_CHECK_IMPL(ctx, v.has_value() && *v == "application/json");
        }

        // Overwrite.
        req.setHeader("content-type", "text/plain");
        KAS_TEST_CHECK_IMPL(ctx, req.getHeaders().size() == 1);
        {
            auto v = req.getHeader("Content-Type");
            KAS_TEST_CHECK_IMPL(ctx, v.has_value() && *v == "text/plain");
        }

        // Several distinct headers.
        req.setHeader("Accept", "*/*");
        req.setHeader("X-Custom", "1");
        KAS_TEST_CHECK_IMPL(ctx, req.getHeaders().size() == 3);

        // Transparent lookup by string_view.
        std::string_view name = "accept";
        auto v = req.getHeader(name);
        KAS_TEST_CHECK_IMPL(ctx, v.has_value() && *v == "*/*");

        // Missing header.
        KAS_TEST_CHECK_IMPL(ctx, !req.hasHeader("X-Missing"));
        KAS_TEST_CHECK_IMPL(ctx, !req.getHeader("X-Missing").has_value());

        // Empty value.
        req.setHeader("X-Empty", "");
        {
            auto e = req.getHeader("x-empty");
            KAS_TEST_CHECK_IMPL(ctx, e.has_value() && e->empty());
        }
    }

    /// setHeader with an empty name is currently allowed (an entry is created).
    /// Pins this contract.
    inline void testHeaderEmptyName(TestContext& ctx)
    {
        Request req;
        req.setHeader("", "v");
        KAS_TEST_CHECK_IMPL(ctx, req.getHeaders().size() == 1);
        auto v = req.getHeader("");
        KAS_TEST_CHECK_IMPL(ctx, v.has_value() && *v == "v");
    }

    // ------------------------------------------------------------
    // body
    // ------------------------------------------------------------

    /// setBody/getBody/getBodySize, including replacement, empty, and binary data.
    inline void testBody(TestContext& ctx)
    {
        Request req;
        KAS_TEST_CHECK_IMPL(ctx, req.getBody().empty());
        KAS_TEST_CHECK_IMPL(ctx, req.getBodySize() == 0);

        req.setBody("hello");
        KAS_TEST_CHECK_IMPL(ctx, req.getBody() == "hello");
        KAS_TEST_CHECK_IMPL(ctx, req.getBodySize() == 5);

        // Replacement.
        req.setBody("world!");
        KAS_TEST_CHECK_IMPL(ctx, req.getBody() == "world!");
        KAS_TEST_CHECK_IMPL(ctx, req.getBodySize() == 6);

        // Empty body.
        req.setBody("");
        KAS_TEST_CHECK_IMPL(ctx, req.getBody().empty());
        KAS_TEST_CHECK_IMPL(ctx, req.getBodySize() == 0);

        // Body with embedded NUL bytes.
        std::string binary("a\0b", 3);
        req.setBody(binary);
        KAS_TEST_CHECK_IMPL(ctx, req.getBodySize() == 3);
        KAS_TEST_CHECK_IMPL(ctx, req.getBody() == binary);
    }

    // ------------------------------------------------------------
    // connection info
    // ------------------------------------------------------------

    /// Remote address and keep-alive flag getters/setters.
    inline void testConnectionInfo(TestContext& ctx)
    {
        Request req;
        KAS_TEST_CHECK_IMPL(ctx, req.getRemoteAddress().empty());
        KAS_TEST_CHECK_IMPL(ctx, req.isKeepAlive() == true);

        req.setRemoteAddress("127.0.0.1:54321");
        KAS_TEST_CHECK_IMPL(ctx, req.getRemoteAddress() == "127.0.0.1:54321");

        req.setKeepAlive(false);
        KAS_TEST_CHECK_IMPL(ctx, req.isKeepAlive() == false);

        req.setKeepAlive(true);
        KAS_TEST_CHECK_IMPL(ctx, req.isKeepAlive() == true);
    }

    // ------------------------------------------------------------
    // fluent / move / copy
    // ------------------------------------------------------------

    /// Lvalue fluent chain across all setters returns a reference to the same object.
    inline void testFluentChain(TestContext& ctx)
    {
        Request req;
        Request& ref = req
                           .setMethod("POST")
                           .setTarget("/api/items?x=1")
                           .setVersion(HttpVersion::HTTP_1_1)
                           .setHeader("Content-Type", "application/json")
                           .setBody("{}")
                           .setRemoteAddress("10.0.0.1")
                           .setKeepAlive(false);

        KAS_TEST_CHECK_IMPL(ctx, &ref == &req);
        KAS_TEST_CHECK_IMPL(ctx, req.getMethod() == "POST");
        KAS_TEST_CHECK_IMPL(ctx, req.getPath() == "/api/items");
        {
            auto x = req.getQueryParam("x");
            KAS_TEST_CHECK_IMPL(ctx, x.has_value() && *x == "1");
        }
        {
            auto ct = req.getHeader("content-type");
            KAS_TEST_CHECK_IMPL(ctx, ct.has_value() && *ct == "application/json");
        }
        KAS_TEST_CHECK_IMPL(ctx, req.getBody() == "{}");
        KAS_TEST_CHECK_IMPL(ctx, req.getRemoteAddress() == "10.0.0.1");
        KAS_TEST_CHECK_IMPL(ctx, req.isKeepAlive() == false);
    }

    // Move ctor and move assignment.
    // NOTE: moved-from std::string is "valid but unspecified",
    // so we do NOT check that the source's fields are empty.

    /// Move construction and move assignment preserve all request state.
    inline void testMove(TestContext& ctx)
    {
        Request src("GET", "/a?x=1");
        src.setHeader("X-Test", "1");
        src.setBody("payload");

        // Move ctor.
        Request dst = std::move(src);
        KAS_TEST_CHECK_IMPL(ctx, dst.getMethod() == "GET");
        KAS_TEST_CHECK_IMPL(ctx, dst.getPath() == "/a");
        {
            auto x = dst.getQueryParam("x");
            KAS_TEST_CHECK_IMPL(ctx, x.has_value() && *x == "1");
        }
        {
            auto h = dst.getHeader("x-test");
            KAS_TEST_CHECK_IMPL(ctx, h.has_value() && *h == "1");
        }
        KAS_TEST_CHECK_IMPL(ctx, dst.getBody() == "payload");

        // Moved-from: only check that the object is valid (no UB).
        // No checks on field emptiness - the standard does not guarantee it.
        (void)src.getMethod();
        (void)src.getTarget();
        (void)src.getBody();
        (void)src.getHeaders();
        (void)src.getQueryParams();

        // Move assignment.
        Request src2("POST", "/b?y=2");
        src2.setBody("data2");

        Request dst2;
        dst2 = std::move(src2);
        KAS_TEST_CHECK_IMPL(ctx, dst2.getMethod() == "POST");
        KAS_TEST_CHECK_IMPL(ctx, dst2.getPath() == "/b");
        {
            auto y = dst2.getQueryParam("y");
            KAS_TEST_CHECK_IMPL(ctx, y.has_value() && *y == "2");
        }
        KAS_TEST_CHECK_IMPL(ctx, dst2.getBody() == "data2");
    }

    /// Copy construction and copy assignment perform a deep copy.
    inline void testCopy(TestContext& ctx)
    {
        Request src("GET", "/a?x=1");
        src.setHeader("X-Test", "1");
        src.setBody("payload");

        // Copy ctor.
        Request copy = src;
        KAS_TEST_CHECK_IMPL(ctx, copy.getMethod() == "GET");
        KAS_TEST_CHECK_IMPL(ctx, copy.getPath() == "/a");
        {
            auto x = copy.getQueryParam("x");
            KAS_TEST_CHECK_IMPL(ctx, x.has_value() && *x == "1");
        }
        {
            auto h = copy.getHeader("x-test");
            KAS_TEST_CHECK_IMPL(ctx, h.has_value() && *h == "1");
        }
        KAS_TEST_CHECK_IMPL(ctx, copy.getBody() == "payload");

        // Modifying the copy does not affect the source.
        copy.setMethod("POST");
        copy.setBody("changed");
        copy.setHeader("X-Test", "2");

        KAS_TEST_CHECK_IMPL(ctx, src.getMethod() == "GET");
        KAS_TEST_CHECK_IMPL(ctx, src.getBody() == "payload");
        {
            auto h = src.getHeader("X-Test");
            KAS_TEST_CHECK_IMPL(ctx, h.has_value() && *h == "1");
        }

        // Copy assignment.
        Request dst;
        dst = src;
        KAS_TEST_CHECK_IMPL(ctx, dst.getMethod() == "GET");
        KAS_TEST_CHECK_IMPL(ctx, dst.getBody() == "payload");
    }

    // Self-move-assignment must not break the object (at least not UB).
    // operator= must guard against self-assignment.

    /// Self copy/move assignment leaves the object valid.
    inline void testSelfAssignment(TestContext& ctx)
    {
        Request req("GET", "/a?x=1");
        req.setBody("payload");

        // Self copy assignment.
        req = req;
        KAS_TEST_CHECK_IMPL(ctx, req.getMethod() == "GET");
        KAS_TEST_CHECK_IMPL(ctx, req.getBody() == "payload");
        {
            auto x = req.getQueryParam("x");
            KAS_TEST_CHECK_IMPL(ctx, x.has_value() && *x == "1");
        }

        // Self move assignment - the standard says "valid but unspecified",
        // but an implementation based on std::exchange/swap usually leaves
        // the object usable. We only check that it does not crash and the
        // object remains valid.
        //
        // Direct `req = std::move(req)` triggers -Wself-move, so we hide
        // the source behind a pointer.
        Request* p = &req;
        req = std::move(*p);
        (void)req.getMethod();
        (void)req.getBody();
    }

    // ------------------------------------------------------------
    // hash transparency
    // ------------------------------------------------------------

    /// Header keys are case-insensitive; query keys are case-sensitive.
    inline void testHashTransparency(TestContext& ctx)
    {
        // headers: "Content-Type" == "content-type" == "CONTENT-TYPE"
        Request req;
        req.setHeader("Content-Type", "text/html");
        req.setHeader("content-type", "text/html"); // overwrite, not a duplicate
        KAS_TEST_CHECK_IMPL(ctx, req.getHeaders().size() == 1);

        // query: "User" != "user"
        Request req2("GET", "/x?User=1&user=2");
        KAS_TEST_CHECK_IMPL(ctx, req2.getQueryParams().size() == 2);
    }

    // Hash functions: equal keys must produce equal hashes - a prerequisite
    // for correct unordered_map behavior.

    /// CaseInsensitiveHash folds case; TransparentStringHash is case-sensitive.
    inline void testHashCollisions(TestContext& ctx)
    {
        const CaseInsensitiveHash ci_hash;

        // CaseInsensitiveHash: different cases -> same hash.
        KAS_TEST_CHECK_IMPL(ctx, ci_hash("Content-Type") == ci_hash("content-type"));
        KAS_TEST_CHECK_IMPL(ctx, ci_hash("CONTENT-TYPE") == ci_hash("content-type"));
        KAS_TEST_CHECK_IMPL(ctx, ci_hash("CoNtEnT-TyPe") == ci_hash("content-type"));
        KAS_TEST_CHECK_IMPL(ctx, ci_hash("") == ci_hash(""));

        // Different strings (not just case) usually produce different hashes,
        // but collisions are allowed. We only check the typical case.
        KAS_TEST_CHECK_IMPL(ctx, ci_hash("Content-Type") != ci_hash("Accept"));

        // TransparentStringHash: case-sensitive.
        const TransparentStringHash ts_hash;
        KAS_TEST_CHECK_IMPL(ctx, ts_hash("User") != ts_hash("user"));
        KAS_TEST_CHECK_IMPL(ctx, ts_hash("User") == ts_hash("User"));
        KAS_TEST_CHECK_IMPL(ctx, ts_hash("") == ts_hash(""));
    }

    // Transparent lookup must work for const char*, std::string, and
    // string_view without creating temporary keys.

    /// Lookups via const char*, std::string, and string_view all succeed.
    inline void testTransparentLookup(TestContext& ctx)
    {
        Request req("GET", "/x?lang=en");
        req.setHeader("Accept", "*/*");

        // getHeader: const char*
        {
            auto v = req.getHeader("accept");
            KAS_TEST_CHECK_IMPL(ctx, v.has_value() && *v == "*/*");
        }
        // getHeader: std::string
        {
            std::string name = "ACCEPT";
            auto v = req.getHeader(name);
            KAS_TEST_CHECK_IMPL(ctx, v.has_value() && *v == "*/*");
        }
        // getHeader: string_view
        {
            std::string_view name = "Accept";
            auto v = req.getHeader(name);
            KAS_TEST_CHECK_IMPL(ctx, v.has_value() && *v == "*/*");
        }

        // getQueryParam: const char*
        {
            auto v = req.getQueryParam("lang");
            KAS_TEST_CHECK_IMPL(ctx, v.has_value() && *v == "en");
        }
        // getQueryParam: std::string
        {
            std::string key = "lang";
            auto v = req.getQueryParam(key);
            KAS_TEST_CHECK_IMPL(ctx, v.has_value() && *v == "en");
        }
        // getQueryParam: string_view
        {
            std::string_view key = "lang";
            auto v = req.getQueryParam(key);
            KAS_TEST_CHECK_IMPL(ctx, v.has_value() && *v == "en");
        }

        // hasHeader: all three forms.
        KAS_TEST_CHECK_IMPL(ctx, req.hasHeader("Accept"));
        KAS_TEST_CHECK_IMPL(ctx, req.hasHeader(std::string { "accept" }));
        KAS_TEST_CHECK_IMPL(ctx, req.hasHeader(std::string_view { "ACCEPT" }));
    }

    // ------------------------------------------------------------
    // return types (compile-time)
    // ------------------------------------------------------------

    /// static_assert checks of getter return types and special member functions.
    inline void testReturnTypes(TestContext& ctx)
    {
        (void)ctx;

        Request req;

        static_assert(std::is_same_v<decltype(req.getMethod()), const std::string&>);
        static_assert(std::is_same_v<decltype(req.getTarget()), const std::string&>);
        static_assert(std::is_same_v<decltype(req.getPath()), const std::string&>);
        static_assert(std::is_same_v<decltype(req.getQueryString()), const std::string&>);
        static_assert(std::is_same_v<decltype(req.getBody()), const std::string&>);
        static_assert(std::is_same_v<decltype(req.getRemoteAddress()), const std::string&>);

        static_assert(std::is_same_v<decltype(req.getVersion()), HttpVersion>);
        static_assert(std::is_same_v<decltype(req.isKeepAlive()), bool>);

        static_assert(std::is_same_v<
            decltype(req.getHeaders()),
            const Request::headers_t&>);
        static_assert(std::is_same_v<
            decltype(req.getQueryParams()),
            const Request::query_params_t&>);

        static_assert(std::is_same_v<
            decltype(req.getHeader("x")),
            std::optional<std::string_view>>);
        static_assert(std::is_same_v<
            decltype(req.getQueryParam("x")),
            std::optional<std::string_view>>);

        static_assert(std::is_copy_constructible_v<Request>);
        static_assert(std::is_copy_assignable_v<Request>);
        static_assert(std::is_move_constructible_v<Request>);
        static_assert(std::is_move_assignable_v<Request>);
    }

} // namespace

// ============================================================
// Entry point
// ============================================================

/**
 * @brief Runs all Request tests.
 *
 * Executes every test case defined in this header against a shared
 * TestContext and prints a summary for the
 * "kas::net::http::test_request::tests" suite.
 */
inline bool runTests()
{
    TestContext ctx;
    testDefaultConstructor(ctx);
    testParameterizedConstructor(ctx);
    testMethod(ctx);
    testMethodFluent(ctx);
    testTargetParsing(ctx);
    testQueryParams(ctx);
    testQueryParamsDuplicates(ctx);
    testQueryParamsEdgeCases(ctx);
    testQueryEdgeCasesExtended(ctx);
    testVersion(ctx);
    testVersionEdgeCases(ctx);
    testHeaders(ctx);
    testHeaderEmptyName(ctx);
    testBody(ctx);
    testConnectionInfo(ctx);
    testFluentChain(ctx);
    testMove(ctx);
    testCopy(ctx);
    testSelfAssignment(ctx);
    testHashTransparency(ctx);
    testHashCollisions(ctx);
    testTransparentLookup(ctx);
    testReturnTypes(ctx);

    return printSummary("kas::net::http::test_request::tests", ctx);
}

} // namespace kas::net::http::test_request::tests

#endif // KASLIBCXX_NET_HTTP_REQUEST_TESTS_H