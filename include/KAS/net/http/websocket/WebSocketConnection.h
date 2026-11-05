/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_NET_HTTP_WEBSOCKET_CONNECTION_H
#define KASLIBCXX_NET_HTTP_WEBSOCKET_CONNECTION_H

#include <cstdint>
#include <optional>
#include <string_view>

namespace kas::net::http {

/**
 * @brief WebSocket connection (RFC 6455) bound to an already established socket.
 *
 * The class encapsulates message exchange over the WebSocket protocol:
 * reading (with reassembly of continuation frames), sending text and binary
 * messages, and handling control frames Ping/Pong/Close.
 *
 * Ownership of the file descriptor returned by @ref fd() remains with the
 * caller: the class does not close the socket in its destructor. Closing
 * (for example, via @ref close() or directly) is the owner's responsibility.
 *
 * @note The class is not thread-safe: concurrent calls to methods of the
 *       same instance from different threads are not allowed without
 *       external synchronization.
 */
class WebSocketConnection final {
public:
    /**
     * @brief Constructs a wrapper over an existing socket.
     *
     * @param fd File descriptor of an already established TCP connection
     *           on which the WebSocket handshake has been performed.
     */
    explicit WebSocketConnection(int fd) noexcept
        : m_fd { fd }
    {
    }

    /**
     * @brief Reads the next complete message.
     *
     * Automatically reassembles continuation frames (opcode 0x0) when
     * needed, and handles control frames as they arrive (Ping is answered
     * with Pong, Close terminates reading).
     *
     * @return The complete message (text or binary) as a string, or
     *         std::nullopt if the connection is closed or a protocol/I/O
     *         error occurred.
     */
    [[nodiscard]] std::optional<std::string> readMessage();

    /**
     * @brief Sends a text message (opcode 0x1).
     *
     * @param msg Message body.
     * @return true on success, false on error.
     */
    [[nodiscard]] bool sendText(std::string_view msg) noexcept;

    /**
     * @brief Sends a binary message (opcode 0x2).
     *
     * @param msg Message body.
     * @return true on success, false on error.
     */
    [[nodiscard]] bool sendBinary(std::string_view msg) noexcept;

    /**
     * @brief Sends a control Ping frame (opcode 0x9).
     *
     * @param payload Optional ping data (at most 125 bytes).
     * @return true on success, false on error.
     */
    [[nodiscard]] bool ping(std::string_view payload = { }) noexcept;

    /**
     * @brief Initiates connection close by sending a Close frame (opcode 0x8).
     *
     * @param code Close code (see RFC 6455, section 7.4.1). Defaults to
     *             1000 (Normal Closure).
     * @param reason Optional close reason (UTF-8, at most 123 bytes).
     * @return true on success, false on error.
     */
    [[nodiscard]] bool close(std::uint16_t code = 1000, std::string_view reason = { }) noexcept;

    /**
     * @brief Returns the socket file descriptor of the connection.
     *
     * @return The file descriptor passed at construction time.
     */
    [[nodiscard]] int fd() const noexcept { return m_fd; }

private:
    int m_fd;
};

} // namespace kas::net::http

#endif