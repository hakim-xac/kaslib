/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_NET_HTTP_WEBSOCKET_FRAME_H
#define KASLIBCXX_NET_HTTP_WEBSOCKET_FRAME_H

#include <KAS/net/http/HttpRoute.h>

namespace kas::net::http {

/**
 * @brief WebSocket frame opcode as defined in RFC 6455, section 5.2.
 *
 * Occupies the lower 4 bits of the first frame byte. Values 0x3-0x7 and
 * 0xB-0xF are reserved and are not used by this implementation.
 */
enum class WsOpcode : std::uint8_t {
    Continuation = 0x0, ///< Continuation of a fragmented message.
    Text = 0x1, ///< Text message (UTF-8).
    Binary = 0x2, ///< Binary message.
    Close = 0x8, ///< Control frame: connection close.
    Ping = 0x9, ///< Control frame: liveness check.
    Pong = 0xA, ///< Control frame: reply to Ping.
};

/**
 * @brief A parsed WebSocket frame.
 *
 * Represents a single WebSocket protocol frame in "unpacked" form:
 * header bits are separated from the payload, and masking (if present)
 * has already been removed.
 */
struct WsFrame {
    bool fin = true; ///< FIN flag: last frame of the message.
    WsOpcode opcode = WsOpcode::Text; ///< Frame opcode.
    bool masked = false; ///< Whether the frame was masked (client -> server: always true).
    std::string payload; ///< Payload after unmasking.
};

/**
 * @brief Reads a single WebSocket frame from a socket.
 *
 * Parses the frame header, validates it (allowed opcode, length limits,
 * control frame masking rules), reads the payload, and removes the mask
 * if needed.
 *
 * @param fd Socket file descriptor.
 * @return The parsed frame, or std::nullopt on EOF, I/O error, or an
 *         invalid frame.
 *
 * @note The function reads exactly one frame; reassembly of fragmented
 *       messages is performed at a higher level (see WebSocketConnection).
 */
[[nodiscard]] std::optional<WsFrame> readFrame(int fd) noexcept;

/**
 * @brief Writes a single WebSocket frame to a socket.
 *
 * Builds the header (FIN, opcode, length) and sends the payload.
 * Frames sent by the server are not masked, as required by RFC 6455,
 * section 5.1.
 *
 * @param fd      Socket file descriptor.
 * @param opcode  Frame opcode.
 * @param payload Payload (not masked).
 * @param fin     FIN flag: true means the last frame of the message.
 * @return true on success, false on error.
 */
[[nodiscard]] bool writeFrame(int fd, WsOpcode opcode, std::string_view payload, bool fin = true) noexcept;

/**
 * @brief Sends a control Close frame (opcode 0x8).
 *
 * The frame body consists of a 2-byte big-endian close code followed by
 * an optional UTF-8 reason.
 *
 * @param fd     Socket file descriptor.
 * @param code   Close code (see RFC 6455, section 7.4.1). Defaults to
 *               1000 (Normal Closure).
 * @param reason Optional close reason (UTF-8, at most 123 bytes).
 * @return true on success, false on error.
 */
bool writeClose(int fd, std::uint16_t code = 1000, std::string_view reason = { }) noexcept;

/**
 * @brief Sends a control Pong frame (opcode 0xA).
 *
 * Typically used as a reply to an incoming Ping. The payload should match
 * the payload of the received Ping frame.
 *
 * @param fd      Socket file descriptor.
 * @param payload Pong data (at most 125 bytes per RFC 6455, section 5.5).
 * @return true on success, false on error.
 */
[[nodiscard]] bool writePong(int fd, std::string_view payload) noexcept;

} // namespace kas::net::http

#endif