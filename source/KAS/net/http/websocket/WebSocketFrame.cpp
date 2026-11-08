// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

// WebSocketFrame.cpp
#include "KAS/net/http/websocket/WebSocketFrame.h"
#include "KAS/net/general.h"

namespace kas::net::http {

namespace {

    /**
     * @brief Reads exactly @p n bytes from @p fd into @p dst.
     *
     * Loops over recv() until @p n bytes have been read. EINTR is
     * retried transparently. Any other recv() error (including a
     * timeout expressed as EAGAIN/EWOULDBLOCK) or EOF before @p n
     * bytes are read makes the function return false.
     *
     * @param fd  Socket file descriptor.
     * @param dst Destination buffer of at least @p n bytes.
     * @param n   Number of bytes to read.
     * @return true if exactly @p n bytes were read, false on EOF or error.
     */
    bool readExact(int fd, std::uint8_t* dst, std::size_t n) noexcept
    {
        std::size_t got = 0;
        while (got < n) {
            const ssize_t r = ::recv(fd, dst + got, n - got, 0);
            if (r < 0) {
                if (errno == EINTR)
                    continue;
                return false; // EAGAIN (timeout) or a real error
            }
            if (r == 0)
                return false; // EOF
            got += static_cast<std::size_t>(r);
        }
        return true;
    }

    /**
     * @brief Upper bound on the payload size of a single WebSocket frame.
     *
     * Guards against DoS: a client declaring len = 2^63 would make the
     * server allocate gigabytes and die with std::bad_alloc (which,
     * inside noexcept code, calls std::terminate). Frames whose payload
     * exceeds this limit must be rejected.
     */
    constexpr std::uint64_t MAX_FRAME_PAYLOAD = 64ULL * 1024 * 1024; // 64 MiB
} // namespace

//------------------

/**
 * @brief Reads a single WebSocket frame from @p fd.
 *
 * Parses the frame header (FIN, opcode, mask bit), the extended
 * payload length (16 or 64 bit, when present), the masking key, and
 * the payload itself. Masking is removed in place.
 *
 * Validation performed:
 *  - the extended length must fit into std::size_t (relevant on 32-bit
 *    platforms);
 *  - the payload must not exceed MAX_FRAME_PAYLOAD (DoS guard);
 *  - control frames (Close, Ping, Pong) must be unfragmented (FIN = 1)
 *    and carry at most 125 bytes of payload;
 *  - the mask bit must be set (client-to-server frames are always
 *    masked per RFC 6455, section 5.1).
 *
 * @param fd Socket file descriptor.
 * @return The parsed frame, or std::nullopt on EOF, I/O error, or an
 *         invalid frame.
 */
std::optional<WsFrame>
readFrame(int fd) noexcept
{
    std::uint8_t hdr[2];
    if (!readExact(fd, hdr, 2))
        return std::nullopt;

    WsFrame frame;
    frame.fin = (hdr[0] & 0x80) != 0;
    frame.opcode = static_cast<WsOpcode>(hdr[0] & 0x0F);
    frame.masked = (hdr[1] & 0x80) != 0;

    std::uint64_t len = hdr[1] & 0x7F;

    if (len == 126) {
        std::uint8_t ext[2];
        if (!readExact(fd, ext, 2))
            return std::nullopt;
        len = (std::uint64_t { ext[0] } << 8) | ext[1];
    } else if (len == 127) {
        std::uint8_t ext[8];
        if (!readExact(fd, ext, 8))
            return std::nullopt;
        len = 0;
        for (auto b : ext)
            len = (len << 8) | b;
    }

    // On 32-bit platforms, len may not fit into size_t.
    if (len > std::numeric_limits<std::size_t>::max())
        return std::nullopt;

    // DoS guard: reject oversized payloads before allocating.
    if (len > MAX_FRAME_PAYLOAD)
        return std::nullopt;

    // Control frames: payload <= 125 bytes, must not be fragmented.
    const bool is_control = (static_cast<std::uint8_t>(frame.opcode) & 0x08) != 0;
    if (is_control && (len > 125 || !frame.fin))
        return std::nullopt;

    // Client-to-server frames are always masked.
    if (!frame.masked)
        return std::nullopt;

    std::uint8_t mask[4];
    if (!readExact(fd, mask, 4))
        return std::nullopt;

    frame.payload.resize(static_cast<std::size_t>(len));
    if (len > 0) {
        if (!readExact(fd, reinterpret_cast<std::uint8_t*>(frame.payload.data()),
                static_cast<std::size_t>(len)))
            return std::nullopt;
        for (std::size_t i = 0; i < frame.payload.size(); ++i)
            frame.payload[i] ^= static_cast<char>(mask[i % 4]);
    }

    return frame;
}

//------------------

bool writeFrame(int fd, WsOpcode opcode, std::string_view payload, bool fin) noexcept
{
    std::string buf;
    buf.reserve(payload.size() + 10);

    buf.push_back(static_cast<char>((fin ? 0x80 : 0x00)
        | static_cast<std::uint8_t>(opcode)));

    const std::size_t len = payload.size();
    if (len < 126) {
        buf.push_back(static_cast<char>(len));
    } else if (len <= 0xFFFF) {
        buf.push_back(126);
        buf.push_back(static_cast<char>((len >> 8) & 0xFF));
        buf.push_back(static_cast<char>(len & 0xFF));
    } else {
        buf.push_back(127);
        for (int i = 7; i >= 0; --i)
            buf.push_back(static_cast<char>((len >> (i * 8)) & 0xFF));
    }

    buf.append(payload.data(), payload.size());

    const auto* bytes = reinterpret_cast<const std::uint8_t*>(buf.data());
    return general::sendBytes(fd, std::span<const std::uint8_t> { bytes, buf.size() });
}

//------------------

bool writeClose(int fd, std::uint16_t code, std::string_view reason) noexcept
{
    std::string payload;
    payload.push_back(static_cast<char>((code >> 8) & 0xFF));
    payload.push_back(static_cast<char>(code & 0xFF));
    payload.append(reason.data(), reason.size());
    return writeFrame(fd, WsOpcode::Close, payload);
}

//------------------

bool writePong(int fd, std::string_view payload) noexcept
{
    return writeFrame(fd, WsOpcode::Pong, payload);
}

//------------------

} // namespace kas::net::http