/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_NET_GENERAL_H
#define KASLIBCXX_NET_GENERAL_H

#include "socket/SocketScoped.h"

#include <KAS/expected/expected.h>

#include <arpa/inet.h>
#include <chrono>
#include <cstdint>
#include <fcntl.h>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace kas::net::general {

/**
 * @brief Parsed request line of an HTTP request.
 *
 * Holds views into the original buffer; the caller must keep that buffer
 * alive for as long as this structure is used.
 */
struct FirstLineRequest {
    std::string_view method; ///< HTTP method (for example, "GET").
    std::string_view host; ///< Request target (path or absolute URI).
    std::string_view http_version; ///< HTTP version string (for example, "HTTP/1.1").
};

/**
 * @brief Reason a read operation did not return data.
 */
enum class ReadError {
    Eof, ///< Peer closed the connection (EOF).
    Timeout, ///< Receive timeout expired.
    Error ///< Any other I/O error.
};

/**
 * @brief Returns the SO_SNDTIMEO value of a socket.
 * @param socket Socket file descriptor.
 * @return Send timeout, or std::nullopt if it cannot be queried.
 */
[[nodiscard]] std::optional<std::chrono::milliseconds> getSendTimeoutMs(int socket);

/**
 * @brief Returns the SO_RCVTIMEO value of a socket.
 * @param socket Socket file descriptor.
 * @return Receive timeout, or std::nullopt if it cannot be queried.
 */
[[nodiscard]] std::optional<std::chrono::milliseconds> getReceiveTimeoutMs(int socket);

/**
 * @brief Formats a file time as an HTTP date (IMF-fixdate, RFC 9110).
 * @param time File timestamp.
 * @return Date string suitable for the Date/Last-Modified headers.
 */
[[nodiscard]] std::string formatHttpDate(std::filesystem::file_time_type time);

/**
 * @brief Computes an ETag for a file.
 *
 * The exact algorithm is implementation-defined; the returned value is
 * suitable for the ETag response header (quoted string).
 *
 * @param filepath Path to the file.
 * @return ETag value.
 */
[[nodiscard]] std::string generateETag(const std::filesystem::path& filepath);

/**
 * @brief Sets the receive timeout on a socket.
 * @param socket_id Socket file descriptor.
 * @param timeout_ms Timeout value.
 */
void setReceiveTimeoutMs(int socket_id, std::chrono::milliseconds timeout_ms);

/**
 * @brief Sets the send timeout on a socket.
 * @param socket_id Socket file descriptor.
 * @param timeout_ms Timeout value.
 */
void setSendTimeoutMs(int socket_id, std::chrono::milliseconds timeout_ms);

/**
 * @brief Switches a socket back to blocking mode.
 * @param socket_id Socket file descriptor.
 */
void setBlockingSocket(int socket_id);

/**
 * @brief Switches a socket to non-blocking mode.
 * @param socket_id Socket file descriptor.
 */
void setNoBlockingSocket(int socket_id);

/**
 * @brief Enables TCP keep-alive on a socket.
 * @param fd          Socket file descriptor.
 * @param timeout_sec Idle time before probes are sent.
 */
void setTcpKeepAlive(int fd, std::chrono::seconds timeout_sec);

/**
 * @brief Returns the configured TCP keep-alive timeout.
 * @return Keep-alive timeout in seconds.
 */
[[nodiscard]] std::chrono::seconds getKeepAliveTimeoutS();

/**
 * @brief Reads bytes from a socket.
 *
 * If @p need_bytes is non-zero, the function attempts to read exactly that
 * many bytes; otherwise it reads whatever is currently available.
 *
 * @param socket     Socket file descriptor.
 * @param need_bytes Number of bytes to read, 0 for "as available".
 * @return The bytes read, or std::nullopt on EOF/error.
 */
[[nodiscard]] std::optional<std::vector<std::uint8_t>> readBytes(socket::SocketScoped::socket_t socket, std::size_t need_bytes = 0);

/**
 * @brief Sends a request and reads the response bytes.
 *
 * Writes @p buffer to @p socket, then reads the response using
 * @p need_bytes as the target length (0 means "read as available").
 *
 * @param socket     Socket file descriptor.
 * @param buffer     Request bytes to send.
 * @param need_bytes Number of response bytes to read, 0 for "as available".
 * @return The response bytes, or an error string on failure.
 */
[[nodiscard]] kas::expected_string<std::vector<std::uint8_t>> query(socket::SocketScoped::socket_t socket, std::span<const std::uint8_t> buffer, std::size_t need_bytes = 0);

/**
 * @brief Reports whether the given fd is a valid open socket.
 * @param socket_fd Socket file descriptor.
 * @return true if the fd refers to an open socket.
 */
[[nodiscard]] bool isSocketValid(int socket_fd);

/**
 * @brief Sends all bytes of @p data over a socket.
 *
 * Loops over send(2) until @p data is fully written. MSG_NOSIGNAL is
 * used to avoid SIGPIPE on broken connections. EINTR is retried
 * transparently; any other send() error or a zero-length return (peer
 * closed the connection) makes the function return false.
 *
 * An empty @p data is treated as a valid zero-length send:
 *  - UDP: a zero-length datagram is delivered;
 *  - TCP: send() with len 0 is a no-op and does not report an error.
 *
 * @param socket_id Socket file descriptor.
 * @param data      Bytes to send.
 * @return true if all bytes were sent, false on error or early close.
 */
[[nodiscard]] bool sendBytes(std::int32_t socket_id, std::span<const std::uint8_t> data) noexcept;

/**
 * @brief Reads all available bytes until EOF.
 * @param socket_id Socket file descriptor.
 * @return The bytes read, or a ReadError describing the failure.
 */
[[nodiscard]] kas::expected_type<std::vector<std::uint8_t>, ReadError> readAllBytes(std::int32_t socket_id);

/**
 * @brief Reads exactly @p need_bytes bytes from a socket.
 *
 * Reads in chunks of up to 256 bytes and accumulates them until
 * @p need_bytes bytes have been read, EOF is reached, or recv() fails.
 * On partial success (some bytes read, then EOF or error) the bytes
 * read so far are returned as a successful result; if nothing was read,
 * the error is returned.
 *
 * @param socket_id  Socket file descriptor.
 * @param need_bytes Number of bytes to read. 0 returns an empty vector
 *                   as a valid result.
 * @return The bytes read (exactly @p need_bytes on success), or a
 *         ReadError if no bytes could be read.
 */
[[nodiscard]] kas::expected_type<std::vector<std::uint8_t>, ReadError> readExactBytes(std::int32_t socket_id, std::size_t need_bytes);

} // namespace kas::net::general

#endif