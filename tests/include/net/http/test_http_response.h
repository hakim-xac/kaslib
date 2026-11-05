/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_NET_HTTP_HTTP_RESPONSE_TESTS_H
#define KASLIBCXX_NET_HTTP_HTTP_RESPONSE_TESTS_H

#include "KAS/net/http/Response.h"
#include "tests/tests.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <unistd.h>

namespace kas::net::http::http_response::tests {

namespace {
    using namespace kas::tests;
    using kas::net::http::Response;
    using kas::net::http::ContentTypes::CSV;
    using kas::net::http::ContentTypes::HTML;
    using kas::net::http::ContentTypes::JSON;
    using kas::net::http::ContentTypes::SVG;
    using kas::net::http::ContentTypes::TXT;
    using kas::net::http::response_types::BadGateway;
    using kas::net::http::response_types::BadRequest;
    using kas::net::http::response_types::Conflict;
    using kas::net::http::response_types::Created;
    using kas::net::http::response_types::Forbidden;
    using kas::net::http::response_types::InternalServerError;
    using kas::net::http::response_types::MethodNotAllowed;
    using kas::net::http::response_types::NoContent;
    using kas::net::http::response_types::NotFound;
    using kas::net::http::response_types::OK;

    // ------------------------------------------------------------
    // Constructor / default state
    // ------------------------------------------------------------

    /// Default-constructed response: 200 OK, empty content type, headers, and body.
    inline void testDefaultConstructor(TestContext& ctx)
    {
        Response r;
        KAS_TEST_CHECK_IMPL(ctx, r.getStatusCode() == 200);
        KAS_TEST_CHECK_IMPL(ctx, r.getStatusText() == "200 OK");
        KAS_TEST_CHECK_IMPL(ctx, r.getReasonPhrase() == "OK");
        KAS_TEST_CHECK_IMPL(ctx, r.getContentType().empty());
        KAS_TEST_CHECK_IMPL(ctx, r.getHeaders().empty());
        KAS_TEST_CHECK_IMPL(ctx, r.getBody().empty());
        KAS_TEST_CHECK_IMPL(ctx, r.getBodySize() == 0);
    }

    // ------------------------------------------------------------
    // status: setResponseCode<T>
    // ------------------------------------------------------------

    /// setResponseCode<T> updates the numeric code, full status line, and reason phrase.
    inline void testStatusCode(TestContext& ctx)
    {
        Response r;

        r.setResponseCode<NotFound>();
        KAS_TEST_CHECK_IMPL(ctx, r.getStatusCode() == 404);
        KAS_TEST_CHECK_IMPL(ctx, r.getStatusText() == "404 Not Found");
        KAS_TEST_CHECK_IMPL(ctx, r.getReasonPhrase() == "Not Found");

        r.setResponseCode<MethodNotAllowed>();
        KAS_TEST_CHECK_IMPL(ctx, r.getStatusCode() == 405);
        KAS_TEST_CHECK_IMPL(ctx, r.getStatusText() == "405 Method Not Allowed");
        KAS_TEST_CHECK_IMPL(ctx, r.getReasonPhrase() == "Method Not Allowed");

        r.setResponseCode<Created>();
        KAS_TEST_CHECK_IMPL(ctx, r.getStatusCode() == 201);
        KAS_TEST_CHECK_IMPL(ctx, r.getReasonPhrase() == "Created");

        r.setResponseCode<NoContent>();
        KAS_TEST_CHECK_IMPL(ctx, r.getStatusCode() == 204);
        KAS_TEST_CHECK_IMPL(ctx, r.getReasonPhrase() == "No Content");

        r.setResponseCode<BadRequest>();
        KAS_TEST_CHECK_IMPL(ctx, r.getStatusCode() == 400);

        r.setResponseCode<Forbidden>();
        KAS_TEST_CHECK_IMPL(ctx, r.getStatusCode() == 403);

        r.setResponseCode<Conflict>();
        KAS_TEST_CHECK_IMPL(ctx, r.getStatusCode() == 409);

        r.setResponseCode<InternalServerError>();
        KAS_TEST_CHECK_IMPL(ctx, r.getStatusCode() == 500);

        r.setResponseCode<BadGateway>();
        KAS_TEST_CHECK_IMPL(ctx, r.getStatusCode() == 502);

        r.setResponseCode<OK>();
        KAS_TEST_CHECK_IMPL(ctx, r.getStatusCode() == 200);
    }

    /// Rvalue fluent chain: setResponseCode<T>() && -> setContentType<T>() && -> setBody() &&.
    inline void testStatusCodeFluentRvalue(TestContext& ctx)
    {
        Response r = Response { }
                         .setResponseCode<Created>()
                         .setContentType<JSON>()
                         .setBody("{}");

        KAS_TEST_CHECK_IMPL(ctx, r.getStatusCode() == 201);
        KAS_TEST_CHECK_IMPL(ctx, r.getStatusText() == "201 Created");
        KAS_TEST_CHECK_IMPL(ctx, r.getContentType() == "application/json");
        KAS_TEST_CHECK_IMPL(ctx, r.getBody() == "{}");
    }

    // ------------------------------------------------------------
    // content type
    // ------------------------------------------------------------

    /// setContentType<T> sets the Content-Type header; header lookup is case-insensitive.
    inline void testContentType(TestContext& ctx)
    {
        Response r;

        r.setContentType<TXT>();
        KAS_TEST_CHECK_IMPL(ctx, r.getContentType() == "text/plain");

        r.setContentType<HTML>();
        KAS_TEST_CHECK_IMPL(ctx, r.getContentType() == "text/html");

        r.setContentType<JSON>();
        KAS_TEST_CHECK_IMPL(ctx, r.getContentType() == "application/json");

        r.setContentType<CSV>();
        KAS_TEST_CHECK_IMPL(ctx, r.getContentType() == "text/csv");

        r.setContentType<SVG>();
        KAS_TEST_CHECK_IMPL(ctx, r.getContentType() == "image/svg+xml");

        // Case-insensitive access to Content-Type as a header.
        auto h = r.getHeader("content-type");
        KAS_TEST_CHECK_IMPL(ctx, h.has_value() && *h == "image/svg+xml");
    }

    /// Re-setting the content type overwrites the single Content-Type header.
    inline void testContentTypeOverwrite(TestContext& ctx)
    {
        Response r;
        r.setContentType<TXT>();
        r.setContentType<JSON>();
        KAS_TEST_CHECK_IMPL(ctx, r.getContentType() == "application/json");
        KAS_TEST_CHECK_IMPL(ctx, r.getHeaders().size() == 1);
    }

    // ------------------------------------------------------------
    // headers
    // ------------------------------------------------------------

    /// Header set/get/has/overwrite, case-insensitivity, transparent lookup, empty values.
    inline void testHeaders(TestContext& ctx)
    {
        Response r;
        KAS_TEST_CHECK_IMPL(ctx, r.getHeaders().empty());
        KAS_TEST_CHECK_IMPL(ctx, !r.hasHeader("Content-Type"));

        r.setHeader("X-Test", "1");
        KAS_TEST_CHECK_IMPL(ctx, r.hasHeader("X-Test"));
        KAS_TEST_CHECK_IMPL(ctx, r.hasHeader("x-test"));
        KAS_TEST_CHECK_IMPL(ctx, r.hasHeader("X-TEST"));

        {
            auto v = r.getHeader("X-Test");
            KAS_TEST_CHECK_IMPL(ctx, v.has_value() && *v == "1");
        }
        {
            auto v = r.getHeader("x-test");
            KAS_TEST_CHECK_IMPL(ctx, v.has_value() && *v == "1");
        }

        // Overwrite (case-insensitive).
        r.setHeader("x-test", "2");
        KAS_TEST_CHECK_IMPL(ctx, r.getHeaders().size() == 1);
        {
            auto v = r.getHeader("X-Test");
            KAS_TEST_CHECK_IMPL(ctx, v.has_value() && *v == "2");
        }

        // Several distinct headers.
        r.setHeader("Accept", "*/*");
        r.setHeader("X-Custom", "abc");
        KAS_TEST_CHECK_IMPL(ctx, r.getHeaders().size() == 3);

        // Transparent lookup by string_view.
        std::string_view name = "accept";
        auto v = r.getHeader(name);
        KAS_TEST_CHECK_IMPL(ctx, v.has_value() && *v == "*/*");

        // Missing header.
        KAS_TEST_CHECK_IMPL(ctx, !r.hasHeader("X-Missing"));
        KAS_TEST_CHECK_IMPL(ctx, !r.getHeader("X-Missing").has_value());

        // Empty value.
        r.setHeader("X-Empty", "");
        {
            auto e = r.getHeader("x-empty");
            KAS_TEST_CHECK_IMPL(ctx, e.has_value() && e->empty());
        }
    }

    /// removeHeader is case-insensitive and is a no-op for a missing header.
    inline void testRemoveHeader(TestContext& ctx)
    {
        Response r;
        r.setHeader("X-Test", "1");
        r.setHeader("X-Other", "2");
        KAS_TEST_CHECK_IMPL(ctx, r.getHeaders().size() == 2);

        r.removeHeader("x-test");
        KAS_TEST_CHECK_IMPL(ctx, !r.hasHeader("X-Test"));
        KAS_TEST_CHECK_IMPL(ctx, r.hasHeader("X-Other"));
        KAS_TEST_CHECK_IMPL(ctx, r.getHeaders().size() == 1);

        // Removing a missing header does not fail.
        r.removeHeader("X-Missing");
        KAS_TEST_CHECK_IMPL(ctx, r.getHeaders().size() == 1);
    }

    // ------------------------------------------------------------
    // body
    // ------------------------------------------------------------

    /// setBody/getBody/getBodySize, including replacement, empty, and binary data.
    inline void testBody(TestContext& ctx)
    {
        Response r;
        KAS_TEST_CHECK_IMPL(ctx, r.getBody().empty());
        KAS_TEST_CHECK_IMPL(ctx, r.getBodySize() == 0);

        r.setBody("hello");
        KAS_TEST_CHECK_IMPL(ctx, r.getBody() == "hello");
        KAS_TEST_CHECK_IMPL(ctx, r.getBodySize() == 5);

        // Replacement.
        r.setBody("world!");
        KAS_TEST_CHECK_IMPL(ctx, r.getBody() == "world!");
        KAS_TEST_CHECK_IMPL(ctx, r.getBodySize() == 6);

        // Empty.
        r.setBody("");
        KAS_TEST_CHECK_IMPL(ctx, r.getBody().empty());
        KAS_TEST_CHECK_IMPL(ctx, r.getBodySize() == 0);

        // Binary data with embedded NUL bytes.
        std::string binary("a\0b", 3);
        r.setBody(binary);
        KAS_TEST_CHECK_IMPL(ctx, r.getBodySize() == 3);
        KAS_TEST_CHECK_IMPL(ctx, r.getBody() == binary);
    }

    // ------------------------------------------------------------
    // fluent (lvalue + rvalue)
    // ------------------------------------------------------------

    /// Lvalue fluent chain returns a reference to the same object.
    inline void testFluentLvalue(TestContext& ctx)
    {
        Response r;
        Response& ref = r
                            .setResponseCode<MethodNotAllowed>()
                            .setContentType<TXT>()
                            .setHeader("X-Request-Id", "abc")
                            .setBody("nope");

        KAS_TEST_CHECK_IMPL(ctx, &ref == &r);
        KAS_TEST_CHECK_IMPL(ctx, r.getStatusCode() == 405);
        KAS_TEST_CHECK_IMPL(ctx, r.getContentType() == "text/plain");
        {
            auto v = r.getHeader("x-request-id");
            KAS_TEST_CHECK_IMPL(ctx, v.has_value() && *v == "abc");
        }
        KAS_TEST_CHECK_IMPL(ctx, r.getBody() == "nope");
    }

    // ------------------------------------------------------------
    // move
    // ------------------------------------------------------------

    /// Move construction preserves all response state; the source is left moved-from.
    inline void testMove(TestContext& ctx)
    {
        Response src;
        src.setResponseCode<NotFound>()
            .setContentType<JSON>()
            .setHeader("X-Test", "1")
            .setBody("payload");

        Response dst = std::move(src);
        KAS_TEST_CHECK_IMPL(ctx, dst.getStatusCode() == 404);
        KAS_TEST_CHECK_IMPL(ctx, dst.getStatusText() == "404 Not Found");
        KAS_TEST_CHECK_IMPL(ctx, dst.getContentType() == "application/json");
        {
            auto h = dst.getHeader("x-test");
            KAS_TEST_CHECK_IMPL(ctx, h.has_value() && *h == "1");
        }
        KAS_TEST_CHECK_IMPL(ctx, dst.getBody() == "payload");

        // Source is moved-from; not UB, but typically empty.
        KAS_TEST_CHECK_IMPL(ctx, src.getBody().empty());
    }

    // ------------------------------------------------------------
    // serialize
    // ------------------------------------------------------------

    /// serialize() starts with the correct HTTP/1.1 status line.
    inline void testSerializeStatusLine(TestContext& ctx)
    {
        Response r;
        r.setResponseCode<OK>();
        const std::string wire = r.serialize();
        KAS_TEST_CHECK_IMPL(ctx, wire.rfind("HTTP/1.1 200 OK\r\n", 0) == 0);

        Response r2;
        r2.setResponseCode<MethodNotAllowed>();
        const std::string wire2 = r2.serialize();
        KAS_TEST_CHECK_IMPL(ctx, wire2.rfind("HTTP/1.1 405 Method Not Allowed\r\n", 0) == 0);
    }

    /// serialize() adds Content-Length from the logical body size.
    inline void testSerializeContentLength(TestContext& ctx)
    {
        Response r;
        r.setBody("hi");
        const std::string wire = r.serialize();
        KAS_TEST_CHECK_IMPL(ctx, wire.find("Content-Length: 2\r\n") != std::string::npos);

        // Empty body -> 0.
        Response r2;
        const std::string wire2 = r2.serialize();
        KAS_TEST_CHECK_IMPL(ctx, wire2.find("Content-Length: 0\r\n") != std::string::npos);
    }

    /// serialize() emits the Content-Type header when set.
    inline void testSerializeContentType(TestContext& ctx)
    {
        Response r;
        r.setContentType<JSON>();
        const std::string wire = r.serialize();
        KAS_TEST_CHECK_IMPL(ctx, wire.find("Content-Type: application/json\r\n") != std::string::npos);
    }

    /// serialize() adds a Date header in IMF-fixdate form (ending with " GMT").
    inline void testSerializeDate(TestContext& ctx)
    {
        Response r;
        const std::string wire = r.serialize();
        KAS_TEST_CHECK_IMPL(ctx, wire.find("Date: ") != std::string::npos);
        // IMF-fixdate ends with " GMT".
        const auto dpos = wire.find("Date: ");
        KAS_TEST_CHECK_IMPL(ctx, dpos != std::string::npos);
        const auto crlf = wire.find("\r\n", dpos);
        KAS_TEST_CHECK_IMPL(ctx, crlf != std::string::npos);
        const std::string date_line = wire.substr(dpos, crlf - dpos);
        KAS_TEST_CHECK_IMPL(ctx, date_line.size() > 6 && date_line.substr(date_line.size() - 4) == " GMT");
    }

    /// serialize() contains status line, headers, and ends with the empty-line separator, but no body.
    inline void testSerializeHeadersAndBody(TestContext& ctx)
    {
        Response r;
        r.setResponseCode<Created>()
            .setContentType<TXT>()
            .setHeader("X-Test", "1")
            .setBody("hello");

        const std::string wire = r.serialize();

        // Status line.
        KAS_TEST_CHECK_IMPL(ctx, wire.rfind("HTTP/1.1 201 Created\r\n", 0) == 0);

        // Headers.
        KAS_TEST_CHECK_IMPL(ctx, wire.find("X-Test: 1\r\n") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, wire.find("Content-Type: text/plain\r\n") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, wire.find("Content-Length: 5\r\n") != std::string::npos);

        // serialize() no longer contains the body; it ends with "\r\n\r\n".
        KAS_TEST_CHECK_IMPL(ctx, wire.size() >= 4);
        KAS_TEST_CHECK_IMPL(ctx, wire.compare(wire.size() - 4, 4, "\r\n\r\n") == 0);

        // Body must not appear in serialize().
        KAS_TEST_CHECK_IMPL(ctx, wire.find("hello") == std::string::npos);
    }

    /// sendTo() writes headers followed by the body.
    inline void testSendToWritesBody(TestContext& ctx)
    {
        int sv[2] { };
        KAS_TEST_CHECK_IMPL(ctx, ::socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

        Response r;
        r.setResponseCode<OK>()
            .setContentType<TXT>()
            .setBody("hello");

        const bool sent = r.sendTo(sv[0]).has_value();
        KAS_TEST_CHECK_IMPL(ctx, sent);

        // Close the writing end so recv sees EOF after the body.
        ::close(sv[0]);

        std::string buf;
        char tmp[256];
        while (true) {
            const ssize_t n = ::recv(sv[1], tmp, sizeof(tmp), 0);
            if (n <= 0)
                break;
            buf.append(tmp, static_cast<std::size_t>(n));
        }
        ::close(sv[1]);

        // Headers.
        KAS_TEST_CHECK_IMPL(ctx, buf.rfind("HTTP/1.1 200 OK\r\n", 0) == 0);
        KAS_TEST_CHECK_IMPL(ctx, buf.find("Content-Length: 5\r\n") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, buf.find("\r\n\r\nhello") != std::string::npos);
    }

    /// sendTo(fd, skip_body=true) writes only headers (for HEAD responses).
    inline void testSendToSkipBody(TestContext& ctx)
    {
        int sv[2] { };
        KAS_TEST_CHECK_IMPL(ctx, ::socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

        Response r;
        r.setResponseCode<OK>()
            .setContentType<TXT>()
            .setBody("hello");

        // HEAD: headers only.
        KAS_TEST_CHECK_IMPL(ctx, r.sendTo(sv[0], /*skip_body=*/true).has_value());
        ::close(sv[0]);

        std::string buf;
        char tmp[256];
        while (true) {
            const ssize_t n = ::recv(sv[1], tmp, sizeof(tmp), 0);
            if (n <= 0)
                break;
            buf.append(tmp, static_cast<std::size_t>(n));
        }
        ::close(sv[1]);

        KAS_TEST_CHECK_IMPL(ctx, buf.rfind("HTTP/1.1 200 OK\r\n", 0) == 0);
        KAS_TEST_CHECK_IMPL(ctx, buf.find("Content-Length: 5\r\n") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, buf.find("hello") == std::string::npos); // no body
    }

    /// serialize() of an empty body still ends with the "\r\n\r\n" separator.
    inline void testSerializeEmptyBodyHasDoubleCrlf(TestContext& ctx)
    {
        Response r;
        r.setResponseCode<NoContent>();
        const std::string wire = r.serialize();
        // "\r\n\r\n" separates headers from the empty body; the string ends with it.
        KAS_TEST_CHECK_IMPL(ctx, wire.size() >= 4);
        KAS_TEST_CHECK_IMPL(ctx, wire.compare(wire.size() - 4, 4, "\r\n\r\n") == 0);
    }

    // ------------------------------------------------------------
    // header hash transparency (case-insensitive)
    // ------------------------------------------------------------

    /// Setting the same header with different casing overwrites instead of duplicating.
    inline void testHashTransparency(TestContext& ctx)
    {
        Response r;
        r.setHeader("Content-Type", "text/html");
        r.setHeader("content-type", "text/html"); // overwrite, not a duplicate
        r.setHeader("CONTENT-TYPE", "text/html"); // overwrite, not a duplicate
        KAS_TEST_CHECK_IMPL(ctx, r.getHeaders().size() == 1);
        KAS_TEST_CHECK_IMPL(ctx, r.getContentType() == "text/html");
    }

} // namespace

// ============================================================
// Entry point
// ============================================================

/**
 * @brief Runs all HttpResponse tests.
 *
 * Executes every test case defined in this header against a shared
 * TestContext and prints a summary for the
 * "kas::net::http::http_response::tests" suite.
 */
inline bool runTests()
{
    TestContext ctx;

    // constructor / basics
    testDefaultConstructor(ctx);

    // status
    testStatusCode(ctx);
    testStatusCodeFluentRvalue(ctx);

    // content-type
    testContentType(ctx);
    testContentTypeOverwrite(ctx);

    // headers
    testHeaders(ctx);
    testRemoveHeader(ctx);

    // body
    testBody(ctx);

    // fluent
    testFluentLvalue(ctx);

    // move
    testMove(ctx);

    // serialize
    testSerializeStatusLine(ctx);
    testSerializeContentLength(ctx);
    testSerializeContentType(ctx);
    testSerializeDate(ctx);
    testSerializeHeadersAndBody(ctx);
    testSerializeEmptyBodyHasDoubleCrlf(ctx);

    // sendTo
    testSendToWritesBody(ctx);
    testSendToSkipBody(ctx);

    // hash
    testHashTransparency(ctx);

    return printSummary("kas::net::http::http_response::tests", ctx);
}

} // namespace kas::net::http::http_response::tests

#endif // KASLIBCXX_NET_HTTP_HTTP_RESPONSE_TESTS_H