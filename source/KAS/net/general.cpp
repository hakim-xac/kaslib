// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include "KAS/net/general.h"
#include "KAS/functional/FuncGuard.h"
#include "KAS/net/http/ContentTypes.h"
#include "KAS/utils/utils.h"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/tcp.h>
#include <poll.h>
#include <span>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/time.h>
#include <thread>
#include <unordered_map>
#include <utility>

namespace kas::net::general {
namespace {

    /// Default TCP keep-alive timeout used by getKeepAliveTimeoutS().
    const std::chrono::seconds G_KEEP_ALIVE_TIMEOUT_S { 5 };

    //========================================================

    /**
     * @brief Converts a partially read buffer plus a stop reason into a result.
     *
     * If @p result is empty, returns the error (as @p reason). If some
     * bytes were read before the error/EOF occurred, returns those bytes
     * as a successful result, discarding @p reason. This gives callers
     * the best of both: the data that was actually received, or a
     * meaningful error when nothing was received.
     *
     * @param result Bytes read so far (moved in).
     * @param reason Reason the read stopped (EOF, timeout, error).
     * @return The bytes on success, or an error value if no bytes were read.
     */
    [[nodiscard]]
    kas::expected_type<std::vector<std::uint8_t>, ReadError>
    finishRead(std::vector<std::uint8_t>&& result, ReadError reason)
    {
        if (result.empty())
            return kas::unexpected<ReadError> { reason };
        return std::move(result);
    }

    /**
     * @brief Maps a recv(2) errno value to a ReadError.
     *
     * EAGAIN/EWOULDBLOCK (a receive timeout, or a non-blocking socket
     * with no data) is reported as ReadError::Timeout; any other errno
     * is reported as ReadError::Error.
     *
     * @param err Value of errno after a failed recv().
     * @return The corresponding ReadError.
     */
    [[nodiscard]] ReadError classifyRecvError(int err) noexcept
    {
        if (err == EAGAIN || err == EWOULDBLOCK)
            return ReadError::Timeout;
        return ReadError::Error;
    }
}

//------------------

std::optional<std::chrono::milliseconds>
getSendTimeoutMs(int socket)
{
    timeval tv { };
    socklen_t len { sizeof(tv) };

    if (getsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &tv, &len) == 0) {
        auto ms { std::chrono::seconds(tv.tv_sec) + std::chrono::microseconds(tv.tv_usec) };
        return std::chrono::duration_cast<std::chrono::milliseconds>(ms);
    }

    return std::nullopt;
}

//------------------

std::optional<std::chrono::milliseconds>
getReceiveTimeoutMs(int socket)
{
    timeval tv { };
    socklen_t len { sizeof(tv) };

    if (getsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &tv, &len) == 0) {
        auto ms { std::chrono::seconds(tv.tv_sec) + std::chrono::microseconds(tv.tv_usec) };
        return std::chrono::duration_cast<std::chrono::milliseconds>(ms);
    }

    return std::nullopt;
}

//------------------

std::string
formatHttpDate(std::filesystem::file_time_type time)
{
    const auto duration { time.time_since_epoch() };
    const auto sys_duration { std::chrono::duration_cast<std::chrono::system_clock::duration>(duration) };
    const auto sctp { std::chrono::system_clock::time_point(sys_duration) };
    const auto time_t { std::chrono::system_clock::to_time_t(sctp) };
    std::tm tm { };
    gmtime_r(&time_t, &tm);
    char buffer[100] { };
    strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", &tm);

    return buffer;
}

//------------------

std::string
generateETag(const std::filesystem::path& filepath)
{
    const auto file_size { static_cast<std::uintmax_t>(std::filesystem::file_size(filepath)) };
    const auto last_modified { std::filesystem::last_write_time(filepath) };
    const auto duration { last_modified.time_since_epoch() };
    const auto sys_duration { std::chrono::duration_cast<std::chrono::system_clock::duration>(duration) };
    const auto sctp { std::chrono::system_clock::time_point(sys_duration) };
    const auto time_t { static_cast<std::uintmax_t>(std::chrono::system_clock::to_time_t(sctp)) };

    return utils::integerToHEX(file_size, false, false, 1) + utils::integerToHEX(time_t, false, false, 1);
}

//------------------

void setReceiveTimeoutMs(int socket_id, std::chrono::milliseconds timeout_ms)
{
    const auto sec { std::chrono::duration_cast<std::chrono::seconds>(timeout_ms) };
    const auto usec { std::chrono::duration_cast<std::chrono::microseconds>(timeout_ms - sec) };

    const timeval tv {
        .tv_sec { static_cast<time_t>(sec.count()) },
        .tv_usec { static_cast<suseconds_t>(usec.count()) }
    };

    setsockopt(socket_id, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}

//------------------

void setSendTimeoutMs(int socket_id, std::chrono::milliseconds timeout_ms)
{
    const auto sec { std::chrono::duration_cast<std::chrono::seconds>(timeout_ms) };
    const auto usec { std::chrono::duration_cast<std::chrono::microseconds>(timeout_ms - sec) };

    const timeval tv {
        .tv_sec { static_cast<time_t>(sec.count()) },
        .tv_usec { static_cast<suseconds_t>(usec.count()) }
    };

    setsockopt(socket_id, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

//------------------

void setBlockingSocket(int socket_id)
{
    int socket_flags { fcntl(socket_id, F_GETFL, 0) };
    (socket_flags < 0 || fcntl(socket_id, F_SETFL, socket_flags & ~O_NONBLOCK));
}

//------------------

void setNoBlockingSocket(int socket_id)
{
    int socket_flags { fcntl(socket_id, F_GETFL, 0) };
    (socket_flags < 0 || fcntl(socket_id, F_SETFL, socket_flags | O_NONBLOCK));
}

//------------------

void setTcpKeepAlive(int fd, std::chrono::seconds timeout_sec)
{
    const auto keepalive_on { 1 };
    const auto keepidle { timeout_sec.count() };
    const auto keepintvl { 1 };
    const auto keepcnt { 5 };

    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive_on, sizeof(keepalive_on));
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle));
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl));
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt));
}

//------------------

std::chrono::seconds
getKeepAliveTimeoutS()
{
    return G_KEEP_ALIVE_TIMEOUT_S;
}

//------------------

bool sendBytes(std::int32_t socket_id, std::span<const std::uint8_t> data) noexcept
{
    // Empty payload is a valid zero-length send:
    //  - UDP: a 0-byte datagram is sent;
    //  - TCP: send() with len 0 is a no-op and is not an error.
    if (data.empty()) {
        const ssize_t write_bytes = ::send(socket_id, nullptr, 0, MSG_NOSIGNAL);
        return write_bytes == 0;
    }

    std::size_t total_sent = 0;
    while (total_sent < data.size()) {
        const ssize_t write_bytes = ::send(
            socket_id,
            data.data() + total_sent,
            data.size() - total_sent,
            MSG_NOSIGNAL);

        if (write_bytes < 0) {
            if (errno == EINTR)
                continue; // interrupted by a signal - retry
            return false;
        }

        if (write_bytes == 0)
            return false; // connection closed

        total_sent += static_cast<std::size_t>(write_bytes);
    }

    return true;
}

//------------------

kas::expected_type<std::vector<std::uint8_t>, ReadError>
readAllBytes(std::int32_t socket_id)
{
    constexpr std::size_t MAX_BUFFER_LENGTH { 256 };
    std::array<std::uint8_t, MAX_BUFFER_LENGTH> buffer { };

    std::vector<std::uint8_t> result { };

    ReadError reason { ReadError::Error };

    while (true) {
        const ssize_t read_bytes = ::recv(
            socket_id, buffer.data(), MAX_BUFFER_LENGTH, MSG_NOSIGNAL);

        if (read_bytes > 0) {
            std::copy_n(buffer.begin(), read_bytes, std::back_inserter(result));
            continue;
        }

        if (read_bytes == 0) {
            reason = ReadError::Eof;
            break;
        }

        // read_bytes == -1
        reason = classifyRecvError(errno);
        break;
    }

    return finishRead(std::move(result), reason);
}

//------------------

kas::expected_type<std::vector<std::uint8_t>, ReadError>
readExactBytes(std::int32_t socket_id, std::size_t need_bytes)
{
    constexpr std::size_t MAX_BUFFER_LENGTH { 256 };
    std::array<std::uint8_t, MAX_BUFFER_LENGTH> buffer { };

    // Zero bytes requested - empty result is valid.
    if (need_bytes == 0)
        return std::vector<std::uint8_t> { };

    std::vector<std::uint8_t> result { };
    result.reserve(need_bytes);

    std::size_t total_read_bytes = 0;
    ReadError reason { ReadError::Error };

    while (total_read_bytes < need_bytes) {
        const std::size_t remaining = need_bytes - total_read_bytes;
        const std::size_t to_read = std::min(remaining, MAX_BUFFER_LENGTH);

        const ssize_t read_bytes = ::recv(
            socket_id, buffer.data(), to_read, MSG_NOSIGNAL);

        if (read_bytes > 0) {
            std::copy_n(buffer.begin(), read_bytes, std::back_inserter(result));
            total_read_bytes += static_cast<std::size_t>(read_bytes);
            continue;
        }

        if (read_bytes == 0) {
            reason = ReadError::Eof;
            break;
        }

        reason = classifyRecvError(errno);
        break;
    }

    // If exactly need_bytes were read - success, even if the next recv
    // would have been EOF (we exited by total == need_bytes, not by EOF).
    if (total_read_bytes == need_bytes)
        return result;

    // Otherwise - partial data or nothing; the reason is in reason.
    return finishRead(std::move(result), reason);
}

//------------------

std::optional<std::vector<std::uint8_t>>
readBytes(socket::SocketScoped::socket_t socket, std::size_t need_bytes)
{
    constexpr static size_t DEFAULT_RECEIVE_BUFFER_SIZE { 4 * 1024 };

    std::vector<std::uint8_t> res { };
    if (need_bytes > 0)
        res.reserve(need_bytes);

    std::array<std::uint8_t, DEFAULT_RECEIVE_BUFFER_SIZE> buffer { };
    while (true) {
        size_t bytes_to_read { DEFAULT_RECEIVE_BUFFER_SIZE };

        if (need_bytes > 0) {
            const size_t remaining_bytes { need_bytes - res.size() };
            if (remaining_bytes == 0)
                break;
            bytes_to_read = std::min(DEFAULT_RECEIVE_BUFFER_SIZE, remaining_bytes);
        }

        errno = 0;
        const ssize_t read_bytes { ::recv(socket, buffer.data(), bytes_to_read, MSG_NOSIGNAL) };

        if (read_bytes > 0) {
            res.insert(std::end(res), std::data(buffer), std::data(buffer) + read_bytes);

            if (static_cast<size_t>(read_bytes) < bytes_to_read)
                break;
        } else if (read_bytes == 0)
            break;
        else {
            if ((errno == EAGAIN || errno == EWOULDBLOCK) && !res.empty())
                break;
            return std::nullopt;
        }
    }

    if (res.empty()) [[unlikely]]
        return std::nullopt;
    return res;
}

//------------------

kas::expected_string<std::vector<std::uint8_t>>
query(socket::SocketScoped::socket_t socket, std::span<const std::uint8_t> buffer, std::size_t need_bytes)
{
    if (!sendBytes(socket, buffer)) [[unlikely]]
        return kas::unexpected_string { "Can not write data to socket!" };

    auto read_success_opt { readBytes(socket, need_bytes) };
    if (!read_success_opt) [[unlikely]]
        return kas::unexpected_string { "Can not read data from socket!" };

    return std::move(read_success_opt.value());
}

//------------------

bool isSocketValid(int socket_fd)
{
    int error = 0;
    socklen_t len = sizeof(error);
    return getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &error, &len) == 0 && error == 0;
}

//------------------

} // namespace kas::net::general