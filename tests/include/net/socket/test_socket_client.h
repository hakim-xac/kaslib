/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_NET_SOCKET_SOCKET_CLIENT_TESTS_H
#define KASLIBCXX_NET_SOCKET_SOCKET_CLIENT_TESTS_H

#include <arpa/inet.h>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <netinet/in.h>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "KAS/net/socket/SocketClient.h"
#include "tests/tests.h"

namespace kas::net::socket_client::tests {

namespace {
    using namespace kas::tests;
    using kas::net::socket::SocketClient;

    // ------------------------------------------------------------------
    // Default timeout for all client tests
    // ------------------------------------------------------------------
    constexpr auto kTimeout = std::chrono::milliseconds { 1000 };

    // ------------------------------------------------------------------
    // TcpServer - emulates a TCP server on loopback
    // ------------------------------------------------------------------

    /// Minimal TCP server used to drive SocketClient tests.
    ///
    /// Binds to 127.0.0.1 on an ephemeral port, accepts one client, and
    /// offers helpers to send/read exact byte counts and to close the
    /// accepted client socket.
    class TcpServer {
    public:
        /// Creates, binds, and starts listening on an ephemeral port.
        TcpServer()
        {
            m_fd = ::socket(AF_INET, SOCK_STREAM, 0);
            if (m_fd < 0)
                return;

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

        /// Closes the accepted client and the listening socket.
        ~TcpServer()
        {
            if (m_client_fd >= 0)
                ::close(m_client_fd);
            if (m_fd >= 0)
                ::close(m_fd);
        }

        TcpServer(const TcpServer&) = delete;
        TcpServer& operator=(const TcpServer&) = delete;

        /// Reports whether the listener was set up successfully.
        [[nodiscard]] bool ok() const noexcept { return m_fd >= 0 && m_port != 0; }
        /// Returns the bound port.
        [[nodiscard]] std::uint16_t port() const noexcept { return m_port; }

        /// Returns the "host:port" source string for SocketClient.
        [[nodiscard]] std::string source() const
        {
            return "127.0.0.1:" + std::to_string(m_port);
        }

        /// Accepts a single client (blocking).
        bool acceptClient()
        {
            m_client_fd = ::accept(m_fd, nullptr, nullptr);
            return m_client_fd >= 0;
        }

        /// Sends the whole @p data to the accepted client.
        bool sendToClient(std::string_view data)
        {
            if (m_client_fd < 0)
                return false;
            const ssize_t n = ::send(m_client_fd, data.data(), data.size(), MSG_NOSIGNAL);
            return n == static_cast<ssize_t>(data.size());
        }

        /// Reads exactly @p n bytes from the accepted client.
        bool readExactFromClient(void* dst, std::size_t n)
        {
            if (m_client_fd < 0)
                return false;
            auto* p = static_cast<char*>(dst);
            std::size_t total = 0;
            while (total < n) {
                const ssize_t r = ::recv(m_client_fd, p + total, n - total, 0);
                if (r <= 0)
                    return false;
                total += static_cast<std::size_t>(r);
            }
            return true;
        }

        /// Closes the accepted client socket, if any.
        void closeClient()
        {
            if (m_client_fd >= 0) {
                ::close(m_client_fd);
                m_client_fd = -1;
            }
        }

    private:
        int m_fd { -1 }; ///< Listening socket.
        int m_client_fd { -1 }; ///< Accepted client socket.
        std::uint16_t m_port { 0 }; ///< Bound port.
    };

    // ------------------------------------------------------------------
    // UdpServer - emulates a UDP server on loopback
    // ------------------------------------------------------------------

    /// Minimal UDP server used to drive SocketClient datagram tests.
    class UdpServer {
    public:
        /// Creates and binds a UDP socket on an ephemeral loopback port.
        UdpServer()
        {
            m_fd = ::socket(AF_INET, SOCK_DGRAM, 0);
            if (m_fd < 0)
                return;

            sockaddr_in addr { };
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            addr.sin_port = 0;
            if (::bind(m_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
                return;

            socklen_t len = sizeof(addr);
            if (::getsockname(m_fd, reinterpret_cast<sockaddr*>(&addr), &len) != 0)
                return;

            m_port = ntohs(addr.sin_port);
        }

        /// Closes the UDP socket.
        ~UdpServer()
        {
            if (m_fd >= 0)
                ::close(m_fd);
        }

        UdpServer(const UdpServer&) = delete;
        UdpServer& operator=(const UdpServer&) = delete;

        /// Reports whether the server was set up successfully.
        [[nodiscard]] bool ok() const noexcept { return m_fd >= 0 && m_port != 0; }

        /// Returns the "host:port" source string for SocketClient.
        [[nodiscard]] std::string source() const
        {
            return "127.0.0.1:" + std::to_string(m_port);
        }

        /// Receives a single datagram (blocking recvfrom).
        [[nodiscard]] std::optional<std::vector<std::uint8_t>>
        recvDatagram()
        {
            std::vector<std::uint8_t> buf(65536);
            sockaddr_in from { };
            socklen_t from_len = sizeof(from);
            const ssize_t n = ::recvfrom(m_fd, buf.data(), buf.size(), 0,
                reinterpret_cast<sockaddr*>(&from), &from_len);
            if (n <= 0)
                return std::nullopt;
            buf.resize(static_cast<std::size_t>(n));
            return buf;
        }

        /// Sends a datagram back to the client of the last recvfrom.
        bool sendDatagramToClient(std::span<const std::uint8_t> data)
        {
            if (m_client_addr.sin_family == 0)
                return false;
            const ssize_t n = ::sendto(m_fd, data.data(), data.size(), MSG_NOSIGNAL,
                reinterpret_cast<const sockaddr*>(&m_client_addr), sizeof(m_client_addr));
            return n == static_cast<ssize_t>(data.size());
        }

        /// Receives one datagram and echoes it back (recvfrom + sendto).
        [[nodiscard]] std::optional<std::vector<std::uint8_t>>
        echoOnce()
        {
            std::vector<std::uint8_t> buf(65536);
            socklen_t from_len = sizeof(m_client_addr);
            const ssize_t n = ::recvfrom(m_fd, buf.data(), buf.size(), 0,
                reinterpret_cast<sockaddr*>(&m_client_addr), &from_len);
            if (n <= 0)
                return std::nullopt;
            buf.resize(static_cast<std::size_t>(n));

            const ssize_t sent = ::sendto(m_fd, buf.data(), buf.size(), MSG_NOSIGNAL,
                reinterpret_cast<const sockaddr*>(&m_client_addr), from_len);
            if (sent != static_cast<ssize_t>(buf.size()))
                return std::nullopt;

            return buf;
        }

    private:
        int m_fd { -1 }; ///< UDP socket.
        std::uint16_t m_port { 0 }; ///< Bound port.
        sockaddr_in m_client_addr { }; ///< Address of the last sender.
    };

    // ==================================================================
    // makeFailsNoServer - connect to port 0 (EINVAL immediately)
    // ==================================================================

    /// Connecting to port 0 fails and yields std::nullopt.
    inline void makeFailsNoServer(TestContext& ctx)
    {
        const auto client = SocketClient::make("127.0.0.1:0");
        KAS_TEST_CHECK_IMPL(ctx, !client.has_value());
    }

    // ==================================================================
    // makeSucceeds
    // ==================================================================

    /// Successful make(): the client is connected and exposes its address.
    inline void makeSucceeds(TestContext& ctx)
    {
        TcpServer server;
        KAS_TEST_CHECK_IMPL(ctx, server.ok());
        if (!server.ok())
            return;

        std::thread accept_thread([&server] { (void)server.acceptClient(); });

        const auto client_opt = SocketClient::make(server.source());
        accept_thread.join();

        KAS_TEST_CHECK_IMPL(ctx, client_opt.has_value());
        if (!client_opt)
            return;

        KAS_TEST_CHECK_IMPL(ctx, client_opt->isConnected());
        KAS_TEST_CHECK_IMPL(ctx, client_opt->isSocketCreated());
        KAS_TEST_CHECK_IMPL(ctx, client_opt->getSocketID() >= 0);
        KAS_TEST_CHECK_IMPL(ctx, client_opt->getSource() == server.source());
        KAS_TEST_CHECK_IMPL(ctx, client_opt->getSocketAddress().has_value());
        KAS_TEST_CHECK_IMPL(ctx,
            client_opt->getSocketAddress()->asSockaddrIN()->sin_family == AF_INET);
    }

    // ==================================================================
    // sendString / readString - with timeouts
    // ==================================================================

    /// String round-trip: client sends "hello" and reads back exactly 5 bytes.
    inline void sendAndReadString(TestContext& ctx)
    {
        TcpServer server;
        if (!server.ok()) {
            KAS_TEST_CHECK_IMPL(ctx, false);
            return;
        }

        std::thread server_thread([&server] {
            if (!server.acceptClient())
                return;
            char buf[5] { };
            if (!server.readExactFromClient(buf, 5))
                return;
            server.sendToClient(std::string_view(buf, 5));
            server.closeClient();
        });

        const auto client_opt = SocketClient::make(server.source());
        KAS_TEST_CHECK_IMPL(ctx, client_opt.has_value());
        if (!client_opt) {
            server_thread.join();
            return;
        }

        auto& client = *client_opt;
        client.setSendTimeoutMs(kTimeout).setReceiveTimeoutMs(kTimeout);

        KAS_TEST_CHECK_IMPL(ctx, client.sendString("hello"));

        // readBytes(5) reads exactly 5 bytes (the server sent 5), no EOF wait.
        const auto response = client.readBytes(5);
        KAS_TEST_CHECK_IMPL(ctx, response.has_value());
        if (response) {
            KAS_TEST_CHECK_IMPL(ctx, response->size() == 5u);
            KAS_TEST_CHECK_IMPL(ctx,
                std::string(response->begin(), response->end()) == "hello");
        }

        server_thread.join();
    }

    // ==================================================================
    // sendBytes / readBytes
    // ==================================================================

    /// Byte round-trip: client sends 5 bytes and reads back the same 5 bytes.
    inline void sendAndReadBytes(TestContext& ctx)
    {
        TcpServer server;
        if (!server.ok()) {
            KAS_TEST_CHECK_IMPL(ctx, false);
            return;
        }

        std::thread server_thread([&server] {
            if (!server.acceptClient())
                return;
            char buf[5] { };
            if (!server.readExactFromClient(buf, 5))
                return;
            server.sendToClient(std::string_view(buf, 5));
            server.closeClient();
        });

        const auto client_opt = SocketClient::make(server.source());
        if (!client_opt) {
            server_thread.join();
            return;
        }

        auto& client = *client_opt;
        client.setSendTimeoutMs(kTimeout).setReceiveTimeoutMs(kTimeout);

        const std::vector<std::uint8_t> data { 1, 2, 3, 4, 5 };
        KAS_TEST_CHECK_IMPL(ctx, client.sendBytes(std::span<const std::uint8_t>(data)));

        const auto response = client.readBytes(5);
        KAS_TEST_CHECK_IMPL(ctx, response.has_value());
        if (response) {
            KAS_TEST_CHECK_IMPL(ctx, *response == data);
        }

        server_thread.join();
    }

    // ==================================================================
    // readBytes(need_bytes) - exactly N
    // ==================================================================

    /// readBytes(3) returns exactly the first 3 bytes even if more are available.
    inline void readBytesExactCount(TestContext& ctx)
    {
        TcpServer server;
        if (!server.ok()) {
            KAS_TEST_CHECK_IMPL(ctx, false);
            return;
        }

        std::thread server_thread([&server] {
            if (!server.acceptClient())
                return;
            server.sendToClient("abcdef");
            server.closeClient();
        });

        const auto client_opt = SocketClient::make(server.source());
        if (!client_opt) {
            server_thread.join();
            return;
        }

        auto& client = *client_opt;
        client.setReceiveTimeoutMs(kTimeout);

        const auto response = client.readBytes(3);
        KAS_TEST_CHECK_IMPL(ctx, response.has_value());
        if (response) {
            KAS_TEST_CHECK_IMPL(ctx, response->size() == 3u);
            KAS_TEST_CHECK_IMPL(ctx,
                std::string(response->begin(), response->end()) == "abc");
        }

        server_thread.join();
    }

    // ==================================================================
    // Timeout - server does not answer -> readBytes -> nullopt (does not hang)
    // ==================================================================

    /// A short receive timeout makes readBytes(0) return nullopt instead of hanging.
    inline void readTimeoutReturnsNullopt(TestContext& ctx)
    {
        TcpServer server;
        if (!server.ok()) {
            KAS_TEST_CHECK_IMPL(ctx, false);
            return;
        }

        std::thread server_thread([&server] {
            if (!server.acceptClient())
                return;
            // Send nothing and do not close - the client must hit the timeout.
            std::this_thread::sleep_for(std::chrono::milliseconds { 300 });
            server.closeClient();
        });

        const auto client_opt = SocketClient::make(server.source());
        if (!client_opt) {
            server_thread.join();
            return;
        }

        auto& client = *client_opt;
        client.setReceiveTimeoutMs(std::chrono::milliseconds { 100 }); // short timeout

        // readBytes(0) = until EOF, but with a timeout recv returns -1 -> nullopt.
        const auto response = client.readBytes(0);
        KAS_TEST_CHECK_IMPL(ctx, !response.has_value());

        server_thread.join();
    }

    // ==================================================================
    // Timeout - send with a full buffer (not tested directly,
    // but setSendTimeoutMs must not fail)
    // ==================================================================

    /// setSendTimeoutMs/setReceiveTimeoutMs return a reference to the same client.
    inline void setTimeoutsReturnRef(TestContext& ctx)
    {
        TcpServer server;
        if (!server.ok()) {
            KAS_TEST_CHECK_IMPL(ctx, false);
            return;
        }

        std::thread accept_thread([&server] { (void)server.acceptClient(); });

        const auto client_opt = SocketClient::make(server.source());
        accept_thread.join();
        if (!client_opt) {
            KAS_TEST_CHECK_IMPL(ctx, false);
            return;
        }

        const auto& ref1 = client_opt->setSendTimeoutMs(kTimeout);
        KAS_TEST_CHECK_IMPL(ctx, &ref1 == &*client_opt);

        const auto& ref2 = client_opt->setReceiveTimeoutMs(kTimeout);
        KAS_TEST_CHECK_IMPL(ctx, &ref2 == &*client_opt);
    }

    // ==================================================================
    // connectRvalue - via make
    // ==================================================================

    /// make() with the default parameters produces a usable client.
    inline void connectRvalue(TestContext& ctx)
    {
        TcpServer server;
        if (!server.ok()) {
            KAS_TEST_CHECK_IMPL(ctx, false);
            return;
        }

        std::thread accept_thread([&server] { (void)server.acceptClient(); });

        const auto client_opt = SocketClient::make(server.source());
        accept_thread.join();
        KAS_TEST_CHECK_IMPL(ctx, client_opt.has_value());
    }

    // ==================================================================
    // connectionTime
    // ==================================================================

    /// Connection time lies between the before/after markers.
    inline void connectionTime(TestContext& ctx)
    {
        TcpServer server;
        if (!server.ok()) {
            KAS_TEST_CHECK_IMPL(ctx, false);
            return;
        }

        const auto before = std::chrono::system_clock::now();

        std::thread accept_thread([&server] { (void)server.acceptClient(); });
        const auto client_opt = SocketClient::make(server.source());
        accept_thread.join();
        if (!client_opt) {
            KAS_TEST_CHECK_IMPL(ctx, false);
            return;
        }

        const auto after = std::chrono::system_clock::now();
        const auto& t = client_opt->getConnectionTime();
        KAS_TEST_CHECK_IMPL(ctx, t >= before);
        KAS_TEST_CHECK_IMPL(ctx, t <= after);
    }

    /// readBytes(0) reassembles a payload delivered in two segments.
    inline void readBytesZeroHandlesSegmentation(TestContext& ctx)
    {
        TcpServer server;
        if (!server.ok()) {
            KAS_TEST_CHECK_IMPL(ctx, false);
            return;
        }

        std::thread server_thread([&server] {
            if (!server.acceptClient())
                return;
            // Send in two parts with a pause - TCP may segment the payload.
            server.sendToClient("hel");
            std::this_thread::sleep_for(std::chrono::milliseconds { 20 });
            server.sendToClient("lo");
            server.closeClient();
        });

        const auto client_opt = SocketClient::make(server.source());
        if (!client_opt) {
            server_thread.join();
            return;
        }

        auto& client = *client_opt;
        client.setReceiveTimeoutMs(kTimeout);

        const auto response = client.readBytes(0);
        KAS_TEST_CHECK_IMPL(ctx, response.has_value());
        if (response) {
            KAS_TEST_CHECK_IMPL(ctx,
                std::string(response->begin(), response->end()) == "hello");
        }

        server_thread.join();
    }

    // ==================================================================
    // readDatagram - UDP
    // ==================================================================

    // --- 1. readDatagram - echoes one datagram --------------------

    /// UDP round-trip of a 5-byte datagram.
    inline void readDatagramBasic(TestContext& ctx)
    {
        UdpServer server;
        if (!server.ok()) {
            KAS_TEST_CHECK_IMPL(ctx, false);
            return;
        }

        std::thread server_thread([&server] {
            (void)server.echoOnce();
        });

        const auto client_opt = SocketClient::make(
            server.source(), AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        KAS_TEST_CHECK_IMPL(ctx, client_opt.has_value());
        if (!client_opt) {
            server_thread.join();
            return;
        }

        auto& client = *client_opt;
        client.setReceiveTimeoutMs(kTimeout);

        const std::vector<std::uint8_t> data { 1, 2, 3, 4, 5 };
        KAS_TEST_CHECK_IMPL(ctx, client.sendBytes(std::span<const std::uint8_t>(data)));

        const auto response = client.readDatagram();
        KAS_TEST_CHECK_IMPL(ctx, response.has_value());
        if (response) {
            KAS_TEST_CHECK_IMPL(ctx, *response == data);
        }

        server_thread.join();
    }

    // --- 2. readDatagram - empty datagram -----------------------

    /// An empty datagram is treated as a read failure (recv returns 0).
    inline void readDatagramEmpty(TestContext& ctx)
    {
        UdpServer server;
        if (!server.ok()) {
            KAS_TEST_CHECK_IMPL(ctx, false);
            return;
        }

        std::thread server_thread([&server] {
            (void)server.echoOnce();
        });

        const auto client_opt = SocketClient::make(
            server.source(), AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (!client_opt) {
            server_thread.join();
            return;
        }

        auto& client = *client_opt;
        client.setReceiveTimeoutMs(kTimeout);

        // Empty datagram - send with 0 bytes.
        KAS_TEST_CHECK_IMPL(ctx, client.sendBytes(std::span<const std::uint8_t> { }));

        const auto response = client.readDatagram();
        // recv returns 0 bytes (a datagram) -> readDatagram returns nullopt
        // (because recv_bytes <= 0).
        KAS_TEST_CHECK_IMPL(ctx, !response.has_value());

        server_thread.join();
    }

    // --- 3. readDatagram - multiple datagrams ---------------------

    /// Three consecutive datagrams are echoed and read back in order.
    inline void readDatagramMultiple(TestContext& ctx)
    {
        UdpServer server;
        if (!server.ok()) {
            KAS_TEST_CHECK_IMPL(ctx, false);
            return;
        }

        std::thread server_thread([&server] {
            (void)server.echoOnce(); // 1st
            (void)server.echoOnce(); // 2nd
            (void)server.echoOnce(); // 3rd
        });

        const auto client_opt = SocketClient::make(
            server.source(), AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (!client_opt) {
            server_thread.join();
            return;
        }

        auto& client = *client_opt;
        client.setReceiveTimeoutMs(kTimeout);

        for (int i = 1; i <= 3; ++i) {
            const std::vector<std::uint8_t> data {
                static_cast<std::uint8_t>(i),
                static_cast<std::uint8_t>(i * 10),
                static_cast<std::uint8_t>(i * 100)
            };
            KAS_TEST_CHECK_IMPL(ctx, client.sendBytes(std::span<const std::uint8_t>(data)));

            const auto response = client.readDatagram();
            KAS_TEST_CHECK_IMPL(ctx, response.has_value());
            if (response)
                KAS_TEST_CHECK_IMPL(ctx, *response == data);
        }

        server_thread.join();
    }

    // --- 4. readDatagram - timeout (server does not answer) ------------

    /// A short receive timeout makes readDatagram return nullopt.
    inline void readDatagramTimeout(TestContext& ctx)
    {
        UdpServer server;
        if (!server.ok()) {
            KAS_TEST_CHECK_IMPL(ctx, false);
            return;
        }

        // The server stays silent (sends nothing).
        std::thread server_thread([&server] {
            std::this_thread::sleep_for(std::chrono::milliseconds { 300 });
        });

        const auto client_opt = SocketClient::make(
            server.source(), AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (!client_opt) {
            server_thread.join();
            return;
        }

        auto& client = *client_opt;
        client.setReceiveTimeoutMs(std::chrono::milliseconds { 100 });

        const auto response = client.readDatagram();
        KAS_TEST_CHECK_IMPL(ctx, !response.has_value());

        server_thread.join();
    }

    // --- 5. readDatagram - large datagram (1 KiB) --------------

    /// A 1 KiB datagram is echoed back byte-for-byte.
    inline void readDatagramLarge(TestContext& ctx)
    {
        UdpServer server;
        if (!server.ok()) {
            KAS_TEST_CHECK_IMPL(ctx, false);
            return;
        }

        std::thread server_thread([&server] {
            (void)server.echoOnce();
        });

        const auto client_opt = SocketClient::make(
            server.source(), AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (!client_opt) {
            server_thread.join();
            return;
        }

        auto& client = *client_opt;
        client.setReceiveTimeoutMs(kTimeout);

        std::vector<std::uint8_t> data(1024);
        for (std::size_t i = 0; i < data.size(); ++i)
            data[i] = static_cast<std::uint8_t>(i & 0xFF);

        KAS_TEST_CHECK_IMPL(ctx, client.sendBytes(std::span<const std::uint8_t>(data)));

        const auto response = client.readDatagram();
        KAS_TEST_CHECK_IMPL(ctx, response.has_value());
        if (response)
            KAS_TEST_CHECK_IMPL(ctx, *response == data);

        server_thread.join();
    }

    // ==================================================================
    // Type properties
    // ==================================================================
    static_assert(!std::is_copy_constructible_v<SocketClient>);
    static_assert(std::is_move_constructible_v<SocketClient>);

} // namespace

// ============================================================
// Entry point
// ============================================================

/**
 * @brief Runs all SocketClient tests.
 *
 * Executes every test case defined in this header against a shared
 * TestContext and prints a summary for the
 * "kas::net::socket_client::tests" suite.
 */
inline bool runTests()
{
    TestContext ctx;

    makeFailsNoServer(ctx);
    makeSucceeds(ctx);
    sendAndReadString(ctx);
    sendAndReadBytes(ctx);
    readBytesExactCount(ctx);
    readTimeoutReturnsNullopt(ctx);
    setTimeoutsReturnRef(ctx);
    connectRvalue(ctx);
    connectionTime(ctx);
    readBytesZeroHandlesSegmentation(ctx);
    // --- readDatagram ---
    readDatagramBasic(ctx);
    readDatagramEmpty(ctx);
    readDatagramMultiple(ctx);
    readDatagramTimeout(ctx);
    readDatagramLarge(ctx);

    return printSummary("kas::net::socket_client::tests", ctx);
}

} // namespace kas::net::socket_client::tests

#endif