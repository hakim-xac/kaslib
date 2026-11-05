/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_NET_SOCKET_CLIENT_CONNECTION_H
#define KASLIBCXX_NET_SOCKET_CLIENT_CONNECTION_H

#include "SocketAddress.h"
#include "SocketScoped.h"

#include <chrono>
#include <optional>

namespace kas::net::socket {

/**
 * @brief A single accepted client connection.
 *
 * Owns an accepted socket together with the peer's SocketAddress and the
 * time the connection was established. The connection is move-only (the
 * underlying socket handle must not be duplicated), and it closes the socket
 * on destruction unless it was released earlier via @ref close().
 *
 * Typical creation path: @ref getClientConnection takes a listening socket
 * from the server, performs accept(), and returns a ClientConnection.
 *
 * @note The class is not thread-safe: concurrent calls to methods of the
 *       same instance from different threads are not allowed without
 *       external synchronization.
 */
class ClientConnection final {
public:
    /// Socket handle type used by the underlying scoped socket wrapper.
    using socket_client_t = SocketScoped::socket_t;

public:
    /**
     * @brief Accepts a new client connection on the given listening socket.
     *
     * Calls accept() on @p socket_server, fills the peer address, and
     * records the current time as the connection time.
     *
     * @param socket_server Listening socket.
     * @return The accepted connection, or std::nullopt on accept error.
     */
    [[nodiscard]] static std::optional<ClientConnection> getClientConnection(socket_client_t socket_server);

    ClientConnection(const ClientConnection&) = delete;
    /**
     * @brief Move constructor: transfers ownership of the socket and state.
     * @param other Source connection; left without an owned socket.
     */
    ClientConnection(ClientConnection&& other) noexcept;
    ClientConnection& operator=(const ClientConnection&) = delete;

    /**
     * @brief Move assignment: closes the current socket (if any) and takes
     *        over the state of @p other.
     * @param other Source connection.
     * @return Reference to this connection.
     */
    ClientConnection& operator=(ClientConnection&& other) noexcept;

    /**
     * @brief Swaps the state of two connections.
     * @param other Connection to swap with.
     */
    void swap(ClientConnection& other) noexcept;

    /// Closes the owned socket, if any.
    ~ClientConnection();

    /**
     * @brief Closes the socket immediately.
     *
     * Safe to call multiple times; subsequent calls have no additional
     * effect.
     */
    void close();

    /**
     * @brief Returns the time the connection was established.
     * @return Connection time point.
     */
    [[nodiscard]] const std::chrono::system_clock::time_point& getConnectionTime() const& noexcept;

    /// Updates the connection time to the current system time.
    void updateConnectionTime();

    /**
     * @brief Returns the owned socket handle.
     * @return Socket handle (may be invalid after @ref close() or a move).
     */
    [[nodiscard]] socket_client_t getSocketID() const noexcept;

    /**
     * @brief Returns the peer address.
     * @return Remote SocketAddress.
     */
    [[nodiscard]] const SocketAddress& getRemoteAddress() const&;

private:
    /**
     * @brief Constructs a connection from an already accepted socket and
     *        its peer address.
     * @param client_socket         Accepted socket handle.
     * @param client_socket_address Peer address.
     */
    explicit ClientConnection(socket_client_t client_socket, SocketAddress client_socket_address);

private:
    std::chrono::system_clock::time_point m_connection_time; ///< Time the connection was established.
    SocketAddress m_client_socket_address; ///< Peer address.
    socket_client_t m_client_socket; ///< Owned socket handle.
};

} // namespace kas::net::socket

#endif