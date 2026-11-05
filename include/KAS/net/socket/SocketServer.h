/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_NET_SOCKET_SOCKET_SERVER_H
#define KASLIBCXX_NET_SOCKET_SOCKET_SERVER_H

#include "SocketScoped.h"
#include "TcpTypes.h"

#include <KAS/net/general.h>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace kas::net::socket {

/**
 * @brief Listening server socket wrapper.
 *
 * Owns a SocketScoped handle configured for listening on a source address
 * (for example, "0.0.0.0:8080" or a Unix socket path) and keeps the
 * per-client receive/send timeouts that the HTTP layer applies to accepted
 * connections. The class provides @ref listen to start accepting
 * connections and helpers to toggle blocking mode and configure timeouts.
 *
 * Instances are created either via @ref make (which reports failure as
 * std::nullopt) or directly with the constructor (which exposes the created
 * handle through @ref isSocketCreated / @ref getSocketID).
 *
 * @note The class is not thread-safe: concurrent calls to methods of the
 *       same instance from different threads are not allowed without
 *       external synchronization.
 */
class SocketServer final {
public:
    //------------------
    /// Socket handle type.
    using socket_t = SocketScoped::socket_t;
    /// Address family type (raw AF_* value).
    using socket_domain_t = SocketScoped::domain_t;
    /// Socket type (for example, SOCK_STREAM).
    using socket_type_t = SocketScoped::type_t;
    /// Protocol type (for example, IPPROTO_IP).
    using socket_protocol_t = SocketScoped::protocol_t;
    /// Type used for per-client receive timeouts.
    using client_receive_timeout_ms_t = std::chrono::milliseconds;
    /// Type used for per-client send timeouts.
    using client_send_timeout_ms_t = std::chrono::milliseconds;
    //------------------

    /// Default address family (inherited from SocketScoped).
    constexpr static socket_domain_t DEFAULT_SOCKET_DOMAIN = SocketScoped::DEFAULT_SOCKET_DOMAIN;
    /// Default socket type (inherited from SocketScoped).
    constexpr static socket_type_t DEFAULT_SOCKET_TYPE = SocketScoped::DEFAULT_SOCKET_TYPE;
    /// Default protocol (inherited from SocketScoped).
    constexpr static socket_protocol_t DEFAULT_SOCKET_PROTOCOL = SocketScoped::DEFAULT_SOCKET_PROTOCOL;
    /// Default per-client receive timeout (3000 ms).
    constexpr static client_receive_timeout_ms_t DEFAULT_CLIENT_RECEIVE_TIMEOUT_MS = client_receive_timeout_ms_t { 3000 };
    /// Default per-client send timeout (3000 ms).
    constexpr static client_send_timeout_ms_t DEFAULT_CLIENT_SEND_TIMEOUT_MS = client_send_timeout_ms_t { 3000 };

    //------------------

    /**
     * @brief Creates a server socket for the given source.
     *
     * @param sourse   Listen address in textual form (for example,
     *                 "0.0.0.0:8080").
     * @param tcp_type TCP type selector (see TcpTypes).
     * @return The constructed server, or std::nullopt on error.
     */
    [[nodiscard]] static std::optional<SocketServer> make(std::string sourse, TcpTypes tcp_type);
    //------------------

    /**
     * @brief Creates a server socket with explicit parameters.
     *
     * On failure the underlying handle is left invalid; check
     * @ref isSocketCreated to detect this case.
     *
     * @param source                   Listen address in textual form.
     * @param domain                   Address family.
     * @param type                     Socket type.
     * @param protocol                 Socket protocol.
     * @param client_receive_timeout_ms Per-client receive timeout.
     * @param client_send_timeout_ms    Per-client send timeout.
     */
    explicit SocketServer(
        std::string source,
        socket_domain_t domain = DEFAULT_SOCKET_DOMAIN,
        socket_type_t type = DEFAULT_SOCKET_TYPE,
        socket_protocol_t protocol = DEFAULT_SOCKET_PROTOCOL,
        client_receive_timeout_ms_t client_receive_timeout_ms = DEFAULT_CLIENT_RECEIVE_TIMEOUT_MS,
        client_send_timeout_ms_t client_send_timeout_ms = DEFAULT_CLIENT_SEND_TIMEOUT_MS);
    //------------------

    SocketServer(const SocketServer&) = delete;
    SocketServer& operator=(const SocketServer&) = delete;
    SocketServer(SocketServer&& other) noexcept = default;
    SocketServer& operator=(SocketServer&& other) noexcept = default;
    ~SocketServer() = default;
    //------------------

    /**
     * @brief Returns the owned socket handle.
     * @return Socket handle (may be invalid if creation failed or the socket
     *         was closed).
     */
    [[nodiscard]] socket_t getSocketID() const noexcept;
    //------------------

    /**
     * @brief Reports whether the owned socket handle is valid.
     * @return true if the socket was created and not yet closed.
     */
    [[nodiscard]] bool isSocketCreated() const noexcept;
    //------------------

    /**
     * @brief Returns the listen address passed at construction.
     * @return Address string.
     */
    [[nodiscard]] std::string_view getIpAddress() const noexcept;
    //------------------

    /**
     * @brief Sets the per-client receive timeout.
     * @param timeout_ms Timeout value.
     * @return Reference to this server.
     */
    SocketServer& setClientReceiveTimeoutMs(std::chrono::milliseconds timeout_ms) &;
    //------------------

    /**
     * @brief Sets the per-client send timeout.
     * @param timeout_ms Timeout value.
     * @return Reference to this server.
     */
    SocketServer& setClientSendTimeoutMs(std::chrono::milliseconds timeout_ms) &;
    //------------------

    /**
     * @brief Switches the socket to non-blocking mode.
     * @return Reference to this server.
     */
    SocketServer& setNoBlocking() &;
    //------------------

    /**
     * @brief Switches the socket back to blocking mode.
     * @return Reference to this server.
     */
    SocketServer& setBlocking() &;
    //------------------

    /**
     * @brief Starts listening for incoming connections.
     * @param max_listen Backlog size for ::listen (default 1024).
     * @return true on success, false on error.
     */
    [[nodiscard]] bool listen(int32_t max_listen = 1024) noexcept;
    //------------------

    /**
     * @brief Reads a chunk of bytes from the socket.
     *
     * Intended for sockets where the server itself reads data (for example,
     * datagram sockets); for accepted stream connections the HTTP layer uses
     * ClientConnection instead.
     *
     * @return The bytes read, or a ReadError describing the failure.
     */
    [[nodiscard]] kas::expected_type<std::vector<std::uint8_t>, general::ReadError> readBytes() const;
    //------------------

    /// Closes the owned socket immediately (idempotent).
    void closeSocket() noexcept;
    //------------------

    /**
     * @brief Returns the listen address passed at construction.
     * @return Source string.
     */
    [[nodiscard]] const std::string& getSource() const& noexcept;
    //------------------

private:
    SocketScoped m_socket_scoped; ///< Owned scoped listening socket.
    std::string m_source; ///< Listen address as passed at construction.
    socket_domain_t m_domain; ///< Address family.
    client_receive_timeout_ms_t m_client_receive_timeout_ms; ///< Per-client receive timeout.
    client_send_timeout_ms_t m_client_send_timeout_ms; ///< Per-client send timeout.
};
} // namespace kas::net::socket
#endif