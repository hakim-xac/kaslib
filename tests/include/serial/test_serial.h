/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_SERIAL_TEST_SERIAL_TESTS_H
#define KASLIBCXX_SERIAL_TEST_SERIAL_TESTS_H

#include "KAS/serial/Serial.h"
#include "tests/tests.h"

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace kas::serial::test_serial::tests {

namespace {
    using namespace kas::tests;
    using kas::serial::Serial;

    // ============================================================
    // Helpers
    // ============================================================

    /// RAII pty master + slave path (/dev/pts/N).
    struct PtyPair {
        int master { -1 };
        std::string slave_path;

        ~PtyPair()
        {
            if (master >= 0)
                ::close(master);
        }
    };

    bool createPty(PtyPair& p)
    {
        p.master = ::posix_openpt(O_RDWR | O_NOCTTY);
        if (p.master < 0)
            return false;
        if (::grantpt(p.master) != 0 || ::unlockpt(p.master) != 0) {
            ::close(p.master);
            p.master = -1;
            return false;
        }
        const char* name = ::ptsname(p.master);
        if (!name) {
            ::close(p.master);
            p.master = -1;
            return false;
        }
        p.slave_path = name;
        return true;
    }

    /// Converts a string to a byte span.
    std::span<const std::uint8_t> asBytes(std::string_view s) noexcept
    {
        return { reinterpret_cast<const std::uint8_t*>(s.data()), s.size() };
    }

    // ============================================================
    // Compile-time checks
    // ============================================================
    void runCompileTimeChecks()
    {
        static_assert(!std::is_copy_constructible_v<Serial>);
        static_assert(!std::is_copy_assignable_v<Serial>);
        static_assert(std::is_move_constructible_v<Serial>);
        static_assert(std::is_move_assignable_v<Serial>);
        static_assert(!std::is_default_constructible_v<Serial>);
    }

    // ============================================================
    // 1. Empty port name -> error
    // ============================================================
    void testEmptyPortName(TestContext& ctx)
    {
        auto s = Serial::make(structs::PortName { "" },
            structs::BaudRate { enums::BaudRate::B_9600 });
        KAS_TEST_CHECK_IMPL(ctx, !s.has_value());
        KAS_TEST_CHECK_IMPL(ctx, s.has_error());
        KAS_TEST_CHECK_IMPL(ctx, !s.error().empty());
    }

    // ============================================================
    // 2. Non-existent device -> error, message mentions open()
    // ============================================================
    void testNonexistentDevice(TestContext& ctx)
    {
        auto s = Serial::make(structs::PortName { "/dev/does-not-exist-kaslib" },
            structs::BaudRate { enums::BaudRate::B_9600 });
        KAS_TEST_CHECK_IMPL(ctx, !s.has_value());
        KAS_TEST_CHECK_IMPL(ctx, s.has_error());
        KAS_TEST_CHECK_IMPL(ctx, s.error().find("open") != std::string::npos);
    }

    // ============================================================
    // 3. Regular file (not a tty) -> error, message mentions tcgetattr
    // ============================================================
    void testRegularFile(TestContext& ctx)
    {
        auto s = Serial::make(structs::PortName { "/dev/null" },
            structs::BaudRate { enums::BaudRate::B_9600 });
        KAS_TEST_CHECK_IMPL(ctx, !s.has_value());
        KAS_TEST_CHECK_IMPL(ctx, s.has_error());
        KAS_TEST_CHECK_IMPL(ctx, s.error().find("tcgetattr") != std::string::npos);
    }

    // ============================================================
    // 4. hardware_flow_control + rts_enabled -> error
    // ============================================================
    void testConflictingRtsConfig(TestContext& ctx)
    {
        structs::SerialConfig cfg { };
        cfg.hardware_flow_control = true;
        cfg.rts_enabled = true;

        auto s = Serial::make(structs::PortName { "/dev/null" },
            structs::BaudRate { enums::BaudRate::B_9600 }, cfg);
        KAS_TEST_CHECK_IMPL(ctx, !s.has_value());
        KAS_TEST_CHECK_IMPL(ctx, s.has_error());
        KAS_TEST_CHECK_IMPL(ctx,
            s.error().find("hardware_flow_control") != std::string::npos);
    }

    // ============================================================
    // 5. hardware_flow_control + rts_invert -> error
    // ============================================================
    void testConflictingRtsInvertConfig(TestContext& ctx)
    {
        structs::SerialConfig cfg { };
        cfg.hardware_flow_control = true;
        cfg.rts_invert = true;

        auto s = Serial::make(structs::PortName { "/dev/null" },
            structs::BaudRate { enums::BaudRate::B_9600 }, cfg);
        KAS_TEST_CHECK_IMPL(ctx, !s.has_value());
        KAS_TEST_CHECK_IMPL(ctx, s.has_error());
        KAS_TEST_CHECK_IMPL(ctx,
            s.error().find("hardware_flow_control") != std::string::npos);
    }

    // ============================================================
    // 6. make() does not throw on expected errors
    // ============================================================
    void testMakeDoesNotThrowOnExpectedErrors(TestContext& ctx)
    {
        bool threw = false;
        try {
            auto s = Serial::make(structs::PortName { "" },
                structs::BaudRate { enums::BaudRate::B_9600 });
            KAS_TEST_CHECK_IMPL(ctx, !s.has_value());
        } catch (...) {
            threw = true;
        }
        KAS_TEST_CHECK_IMPL(ctx, !threw);
    }

    // ============================================================
    // 7. Default config does not trip the validator
    // ============================================================
    void testDefaultConfigDoesNotTripValidator(TestContext& ctx)
    {
        structs::SerialConfig cfg { };
        auto s = Serial::make(structs::PortName { "/dev/does-not-exist-kaslib" },
            structs::BaudRate { enums::BaudRate::B_115200 }, cfg);
        KAS_TEST_CHECK_IMPL(ctx, !s.has_value());
        KAS_TEST_CHECK_IMPL(ctx, s.error().find("open") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx,
            s.error().find("hardware_flow_control") == std::string::npos);
    }

    // ============================================================
    // 8. Move construction of an error expected is a no-op
    // ============================================================
    void testMoveOfErrorExpected(TestContext& ctx)
    {
        auto a = Serial::make(structs::PortName { "" },
            structs::BaudRate { enums::BaudRate::B_9600 });
        auto b = std::move(a);
        KAS_TEST_CHECK_IMPL(ctx, !b.has_value());
        KAS_TEST_CHECK_IMPL(ctx, b.has_error());
    }

    // ============================================================
    // 9. make() succeeds on a real tty (pty slave)
    // ============================================================
    void testMakeOnPtySucceeds(TestContext& ctx)
    {
        PtyPair p;
        KAS_TEST_REQUIRE_IMPL(ctx, createPty(p));

        auto s = Serial::make(structs::PortName { p.slave_path },
            structs::BaudRate { enums::BaudRate::B_9600 });
        KAS_TEST_CHECK_IMPL(ctx, s.has_value());
        if (!s)
            std::fprintf(stderr, "make failed: %s\n", s.error().c_str());
    }

    // ============================================================
    // 10. Accessors return the requested values
    // ============================================================
    void testAccessors(TestContext& ctx)
    {
        PtyPair p;
        KAS_TEST_REQUIRE_IMPL(ctx, createPty(p));

        structs::SerialConfig cfg { };
        cfg.raw_mode = true;

        auto s = Serial::make(structs::PortName { p.slave_path },
            structs::BaudRate { enums::BaudRate::B_115200 }, cfg);
        KAS_TEST_REQUIRE_IMPL(ctx, s.has_value());

        KAS_TEST_CHECK_IMPL(ctx, s->getPort() == p.slave_path);
        KAS_TEST_CHECK_IMPL(ctx, s->getBaudrate() == enums::BaudRate::B_115200);
        KAS_TEST_CHECK_IMPL(ctx, s->getSerialConfig().raw_mode == true);
    }

    // ============================================================
    // 11. write(string_view) sends bytes; they arrive on master
    // ============================================================
    void testWriteSendsBytes(TestContext& ctx)
    {
        PtyPair p;
        KAS_TEST_REQUIRE_IMPL(ctx, createPty(p));

        auto s = Serial::make(structs::PortName { p.slave_path },
            structs::BaudRate { enums::BaudRate::B_115200 });
        KAS_TEST_REQUIRE_IMPL(ctx, s.has_value());

        const std::string payload = "hello";
        KAS_TEST_CHECK_IMPL(ctx, s->write(payload));

        char buf[64] { };
        const ssize_t rn = ::read(p.master, buf, sizeof(buf));
        KAS_TEST_CHECK_IMPL(ctx, rn == static_cast<ssize_t>(payload.size()));

        const std::string_view received { buf, static_cast<std::size_t>(rn) };
        KAS_TEST_CHECK_IMPL(ctx, received == payload);
    }

    // ============================================================
    // 12. write(span) overload
    // ============================================================
    void testWriteSpan(TestContext& ctx)
    {
        PtyPair p;
        KAS_TEST_REQUIRE_IMPL(ctx, createPty(p));

        auto s = Serial::make(structs::PortName { p.slave_path },
            structs::BaudRate { enums::BaudRate::B_115200 });
        KAS_TEST_REQUIRE_IMPL(ctx, s.has_value());

        const std::vector<std::uint8_t> payload { 'a', 'b', 'c' };
        KAS_TEST_CHECK_IMPL(ctx, s->write(std::span<const std::uint8_t> { payload }));

        char buf[64] { };
        const ssize_t rn = ::read(p.master, buf, sizeof(buf));
        KAS_TEST_CHECK_IMPL(ctx, rn == 3);

        const std::string_view received { buf, 3 };
        KAS_TEST_CHECK_IMPL(ctx, received == "abc");
    }

    // ============================================================
    // 13. write(empty) is a no-op and returns true
    // ============================================================
    void testWriteEmptySpan(TestContext& ctx)
    {
        PtyPair p;
        KAS_TEST_REQUIRE_IMPL(ctx, createPty(p));

        auto s = Serial::make(structs::PortName { p.slave_path },
            structs::BaudRate { enums::BaudRate::B_115200 });
        KAS_TEST_REQUIRE_IMPL(ctx, s.has_value());

        KAS_TEST_CHECK_IMPL(ctx, s->write(std::span<const std::uint8_t> { }));
    }

    // ============================================================
    // 14. read() receives bytes written on the master side
    // ============================================================
    void testReadReceivesBytes(TestContext& ctx)
    {
        PtyPair p;
        KAS_TEST_REQUIRE_IMPL(ctx, createPty(p));

        auto s = Serial::make(structs::PortName { p.slave_path },
            structs::BaudRate { enums::BaudRate::B_115200 });
        KAS_TEST_REQUIRE_IMPL(ctx, s.has_value());

        const std::string payload = "world";
        KAS_TEST_CHECK_IMPL(ctx,
            ::write(p.master, payload.data(), payload.size())
                == static_cast<ssize_t>(payload.size()));

        auto r = s->read(payload.size());
        KAS_TEST_CHECK_IMPL(ctx, r.has_value());
        if (r) {
            KAS_TEST_CHECK_IMPL(ctx, r->size() == payload.size());
            KAS_TEST_CHECK_IMPL(ctx, std::ranges::equal(*r, asBytes(payload)));
        }
    }

    // ============================================================
    // 15. read(0) reads everything available
    // ============================================================
    void testReadZeroReadsAllAvailable(TestContext& ctx)
    {
        PtyPair p;
        KAS_TEST_REQUIRE_IMPL(ctx, createPty(p));

        auto s = Serial::make(structs::PortName { p.slave_path },
            structs::BaudRate { enums::BaudRate::B_115200 });
        KAS_TEST_REQUIRE_IMPL(ctx, s.has_value());

        s->setTimeout(std::chrono::milliseconds { 100 });

        const std::string payload = "abc";
        KAS_TEST_CHECK_IMPL(ctx,
            ::write(p.master, payload.data(), payload.size())
                == static_cast<ssize_t>(payload.size()));

        auto r = s->read(0);
        KAS_TEST_CHECK_IMPL(ctx, r.has_value());
        if (r) {
            KAS_TEST_CHECK_IMPL(ctx, r->size() == payload.size());
            KAS_TEST_CHECK_IMPL(ctx, std::ranges::equal(*r, asBytes(payload)));
        }
    }

    // ============================================================
    // 16. read() on EOF returns nullopt
    // ============================================================
    void testReadOnEof(TestContext& ctx)
    {
        PtyPair p;
        KAS_TEST_REQUIRE_IMPL(ctx, createPty(p));

        auto s = Serial::make(structs::PortName { p.slave_path },
            structs::BaudRate { enums::BaudRate::B_115200 });
        KAS_TEST_REQUIRE_IMPL(ctx, s.has_value());

        s->setTimeout(std::chrono::milliseconds { 100 });

        ::close(p.master);
        p.master = -1;

        auto r = s->read(64);
        KAS_TEST_CHECK_IMPL(ctx, !r.has_value());
    }

    // ============================================================
    // 17. readExact() reads exactly N bytes
    // ============================================================
    void testReadExact(TestContext& ctx)
    {
        PtyPair p;
        KAS_TEST_REQUIRE_IMPL(ctx, createPty(p));

        auto s = Serial::make(structs::PortName { p.slave_path },
            structs::BaudRate { enums::BaudRate::B_115200 });
        KAS_TEST_REQUIRE_IMPL(ctx, s.has_value());

        const std::string payload = "abcdef";
        KAS_TEST_CHECK_IMPL(ctx,
            ::write(p.master, payload.data(), payload.size())
                == static_cast<ssize_t>(payload.size()));

        auto r = s->readExact(3);
        KAS_TEST_CHECK_IMPL(ctx, r.has_value());
        if (r) {
            KAS_TEST_CHECK_IMPL(ctx, r->size() == 3);
            KAS_TEST_CHECK_IMPL(ctx, std::ranges::equal(*r, asBytes("abc")));
        }

        auto r2 = s->readExact(3);
        KAS_TEST_CHECK_IMPL(ctx, r2.has_value());
        if (r2) {
            KAS_TEST_CHECK_IMPL(ctx, r2->size() == 3);
            KAS_TEST_CHECK_IMPL(ctx, std::ranges::equal(*r2, asBytes("def")));
        }
    }

    // ============================================================
    // 18. readExact(0) returns empty vector (not nullopt)
    // ============================================================
    void testReadExactZero(TestContext& ctx)
    {
        PtyPair p;
        KAS_TEST_REQUIRE_IMPL(ctx, createPty(p));

        auto s = Serial::make(structs::PortName { p.slave_path },
            structs::BaudRate { enums::BaudRate::B_115200 });
        KAS_TEST_REQUIRE_IMPL(ctx, s.has_value());

        auto r = s->readExact(0);
        KAS_TEST_CHECK_IMPL(ctx, r.has_value());
        KAS_TEST_CHECK_IMPL(ctx, r && r->empty());
    }

    // ============================================================
    // 19. readExact() on EOF mid-stream returns nullopt
    // ============================================================
    void testReadExactEofMidStream(TestContext& ctx)
    {
        PtyPair p;
        KAS_TEST_REQUIRE_IMPL(ctx, createPty(p));

        auto s = Serial::make(structs::PortName { p.slave_path },
            structs::BaudRate { enums::BaudRate::B_115200 });
        KAS_TEST_REQUIRE_IMPL(ctx, s.has_value());

        s->setTimeout(std::chrono::milliseconds { 100 });

        KAS_TEST_CHECK_IMPL(ctx, ::write(p.master, "ab", 2) == 2);
        ::close(p.master);
        p.master = -1;

        auto r = s->readExact(5);
        KAS_TEST_CHECK_IMPL(ctx, !r.has_value());
    }

    // ============================================================
    // 20. query(): send + receive echo round-trip
    // ============================================================
    void testQueryRoundTrip(TestContext& ctx)
    {
        PtyPair p;
        KAS_TEST_REQUIRE_IMPL(ctx, createPty(p));

        auto s = Serial::make(structs::PortName { p.slave_path },
            structs::BaudRate { enums::BaudRate::B_115200 });
        KAS_TEST_REQUIRE_IMPL(ctx, s.has_value());

        s->setTimeout(std::chrono::milliseconds { 2000 });

        const std::string request = "ping";

        // Echo master-side: read the request, write it back.
        std::thread echo([&p, &request] {
            char buf[64] { };
            const ssize_t rn = ::read(p.master, buf, request.size());
            if (rn > 0)
                (void)::write(p.master, buf, static_cast<std::size_t>(rn));
        });

        auto resp = s->query(asBytes(request), request.size());
        echo.join();

        KAS_TEST_CHECK_IMPL(ctx, resp.has_value());
        if (resp) {
            KAS_TEST_CHECK_IMPL(ctx, resp->size() == request.size());
            KAS_TEST_CHECK_IMPL(ctx, std::ranges::equal(*resp, asBytes(request)));
        }
    }

    // ============================================================
    // 21. query() times out when nothing responds
    // ============================================================
    void testQueryTimeout(TestContext& ctx)
    {
        PtyPair p;
        KAS_TEST_REQUIRE_IMPL(ctx, createPty(p));

        auto s = Serial::make(structs::PortName { p.slave_path },
            structs::BaudRate { enums::BaudRate::B_115200 });
        KAS_TEST_REQUIRE_IMPL(ctx, s.has_value());

        s->setTimeout(std::chrono::milliseconds { 50 });

        auto resp = s->query(asBytes("ping"), 4);
        KAS_TEST_CHECK_IMPL(ctx, !resp.has_value());
        KAS_TEST_CHECK_IMPL(ctx, !resp.error().empty());
    }

    // ============================================================
    // 22. waitReadable(): true when data is present
    // ============================================================
    void testWaitReadableTrue(TestContext& ctx)
    {
        PtyPair p;
        KAS_TEST_REQUIRE_IMPL(ctx, createPty(p));

        auto s = Serial::make(structs::PortName { p.slave_path },
            structs::BaudRate { enums::BaudRate::B_115200 });
        KAS_TEST_REQUIRE_IMPL(ctx, s.has_value());

        KAS_TEST_CHECK_IMPL(ctx, ::write(p.master, "x", 1) == 1);
        KAS_TEST_CHECK_IMPL(ctx,
            s->waitReadable(std::chrono::milliseconds { 2000 }));
    }

    // ============================================================
    // 23. waitReadable(): false on timeout (no data)
    // ============================================================
    void testWaitReadableTimeout(TestContext& ctx)
    {
        PtyPair p;
        KAS_TEST_REQUIRE_IMPL(ctx, createPty(p));

        auto s = Serial::make(structs::PortName { p.slave_path },
            structs::BaudRate { enums::BaudRate::B_115200 });
        KAS_TEST_REQUIRE_IMPL(ctx, s.has_value());

        KAS_TEST_CHECK_IMPL(ctx,
            !s->waitReadable(std::chrono::milliseconds { 50 }));
    }

    // ============================================================
    // 24. waitReadable(): zero timeout returns immediately
    // ============================================================
    void testWaitReadableZero(TestContext& ctx)
    {
        PtyPair p;
        KAS_TEST_REQUIRE_IMPL(ctx, createPty(p));

        auto s = Serial::make(structs::PortName { p.slave_path },
            structs::BaudRate { enums::BaudRate::B_115200 });
        KAS_TEST_REQUIRE_IMPL(ctx, s.has_value());

        // No data — immediate false.
        KAS_TEST_CHECK_IMPL(ctx,
            !s->waitReadable(std::chrono::milliseconds { 0 }));

        // Data present — immediate true.
        KAS_TEST_CHECK_IMPL(ctx, ::write(p.master, "y", 1) == 1);
        KAS_TEST_CHECK_IMPL(ctx,
            s->waitReadable(std::chrono::milliseconds { 0 }));
    }

    // ============================================================
    // 25. setTimeout() does not throw and is chainable
    // ============================================================
    void testSetTimeoutChainable(TestContext& ctx)
    {
        PtyPair p;
        KAS_TEST_REQUIRE_IMPL(ctx, createPty(p));

        auto s = Serial::make(structs::PortName { p.slave_path },
            structs::BaudRate { enums::BaudRate::B_115200 });
        KAS_TEST_REQUIRE_IMPL(ctx, s.has_value());

        KAS_TEST_CHECK_IMPL(ctx,
            s->setTimeout(std::chrono::milliseconds { 500 })
                .write("ok"));
    }

    // ============================================================
    // 26. Mixed read/write round-trip
    // ============================================================
    void testReadWriteRoundTrip(TestContext& ctx)
    {
        PtyPair p;
        KAS_TEST_REQUIRE_IMPL(ctx, createPty(p));

        auto s = Serial::make(structs::PortName { p.slave_path },
            structs::BaudRate { enums::BaudRate::B_115200 });
        KAS_TEST_REQUIRE_IMPL(ctx, s.has_value());

        s->setTimeout(std::chrono::milliseconds { 2000 });

        const std::string payload = "roundtrip";
        KAS_TEST_CHECK_IMPL(ctx, s->write(payload));

        char buf[64] { };
        const ssize_t rn = ::read(p.master, buf, payload.size());
        KAS_TEST_REQUIRE_IMPL(ctx, rn == static_cast<ssize_t>(payload.size()));

        KAS_TEST_CHECK_IMPL(ctx,
            ::write(p.master, buf, static_cast<std::size_t>(rn))
                == static_cast<ssize_t>(rn));

        auto r = s->readExact(payload.size());
        KAS_TEST_CHECK_IMPL(ctx, r.has_value());
        if (r)
            KAS_TEST_CHECK_IMPL(ctx, std::ranges::equal(*r, asBytes(payload)));
    }

} // namespace

// ============================================================
// Entry point
// ============================================================
/**
 * @brief Runs all Serial tests and prints a summary.
 */
inline bool runTests()
{
    TestContext ctx;

    runCompileTimeChecks();

    // errors (no pty needed)
    testEmptyPortName(ctx);
    testNonexistentDevice(ctx);
    testRegularFile(ctx);
    testConflictingRtsConfig(ctx);
    testConflictingRtsInvertConfig(ctx);
    testMakeDoesNotThrowOnExpectedErrors(ctx);
    testDefaultConfigDoesNotTripValidator(ctx);
    testMoveOfErrorExpected(ctx);

    // success + I/O (pty-based)
    testMakeOnPtySucceeds(ctx);
    testAccessors(ctx);
    testWriteSendsBytes(ctx);
    testWriteSpan(ctx);
    testWriteEmptySpan(ctx);
    testReadReceivesBytes(ctx);
    testReadZeroReadsAllAvailable(ctx);
    testReadOnEof(ctx);
    testReadExact(ctx);
    testReadExactZero(ctx);
    testReadExactEofMidStream(ctx);
    testQueryRoundTrip(ctx);
    testQueryTimeout(ctx);
    testWaitReadableTrue(ctx);
    testWaitReadableTimeout(ctx);
    testWaitReadableZero(ctx);
    testSetTimeoutChainable(ctx);
    testReadWriteRoundTrip(ctx);

    return printSummary("kas::serial::test_serial::tests", ctx);
}

} // namespace kas::serial::test_serial::tests

#endif