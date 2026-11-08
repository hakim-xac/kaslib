/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_NET_SOCKET_SOCKET_SERVER_TESTS_H
#define KASLIBCXX_NET_SOCKET_SOCKET_SERVER_TESTS_H

#include <arpa/inet.h>
#include <chrono>
#include <cstdint>
#include <netinet/in.h>
#include <optional>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <utility>

#include "KAS/net/socket/ClientConnection.h"
#include "KAS/net/socket/SocketClient.h"
#include "KAS/net/socket/SocketServer.h"
#include "KAS/net/socket/TcpTypes.h"
#include "tests/tests.h"

namespace kas::net::socket_server::tests {

namespace {
    using namespace kas::tests;
    using kas::net::socket::ClientConnection;
    using kas::net::socket::SocketClient;
    using kas::net::socket::SocketServer;
    using kas::net::socket::TcpTypes;

    // ==================================================================
    // make - IPv4 TCP
    // ==================================================================

    /// make("127.0.0.1:0", TCP) creates a valid socket.
    inline void makeTcp(TestContext& ctx)
    {
        const auto srv = SocketServer::make("127.0.0.1:0", TcpTypes::TCP);
        KAS_TEST_CHECK_IMPL(ctx, srv.has_value());
        if (!srv)
            return;
        KAS_TEST_CHECK_IMPL(ctx, srv->isSocketCreated());
        KAS_TEST_CHECK_IMPL(ctx, srv->getSocketID() >= 0);
    }

    // ==================================================================
    // listen - success
    // ==================================================================

    /// listen() on a freshly created server succeeds.
    inline void listenBasic(TestContext& ctx)
    {
        auto srv = SocketServer::make("127.0.0.1:0", TcpTypes::TCP);
        if (!srv) {
            KAS_TEST_CHECK_IMPL(ctx, false);
            return;
        }

        KAS_TEST_CHECK_IMPL(ctx, srv->listen());
    }

    // ==================================================================
    // listen + connect + accept via getClientConnection
    // ==================================================================

    /// After listen(), a client can connect and the server can accept it.
    inline void listenAndAccept(TestContext& ctx)
    {
        auto srv_opt = SocketServer::make("127.0.0.1:0", TcpTypes::TCP);
        KAS_TEST_CHECK_IMPL(ctx, srv_opt.has_value());
        if (!srv_opt)
            return;

        auto& srv = *srv_opt;
        KAS_TEST_CHECK_IMPL(ctx, srv.listen());

        sockaddr_in actual { };
        socklen_t len = sizeof(actual);
        ::getsockname(srv.getSocketID(), reinterpret_cast<sockaddr*>(&actual), &len);
        const std::uint16_t port = ntohs(actual.sin_port);

        const std::string source = "127.0.0.1:" + std::to_string(port);

        std::atomic<bool> client_connected { false };
        std::thread client_thread([&] {
            const auto cli = SocketClient::make(source);
            client_connected.store(cli.has_value(), std::memory_order_release);
            if (cli) {
                cli->setSendTimeoutMs(std::chrono::milliseconds { 500 });
                cli->setReceiveTimeoutMs(std::chrono::milliseconds { 500 });
            }
        });

        const auto conn = kas::net::socket::ClientConnection::getClientConnection(srv.getSocketID());
        KAS_TEST_CHECK_IMPL(ctx, conn.has_value());
        if (conn) {
            KAS_TEST_CHECK_IMPL(ctx, conn->getSocketID() >= 0);
            KAS_TEST_CHECK_IMPL(ctx, conn->getRemoteAddress().asSockaddr() != nullptr);
        }

        client_thread.join();
        KAS_TEST_CHECK_IMPL(ctx, client_connected.load(std::memory_order_acquire));
    }

    // ==================================================================
    // setClientReceiveTimeoutMs / setClientSendTimeoutMs
    // ==================================================================

    /// Both timeout setters return a reference to the same server.
    inline void setClientTimeouts(TestContext& ctx)
    {
        auto srv_opt = SocketServer::make("127.0.0.1:0", TcpTypes::TCP);
        if (!srv_opt) {
            KAS_TEST_CHECK_IMPL(ctx, false);
            return;
        }

        auto& srv = *srv_opt;
        SocketServer& ref = srv.setClientReceiveTimeoutMs(std::chrono::milliseconds { 100 });
        KAS_TEST_CHECK_IMPL(ctx, &ref == &srv);

        SocketServer& ref2 = srv.setClientSendTimeoutMs(std::chrono::milliseconds { 100 });
        KAS_TEST_CHECK_IMPL(ctx, &ref2 == &srv);
    }

    // ==================================================================
    // setNoBlocking / setBlocking
    // ==================================================================

    /// setNoBlocking/setBlocking return a reference to the same server.
    inline void nonBlockingAndBlocking(TestContext& ctx)
    {
        auto srv_opt = SocketServer::make("127.0.0.1:0", TcpTypes::TCP);
        if (!srv_opt) {
            KAS_TEST_CHECK_IMPL(ctx, false);
            return;
        }

        const auto& ref1 = srv_opt->setNoBlocking();
        KAS_TEST_CHECK_IMPL(ctx, &ref1 == &*srv_opt);

        const auto& ref2 = srv_opt->setBlocking();
        KAS_TEST_CHECK_IMPL(ctx, &ref2 == &*srv_opt);
    }

    // ==================================================================
    // getIpAddress / getSocketID / isSocketCreated
    // ==================================================================

    /// Accessors return the configured source and a valid handle.
    inline void accessors(TestContext& ctx)
    {
        auto srv_opt = SocketServer::make("127.0.0.1:0", TcpTypes::TCP);
        if (!srv_opt) {
            KAS_TEST_CHECK_IMPL(ctx, false);
            return;
        }

        KAS_TEST_CHECK_IMPL(ctx, srv_opt->isSocketCreated());
        KAS_TEST_CHECK_IMPL(ctx, srv_opt->getSocketID() >= 0);
        KAS_TEST_CHECK_IMPL(ctx, srv_opt->getIpAddress() == "127.0.0.1:0");
    }

    // ==================================================================
    // make - invalid source -> listen fails
    // ==================================================================

    /// A malformed source still creates a socket, but listen() fails (bind error).
    inline void makeInvalidSource(TestContext& ctx)
    {
        auto srv = SocketServer::make("not-a-source", TcpTypes::TCP);
        KAS_TEST_CHECK_IMPL(ctx, srv.has_value()); // the socket is created
        if (!srv)
            return;
        KAS_TEST_CHECK_IMPL(ctx, !srv->listen()); // bind fail
    }

    // ==================================================================
    // move ctor/assign
    // ==================================================================

    /// Move construction transfers the fd to the new server.
    inline void moveSemantics(TestContext& ctx)
    {
        auto srv_opt = SocketServer::make("127.0.0.1:0", TcpTypes::TCP);
        if (!srv_opt) {
            KAS_TEST_CHECK_IMPL(ctx, false);
            return;
        }

        const int fd = srv_opt->getSocketID();
        SocketServer moved { std::move(*srv_opt) };
        KAS_TEST_CHECK_IMPL(ctx, moved.getSocketID() == fd);
    }

    /// A Unix-domain server can be created and started listening.
    inline void makeUnix(TestContext& ctx)
    {
        const std::string path = "/tmp/kas_srv_test_" + std::to_string(::getpid());
        ::unlink(path.c_str());

        auto srv = SocketServer::make(path, TcpTypes::UNIX);
        KAS_TEST_CHECK_IMPL(ctx, srv.has_value());
        if (srv) {
            KAS_TEST_CHECK_IMPL(ctx, srv->listen());
            ::unlink(path.c_str());
        }
    }

    /// closeSocket invalidates the handle.
    inline void closeSocket(TestContext& ctx)
    {
        auto srv = SocketServer::make("127.0.0.1:0", TcpTypes::TCP);
        if (!srv) {
            KAS_TEST_CHECK_IMPL(ctx, false);
            return;
        }
        srv->closeSocket();
        KAS_TEST_CHECK_IMPL(ctx, !srv->isSocketCreated());
    }

    /// Accepting without a prior listen() fails (getClientConnection returns nullopt).
    inline void acceptWithoutListenFails(TestContext& ctx)
    {
        auto srv = SocketServer::make("127.0.0.1:0", TcpTypes::TCP);
        if (!srv) {
            KAS_TEST_CHECK_IMPL(ctx, false);
            return;
        }
        // Without listen().
        const auto conn = ClientConnection::getClientConnection(srv->getSocketID());
        KAS_TEST_CHECK_IMPL(ctx, !conn.has_value());
    }

    /// Binding a second server to the same port fails with EADDRINUSE.
    inline void listenTwiceFails(TestContext& ctx)
    {
        auto srv1 = SocketServer::make("127.0.0.1:0", TcpTypes::TCP);
        if (!srv1 || !srv1->listen()) {
            KAS_TEST_CHECK_IMPL(ctx, false);
            return;
        }

        sockaddr_in actual { };
        socklen_t len = sizeof(actual);
        ::getsockname(srv1->getSocketID(), reinterpret_cast<sockaddr*>(&actual), &len);
        const std::string source = "127.0.0.1:" + std::to_string(ntohs(actual.sin_port));

        auto srv2 = SocketServer::make(source, TcpTypes::TCP);
        KAS_TEST_CHECK_IMPL(ctx, srv2.has_value());
        if (srv2)
            KAS_TEST_CHECK_IMPL(ctx, !srv2->listen()); // EADDRINUSE
    }

    // ==================================================================
    // Type properties
    // ==================================================================
    static_assert(!std::is_copy_constructible_v<SocketServer>);
    static_assert(!std::is_copy_assignable_v<SocketServer>);
    static_assert(std::is_move_constructible_v<SocketServer>);
    static_assert(std::is_nothrow_move_constructible_v<SocketServer>);

} // namespace

// ============================================================
// Entry point
// ============================================================

/**
 * @brief Runs all SocketServer tests.
 *
 * Executes every test case defined in this header against a shared
 * TestContext and prints a summary for the
 * "kas::net::socket_server::tests" suite.
 */
inline bool runTests()
{
    TestContext ctx;

    makeTcp(ctx);
    listenBasic(ctx);
    listenAndAccept(ctx);
    setClientTimeouts(ctx);
    nonBlockingAndBlocking(ctx);
    accessors(ctx);
    makeInvalidSource(ctx);
    moveSemantics(ctx);

    makeUnix(ctx);
    closeSocket(ctx);
    acceptWithoutListenFails(ctx);

    return printSummary("kas::net::socket_server::tests", ctx);
}

} // namespace kas::net::socket_server::tests

#endif