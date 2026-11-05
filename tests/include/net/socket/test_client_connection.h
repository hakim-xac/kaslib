/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_NET_HTTP_CLIENT_CONNECTION_TESTS_H
#define KASLIBCXX_NET_HTTP_CLIENT_CONNECTION_TESTS_H

#include <arpa/inet.h>
#include <chrono>
#include <cstdint>
#include <fcntl.h>
#include <netinet/in.h>
#include <optional>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <utility>

#include "KAS/net/socket/ClientConnection.h"
#include "KAS/net/socket/SocketClient.h"
#include "tests/tests.h"

namespace kas::net::socket::client_connection::tests {

namespace {
    using namespace kas::tests;
    using kas::net::socket::ClientConnection;
    using kas::net::socket::SocketClient;

    // ------------------------------------------------------------------
    // TCP server on loopback (listen/accept only)
    // ------------------------------------------------------------------

    /// Minimal TCP listener bound to 127.0.0.1 on an ephemeral port.
    ///
    /// Used to exercise ClientConnection::getClientConnection: the test
    /// spawns a thread that calls accept() on fd() while the main thread
    /// connects via SocketClient::make(source()).
    class TcpListener {
    public:
        /// Creates, binds, and starts listening; port() is valid only if ok().
        TcpListener()
        {
            m_fd = ::socket(AF_INET, SOCK_STREAM, 0);
            if (m_fd < 0)
                return;

            int one = 1;
            ::setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

            sockaddr_in addr { };
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            addr.sin_port = 0;
            if (::bind(m_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
                return;
            if (::listen(m_fd, 1) != 0)
                return;

            socklen_t len = sizeof(addr);
            if (::getsockname(m_fd, reinterpret_cast<sockaddr*>(&addr), &len) != 0)
                return;
            m_port = ntohs(addr.sin_port);
        }

        /// Closes the listening socket.
        ~TcpListener()
        {
            if (m_fd >= 0)
                ::close(m_fd);
        }

        TcpListener(const TcpListener&) = delete;
        TcpListener& operator=(const TcpListener&) = delete;

        /// Reports whether the listener was set up successfully.
        [[nodiscard]] bool ok() const noexcept { return m_fd >= 0 && m_port != 0; }
        /// Returns the listening socket fd.
        [[nodiscard]] int fd() const noexcept { return m_fd; }

        /// Returns the "host:port" source string for SocketClient.
        [[nodiscard]] std::string source() const
        {
            return "127.0.0.1:" + std::to_string(m_port);
        }

    private:
        int m_fd { -1 }; ///< Listening socket fd.
        std::uint16_t m_port { 0 }; ///< Bound port.
    };

    // ==================================================================
    // getClientConnection - success
    // ==================================================================

    /// Accepting a client via getClientConnection yields a valid IPv4 connection.
    inline void getConnectionBasic(TestContext& ctx)
    {
        TcpListener listener;
        if (!listener.ok()) {
            KAS_TEST_CHECK_IMPL(ctx, false);
            return;
        }

        std::optional<ClientConnection> conn_opt;
        std::thread accept_thread([&] {
            conn_opt = ClientConnection::getClientConnection(listener.fd());
        });

        const auto client = SocketClient::make(listener.source());
        accept_thread.join();

        KAS_TEST_CHECK_IMPL(ctx, client.has_value());
        KAS_TEST_CHECK_IMPL(ctx, conn_opt.has_value());
        if (!conn_opt)
            return;

        const auto& conn = *conn_opt;
        KAS_TEST_CHECK_IMPL(ctx, conn.getSocketID() >= 0);
        KAS_TEST_CHECK_IMPL(ctx,
            conn.getRemoteAddress().asSockaddr() != nullptr);
        KAS_TEST_CHECK_IMPL(ctx,
            conn.getRemoteAddress().asSockaddr()->sa_family == AF_INET);
    }

    // ==================================================================
    // getClientConnection - fail (invalid fd)
    // ==================================================================

    /// Accepting on an invalid fd yields std::nullopt.
    inline void getConnectionInvalidFd(TestContext& ctx)
    {
        const auto conn = ClientConnection::getClientConnection(-1);
        KAS_TEST_CHECK_IMPL(ctx, !conn.has_value());
    }

    // ==================================================================
    // getRemoteAddress - IPv4 loopback
    // ==================================================================

    /// The peer address of a loopback connection is 127.0.0.1.
    inline void getConnectionAddressIsLoopback(TestContext& ctx)
    {
        TcpListener listener;
        if (!listener.ok()) {
            KAS_TEST_CHECK_IMPL(ctx, false);
            return;
        }

        std::optional<ClientConnection> conn_opt;
        std::thread accept_thread([&] {
            conn_opt = ClientConnection::getClientConnection(listener.fd());
        });

        const auto client = SocketClient::make(listener.source());
        accept_thread.join();

        if (!conn_opt) {
            KAS_TEST_CHECK_IMPL(ctx, false);
            return;
        }

        const auto* sin = conn_opt->getRemoteAddress().asSockaddrIN();
        KAS_TEST_CHECK_IMPL(ctx, sin->sin_family == AF_INET);
        KAS_TEST_CHECK_IMPL(ctx,
            sin->sin_addr.s_addr == htonl(INADDR_LOOPBACK));
    }

    // ==================================================================
    // getConnectionTime / updateConnectionTime
    // ==================================================================

    /// Connection time lies between the before/after markers; update advances it.
    inline void connectionTimeAndUpdate(TestContext& ctx)
    {
        TcpListener listener;
        if (!listener.ok()) {
            KAS_TEST_CHECK_IMPL(ctx, false);
            return;
        }

        std::optional<ClientConnection> conn_opt;
        std::thread accept_thread([&] {
            conn_opt = ClientConnection::getClientConnection(listener.fd());
        });

        const auto before = std::chrono::system_clock::now();
        const auto client = SocketClient::make(listener.source());
        accept_thread.join();
        const auto after = std::chrono::system_clock::now();

        if (!conn_opt) {
            KAS_TEST_CHECK_IMPL(ctx, false);
            return;
        }

        const auto t = conn_opt->getConnectionTime();
        KAS_TEST_CHECK_IMPL(ctx, t >= before);
        KAS_TEST_CHECK_IMPL(ctx, t <= after);

        std::this_thread::sleep_for(std::chrono::milliseconds { 10 });
        conn_opt->updateConnectionTime();

        const auto t2 = conn_opt->getConnectionTime();
        KAS_TEST_CHECK_IMPL(ctx, t2 > t);
    }

    // ==================================================================
    // close - fd is closed
    // ==================================================================

    /// close() invalidates the fd and makes it unusable (fcntl returns EBADF).
    inline void closeClosesFd(TestContext& ctx)
    {
        TcpListener listener;
        if (!listener.ok()) {
            KAS_TEST_CHECK_IMPL(ctx, false);
            return;
        }

        std::optional<ClientConnection> conn_opt;
        std::thread accept_thread([&] {
            conn_opt = ClientConnection::getClientConnection(listener.fd());
        });

        const auto client = SocketClient::make(listener.source());
        accept_thread.join();

        if (!conn_opt) {
            KAS_TEST_CHECK_IMPL(ctx, false);
            return;
        }

        const int fd = conn_opt->getSocketID();
        KAS_TEST_CHECK_IMPL(ctx, fd >= 0);

        conn_opt->close();
        KAS_TEST_CHECK_IMPL(ctx, conn_opt->getSocketID() == -1);

        // The fd is closed - fcntl returns EBADF.
        KAS_TEST_CHECK_IMPL(ctx, ::fcntl(fd, F_GETFD) == -1);
    }

    // ==================================================================
    // move ctor - the source becomes empty
    // ==================================================================

    /// Move construction transfers the fd and leaves the source invalid.
    inline void moveConstruct(TestContext& ctx)
    {
        TcpListener listener;
        if (!listener.ok()) {
            KAS_TEST_CHECK_IMPL(ctx, false);
            return;
        }

        std::optional<ClientConnection> conn_opt;
        std::thread accept_thread([&] {
            conn_opt = ClientConnection::getClientConnection(listener.fd());
        });

        const auto client = SocketClient::make(listener.source());
        accept_thread.join();

        if (!conn_opt) {
            KAS_TEST_CHECK_IMPL(ctx, false);
            return;
        }

        const int fd = conn_opt->getSocketID();

        ClientConnection moved { std::move(*conn_opt) };
        KAS_TEST_CHECK_IMPL(ctx, conn_opt->getSocketID() == -1);
        KAS_TEST_CHECK_IMPL(ctx, moved.getSocketID() == fd);
    }

    // ==================================================================
    // move assign - depends on the implementation's semantics
    // ==================================================================
    //
    // In the current implementation operator= performs a swap.
    // Both objects remain alive after move-assign; fds are closed in
    // their destructors. If the implementation changes to close+move,
    // this test must be rewritten.

    /// Move assignment currently swaps state between the two connections.
    inline void moveAssignSwaps(TestContext& ctx)
    {
        TcpListener listener1, listener2;
        if (!listener1.ok() || !listener2.ok()) {
            KAS_TEST_CHECK_IMPL(ctx, false);
            return;
        }

        std::optional<ClientConnection> conn1_opt, conn2_opt;
        std::thread accept1([&] { conn1_opt = ClientConnection::getClientConnection(listener1.fd()); });
        std::thread accept2([&] { conn2_opt = ClientConnection::getClientConnection(listener2.fd()); });

        const auto client1 = SocketClient::make(listener1.source());
        const auto client2 = SocketClient::make(listener2.source());
        accept1.join();
        accept2.join();

        if (!conn1_opt || !conn2_opt) {
            KAS_TEST_CHECK_IMPL(ctx, false);
            return;
        }

        const int fd1 = conn1_opt->getSocketID();
        const int fd2 = conn2_opt->getSocketID();

        *conn1_opt = std::move(*conn2_opt);

        // swap: conn1 = fd2, conn2 = fd1
        KAS_TEST_CHECK_IMPL(ctx, conn1_opt->getSocketID() == fd2);
        KAS_TEST_CHECK_IMPL(ctx, conn2_opt->getSocketID() == fd1);
    }

    // ==================================================================
    // swap
    // ==================================================================

    /// swap() exchanges fds between two connections.
    inline void swapBasic(TestContext& ctx)
    {
        TcpListener listener1, listener2;
        if (!listener1.ok() || !listener2.ok()) {
            KAS_TEST_CHECK_IMPL(ctx, false);
            return;
        }

        std::optional<ClientConnection> conn1_opt, conn2_opt;
        std::thread accept1([&] { conn1_opt = ClientConnection::getClientConnection(listener1.fd()); });
        std::thread accept2([&] { conn2_opt = ClientConnection::getClientConnection(listener2.fd()); });

        const auto client1 = SocketClient::make(listener1.source());
        const auto client2 = SocketClient::make(listener2.source());
        accept1.join();
        accept2.join();

        if (!conn1_opt || !conn2_opt) {
            KAS_TEST_CHECK_IMPL(ctx, false);
            return;
        }

        const int fd1 = conn1_opt->getSocketID();
        const int fd2 = conn2_opt->getSocketID();

        conn1_opt->swap(*conn2_opt);

        KAS_TEST_CHECK_IMPL(ctx, conn1_opt->getSocketID() == fd2);
        KAS_TEST_CHECK_IMPL(ctx, conn2_opt->getSocketID() == fd1);
    }

    // ==================================================================
    // Type properties
    // ==================================================================
    static_assert(!std::is_copy_constructible_v<ClientConnection>);
    static_assert(!std::is_copy_assignable_v<ClientConnection>);
    static_assert(std::is_move_constructible_v<ClientConnection>);
    static_assert(std::is_nothrow_move_constructible_v<ClientConnection>);
    static_assert(std::is_nothrow_move_assignable_v<ClientConnection>);

} // namespace

// ============================================================
// Entry point
// ============================================================

/**
 * @brief Runs all ClientConnection tests.
 *
 * Executes every test case defined in this header against a shared
 * TestContext and prints a summary for the
 * "kas::net::socket::client_connection::tests" suite.
 */
inline bool runTests()
{
    TestContext ctx;

    getConnectionBasic(ctx);
    getConnectionInvalidFd(ctx);
    getConnectionAddressIsLoopback(ctx);
    connectionTimeAndUpdate(ctx);
    closeClosesFd(ctx);
    moveConstruct(ctx);
    moveAssignSwaps(ctx);
    swapBasic(ctx);

    return printSummary("kas::net::socket::client_connection::tests", ctx);
}

} // namespace kas::net::socket::client_connection::tests

#endif