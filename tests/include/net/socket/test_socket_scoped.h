/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_NET_SOCKET_SOCKET_SCOPED_TESTS_H
#define KASLIBCXX_NET_SOCKET_SOCKET_SCOPED_TESTS_H

#include "KAS/net/socket/SocketScoped.h"
#include "tests/tests.h"

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace kas::net::socket::socket_scoped::tests {

namespace {
    using namespace kas::tests;
    using kas::net::socket::SocketScoped;

    // ==================================================================
    // Construction
    // ==================================================================

    /// Default constructor creates a valid socket.
    inline void ctorDefault(TestContext& ctx)
    {
        SocketScoped s;
        KAS_TEST_CHECK_IMPL(ctx, s.isSocketCreated());
        KAS_TEST_CHECK_IMPL(ctx, s.getSocketID() >= 0);
    }

    /// Explicit constructor with the default parameters creates a valid socket.
    inline void ctorExplicit(TestContext& ctx)
    {
        SocketScoped s {
            SocketScoped::DEFAULT_SOCKET_DOMAIN,
            SocketScoped::DEFAULT_SOCKET_TYPE,
            SocketScoped::DEFAULT_SOCKET_PROTOCOL
        };
        KAS_TEST_CHECK_IMPL(ctx, s.isSocketCreated());
    }

    /// An invalid domain (AF_UNSPEC) leaves the handle invalid.
    inline void ctorInvalidDomain(TestContext& ctx)
    {
        SocketScoped s { AF_UNSPEC, SOCK_STREAM, 0 };
        KAS_TEST_CHECK_IMPL(ctx, !s.isSocketCreated());
        KAS_TEST_CHECK_IMPL(ctx, s.getSocketID() == -1);
    }

    // ==================================================================
    // makeSocketScoped
    // ==================================================================

    /// makeSocketScoped() with defaults succeeds and returns a valid socket.
    inline void makeOk(TestContext& ctx)
    {
        const auto opt = SocketScoped::makeSocketScoped();
        KAS_TEST_CHECK_IMPL(ctx, opt.has_value());
        if (opt)
            KAS_TEST_CHECK_IMPL(ctx, opt->isSocketCreated());
    }

    /// makeSocketScoped() with an invalid domain returns nullopt.
    inline void makeFail(TestContext& ctx)
    {
        const auto opt = SocketScoped::makeSocketScoped(AF_UNSPEC, SOCK_STREAM, 0);
        KAS_TEST_CHECK_IMPL(ctx, !opt.has_value());
    }

    // ==================================================================
    // closeSocket / isSocketCreated / getSocketID
    // ==================================================================

    /// closeSocket invalidates the handle and resets the id to -1.
    inline void closeBasic(TestContext& ctx)
    {
        SocketScoped s;
        KAS_TEST_CHECK_IMPL(ctx, s.isSocketCreated());

        s.closeSocket();
        KAS_TEST_CHECK_IMPL(ctx, !s.isSocketCreated());
        KAS_TEST_CHECK_IMPL(ctx, s.getSocketID() == -1);
    }

    /// Calling closeSocket twice is a no-op on the second call.
    inline void closeTwice(TestContext& ctx)
    {
        SocketScoped s;
        s.closeSocket();
        s.closeSocket(); // no-op
        KAS_TEST_CHECK_IMPL(ctx, !s.isSocketCreated());
    }

    /// The destructor closes the owned fd (fcntl on it returns EBADF).
    inline void dtorCloses(TestContext& ctx)
    {
        int fd = -1;
        {
            SocketScoped s;
            fd = s.getSocketID();
            KAS_TEST_CHECK_IMPL(ctx, fd >= 0);
        }
        KAS_TEST_CHECK_IMPL(ctx, ::fcntl(fd, F_GETFD) == -1);
    }

    // ==================================================================
    // Move semantics (semantics B: swap in operator=)
    // ==================================================================

    // --- move ctor - the source becomes empty (exchange) ------

    /// Move construction transfers the fd and leaves the source invalid.
    inline void moveConstruct(TestContext& ctx)
    {
        SocketScoped a;
        const int id_a = a.getSocketID();
        KAS_TEST_CHECK_IMPL(ctx, id_a >= 0);

        SocketScoped b { std::move(a) };
        KAS_TEST_CHECK_IMPL(ctx, !a.isSocketCreated());
        KAS_TEST_CHECK_IMPL(ctx, a.getSocketID() == -1);
        KAS_TEST_CHECK_IMPL(ctx, b.isSocketCreated());
        KAS_TEST_CHECK_IMPL(ctx, b.getSocketID() == id_a);
    }

    // --- move assign - swap: b gets fd_a, a gets fd_b -----

    /// Move assignment currently swaps fds; both objects remain valid.
    inline void moveAssignSwaps(TestContext& ctx)
    {
        SocketScoped a;
        const int fd_a = a.getSocketID();

        SocketScoped b;
        const int fd_b = b.getSocketID();

        b = std::move(a);

        // swap: b = fd_a, a = fd_b
        KAS_TEST_CHECK_IMPL(ctx, b.isSocketCreated());
        KAS_TEST_CHECK_IMPL(ctx, b.getSocketID() == fd_a);

        KAS_TEST_CHECK_IMPL(ctx, a.isSocketCreated());
        KAS_TEST_CHECK_IMPL(ctx, a.getSocketID() == fd_b);

        // Both fds are still open (neither has been closed).
        KAS_TEST_CHECK_IMPL(ctx, ::fcntl(fd_a, F_GETFD) != -1);
        KAS_TEST_CHECK_IMPL(ctx, ::fcntl(fd_b, F_GETFD) != -1);
    }

    // --- move assign - a's destructor closes fd_b --------

    /// After move-assign, destructors close the swapped fds.
    inline void moveAssignDtorClosesSwapped(TestContext& ctx)
    {
        int fd_a = -1;
        int fd_b = -1;
        {
            SocketScoped a;
            fd_a = a.getSocketID();

            SocketScoped b;
            fd_b = b.getSocketID();

            b = std::move(a);

            // b = fd_a, a = fd_b
            KAS_TEST_CHECK_IMPL(ctx, b.getSocketID() == fd_a);
            KAS_TEST_CHECK_IMPL(ctx, a.getSocketID() == fd_b);
        } // ~a closes fd_b, ~b closes fd_a

        // Both are closed.
        KAS_TEST_CHECK_IMPL(ctx, ::fcntl(fd_a, F_GETFD) == -1);
        KAS_TEST_CHECK_IMPL(ctx, ::fcntl(fd_b, F_GETFD) == -1);
    }

    // --- self move-assign - no-op ---------------------------------

    /// Self move-assignment leaves the object unchanged.
    inline void selfMoveAssign(TestContext& ctx)
    {
        SocketScoped a;
        const int id_a = a.getSocketID();

        SocketScoped& self = a;
        a = std::move(self);

        KAS_TEST_CHECK_IMPL(ctx, a.isSocketCreated());
        KAS_TEST_CHECK_IMPL(ctx, a.getSocketID() == id_a);
    }

    // ==================================================================
    // Type properties
    // ==================================================================
    static_assert(!std::is_copy_constructible_v<SocketScoped>);
    static_assert(!std::is_copy_assignable_v<SocketScoped>);
    static_assert(std::is_move_constructible_v<SocketScoped>);
    static_assert(std::is_nothrow_move_constructible_v<SocketScoped>);
    static_assert(std::is_nothrow_move_assignable_v<SocketScoped>);

} // namespace

// ============================================================
// Entry point
// ============================================================

/**
 * @brief Runs all SocketScoped tests.
 *
 * Executes every test case defined in this header against a shared
 * TestContext and prints a summary for the
 * "kas::net::socket::socket_scoped::tests" suite.
 */
inline bool runTests()
{
    TestContext ctx;

    ctorDefault(ctx);
    ctorExplicit(ctx);
    ctorInvalidDomain(ctx);

    makeOk(ctx);
    makeFail(ctx);

    closeBasic(ctx);
    closeTwice(ctx);
    dtorCloses(ctx);

    moveConstruct(ctx);
    moveAssignSwaps(ctx);
    moveAssignDtorClosesSwapped(ctx);
    selfMoveAssign(ctx);

    return printSummary("kas::net::socket::socket_scoped::tests", ctx);
}

} // namespace kas::net::socket::socket_scoped::tests

#endif