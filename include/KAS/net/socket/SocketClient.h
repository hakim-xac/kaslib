/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_NET_SOCKET_SOCKET_CLIENT_H
#define KASLIBCXX_NET_SOCKET_SOCKET_CLIENT_H

#include "SocketAddress.h"
#include "SocketScoped.h"

#include <KAS/net/general.h>

#include <arpa/inet.h>
#include <chrono>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace kas::net::socket {

/**
 * @brief Client-side socket wrapper for connecting and exchanging data.
 *
 * Encapsulates a SocketScoped handle together with the parsed remote
 * address (SocketAddress), the connection time, and the original source
 * string. Instances are created via @ref make, then connected with
 * @ref connect, after which they can send and receive bytes, strings, or
 * datagrams depending on the socket type.
 *
 * Timeouts for send and receive operations can be configured with
 * @ref setSendTimeoutMs and @ref setReceiveTimeoutMs before connecting or
 * exchanging data.
 *
 * @note The class is not thread-safe: concurrent calls to methods of the
 *       same instance from different threads are not allowed without
 *       external synchronization.
 */
class SocketClient final {
public:
    //------------------
    /// Socket handle type used by the underlying scoped socket wrapper.
    using socket_t = SocketScoped::socket_t;
    /// Address family type (raw AF_* value).
    using domain_t = SocketScoped::domain_t;
    /// Socket type (for example, SOCK_STREAM or SOCK_DGRAM).
    using type_t = SocketScoped::type_t;
    /// Protocol type (for example, IPPROTO_IP or IPPROTO_TCP).
    using protocol_t = SocketScoped::protocol_t;
    //------------------

    /**
     * @brief Creates a client socket for the given source.
     *
     * Parses @p source into a SocketAddress and constructs the underlying
     * socket with the given family, type, and protocol. The socket is not
     * connected yet; call @ref connect for that.
     *
     * @param source   Remote address in textual form (for example,
     *                 "127.0.0.1:8080" or a Unix socket path).
     * @param domain   Address family.
     * @param type     Socket type.
     * @param protocol Socket protocol.
     * @return The constructed client, or std::nullopt on error (invalid
     *         source, socket creation failure, or address parse failure).
     */
    [[nodiscard]] static std::optional<SocketClient>
    make(
        std::string_view source,
        domain_t domain = AF_INET,
        type_t type = SOCK_STREAM,
        protocol_t protocol = IPPROTO_IP);
    //------------------

    /**
     * @brief Returns the owned socket handle.
     * @return Socket handle (may be invalid if the socket was not created).
     */
    [[nodiscard]] socket_t getSocketID() const noexcept;
    //------------------

    /**
     * @brief Reports whether the underlying socket was created successfully.
     * @return true if the socket handle is valid.
     */
    [[nodiscard]] bool isSocketCreated() const noexcept;
    //------------------

    /**
     * @brief Returns the original source string passed to @ref make.
     * @return Source string.
     */
    [[nodiscard]] std::string_view getSource() const& noexcept;
    //------------------

    /**
     * @brief Returns the parsed remote address, if any.
     * @return Optional SocketAddress.
     */
    [[nodiscard]] const std::optional<SocketAddress>& getSocketAddress() const&;
    //------------------

    /**
     * @brief Returns the time the connection was established.
     * @return Connection time point.
     */
    [[nodiscard]] const std::chrono::system_clock::time_point& getConnectionTime() const& noexcept;
    //------------------

    /**
     * @brief Reports whether the socket is currently connected.
     * @return true if connected.
     */
    [[nodiscard]] bool isConnected() const noexcept;
    //------------------

    /**
     * @brief Reads a chunk of bytes from the socket.
     *
     * If @p need_bytes is non-zero, the implementation attempts to read
     * exactly that many bytes; otherwise it reads whatever is currently
     * available (up to the internal buffer size).
     *
     * @param need_bytes Number of bytes to read, 0 for "as available".
     * @return The bytes read, or a ReadError describing the failure.
     */
    [[nodiscard]] kas::expected_type<std::vector<std::uint8_t>, general::ReadError> readBytes(size_t need_bytes = 0) const;
    //------------------

    /**
     * @brief Sends a chunk of bytes over the socket.
     * @param data Bytes to send.
     * @return true on success, false on error.
     */
    [[nodiscard]] bool sendBytes(std::span<const std::uint8_t> data) const noexcept;
    //------------------

    /**
     * @brief Reads a string of bytes from the socket.
     * @param need_bytes Number of bytes to read, 0 for "as available".
     * @return The string read, or std::nullopt on error.
     */
    [[nodiscard]] std::optional<std::string> readString(size_t need_bytes = 0) const;
    //------------------

    /**
     * @brief Reads a single datagram from the socket.
     * @return The datagram payload, or std::nullopt on error.
     */
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> readDatagram() const;
    //------------------

    /**
     * @brief Sends a string over the socket.
     * @param text String to send.
     * @return true on success, false on error.
     */
    [[nodiscard]] bool sendString(std::string text) const noexcept;
    //------------------

    /**
     * @brief Sets the send timeout for subsequent operations.
     * @param send_timeout_ms Timeout value.
     * @return Reference to this client.
     */
    const SocketClient& setSendTimeoutMs(std::chrono::milliseconds send_timeout_ms) const&;
    //------------------

    /**
     * @brief Sets the receive timeout for subsequent operations.
     * @param receive_timeout_ms Timeout value.
     * @return Reference to this client.
     */
    const SocketClient& setReceiveTimeoutMs(std::chrono::milliseconds receive_timeout_ms) const&;
    //------------------

    /**
     * @brief Connects the socket to the configured remote address.
     * @return Reference to this client (lvalue overload).
     */
    SocketClient& connect() &;
    //------------------

    /**
     * @brief Connects the socket to the configured remote address.
     * @return Moved reference to this client (rvalue overload).
     */
    SocketClient&& connect() &&;
    //------------------

private:
    /**
     * @brief Constructs a client for the given source and socket parameters.
     *
     * Prefer the @ref make factory; this constructor is used internally.
     *
     * @param source   Remote address in textual form.
     * @param domain   Address family.
     * @param type     Socket type.
     * @param protocol Socket protocol.
     */
    explicit SocketClient(
        std::string_view source,
        domain_t domain = AF_INET,
        type_t type = SOCK_STREAM,
        protocol_t protocol = IPPROTO_IP);

    /// Initializes the underlying socket and resolves the remote address.
    void init();

    SocketScoped m_socket; ///< Owned scoped socket.
    std::chrono::system_clock::time_point m_connection_time; ///< Time the connection was established.
    std::string m_source; ///< Original source string.
    std::optional<SocketAddress> m_socket_address_opt; ///< Parsed remote address.
    domain_t m_domain; ///< Address family.
};
} // namespace kas::net::socket

#endif