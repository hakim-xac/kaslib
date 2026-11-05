/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_NET_GENERAL_TESTS_H
#define KASLIBCXX_NET_GENERAL_TESTS_H

#include "KAS/net/general.h"
#include "tests/tests.h"

#include <algorithm>
#include <arpa/inet.h>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <netinet/in.h>
#include <span>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <thread>
#include <tuple>
#include <unistd.h>
#include <vector>

namespace kas::net::test_general::tests {

namespace {
    using namespace kas::tests;

    // ------------------------------------------------------------
    // RAII helpers for socketpair / fd
    // ------------------------------------------------------------

    /// RAII guard closing both fds of a socketpair.
    struct FdPairGuard {
        int sv[2] { -1, -1 }; ///< Socketpair fds.
        ~FdPairGuard()
        {
            if (sv[0] >= 0)
                ::close(sv[0]);
            if (sv[1] >= 0)
                ::close(sv[1]);
        }
    };

    /// RAII guard closing a single fd.
    struct FdGuard {
        int fd { -1 }; ///< Managed fd.
        ~FdGuard()
        {
            if (fd >= 0)
                ::close(fd);
        }
    };

    /// Temporary file with the given content; removed in the destructor.
    struct TempFileGuard {
        std::filesystem::path path; ///< Path of the temporary file.
        ~TempFileGuard()
        {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
    };

    /// Builds a unique temporary file path for the current test.
    std::string makeTempFilePath()
    {
        static std::atomic<int> counter { 0 };
        const auto name = "kas_general_test_" + std::to_string(::getpid())
            + "_" + std::to_string(counter.fetch_add(1));
        return (std::filesystem::temp_directory_path() / name).string();
    }

    // ------------------------------------------------------------
    // sendBytes / readBytes / readExactBytes / readAllBytes
    // ------------------------------------------------------------

    /// Sending an empty span is a no-op and returns true.
    inline void testSendBytesEmpty(TestContext& ctx)
    {
        FdPairGuard p;
        KAS_TEST_CHECK_IMPL(ctx, ::socketpair(AF_UNIX, SOCK_STREAM, 0, p.sv) == 0);

        // Empty send is a no-op and returns true.
        const bool ok = kas::net::general::sendBytes(
            p.sv[0], std::span<const std::uint8_t> { });
        KAS_TEST_CHECK_IMPL(ctx, ok);
    }

    /// sendBytes followed by readBytes(payload.size()) round-trips the payload.
    inline void testSendAndReadBytes(TestContext& ctx)
    {
        FdPairGuard p;
        KAS_TEST_CHECK_IMPL(ctx, ::socketpair(AF_UNIX, SOCK_STREAM, 0, p.sv) == 0);

        const std::string payload = "hello, world";
        const std::span<const std::uint8_t> data {
            reinterpret_cast<const std::uint8_t*>(payload.data()),
            payload.size()
        };

        KAS_TEST_CHECK_IMPL(ctx, kas::net::general::sendBytes(p.sv[0], data));

        auto read_opt = kas::net::general::readBytes(p.sv[1], payload.size());
        KAS_TEST_CHECK_IMPL(ctx, read_opt.has_value());
        if (read_opt) {
            KAS_TEST_CHECK_IMPL(ctx, read_opt->size() == payload.size());
            KAS_TEST_CHECK_IMPL(ctx, std::ranges::equal(*read_opt, data));
        }
    }

    /// A 64 KiB payload sent in a thread is fully read by readExactBytes.
    inline void testSendBytesLarge(TestContext& ctx)
    {
        FdPairGuard p;
        KAS_TEST_CHECK_IMPL(ctx, ::socketpair(AF_UNIX, SOCK_STREAM, 0, p.sv) == 0);
        if (p.sv[0] < 0 || p.sv[1] < 0)
            return;

        kas::net::general::setReceiveTimeoutMs(p.sv[1], std::chrono::milliseconds { 5000 });

        std::string payload(64 * 1024, 'x');
        const std::span<const std::uint8_t> data {
            reinterpret_cast<const std::uint8_t*>(payload.data()),
            payload.size()
        };

        std::thread sender([&p, data] {
            std::ignore = kas::net::general::sendBytes(p.sv[0], data);
        });

        auto read_opt = kas::net::general::readExactBytes(p.sv[1], payload.size());
        sender.join();

        KAS_TEST_CHECK_IMPL(ctx, read_opt.has_value());
        if (read_opt) {
            KAS_TEST_CHECK_IMPL(ctx, read_opt->size() == payload.size());
        }
    }

    /// Two readExactBytes(3) calls split "abcdef" into "abc" and "def".
    inline void testReadExactBytes(TestContext& ctx)
    {
        FdPairGuard p;
        KAS_TEST_CHECK_IMPL(ctx, ::socketpair(AF_UNIX, SOCK_STREAM, 0, p.sv) == 0);

        const std::string payload = "abcdef";
        const std::span<const std::uint8_t> data {
            reinterpret_cast<const std::uint8_t*>(payload.data()),
            payload.size()
        };
        KAS_TEST_CHECK_IMPL(ctx, kas::net::general::sendBytes(p.sv[0], data));

        auto read_opt = kas::net::general::readExactBytes(p.sv[1], 3);
        KAS_TEST_CHECK_IMPL(ctx, read_opt.has_value());
        if (read_opt) {
            KAS_TEST_CHECK_IMPL(ctx, read_opt->size() == 3);
            KAS_TEST_CHECK_IMPL(ctx, std::ranges::equal(*read_opt, std::string_view { "abc" }));
        }

        // Read the remainder.
        auto rest_opt = kas::net::general::readExactBytes(p.sv[1], 3);
        KAS_TEST_CHECK_IMPL(ctx, rest_opt.has_value());
        if (rest_opt) {
            KAS_TEST_CHECK_IMPL(ctx, rest_opt->size() == 3);
            KAS_TEST_CHECK_IMPL(ctx, std::ranges::equal(*rest_opt, std::string_view { "def" }));
        }
    }

    /// EOF before enough bytes are read yields nullopt.
    inline void testReadExactBytesEof(TestContext& ctx)
    {
        FdPairGuard p;
        KAS_TEST_CHECK_IMPL(ctx, ::socketpair(AF_UNIX, SOCK_STREAM, 0, p.sv) == 0);

        // Write nothing and close the writing end -> EOF on read.
        ::close(p.sv[0]);
        p.sv[0] = -1;

        auto read_opt = kas::net::general::readExactBytes(p.sv[1], 10);
        // Result is empty -> nullopt.
        KAS_TEST_CHECK_IMPL(ctx, !read_opt.has_value());
    }

    /// readAllBytes collects everything until EOF.
    inline void testReadAllBytes(TestContext& ctx)
    {
        FdPairGuard p;
        KAS_TEST_CHECK_IMPL(ctx, ::socketpair(AF_UNIX, SOCK_STREAM, 0, p.sv) == 0);

        const std::string payload = "line1\nline2\nline3";
        const std::span<const std::uint8_t> data {
            reinterpret_cast<const std::uint8_t*>(payload.data()),
            payload.size()
        };
        KAS_TEST_CHECK_IMPL(ctx, kas::net::general::sendBytes(p.sv[0], data));

        // Close the writing end -> recv sees EOF after the data.
        ::close(p.sv[0]);
        p.sv[0] = -1;

        auto read_opt = kas::net::general::readAllBytes(p.sv[1]);
        KAS_TEST_CHECK_IMPL(ctx, read_opt.has_value());
        if (read_opt) {
            KAS_TEST_CHECK_IMPL(ctx, read_opt->size() == payload.size());
            KAS_TEST_CHECK_IMPL(ctx, std::ranges::equal(read_opt.value(), payload));
        }
    }

    /// query() sends the request and reads back the echoed response.
    inline void testQuery(TestContext& ctx)
    {
        FdPairGuard p;
        KAS_TEST_CHECK_IMPL(ctx, ::socketpair(AF_UNIX, SOCK_STREAM, 0, p.sv) == 0);
        if (p.sv[0] < 0 || p.sv[1] < 0)
            return;

        // Set a receive timeout on the client side so a hung echo cannot
        // block the test forever.
        kas::net::general::setReceiveTimeoutMs(p.sv[0], std::chrono::milliseconds { 2000 });

        const std::string request = "ping";
        const std::span<const std::uint8_t> req_data {
            reinterpret_cast<const std::uint8_t*>(request.data()),
            request.size()
        };

        std::thread echo([&p, &request] {
            // Read exactly request.size() bytes; readExactBytes loops on EINTR
            // and partial reads.
            auto in = kas::net::general::readExactBytes(p.sv[1], request.size());
            if (!in)
                return;

            std::ignore = kas::net::general::sendBytes(p.sv[1], std::span<const std::uint8_t> { in->data(), in->size() });
        });

        auto resp_opt = kas::net::general::query(p.sv[0], req_data, request.size());
        echo.join();

        KAS_TEST_CHECK_IMPL(ctx, resp_opt.has_value());
        if (resp_opt) {
            KAS_TEST_CHECK_IMPL(ctx, resp_opt->size() == request.size());
            KAS_TEST_CHECK_IMPL(ctx, std::ranges::equal(*resp_opt, req_data));
        }
    }

    // ------------------------------------------------------------
    // isSocketValid
    // ------------------------------------------------------------

    /// Open fds are valid; a closed fd is not.
    inline void testIsSocketValid(TestContext& ctx)
    {
        FdPairGuard p;
        KAS_TEST_CHECK_IMPL(ctx, ::socketpair(AF_UNIX, SOCK_STREAM, 0, p.sv) == 0);

        KAS_TEST_CHECK_IMPL(ctx, kas::net::general::isSocketValid(p.sv[0]));
        KAS_TEST_CHECK_IMPL(ctx, kas::net::general::isSocketValid(p.sv[1]));

        // Closed fd.
        ::close(p.sv[0]);
        p.sv[0] = -1;
        KAS_TEST_CHECK_IMPL(ctx, !kas::net::general::isSocketValid(p.sv[0]));
    }

    // ------------------------------------------------------------
    // timeouts
    // ------------------------------------------------------------

    /// Set/get round-trip for send and receive timeouts.
    inline void testReceiveSendTimeoutRoundTrip(TestContext& ctx)
    {
        FdPairGuard p;
        KAS_TEST_CHECK_IMPL(ctx, ::socketpair(AF_UNIX, SOCK_STREAM, 0, p.sv) == 0);

        using namespace std::chrono;

        kas::net::general::setReceiveTimeoutMs(p.sv[0], milliseconds { 1234 });
        kas::net::general::setSendTimeoutMs(p.sv[0], milliseconds { 567 });

        auto recv_opt = kas::net::general::getReceiveTimeoutMs(p.sv[0]);
        KAS_TEST_CHECK_IMPL(ctx, recv_opt.has_value());
        if (recv_opt)
            KAS_TEST_CHECK_IMPL(ctx, recv_opt->count() == 1234);

        auto send_opt = kas::net::general::getSendTimeoutMs(p.sv[0]);
        KAS_TEST_CHECK_IMPL(ctx, send_opt.has_value());
        if (send_opt)
            KAS_TEST_CHECK_IMPL(ctx, send_opt->count() == 567);
    }

    /// getSendTimeoutMs/getReceiveTimeoutMs on an invalid fd return nullopt.
    inline void testGetTimeoutOnInvalidSocket(TestContext& ctx)
    {
        // Closed fd -> getsockopt fails -> nullopt.
        FdGuard g;
        g.fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        KAS_TEST_CHECK_IMPL(ctx, g.fd >= 0);
        ::close(g.fd);
        g.fd = -1;

        // fd = -1.
        auto recv_opt = kas::net::general::getReceiveTimeoutMs(-1);
        KAS_TEST_CHECK_IMPL(ctx, !recv_opt.has_value());

        auto send_opt = kas::net::general::getSendTimeoutMs(-1);
        KAS_TEST_CHECK_IMPL(ctx, !send_opt.has_value());
    }

    // ------------------------------------------------------------
    // blocking / nonblocking
    // ------------------------------------------------------------

    /// setNoBlockingSocket sets O_NONBLOCK; setBlockingSocket clears it.
    inline void testNonBlockingRoundTrip(TestContext& ctx)
    {
        FdPairGuard p;
        KAS_TEST_CHECK_IMPL(ctx, ::socketpair(AF_UNIX, SOCK_STREAM, 0, p.sv) == 0);

        kas::net::general::setNoBlockingSocket(p.sv[0]);
        int flags = ::fcntl(p.sv[0], F_GETFL, 0);
        KAS_TEST_CHECK_IMPL(ctx, flags >= 0 && (flags & O_NONBLOCK));

        kas::net::general::setBlockingSocket(p.sv[0]);
        flags = ::fcntl(p.sv[0], F_GETFL, 0);
        KAS_TEST_CHECK_IMPL(ctx, flags >= 0 && !(flags & O_NONBLOCK));
    }

    // ------------------------------------------------------------
    // TCP keepalive (TCP socket only)
    // ------------------------------------------------------------

    /// setTcpKeepAlive enables SO_KEEPALIVE on a TCP socket.
    inline void testSetTcpKeepAlive(TestContext& ctx)
    {
        FdGuard g;
        g.fd = ::socket(AF_INET, SOCK_STREAM, 0);
        KAS_TEST_CHECK_IMPL(ctx, g.fd >= 0);

        // setsockopt(SO_KEEPALIVE/TCP_*) is valid on a non-listening TCP socket.
        kas::net::general::setTcpKeepAlive(g.fd, std::chrono::seconds { 30 });

        int ka = 0;
        socklen_t len = sizeof(ka);
        const int rc = ::getsockopt(g.fd, SOL_SOCKET, SO_KEEPALIVE, &ka, &len);
        KAS_TEST_CHECK_IMPL(ctx, rc == 0 && ka == 1);
    }

    // ------------------------------------------------------------
    // HTTP date / ETag
    // ------------------------------------------------------------

    /// formatHttpDate produces an IMF-fixdate string ending with " GMT".
    inline void testFormatHttpDate(TestContext& ctx)
    {
        TempFileGuard tf { makeTempFilePath() };
        {
            std::ofstream out(tf.path);
            out << "hello";
        }

        const auto ftime = std::filesystem::last_write_time(tf.path);
        const auto date = kas::net::general::formatHttpDate(ftime);

        // IMF-fixdate: "Sun, 06 Nov 1994 08:49:37 GMT" - at least 29 chars.
        KAS_TEST_CHECK_IMPL(ctx, date.size() == 29);
        KAS_TEST_CHECK_IMPL(ctx, date.substr(date.size() - 4) == " GMT");
        // Comma after the weekday, colons in the time.
        KAS_TEST_CHECK_IMPL(ctx, date[3] == ',');
        KAS_TEST_CHECK_IMPL(ctx, date.find(':') != std::string::npos);
    }

    /// generateETag is deterministic for unchanged files and changes when content changes.
    inline void testGenerateETag(TestContext& ctx)
    {
        TempFileGuard tf { makeTempFilePath() };
        {
            std::ofstream out(tf.path);
            out << "hello";
        }

        const auto etag1 = kas::net::general::generateETag(tf.path);
        KAS_TEST_CHECK_IMPL(ctx, !etag1.empty());

        // ETag is deterministic: two consecutive calls without modifying the
        // file yield the same value.
        const auto etag2 = kas::net::general::generateETag(tf.path);
        KAS_TEST_CHECK_IMPL(ctx, etag1 == etag2);

        // Changing the content changes the size -> changes the ETag.
        {
            std::ofstream out(tf.path, std::ios::app);
            out << " world";
        }
        const auto etag3 = kas::net::general::generateETag(tf.path);
        KAS_TEST_CHECK_IMPL(ctx, etag1 != etag3);
    }

    // ------------------------------------------------------------
    // keep-alive timeout
    // ------------------------------------------------------------

    /// getKeepAliveTimeoutS returns a positive duration.
    inline void testKeepAliveTimeout(TestContext& ctx)
    {
        const auto sec = kas::net::general::getKeepAliveTimeoutS();
        KAS_TEST_CHECK_IMPL(ctx, sec.count() > 0);
    }

} // namespace

// ============================================================
// Entry point
// ============================================================

/**
 * @brief Runs all general net-utility tests.
 *
 * Executes every test case defined in this header against a shared
 * TestContext and prints a summary for the
 * "kas::net::test_general::tests" suite.
 */
inline bool runTests()
{
    TestContext ctx;

    // socket I/O
    testSendBytesEmpty(ctx);
    testSendAndReadBytes(ctx);
    testSendBytesLarge(ctx);
    testReadExactBytes(ctx);
    testReadExactBytesEof(ctx);
    testReadAllBytes(ctx);
    testQuery(ctx);

    // validity
    testIsSocketValid(ctx);

    // timeouts
    testReceiveSendTimeoutRoundTrip(ctx);
    testGetTimeoutOnInvalidSocket(ctx);

    // blocking / nonblocking
    testNonBlockingRoundTrip(ctx);

    // TCP keepalive
    testSetTcpKeepAlive(ctx);

    // date / etag
    testFormatHttpDate(ctx);
    testGenerateETag(ctx);

    // keep-alive timeout
    testKeepAliveTimeout(ctx);

    return printSummary("kas::net::test_general::tests", ctx);
}

} // namespace kas::net::test_general::tests

#endif // KASLIBCXX_NET_GENERAL_TESTS_H