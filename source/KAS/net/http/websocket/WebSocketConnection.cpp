// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include "KAS/net/http/websocket/WebSocketConnection.h"
#include "KAS/net/http/websocket/WebSocketFrame.h"

namespace kas::net::http {

//------------------

std::optional<std::string>
WebSocketConnection::readMessage()
{
    std::string message;
    std::optional<WsOpcode> started;

    while (true) {
        auto frame = readFrame(m_fd);
        if (!frame)
            return std::nullopt;

        // Any inbound frame (Ping/Pong/Text/Binary/Continuation/Close)
        // proves that the peer is alive.
        m_last_activity.store(std::chrono::steady_clock::now(),
            std::memory_order_release);

        switch (frame->opcode) {
        case WsOpcode::Ping:
            if (!writePong(m_fd, frame->payload))
                return std::nullopt;
            continue;

        case WsOpcode::Pong:
            continue;

        case WsOpcode::Close:
            writeClose(m_fd, 1000);
            return std::nullopt;

        case WsOpcode::Text:
        case WsOpcode::Binary: {
            if (started)
                return std::nullopt;
            started = frame->opcode;
            message = std::move(frame->payload);
        } break;
        case WsOpcode::Continuation: {
            if (!started)
                return std::nullopt;
            message += frame->payload;
        } break;
        }

        if (frame->fin)
            break;
    }

    return message;
}

//------------------

bool WebSocketConnection::sendText(std::string_view msg) noexcept
{
    return writeFrame(m_fd, WsOpcode::Text, msg);
}

//------------------

bool WebSocketConnection::sendBinary(std::string_view msg) noexcept
{
    return writeFrame(m_fd, WsOpcode::Binary, msg);
}

//------------------

bool WebSocketConnection::ping(std::string_view payload) noexcept
{
    return writeFrame(m_fd, WsOpcode::Ping, payload);
}

//------------------

bool WebSocketConnection::close(std::uint16_t code, std::string_view reason) noexcept
{
    return writeClose(m_fd, code, reason);
}

//------------------

} // namespace kas::net::http