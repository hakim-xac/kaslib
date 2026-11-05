/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_NET_HTTP_SERVER_TESTS_H
#define KASLIBCXX_NET_HTTP_SERVER_TESTS_H

#include "KAS/net/general.h"
#include "KAS/net/http/ContentTypes.h"
#include "KAS/net/http/HttpRoute.h"
#include "KAS/net/http/HttpServer.h"
#include "KAS/net/http/Methods.h"
#include "KAS/net/http/ResponseTypes.h"
#include "tests/tests.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

namespace kas::net::http::test_server::tests {

namespace {
    using namespace kas::tests;
    using namespace kas::net::http::ContentTypes;
    using namespace kas::net::http::response_types;
    using kas::net::http::HttpRoute;
    using kas::net::http::HttpServer;
    using kas::net::http::Methods;

    // ------------------------------------------------------------
    // Unique AF_UNIX socket path for each test
    // ------------------------------------------------------------

    /// Builds a unique AF_UNIX socket path for the current test.
    std::string makeUniqueSockPath()
    {
        static std::atomic<int> counter { 0 };
        const auto name = "kas_http_test_" + std::to_string(::getpid())
            + "_" + std::to_string(counter.fetch_add(1));
        return (std::filesystem::temp_directory_path() / name).string();
    }

    /// RAII cleanup of the socket file.
    struct SockPathGuard {
        std::string path;
        ~SockPathGuard()
        {
            if (!path.empty())
                ::unlink(path.c_str());
        }
    };

    // ------------------------------------------------------------
    // Minimal HTTP client over AF_UNIX
    // ------------------------------------------------------------

    /// Connects to the server socket at @p sock_path; returns fd or -1 on error.
    int connectToServer(const std::string& sock_path)
    {
        const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0)
            return -1;

        sockaddr_un addr { };
        addr.sun_family = AF_UNIX;
        const auto n = std::min(sock_path.size(), sizeof(addr.sun_path) - 1);
        std::copy_n(sock_path.data(), n, addr.sun_path);

        if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            ::close(fd);
            return -1;
        }
        return fd;
    }

    /// Sends a raw request and reads the whole response until EOF.
    std::string sendAndReceive(int fd, std::string_view request)
    {
        if (!kas::net::general::sendBytes(
                fd,
                std::span<const std::uint8_t> {
                    reinterpret_cast<const std::uint8_t*>(request.data()),
                    request.size() }))
            return { };

        std::string response;
        char buf[4096];
        while (true) {
            const ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
            if (n <= 0)
                break;
            response.append(buf, static_cast<std::size_t>(n));
        }
        return response;
    }

    /// Checks whether @p response contains @p needle.
    bool responseContains(const std::string& response, std::string_view needle)
    {
        return response.find(needle) != std::string::npos;
    }

    // ============================================================
    // Tests
    // ============================================================

    // --------------------------------------------------------
    // 1. Constructor: the socket is created but not listening;
    //    isRunning() == false before start().
    // --------------------------------------------------------
    inline void testConstructor(TestContext& ctx)
    {
        const auto path = makeUniqueSockPath();
        SockPathGuard guard { path };

        HttpServer server { path, 2 };
        KAS_TEST_CHECK_IMPL(ctx, server.getSocketID() >= 0);
        KAS_TEST_CHECK_IMPL(ctx, !server.isRunning());
    }

    // --------------------------------------------------------
    // 2. stop() before start() is a no-op and does not fail.
    // --------------------------------------------------------
    inline void testStopBeforeStart(TestContext& ctx)
    {
        const auto path = makeUniqueSockPath();
        SockPathGuard guard { path };

        HttpServer server { path, 2 };
        server.stop();
        KAS_TEST_CHECK_IMPL(ctx, !server.isRunning());
    }

    // --------------------------------------------------------
    // 3. route(): registration and replacement of a route.
    // --------------------------------------------------------
    inline void testRouteRegistration(TestContext& ctx)
    {
        const auto path = makeUniqueSockPath();
        SockPathGuard guard { path };

        HttpServer server { path, 2 };
        server.route("/", HttpRoute { }.setMethod<Methods::GET>().setHandler([](const Request&, Response& resp) -> expected_string<void> {
            resp.setResponseCode<response_types::OK>()
                .setContentType<ContentTypes::TXT>()
                .setBody("ok");
            return { };
        }));

        // Re-registering the same path replaces the route and does not fail.
        server.route("/", HttpRoute { }.setMethod<Methods::POST>().setHandler([](const Request&, Response& resp) -> expected_string<void> {
            resp.setResponseCode<response_types::OK>()
                .setContentType<ContentTypes::TXT>()
                .setBody("replaced");
            return { };
        }));

        KAS_TEST_CHECK_IMPL(ctx, true);
    }

    // --------------------------------------------------------
    // 4. start() + stop(): basic lifecycle.
    // --------------------------------------------------------
    inline void testStartStop(TestContext& ctx)
    {
        const auto path = makeUniqueSockPath();
        SockPathGuard guard { path };

        HttpServer server { path, 2 };
        KAS_TEST_CHECK_IMPL(ctx, server.start());
        KAS_TEST_CHECK_IMPL(ctx, server.isRunning());

        server.stop();
        KAS_TEST_CHECK_IMPL(ctx, !server.isRunning());
    }

    // --------------------------------------------------------
    // 5. Double start(): the second call returns false.
    // --------------------------------------------------------
    inline void testDoubleStart(TestContext& ctx)
    {
        const auto path = makeUniqueSockPath();
        SockPathGuard guard { path };

        HttpServer server { path, 2 };
        KAS_TEST_CHECK_IMPL(ctx, server.start());
        KAS_TEST_CHECK_IMPL(ctx, !server.start());
        server.stop();
    }

    // --------------------------------------------------------
    // 6. start() -> stop() -> start(): restart works.
    // --------------------------------------------------------
    inline void testRestart(TestContext& ctx)
    {
        const auto path = makeUniqueSockPath();
        SockPathGuard guard { path };

        HttpServer server { path, 2 };
        KAS_TEST_CHECK_IMPL(ctx, server.start());
        server.stop();
        KAS_TEST_CHECK_IMPL(ctx, !server.isRunning());

        KAS_TEST_CHECK_IMPL(ctx, server.start());
        server.stop();
    }

    // --------------------------------------------------------
    // 7. Integration: 200 OK with a body.
    // --------------------------------------------------------
    inline void testGetOk(TestContext& ctx)
    {
        const auto path = makeUniqueSockPath();
        SockPathGuard guard { path };
        HttpServer server { path, 2 };
        server.route("/hello", HttpRoute { }.setMethod<Methods::GET>().setHandler([](const Request&, Response& resp) -> expected_string<void> {
            resp.setResponseCode<response_types::OK>()
                .setContentType<ContentTypes::TXT>()
                .setBody("Hello, World!");
            return { };
        }));

        KAS_TEST_CHECK_IMPL(ctx, server.start());

        const int fd = connectToServer(path);
        KAS_TEST_CHECK_IMPL(ctx, fd >= 0);

        if (fd >= 0) {
            const std::string req = "GET /hello HTTP/1.1\r\n"
                                    "Host: localhost\r\n"
                                    "Connection: close\r\n"
                                    "\r\n";
            const auto resp = sendAndReceive(fd, req);
            ::close(fd);

            KAS_TEST_CHECK_IMPL(ctx, responseContains(resp, "HTTP/1.1 200"));
            KAS_TEST_CHECK_IMPL(ctx, responseContains(resp, "Hello, World!"));
        }

        server.stop();
    }

    // --------------------------------------------------------
    // 8. Integration: 404 Not Found (no matching route).
    // --------------------------------------------------------
    inline void testNotFound(TestContext& ctx)
    {
        const auto path = makeUniqueSockPath();
        SockPathGuard guard { path };

        HttpServer server { path, 2 };

        KAS_TEST_CHECK_IMPL(ctx, server.start());

        const int fd = connectToServer(path);
        KAS_TEST_CHECK_IMPL(ctx, fd >= 0);

        if (fd >= 0) {
            const std::string req = "GET /missing HTTP/1.1\r\n"
                                    "Host: localhost\r\n"
                                    "Connection: close\r\n"
                                    "\r\n";
            const auto resp = sendAndReceive(fd, req);
            ::close(fd);

            KAS_TEST_CHECK_IMPL(ctx, responseContains(resp, "404"));
        }

        server.stop();
    }

    // --------------------------------------------------------
    // 9. Integration: 405 Method Not Allowed with an Allow header.
    // --------------------------------------------------------
    inline void testMethodNotAllowed(TestContext& ctx)
    {
        const auto path = makeUniqueSockPath();
        SockPathGuard guard { path };

        HttpServer server { path, 2 };
        server.route("/only-get", HttpRoute { }.setMethod<Methods::GET>().setHandler([](const Request&, Response& resp) -> expected_string<void> {
            resp.setResponseCode<response_types::OK>()
                .setBody("get");
            return { };
        }));

        KAS_TEST_CHECK_IMPL(ctx, server.start());

        const int fd = connectToServer(path);
        KAS_TEST_CHECK_IMPL(ctx, fd >= 0);

        if (fd >= 0) {
            const std::string req = "POST /only-get HTTP/1.1\r\n"
                                    "Host: localhost\r\n"
                                    "Content-Length: 0\r\n"
                                    "Connection: close\r\n"
                                    "\r\n";
            const auto resp = sendAndReceive(fd, req);
            ::close(fd);

            KAS_TEST_CHECK_IMPL(ctx, responseContains(resp, "405"));
            KAS_TEST_CHECK_IMPL(ctx, responseContains(resp, "Allow:"));
            KAS_TEST_CHECK_IMPL(ctx, responseContains(resp, "GET"));
        }

        server.stop();
    }

    // --------------------------------------------------------
    // 10. Integration: 500 Internal Server Error
    //     (route without a handler).
    // --------------------------------------------------------
    inline void testNoHandler(TestContext& ctx)
    {
        const auto path = makeUniqueSockPath();
        SockPathGuard guard { path };

        HttpServer server { path, 2 };
        server.route("/no-handler", HttpRoute { }.setMethod<Methods::GET>());

        KAS_TEST_CHECK_IMPL(ctx, server.start());

        const int fd = connectToServer(path);
        KAS_TEST_CHECK_IMPL(ctx, fd >= 0);

        if (fd >= 0) {
            const std::string req = "GET /no-handler HTTP/1.1\r\n"
                                    "Host: localhost\r\n"
                                    "Connection: close\r\n"
                                    "\r\n";
            const auto resp = sendAndReceive(fd, req);
            ::close(fd);

            KAS_TEST_CHECK_IMPL(ctx, responseContains(resp, "500"));
        }

        server.stop();
    }

    // --------------------------------------------------------
    // 11. Integration: 400 Bad Request
    //     (handler returned an error).
    // --------------------------------------------------------
    inline void testHandlerError(TestContext& ctx)
    {
        const auto path = makeUniqueSockPath();
        SockPathGuard guard { path };

        HttpServer server { path, 2 };
        server.route("/bad", HttpRoute { }.setMethod<Methods::POST>().setHandler([](const Request&, Response&) -> expected_string<void> {
            return unexpected_string { "empty body" };
        }));

        KAS_TEST_CHECK_IMPL(ctx, server.start());

        const int fd = connectToServer(path);
        KAS_TEST_CHECK_IMPL(ctx, fd >= 0);

        if (fd >= 0) {
            const std::string req = "POST /bad HTTP/1.1\r\n"
                                    "Host: localhost\r\n"
                                    "Content-Length: 0\r\n"
                                    "Connection: close\r\n"
                                    "\r\n";
            const auto resp = sendAndReceive(fd, req);
            ::close(fd);

            KAS_TEST_CHECK_IMPL(ctx, responseContains(resp, "400"));
            KAS_TEST_CHECK_IMPL(ctx, responseContains(resp, "empty body"));
        }

        server.stop();
    }

    // --------------------------------------------------------
    // 12. Integration: the request body reaches the handler
    //     (Content-Length).
    // --------------------------------------------------------
    inline void testRequestBody(TestContext& ctx)
    {
        const auto path = makeUniqueSockPath();
        SockPathGuard guard { path };

        HttpServer server { path, 2 };
        server.route("/echo", HttpRoute { }.setMethod<Methods::POST>().setHandler([](const Request& req, Response& resp) -> expected_string<void> {
            resp.setResponseCode<response_types::OK>()
                .setContentType<ContentTypes::TXT>()
                .setBody(req.getBody());
            return { };
        }));

        KAS_TEST_CHECK_IMPL(ctx, server.start());

        const int fd = connectToServer(path);
        KAS_TEST_CHECK_IMPL(ctx, fd >= 0);

        if (fd >= 0) {
            const std::string body = "hello body";
            const std::string req = "POST /echo HTTP/1.1\r\n"
                                    "Host: localhost\r\n"
                                    "Content-Length: "
                + std::to_string(body.size()) + "\r\n"
                                                "Connection: close\r\n"
                                                "\r\n"
                + body;
            const auto resp = sendAndReceive(fd, req);
            ::close(fd);

            KAS_TEST_CHECK_IMPL(ctx, responseContains(resp, "200"));
            KAS_TEST_CHECK_IMPL(ctx, responseContains(resp, "hello body"));
        }

        server.stop();
    }

    // --------------------------------------------------------
    // 13. Integration: keep-alive - two requests on one connection.
    // --------------------------------------------------------
    inline void testKeepAlive(TestContext& ctx)
    {
        const auto path = makeUniqueSockPath();
        SockPathGuard guard { path };

        HttpServer server { path, 2 };
        server.route("/ping", HttpRoute { }.setMethod<Methods::GET>().setHandler([](const Request&, Response& resp) -> expected_string<void> {
            resp.setResponseCode<response_types::OK>()
                .setContentType<ContentTypes::TXT>()
                .setBody("pong");
            return { };
        }));

        KAS_TEST_CHECK_IMPL(ctx, server.start());

        const int fd = connectToServer(path);
        KAS_TEST_CHECK_IMPL(ctx, fd >= 0);

        if (fd >= 0) {
            const std::string req1 = "GET /ping HTTP/1.1\r\n"
                                     "Host: localhost\r\n"
                                     "\r\n";
            KAS_TEST_CHECK_IMPL(ctx, kas::net::general::sendBytes(fd, std::span<const std::uint8_t> { reinterpret_cast<const std::uint8_t*>(req1.data()), req1.size() }));

            std::string buf;
            char tmp[4096];
            while (buf.find("pong") == std::string::npos) {
                const ssize_t n = ::recv(fd, tmp, sizeof(tmp), 0);
                if (n <= 0)
                    break;
                buf.append(tmp, static_cast<std::size_t>(n));
            }
            KAS_TEST_CHECK_IMPL(ctx, responseContains(buf, "200"));
            KAS_TEST_CHECK_IMPL(ctx, responseContains(buf, "pong"));

            const std::string req2 = "GET /ping HTTP/1.1\r\n"
                                     "Host: localhost\r\n"
                                     "Connection: close\r\n"
                                     "\r\n";
            KAS_TEST_CHECK_IMPL(ctx, kas::net::general::sendBytes(fd, std::span<const std::uint8_t> { reinterpret_cast<const std::uint8_t*>(req2.data()), req2.size() }));

            buf.clear();
            while (true) {
                const ssize_t n = ::recv(fd, tmp, sizeof(tmp), 0);
                if (n <= 0)
                    break;
                buf.append(tmp, static_cast<std::size_t>(n));
            }
            ::close(fd);

            KAS_TEST_CHECK_IMPL(ctx, responseContains(buf, "200"));
            KAS_TEST_CHECK_IMPL(ctx, responseContains(buf, "pong"));
        }

        server.stop();
    }

    // --------------------------------------------------------
    // 14. Integration: parallel clients (pool exercise).
    // --------------------------------------------------------
    inline void testParallelClients(TestContext& ctx)
    {
        const auto path = makeUniqueSockPath();
        SockPathGuard guard { path };

        HttpServer server { path, 4 };
        server.route("/work", HttpRoute { }.setMethod<Methods::GET>().setHandler([](const Request&, Response& resp) -> expected_string<void> {
            resp.setResponseCode<response_types::OK>()
                .setContentType<ContentTypes::TXT>()
                .setBody("done");
            return { };
        }));

        KAS_TEST_CHECK_IMPL(ctx, server.start());

        constexpr int kClients = 4;
        std::atomic<int> ok_count { 0 };

        std::vector<std::thread> clients;
        clients.reserve(kClients);
        for (int i = 0; i < kClients; ++i) {
            clients.emplace_back([&path, &ok_count] {
                const int fd = connectToServer(path);
                if (fd < 0)
                    return;

                const std::string req = "GET /work HTTP/1.1\r\n"
                                        "Host: localhost\r\n"
                                        "Connection: close\r\n"
                                        "\r\n";
                const auto resp = sendAndReceive(fd, req);
                ::close(fd);

                if (responseContains(resp, "200") && responseContains(resp, "done"))
                    ok_count.fetch_add(1);
            });
        }
        for (auto& t : clients)
            t.join();

        KAS_TEST_CHECK_IMPL(ctx, ok_count.load() == kClients);

        server.stop();
    }

    // --------------------------------------------------------
    // 15. Integration: 206 Partial Content for a single range.
    // --------------------------------------------------------
    inline void testRangeSingle(TestContext& ctx)
    {
        const auto path = makeUniqueSockPath();
        SockPathGuard guard { path };

        HttpServer server { path, 2 };
        server.route("/data", HttpRoute { }.setMethod<Methods::GET>().setHandler([](const Request&, Response& resp) -> expected_string<void> {
            resp.setResponseCode<response_types::OK>()
                .setContentType<ContentTypes::TXT>()
                .setBody("0123456789");
            return { };
        }));

        KAS_TEST_CHECK_IMPL(ctx, server.start());

        const int fd = connectToServer(path);
        KAS_TEST_CHECK_IMPL(ctx, fd >= 0);

        if (fd >= 0) {
            const std::string req = "GET /data HTTP/1.1\r\n"
                                    "Host: localhost\r\n"
                                    "Range: bytes=0-2\r\n"
                                    "Connection: close\r\n"
                                    "\r\n";
            const auto resp = sendAndReceive(fd, req);
            ::close(fd);

            KAS_TEST_CHECK_IMPL(ctx, responseContains(resp, "206"));
            KAS_TEST_CHECK_IMPL(ctx, responseContains(resp, "Content-Range: bytes 0-2/10"));
            KAS_TEST_CHECK_IMPL(ctx, responseContains(resp, "Accept-Ranges: bytes"));
            KAS_TEST_CHECK_IMPL(ctx, responseContains(resp, "012"));
            // The body must not contain "3456789".
            KAS_TEST_CHECK_IMPL(ctx, !responseContains(resp, "3456789"));
        }

        server.stop();
    }

    // --------------------------------------------------------
    // 16. Integration: 206 for "bytes=N-" (to the end).
    // --------------------------------------------------------
    inline void testRangeOpenEnd(TestContext& ctx)
    {
        const auto path = makeUniqueSockPath();
        SockPathGuard guard { path };

        HttpServer server { path, 2 };
        server.route("/data", HttpRoute { }.setMethod<Methods::GET>().setHandler([](const Request&, Response& resp) -> expected_string<void> {
            resp.setResponseCode<response_types::OK>()
                .setContentType<ContentTypes::TXT>()
                .setBody("0123456789");
            return { };
        }));

        KAS_TEST_CHECK_IMPL(ctx, server.start());

        const int fd = connectToServer(path);
        KAS_TEST_CHECK_IMPL(ctx, fd >= 0);

        if (fd >= 0) {
            const std::string req = "GET /data HTTP/1.1\r\n"
                                    "Host: localhost\r\n"
                                    "Range: bytes=7-\r\n"
                                    "Connection: close\r\n"
                                    "\r\n";
            const auto resp = sendAndReceive(fd, req);
            ::close(fd);

            KAS_TEST_CHECK_IMPL(ctx, responseContains(resp, "206"));
            KAS_TEST_CHECK_IMPL(ctx, responseContains(resp, "Content-Range: bytes 7-9/10"));
            KAS_TEST_CHECK_IMPL(ctx, responseContains(resp, "789"));
        }

        server.stop();
    }

    // --------------------------------------------------------
    // 17. Integration: 206 for "bytes=-N" (suffix).
    // --------------------------------------------------------
    inline void testRangeSuffix(TestContext& ctx)
    {
        const auto path = makeUniqueSockPath();
        SockPathGuard guard { path };

        HttpServer server { path, 2 };
        server.route("/data", HttpRoute { }.setMethod<Methods::GET>().setHandler([](const Request&, Response& resp) -> expected_string<void> {
            resp.setResponseCode<response_types::OK>()
                .setContentType<ContentTypes::TXT>()
                .setBody("0123456789");
            return { };
        }));

        KAS_TEST_CHECK_IMPL(ctx, server.start());

        const int fd = connectToServer(path);
        KAS_TEST_CHECK_IMPL(ctx, fd >= 0);

        if (fd >= 0) {
            const std::string req = "GET /data HTTP/1.1\r\n"
                                    "Host: localhost\r\n"
                                    "Range: bytes=-3\r\n"
                                    "Connection: close\r\n"
                                    "\r\n";
            const auto resp = sendAndReceive(fd, req);
            ::close(fd);

            KAS_TEST_CHECK_IMPL(ctx, responseContains(resp, "206"));
            KAS_TEST_CHECK_IMPL(ctx, responseContains(resp, "Content-Range: bytes 7-9/10"));
            KAS_TEST_CHECK_IMPL(ctx, responseContains(resp, "789"));
        }

        server.stop();
    }

    // --------------------------------------------------------
    // 18. Integration: multipart/byteranges for several ranges.
    // --------------------------------------------------------
    inline void testRangeMultiple(TestContext& ctx)
    {
        const auto path = makeUniqueSockPath();
        SockPathGuard guard { path };

        HttpServer server { path, 2 };
        server.route("/data", HttpRoute { }.setMethod<Methods::GET>().setHandler([](const Request&, Response& resp) -> expected_string<void> {
            resp.setResponseCode<response_types::OK>()
                .setContentType<ContentTypes::TXT>()
                .setBody("0123456789");
            return { };
        }));

        KAS_TEST_CHECK_IMPL(ctx, server.start());

        const int fd = connectToServer(path);
        KAS_TEST_CHECK_IMPL(ctx, fd >= 0);

        if (fd >= 0) {
            const std::string req = "GET /data HTTP/1.1\r\n"
                                    "Host: localhost\r\n"
                                    "Range: bytes=0-2,7-9\r\n"
                                    "Connection: close\r\n"
                                    "\r\n";
            const auto resp = sendAndReceive(fd, req);
            ::close(fd);

            KAS_TEST_CHECK_IMPL(ctx, responseContains(resp, "206"));
            KAS_TEST_CHECK_IMPL(ctx,
                responseContains(resp, "Content-Type: multipart/byteranges; boundary="));
            KAS_TEST_CHECK_IMPL(ctx, responseContains(resp, "Content-Range: bytes 0-2/10"));
            KAS_TEST_CHECK_IMPL(ctx, responseContains(resp, "Content-Range: bytes 7-9/10"));
            KAS_TEST_CHECK_IMPL(ctx, responseContains(resp, "012"));
            KAS_TEST_CHECK_IMPL(ctx, responseContains(resp, "789"));
        }

        server.stop();
    }

    // --------------------------------------------------------
    // 19. Integration: 416 Range Not Satisfiable.
    // --------------------------------------------------------
    inline void testRangeNotSatisfiable(TestContext& ctx)
    {
        const auto path = makeUniqueSockPath();
        SockPathGuard guard { path };

        HttpServer server { path, 2 };
        server.route("/data", HttpRoute { }.setMethod<Methods::GET>().setHandler([](const Request&, Response& resp) -> expected_string<void> {
            resp.setResponseCode<response_types::OK>()
                .setContentType<ContentTypes::TXT>()
                .setBody("0123456789");
            return { };
        }));

        KAS_TEST_CHECK_IMPL(ctx, server.start());

        const int fd = connectToServer(path);
        KAS_TEST_CHECK_IMPL(ctx, fd >= 0);

        if (fd >= 0) {
            const std::string req = "GET /data HTTP/1.1\r\n"
                                    "Host: localhost\r\n"
                                    "Range: bytes=100-200\r\n"
                                    "Connection: close\r\n"
                                    "\r\n";
            const auto resp = sendAndReceive(fd, req);
            ::close(fd);

            KAS_TEST_CHECK_IMPL(ctx, responseContains(resp, "416"));
            KAS_TEST_CHECK_IMPL(ctx, responseContains(resp, "Content-Range: bytes */10"));
        }

        server.stop();
    }

    // --------------------------------------------------------
    // 20. Integration: full coverage -> 200 with the full body.
    // --------------------------------------------------------
    inline void testRangeFullCoverage(TestContext& ctx)
    {
        const auto path = makeUniqueSockPath();
        SockPathGuard guard { path };

        HttpServer server { path, 2 };
        server.route("/data", HttpRoute { }.setMethod<Methods::GET>().setHandler([](const Request&, Response& resp) -> expected_string<void> {
            resp.setResponseCode<response_types::OK>()
                .setContentType<ContentTypes::TXT>()
                .setBody("0123456789");
            return { };
        }));

        KAS_TEST_CHECK_IMPL(ctx, server.start());

        const int fd = connectToServer(path);
        KAS_TEST_CHECK_IMPL(ctx, fd >= 0);

        if (fd >= 0) {
            const std::string req = "GET /data HTTP/1.1\r\n"
                                    "Host: localhost\r\n"
                                    "Range: bytes=0-9\r\n"
                                    "Connection: close\r\n"
                                    "\r\n";
            const auto resp = sendAndReceive(fd, req);
            ::close(fd);

            // Per RFC 9110 section 14.2, the server may respond with the full body (200).
            KAS_TEST_CHECK_IMPL(ctx, responseContains(resp, "HTTP/1.1 200"));
            KAS_TEST_CHECK_IMPL(ctx, responseContains(resp, "0123456789"));
        }

        server.stop();
    }

    // --------------------------------------------------------
    // 21. Integration: unsupported unit -> 200 (ignored).
    // --------------------------------------------------------
    inline void testRangeInvalidUnit(TestContext& ctx)
    {
        const auto path = makeUniqueSockPath();
        SockPathGuard guard { path };

        HttpServer server { path, 2 };
        server.route("/data", HttpRoute { }.setMethod<Methods::GET>().setHandler([](const Request&, Response& resp) -> expected_string<void> {
            resp.setResponseCode<response_types::OK>()
                .setContentType<ContentTypes::TXT>()
                .setBody("0123456789");
            return { };
        }));

        KAS_TEST_CHECK_IMPL(ctx, server.start());

        const int fd = connectToServer(path);
        KAS_TEST_CHECK_IMPL(ctx, fd >= 0);

        if (fd >= 0) {
            const std::string req = "GET /data HTTP/1.1\r\n"
                                    "Host: localhost\r\n"
                                    "Range: items=0-1\r\n"
                                    "Connection: close\r\n"
                                    "\r\n";
            const auto resp = sendAndReceive(fd, req);
            ::close(fd);

            KAS_TEST_CHECK_IMPL(ctx, responseContains(resp, "HTTP/1.1 200"));
            KAS_TEST_CHECK_IMPL(ctx, responseContains(resp, "0123456789"));
        }

        server.stop();
    }

    // --------------------------------------------------------
    // 22. Integration: malformed syntax -> 200 (ignored).
    // --------------------------------------------------------
    inline void testRangeInvalidSyntax(TestContext& ctx)
    {
        const auto path = makeUniqueSockPath();
        SockPathGuard guard { path };

        HttpServer server { path, 2 };
        server.route("/data", HttpRoute { }.setMethod<Methods::GET>().setHandler([](const Request&, Response& resp) -> expected_string<void> {
            resp.setResponseCode<response_types::OK>()
                .setContentType<ContentTypes::TXT>()
                .setBody("0123456789");
            return { };
        }));

        KAS_TEST_CHECK_IMPL(ctx, server.start());

        const int fd = connectToServer(path);
        KAS_TEST_CHECK_IMPL(ctx, fd >= 0);

        if (fd >= 0) {
            const std::string req = "GET /data HTTP/1.1\r\n"
                                    "Host: localhost\r\n"
                                    "Range: bytes=5-3\r\n" // start > end -> invalid
                                    "Connection: close\r\n"
                                    "\r\n";
            const auto resp = sendAndReceive(fd, req);
            ::close(fd);

            KAS_TEST_CHECK_IMPL(ctx, responseContains(resp, "HTTP/1.1 200"));
            KAS_TEST_CHECK_IMPL(ctx, responseContains(resp, "0123456789"));
        }

        server.stop();
    }

    // --------------------------------------------------------
    // 23. Integration: Accept-Ranges: bytes on regular 200 responses.
    // --------------------------------------------------------
    inline void testAcceptRanges(TestContext& ctx)
    {
        const auto path = makeUniqueSockPath();
        SockPathGuard guard { path };

        HttpServer server { path, 2 };
        server.route("/data", HttpRoute { }.setMethod<Methods::GET>().setHandler([](const Request&, Response& resp) -> expected_string<void> {
            resp.setResponseCode<response_types::OK>()
                .setContentType<ContentTypes::TXT>()
                .setBody("0123456789");
            return { };
        }));

        KAS_TEST_CHECK_IMPL(ctx, server.start());

        const int fd = connectToServer(path);
        KAS_TEST_CHECK_IMPL(ctx, fd >= 0);

        if (fd >= 0) {
            const std::string req = "GET /data HTTP/1.1\r\n"
                                    "Host: localhost\r\n"
                                    "Connection: close\r\n"
                                    "\r\n";
            const auto resp = sendAndReceive(fd, req);
            ::close(fd);

            KAS_TEST_CHECK_IMPL(ctx, responseContains(resp, "HTTP/1.1 200"));
            KAS_TEST_CHECK_IMPL(ctx, responseContains(resp, "Accept-Ranges: bytes"));
        }

        server.stop();
    }

    // --------------------------------------------------------
    // 24. Integration: keep-alive idle timeout closes the connection.
    // --------------------------------------------------------
    inline void testKeepAliveTimeout(TestContext& ctx)
    {
        const auto path = makeUniqueSockPath();
        SockPathGuard guard { path };

        HttpServer server { path, 2 };
        server.setClientReceiveTimeoutMs(std::chrono::milliseconds { 100 });
        server.route("/ping", HttpRoute { }.setMethod<Methods::GET>().setHandler([](const Request&, Response& resp) -> expected_string<void> {
            resp.setResponseCode<response_types::OK>()
                .setContentType<ContentTypes::TXT>()
                .setBody("pong");
            return { };
        }));

        KAS_TEST_CHECK_IMPL(ctx, server.start());

        const int fd = connectToServer(path);
        KAS_TEST_CHECK_IMPL(ctx, fd >= 0);

        if (fd >= 0) {
            const std::string req = "GET /ping HTTP/1.1\r\n"
                                    "Host: localhost\r\n"
                                    "\r\n";
            KAS_TEST_CHECK_IMPL(ctx, kas::net::general::sendBytes(fd, std::span<const std::uint8_t> { reinterpret_cast<const std::uint8_t*>(req.data()), req.size() }));

            // --- read the response fully ---
            std::string response;
            char buf[4096];
            while (response.find("\r\n\r\n") == std::string::npos) {
                const ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
                KAS_TEST_CHECK_IMPL(ctx, n > 0);
                if (n <= 0) {
                    ::close(fd);
                    server.stop();
                    return;
                }
                response.append(buf, static_cast<std::size_t>(n));
            }
            const auto hdr_end = response.find("\r\n\r\n") + 4;
            if (const auto cl_pos = response.find("Content-Length:");
                cl_pos != std::string::npos) {
                const auto cl = std::stoul(response.substr(cl_pos + 15));
                while (response.size() - hdr_end < cl) {
                    const ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
                    if (n <= 0)
                        break;
                    response.append(buf, static_cast<std::size_t>(n));
                }
            }

            // --- wait with margin and drain to EOF ---
            std::this_thread::sleep_for(std::chrono::milliseconds { 500 });

            ssize_t n2 = 0;
            do {
                n2 = ::recv(fd, buf, sizeof(buf), 0);
            } while (n2 > 0);

            if (n2 == -1) {
                std::cerr << "recv errno=" << errno
                          << " (" << std::strerror(errno) << ")\n";
            }
            KAS_TEST_CHECK_IMPL(ctx, n2 == 0); // EOF

            ::close(fd);
        }

        server.stop();
    }

} // namespace

// ============================================================
// Entry point
// ============================================================

/**
 * @brief Runs all HttpServer tests.
 *
 * Executes every test case defined in this header against a shared
 * TestContext and prints a summary for the
 * "kas::net::http::test_server::tests" suite.
 */
inline bool runTests()
{
    TestContext ctx;

    testConstructor(ctx);
    testStopBeforeStart(ctx);
    testRouteRegistration(ctx);
    testStartStop(ctx);
    testDoubleStart(ctx);
    testRestart(ctx);
    testGetOk(ctx);
    testNotFound(ctx);
    testMethodNotAllowed(ctx);
    testNoHandler(ctx);
    testHandlerError(ctx);
    testRequestBody(ctx);
    testKeepAlive(ctx);
    testParallelClients(ctx);

    // Range
    testRangeSingle(ctx);
    testRangeOpenEnd(ctx);
    testRangeSuffix(ctx);
    testRangeMultiple(ctx);
    testRangeNotSatisfiable(ctx);
    testRangeFullCoverage(ctx);
    testRangeInvalidUnit(ctx);
    testRangeInvalidSyntax(ctx);
    testAcceptRanges(ctx);

    testKeepAliveTimeout(ctx);

    return printSummary("kas::net::http::test_server::tests", ctx);
}

} // namespace kas::net::http::test_server::tests

#endif // KASLIBCXX_NET_HTTP_SERVER_TESTS_H