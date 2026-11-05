/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_NET_HTTP_WEBSOCKET_TESTS_H
#define KASLIBCXX_NET_HTTP_WEBSOCKET_TESTS_H

#include "KAS/net/http/Request.h"
#include "KAS/net/http/websocket/WebSocketConnection.h"
#include "KAS/net/http/websocket/WebSocketFrame.h"
#include "KAS/net/http/websocket/WebSocketHandshake.h"
#include "tests/tests.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace kas::net::http::websocket::tests {

namespace {
    using namespace kas::tests;
    using kas::net::http::computeWebSocketAccept;
    using kas::net::http::readFrame;
    using kas::net::http::Request;
    using kas::net::http::WebSocketConnection;
    using kas::net::http::writeClose;
    using kas::net::http::writeFrame;
    using kas::net::http::writePong;
    using kas::net::http::WsFrame;
    using kas::net::http::WsOpcode;

    // ============================================================
    //  Вспомогательное: socketpair
    // ============================================================

    struct SocketPair {
        int a = -1;
        int b = -1;

        SocketPair()
        {
            int fds[2];
            if (::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0) {
                a = fds[0];
                b = fds[1];
            }
        }

        ~SocketPair()
        {
            if (a >= 0)
                ::close(a);
            if (b >= 0)
                ::close(b);
        }

        SocketPair(const SocketPair&) = delete;
        SocketPair& operator=(const SocketPair&) = delete;
    };

    // Читает ровно n байт. false -> EOF/ошибка.
    bool recvExact(int fd, std::uint8_t* dst, std::size_t n)
    {
        std::size_t got = 0;
        while (got < n) {
            const ssize_t r = ::recv(fd, dst + got, n - got, 0);
            if (r <= 0)
                return false;
            got += static_cast<std::size_t>(r);
        }
        return true;
    }

    // Собирает «сырой» фрейм клиента (masked) — для readFrame.
    std::vector<std::uint8_t> makeClientFrame(
        WsOpcode opcode,
        std::string_view payload,
        bool fin = true,
        std::array<std::uint8_t, 4> mask = { 0x12, 0x34, 0x56, 0x78 })
    {
        std::vector<std::uint8_t> out;

        out.push_back(static_cast<std::uint8_t>(
            (fin ? 0x80 : 0x00) | static_cast<std::uint8_t>(opcode)));

        const std::size_t len = payload.size();
        if (len < 126) {
            out.push_back(static_cast<std::uint8_t>(0x80 | len));
        } else if (len <= 0xFFFF) {
            out.push_back(0x80 | 126);
            out.push_back(static_cast<std::uint8_t>((len >> 8) & 0xFF));
            out.push_back(static_cast<std::uint8_t>(len & 0xFF));
        } else {
            out.push_back(0x80 | 127);
            for (int i = 7; i >= 0; --i)
                out.push_back(static_cast<std::uint8_t>((len >> (i * 8)) & 0xFF));
        }

        out.insert(out.end(), mask.begin(), mask.end());

        for (std::size_t i = 0; i < payload.size(); ++i) {
            out.push_back(static_cast<std::uint8_t>(
                static_cast<std::uint8_t>(payload[i]) ^ mask[i % 4]));
        }

        return out;
    }

    bool writeAll(int fd, const std::vector<std::uint8_t>& data)
    {
        std::size_t off = 0;
        while (off < data.size()) {
            const ssize_t n = ::send(fd, data.data() + off, data.size() - off, MSG_NOSIGNAL);
            if (n <= 0)
                return false;
            off += static_cast<std::size_t>(n);
        }
        return true;
    }

    // Читает один серверный (unmasked) фрейм целиком.
    // Возвращает (opcode, payload). nullopt -> EOF/ошибка/невалидно.
    std::optional<std::pair<WsOpcode, std::string>>
    recvServerFrame(int fd)
    {
        std::uint8_t hdr[2];
        if (!recvExact(fd, hdr, 2))
            return std::nullopt;

        const auto opcode = static_cast<WsOpcode>(hdr[0] & 0x0F);
        if (hdr[1] & 0x80)
            return std::nullopt; // сервер не должен маскировать

        std::uint64_t len = hdr[1] & 0x7F;
        if (len == 126) {
            std::uint8_t ext[2];
            if (!recvExact(fd, ext, 2))
                return std::nullopt;
            len = (std::uint64_t { ext[0] } << 8) | ext[1];
        } else if (len == 127) {
            std::uint8_t ext[8];
            if (!recvExact(fd, ext, 8))
                return std::nullopt;
            len = 0;
            for (auto b : ext)
                len = (len << 8) | b;
        }

        std::string payload(static_cast<std::size_t>(len), '\0');
        if (len > 0) {
            if (!recvExact(fd, reinterpret_cast<std::uint8_t*>(payload.data()),
                    static_cast<std::size_t>(len)))
                return std::nullopt;
        }

        return std::pair { opcode, std::move(payload) };
    }

    // ============================================================
    //  Handshake
    // ============================================================

    inline void testHandshakeRfcExample(TestContext& ctx)
    {
        Request req;
        req.setMethod("GET");
        req.setHeader("Upgrade", "websocket");
        req.setHeader("Connection", "Upgrade");
        req.setHeader("Sec-WebSocket-Version", "13");
        req.setHeader("Sec-WebSocket-Key", "dGhlIHNhbXBsZSBub25jZQ==");

        auto accept = computeWebSocketAccept(req);
        KAS_TEST_CHECK_IMPL(ctx, accept.has_value());
        if (accept)
            KAS_TEST_CHECK_IMPL(ctx, *accept == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
    }

    inline void testHandshakeCaseInsensitive(TestContext& ctx)
    {
        Request req;
        req.setMethod("GET");
        req.setHeader("Upgrade", "WebSocket");
        req.setHeader("Connection", "keep-alive, UPGRADE");
        req.setHeader("Sec-WebSocket-Version", "13");
        req.setHeader("Sec-WebSocket-Key", "dGhlIHNhbXBsZSBub25jZQ==");

        auto accept = computeWebSocketAccept(req);
        KAS_TEST_CHECK_IMPL(ctx, accept.has_value());
        if (accept)
            KAS_TEST_CHECK_IMPL(ctx, *accept == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
    }

    inline void testHandshakeNotGet(TestContext& ctx)
    {
        Request req;
        req.setMethod("POST");
        req.setHeader("Upgrade", "websocket");
        req.setHeader("Connection", "Upgrade");
        req.setHeader("Sec-WebSocket-Version", "13");
        req.setHeader("Sec-WebSocket-Key", "dGhlIHNhbXBsZSBub25jZQ==");

        KAS_TEST_CHECK_IMPL(ctx, !computeWebSocketAccept(req).has_value());
    }

    inline void testHandshakeNoUpgrade(TestContext& ctx)
    {
        Request req;
        req.setMethod("GET");
        req.setHeader("Connection", "Upgrade");
        req.setHeader("Sec-WebSocket-Version", "13");
        req.setHeader("Sec-WebSocket-Key", "dGhlIHNhbXBsZSBub25jZQ==");

        KAS_TEST_CHECK_IMPL(ctx, !computeWebSocketAccept(req).has_value());
    }

    inline void testHandshakeWrongUpgrade(TestContext& ctx)
    {
        Request req;
        req.setMethod("GET");
        req.setHeader("Upgrade", "h2c");
        req.setHeader("Connection", "Upgrade");
        req.setHeader("Sec-WebSocket-Version", "13");
        req.setHeader("Sec-WebSocket-Key", "dGhlIHNhbXBsZSBub25jZQ==");

        KAS_TEST_CHECK_IMPL(ctx, !computeWebSocketAccept(req).has_value());
    }

    inline void testHandshakeNoConnectionUpgrade(TestContext& ctx)
    {
        Request req;
        req.setMethod("GET");
        req.setHeader("Upgrade", "websocket");
        req.setHeader("Connection", "keep-alive");
        req.setHeader("Sec-WebSocket-Version", "13");
        req.setHeader("Sec-WebSocket-Key", "dGhlIHNhbXBsZSBub25jZQ==");

        KAS_TEST_CHECK_IMPL(ctx, !computeWebSocketAccept(req).has_value());
    }

    inline void testHandshakeWrongVersion(TestContext& ctx)
    {
        Request req;
        req.setMethod("GET");
        req.setHeader("Upgrade", "websocket");
        req.setHeader("Connection", "Upgrade");
        req.setHeader("Sec-WebSocket-Version", "8");
        req.setHeader("Sec-WebSocket-Key", "dGhlIHNhbXBsZSBub25jZQ==");

        KAS_TEST_CHECK_IMPL(ctx, !computeWebSocketAccept(req).has_value());
    }

    inline void testHandshakeNoVersion(TestContext& ctx)
    {
        Request req;
        req.setMethod("GET");
        req.setHeader("Upgrade", "websocket");
        req.setHeader("Connection", "Upgrade");
        req.setHeader("Sec-WebSocket-Key", "dGhlIHNhbXBsZSBub25jZQ==");

        KAS_TEST_CHECK_IMPL(ctx, !computeWebSocketAccept(req).has_value());
    }

    inline void testHandshakeNoKey(TestContext& ctx)
    {
        Request req;
        req.setMethod("GET");
        req.setHeader("Upgrade", "websocket");
        req.setHeader("Connection", "Upgrade");
        req.setHeader("Sec-WebSocket-Version", "13");

        KAS_TEST_CHECK_IMPL(ctx, !computeWebSocketAccept(req).has_value());
    }

    inline void testHandshakeBadKeyLength(TestContext& ctx)
    {
        Request req;
        req.setMethod("GET");
        req.setHeader("Upgrade", "websocket");
        req.setHeader("Connection", "Upgrade");
        req.setHeader("Sec-WebSocket-Version", "13");
        req.setHeader("Sec-WebSocket-Key", "short");

        KAS_TEST_CHECK_IMPL(ctx, !computeWebSocketAccept(req).has_value());
    }

    // ============================================================
    //  readFrame
    // ============================================================

    inline void testReadTextFrame(TestContext& ctx)
    {
        SocketPair sp;
        KAS_TEST_CHECK_IMPL(ctx, sp.a >= 0 && sp.b >= 0);
        if (sp.a < 0)
            return;

        const std::string payload = "hello";
        KAS_TEST_CHECK_IMPL(ctx, writeAll(sp.a, makeClientFrame(WsOpcode::Text, payload)));

        auto frame = readFrame(sp.b);
        KAS_TEST_CHECK_IMPL(ctx, frame.has_value());
        if (frame) {
            KAS_TEST_CHECK_IMPL(ctx, frame->fin);
            KAS_TEST_CHECK_IMPL(ctx, frame->opcode == WsOpcode::Text);
            KAS_TEST_CHECK_IMPL(ctx, frame->masked);
            KAS_TEST_CHECK_IMPL(ctx, frame->payload == payload);
        }
    }

    inline void testReadBinaryFrame(TestContext& ctx)
    {
        SocketPair sp;
        KAS_TEST_CHECK_IMPL(ctx, sp.a >= 0 && sp.b >= 0);
        if (sp.a < 0)
            return;

        const std::string payload("\x00\x01\x02\xFF", 4);
        KAS_TEST_CHECK_IMPL(ctx, writeAll(sp.a, makeClientFrame(WsOpcode::Binary, payload)));

        auto frame = readFrame(sp.b);
        KAS_TEST_CHECK_IMPL(ctx, frame.has_value());
        if (frame) {
            KAS_TEST_CHECK_IMPL(ctx, frame->opcode == WsOpcode::Binary);
            KAS_TEST_CHECK_IMPL(ctx, frame->payload == payload);
        }
    }

    inline void testReadEmptyFrame(TestContext& ctx)
    {
        SocketPair sp;
        KAS_TEST_CHECK_IMPL(ctx, sp.a >= 0 && sp.b >= 0);
        if (sp.a < 0)
            return;

        KAS_TEST_CHECK_IMPL(ctx, writeAll(sp.a, makeClientFrame(WsOpcode::Text, "")));

        auto frame = readFrame(sp.b);
        KAS_TEST_CHECK_IMPL(ctx, frame.has_value());
        if (frame) {
            KAS_TEST_CHECK_IMPL(ctx, frame->payload.empty());
            KAS_TEST_CHECK_IMPL(ctx, frame->opcode == WsOpcode::Text);
        }
    }

    inline void testReadPing(TestContext& ctx)
    {
        SocketPair sp;
        KAS_TEST_CHECK_IMPL(ctx, sp.a >= 0 && sp.b >= 0);
        if (sp.a < 0)
            return;

        const std::string payload = "ping-payload";
        KAS_TEST_CHECK_IMPL(ctx, writeAll(sp.a, makeClientFrame(WsOpcode::Ping, payload)));

        auto frame = readFrame(sp.b);
        KAS_TEST_CHECK_IMPL(ctx, frame.has_value());
        if (frame) {
            KAS_TEST_CHECK_IMPL(ctx, frame->opcode == WsOpcode::Ping);
            KAS_TEST_CHECK_IMPL(ctx, frame->fin);
            KAS_TEST_CHECK_IMPL(ctx, frame->payload == payload);
        }
    }

    inline void testReadClose(TestContext& ctx)
    {
        SocketPair sp;
        KAS_TEST_CHECK_IMPL(ctx, sp.a >= 0 && sp.b >= 0);
        if (sp.a < 0)
            return;

        std::string payload;
        payload.push_back(static_cast<char>((1000 >> 8) & 0xFF));
        payload.push_back(static_cast<char>(1000 & 0xFF));
        payload += "bye";

        KAS_TEST_CHECK_IMPL(ctx, writeAll(sp.a, makeClientFrame(WsOpcode::Close, payload)));

        auto frame = readFrame(sp.b);
        KAS_TEST_CHECK_IMPL(ctx, frame.has_value());
        if (frame) {
            KAS_TEST_CHECK_IMPL(ctx, frame->opcode == WsOpcode::Close);
            KAS_TEST_CHECK_IMPL(ctx, frame->payload == payload);
        }
    }

    inline void testReadExtendedLength126(TestContext& ctx)
    {
        SocketPair sp;
        KAS_TEST_CHECK_IMPL(ctx, sp.a >= 0 && sp.b >= 0);
        if (sp.a < 0)
            return;

        const std::string payload(200, 'x');
        KAS_TEST_CHECK_IMPL(ctx, writeAll(sp.a, makeClientFrame(WsOpcode::Binary, payload)));

        auto frame = readFrame(sp.b);
        KAS_TEST_CHECK_IMPL(ctx, frame.has_value());
        if (frame) {
            KAS_TEST_CHECK_IMPL(ctx, frame->payload.size() == 200);
            KAS_TEST_CHECK_IMPL(ctx, frame->payload == payload);
        }
    }

    inline void testReadExtendedLength127(TestContext& ctx)
    {
        SocketPair sp;
        KAS_TEST_CHECK_IMPL(ctx, sp.a >= 0 && sp.b >= 0);
        if (sp.a < 0)
            return;

        const std::string payload(70000, 'y');
        KAS_TEST_CHECK_IMPL(ctx, writeAll(sp.a, makeClientFrame(WsOpcode::Binary, payload)));

        auto frame = readFrame(sp.b);
        KAS_TEST_CHECK_IMPL(ctx, frame.has_value());
        if (frame) {
            KAS_TEST_CHECK_IMPL(ctx, frame->payload.size() == 70000);
            KAS_TEST_CHECK_IMPL(ctx, frame->payload == payload);
        }
    }

    inline void testReadUnmaskedRejected(TestContext& ctx)
    {
        SocketPair sp;
        KAS_TEST_CHECK_IMPL(ctx, sp.a >= 0 && sp.b >= 0);
        if (sp.a < 0)
            return;

        std::vector<std::uint8_t> raw {
            0x81, // fin + text
            0x05, // len 5, mask = 0
            'h', 'e', 'l', 'l', 'o'
        };
        KAS_TEST_CHECK_IMPL(ctx, writeAll(sp.a, raw));

        auto frame = readFrame(sp.b);
        KAS_TEST_CHECK_IMPL(ctx, !frame.has_value());
    }

    inline void testReadControlTooLong(TestContext& ctx)
    {
        SocketPair sp;
        KAS_TEST_CHECK_IMPL(ctx, sp.a >= 0 && sp.b >= 0);
        if (sp.a < 0)
            return;

        const std::string payload(200, 'z');
        KAS_TEST_CHECK_IMPL(ctx, writeAll(sp.a, makeClientFrame(WsOpcode::Ping, payload)));

        auto frame = readFrame(sp.b);
        KAS_TEST_CHECK_IMPL(ctx, !frame.has_value());
    }

    inline void testReadControlFragmented(TestContext& ctx)
    {
        SocketPair sp;
        KAS_TEST_CHECK_IMPL(ctx, sp.a >= 0 && sp.b >= 0);
        if (sp.a < 0)
            return;

        KAS_TEST_CHECK_IMPL(ctx,
            writeAll(sp.a, makeClientFrame(WsOpcode::Ping, "p", false)));

        auto frame = readFrame(sp.b);
        KAS_TEST_CHECK_IMPL(ctx, !frame.has_value());
    }

    inline void testRoundTripViaClientFrame(TestContext& ctx)
    {
        SocketPair sp;
        KAS_TEST_CHECK_IMPL(ctx, sp.a >= 0 && sp.b >= 0);
        if (sp.a < 0)
            return;

        const std::string payload = "round-trip";
        KAS_TEST_CHECK_IMPL(ctx, writeAll(sp.a, makeClientFrame(WsOpcode::Text, payload)));

        auto frame = readFrame(sp.b);
        KAS_TEST_CHECK_IMPL(ctx, frame.has_value());
        if (frame)
            KAS_TEST_CHECK_IMPL(ctx, frame->payload == payload);
    }

    inline void testFragmentedReads(TestContext& ctx)
    {
        SocketPair sp;
        KAS_TEST_CHECK_IMPL(ctx, sp.a >= 0 && sp.b >= 0);
        if (sp.a < 0)
            return;

        auto f1 = makeClientFrame(WsOpcode::Text, "hel", false);
        auto f2 = makeClientFrame(WsOpcode::Continuation, "lo", true);

        std::vector<std::uint8_t> all;
        all.insert(all.end(), f1.begin(), f1.end());
        all.insert(all.end(), f2.begin(), f2.end());

        KAS_TEST_CHECK_IMPL(ctx, writeAll(sp.a, all));

        auto frame1 = readFrame(sp.b);
        KAS_TEST_CHECK_IMPL(ctx, frame1.has_value());
        if (frame1) {
            KAS_TEST_CHECK_IMPL(ctx, frame1->opcode == WsOpcode::Text);
            KAS_TEST_CHECK_IMPL(ctx, !frame1->fin);
            KAS_TEST_CHECK_IMPL(ctx, frame1->payload == "hel");
        }

        auto frame2 = readFrame(sp.b);
        KAS_TEST_CHECK_IMPL(ctx, frame2.has_value());
        if (frame2) {
            KAS_TEST_CHECK_IMPL(ctx, frame2->opcode == WsOpcode::Continuation);
            KAS_TEST_CHECK_IMPL(ctx, frame2->fin);
            KAS_TEST_CHECK_IMPL(ctx, frame2->payload == "lo");
        }
    }

    // ============================================================
    //  writeFrame (сервер -> клиент, unmasked)
    // ============================================================

    inline void testWriteTextFrame(TestContext& ctx)
    {
        SocketPair sp;
        KAS_TEST_CHECK_IMPL(ctx, sp.a >= 0 && sp.b >= 0);
        if (sp.a < 0)
            return;

        KAS_TEST_CHECK_IMPL(ctx, writeFrame(sp.a, WsOpcode::Text, "hello"));

        auto frame = recvServerFrame(sp.b);
        KAS_TEST_CHECK_IMPL(ctx, frame.has_value());
        if (frame) {
            KAS_TEST_CHECK_IMPL(ctx, frame->first == WsOpcode::Text);
            KAS_TEST_CHECK_IMPL(ctx, frame->second == "hello");
        }
    }

    inline void testWriteEmptyFrame(TestContext& ctx)
    {
        SocketPair sp;
        KAS_TEST_CHECK_IMPL(ctx, sp.a >= 0 && sp.b >= 0);
        if (sp.a < 0)
            return;

        KAS_TEST_CHECK_IMPL(ctx, writeFrame(sp.a, WsOpcode::Text, ""));

        auto frame = recvServerFrame(sp.b);
        KAS_TEST_CHECK_IMPL(ctx, frame.has_value());
        if (frame) {
            KAS_TEST_CHECK_IMPL(ctx, frame->first == WsOpcode::Text);
            KAS_TEST_CHECK_IMPL(ctx, frame->second.empty());
        }
    }

    inline void testWritePong(TestContext& ctx)
    {
        SocketPair sp;
        KAS_TEST_CHECK_IMPL(ctx, sp.a >= 0 && sp.b >= 0);
        if (sp.a < 0)
            return;

        KAS_TEST_CHECK_IMPL(ctx, writePong(sp.a, "pong"));

        auto frame = recvServerFrame(sp.b);
        KAS_TEST_CHECK_IMPL(ctx, frame.has_value());
        if (frame) {
            KAS_TEST_CHECK_IMPL(ctx, frame->first == WsOpcode::Pong);
            KAS_TEST_CHECK_IMPL(ctx, frame->second == "pong");
        }
    }

    inline void testWriteClose(TestContext& ctx)
    {
        SocketPair sp;
        KAS_TEST_CHECK_IMPL(ctx, sp.a >= 0 && sp.b >= 0);
        if (sp.a < 0)
            return;

        KAS_TEST_CHECK_IMPL(ctx, writeClose(sp.a, 1000, "bye"));

        auto frame = recvServerFrame(sp.b);
        KAS_TEST_CHECK_IMPL(ctx, frame.has_value());
        if (frame) {
            KAS_TEST_CHECK_IMPL(ctx, frame->first == WsOpcode::Close);
            KAS_TEST_CHECK_IMPL(ctx, frame->second.size() == 5);
            if (frame->second.size() == 5) {
                const std::uint16_t code = (static_cast<std::uint16_t>(
                                                static_cast<std::uint8_t>(frame->second[0]))
                                               << 8)
                    | static_cast<std::uint8_t>(frame->second[1]);
                KAS_TEST_CHECK_IMPL(ctx, code == 1000);
                KAS_TEST_CHECK_IMPL(ctx,
                    std::string_view(frame->second.data() + 2, 3) == "bye");
            }
        }
    }

    // ============================================================
    //  WebSocketConnection — readMessage
    // ============================================================

    inline void testConnReadSingleText(TestContext& ctx)
    {
        SocketPair sp;
        KAS_TEST_CHECK_IMPL(ctx, sp.a >= 0 && sp.b >= 0);
        if (sp.a < 0)
            return;

        KAS_TEST_CHECK_IMPL(ctx, writeAll(sp.a, makeClientFrame(WsOpcode::Text, "hello")));

        WebSocketConnection ws { sp.b };
        auto msg = ws.readMessage();
        KAS_TEST_CHECK_IMPL(ctx, msg.has_value());
        if (msg)
            KAS_TEST_CHECK_IMPL(ctx, *msg == "hello");
    }

    inline void testConnReadSingleBinary(TestContext& ctx)
    {
        SocketPair sp;
        KAS_TEST_CHECK_IMPL(ctx, sp.a >= 0 && sp.b >= 0);
        if (sp.a < 0)
            return;

        const std::string payload("\x00\x01\x02\xFF", 4);
        KAS_TEST_CHECK_IMPL(ctx, writeAll(sp.a, makeClientFrame(WsOpcode::Binary, payload)));

        WebSocketConnection ws { sp.b };
        auto msg = ws.readMessage();
        KAS_TEST_CHECK_IMPL(ctx, msg.has_value());
        if (msg)
            KAS_TEST_CHECK_IMPL(ctx, *msg == payload);
    }

    inline void testConnFragmentedMessage(TestContext& ctx)
    {
        SocketPair sp;
        KAS_TEST_CHECK_IMPL(ctx, sp.a >= 0 && sp.b >= 0);
        if (sp.a < 0)
            return;

        auto f1 = makeClientFrame(WsOpcode::Text, "hel", false);
        auto f2 = makeClientFrame(WsOpcode::Continuation, "lo, ", false);
        auto f3 = makeClientFrame(WsOpcode::Continuation, "world", true);

        std::vector<std::uint8_t> all;
        all.insert(all.end(), f1.begin(), f1.end());
        all.insert(all.end(), f2.begin(), f2.end());
        all.insert(all.end(), f3.begin(), f3.end());

        KAS_TEST_CHECK_IMPL(ctx, writeAll(sp.a, all));

        WebSocketConnection ws { sp.b };
        auto msg = ws.readMessage();
        KAS_TEST_CHECK_IMPL(ctx, msg.has_value());
        if (msg)
            KAS_TEST_CHECK_IMPL(ctx, *msg == "hello, world");
    }

    inline void testConnFragmentedEmptyFirst(TestContext& ctx)
    {
        SocketPair sp;
        KAS_TEST_CHECK_IMPL(ctx, sp.a >= 0 && sp.b >= 0);
        if (sp.a < 0)
            return;

        auto f1 = makeClientFrame(WsOpcode::Text, "", false);
        auto f2 = makeClientFrame(WsOpcode::Continuation, "abc", true);

        std::vector<std::uint8_t> all;
        all.insert(all.end(), f1.begin(), f1.end());
        all.insert(all.end(), f2.begin(), f2.end());
        KAS_TEST_CHECK_IMPL(ctx, writeAll(sp.a, all));

        WebSocketConnection ws { sp.b };
        auto msg = ws.readMessage();
        KAS_TEST_CHECK_IMPL(ctx, msg.has_value());
        if (msg)
            KAS_TEST_CHECK_IMPL(ctx, *msg == "abc");
    }

    inline void testConnAutoPong(TestContext& ctx)
    {
        SocketPair sp;
        KAS_TEST_CHECK_IMPL(ctx, sp.a >= 0 && sp.b >= 0);
        if (sp.a < 0)
            return;

        auto ping = makeClientFrame(WsOpcode::Ping, "ping!");
        auto text = makeClientFrame(WsOpcode::Text, "after-ping");

        std::vector<std::uint8_t> all;
        all.insert(all.end(), ping.begin(), ping.end());
        all.insert(all.end(), text.begin(), text.end());
        KAS_TEST_CHECK_IMPL(ctx, writeAll(sp.a, all));

        WebSocketConnection ws { sp.b };
        auto msg = ws.readMessage();
        KAS_TEST_CHECK_IMPL(ctx, msg.has_value());
        if (msg)
            KAS_TEST_CHECK_IMPL(ctx, *msg == "after-ping");

        auto server_frame = recvServerFrame(sp.a);
        KAS_TEST_CHECK_IMPL(ctx, server_frame.has_value());
        if (server_frame) {
            KAS_TEST_CHECK_IMPL(ctx, server_frame->first == WsOpcode::Pong);
            KAS_TEST_CHECK_IMPL(ctx, server_frame->second == "ping!");
        }
    }

    inline void testConnAutoClose(TestContext& ctx)
    {
        SocketPair sp;
        KAS_TEST_CHECK_IMPL(ctx, sp.a >= 0 && sp.b >= 0);
        if (sp.a < 0)
            return;

        std::string payload;
        payload.push_back(static_cast<char>((1000 >> 8) & 0xFF));
        payload.push_back(static_cast<char>(1000 & 0xFF));

        KAS_TEST_CHECK_IMPL(ctx,
            writeAll(sp.a, makeClientFrame(WsOpcode::Close, payload)));

        WebSocketConnection ws { sp.b };
        auto msg = ws.readMessage();
        KAS_TEST_CHECK_IMPL(ctx, !msg.has_value());

        auto server_frame = recvServerFrame(sp.a);
        KAS_TEST_CHECK_IMPL(ctx, server_frame.has_value());
        if (server_frame) {
            KAS_TEST_CHECK_IMPL(ctx, server_frame->first == WsOpcode::Close);
            KAS_TEST_CHECK_IMPL(ctx, server_frame->second.size() >= 2);
            if (server_frame->second.size() >= 2) {
                const std::uint16_t code = (static_cast<std::uint16_t>(
                                                static_cast<std::uint8_t>(server_frame->second[0]))
                                               << 8)
                    | static_cast<std::uint8_t>(server_frame->second[1]);
                KAS_TEST_CHECK_IMPL(ctx, code == 1000);
            }
        }
    }

    inline void testConnEof(TestContext& ctx)
    {
        SocketPair sp;
        KAS_TEST_CHECK_IMPL(ctx, sp.a >= 0 && sp.b >= 0);
        if (sp.a < 0)
            return;

        ::close(sp.a);
        sp.a = -1;

        WebSocketConnection ws { sp.b };
        auto msg = ws.readMessage();
        KAS_TEST_CHECK_IMPL(ctx, !msg.has_value());
    }

    inline void testConnTwoMessages(TestContext& ctx)
    {
        SocketPair sp;
        KAS_TEST_CHECK_IMPL(ctx, sp.a >= 0 && sp.b >= 0);
        if (sp.a < 0)
            return;

        auto f1 = makeClientFrame(WsOpcode::Text, "first");
        auto f2 = makeClientFrame(WsOpcode::Text, "second");

        std::vector<std::uint8_t> all;
        all.insert(all.end(), f1.begin(), f1.end());
        all.insert(all.end(), f2.begin(), f2.end());
        KAS_TEST_CHECK_IMPL(ctx, writeAll(sp.a, all));

        WebSocketConnection ws { sp.b };

        auto m1 = ws.readMessage();
        KAS_TEST_CHECK_IMPL(ctx, m1.has_value());
        if (m1)
            KAS_TEST_CHECK_IMPL(ctx, *m1 == "first");

        auto m2 = ws.readMessage();
        KAS_TEST_CHECK_IMPL(ctx, m2.has_value());
        if (m2)
            KAS_TEST_CHECK_IMPL(ctx, *m2 == "second");
    }

    // ============================================================
    //  WebSocketConnection — send
    // ============================================================

    inline void testConnSendText(TestContext& ctx)
    {
        SocketPair sp;
        KAS_TEST_CHECK_IMPL(ctx, sp.a >= 0 && sp.b >= 0);
        if (sp.a < 0)
            return;

        WebSocketConnection ws { sp.a };
        KAS_TEST_CHECK_IMPL(ctx, ws.sendText("hello"));

        auto frame = recvServerFrame(sp.b);
        KAS_TEST_CHECK_IMPL(ctx, frame.has_value());
        if (frame) {
            KAS_TEST_CHECK_IMPL(ctx, frame->first == WsOpcode::Text);
            KAS_TEST_CHECK_IMPL(ctx, frame->second == "hello");
        }
    }

    inline void testConnSendBinary(TestContext& ctx)
    {
        SocketPair sp;
        KAS_TEST_CHECK_IMPL(ctx, sp.a >= 0 && sp.b >= 0);
        if (sp.a < 0)
            return;

        WebSocketConnection ws { sp.a };
        const std::string payload("\x01\x02\x03", 3);
        KAS_TEST_CHECK_IMPL(ctx, ws.sendBinary(payload));

        auto frame = recvServerFrame(sp.b);
        KAS_TEST_CHECK_IMPL(ctx, frame.has_value());
        if (frame) {
            KAS_TEST_CHECK_IMPL(ctx, frame->first == WsOpcode::Binary);
            KAS_TEST_CHECK_IMPL(ctx, frame->second == payload);
        }
    }

    inline void testConnPing(TestContext& ctx)
    {
        SocketPair sp;
        KAS_TEST_CHECK_IMPL(ctx, sp.a >= 0 && sp.b >= 0);
        if (sp.a < 0)
            return;

        WebSocketConnection ws { sp.a };
        KAS_TEST_CHECK_IMPL(ctx, ws.ping("p"));

        auto frame = recvServerFrame(sp.b);
        KAS_TEST_CHECK_IMPL(ctx, frame.has_value());
        if (frame) {
            KAS_TEST_CHECK_IMPL(ctx, frame->first == WsOpcode::Ping);
            KAS_TEST_CHECK_IMPL(ctx, frame->second == "p");
        }
    }

    inline void testConnClose(TestContext& ctx)
    {
        SocketPair sp;
        KAS_TEST_CHECK_IMPL(ctx, sp.a >= 0 && sp.b >= 0);
        if (sp.a < 0)
            return;

        WebSocketConnection ws { sp.a };
        KAS_TEST_CHECK_IMPL(ctx, ws.close(1000, "bye"));

        auto frame = recvServerFrame(sp.b);
        KAS_TEST_CHECK_IMPL(ctx, frame.has_value());
        if (frame) {
            KAS_TEST_CHECK_IMPL(ctx, frame->first == WsOpcode::Close);
            KAS_TEST_CHECK_IMPL(ctx, frame->second.size() == 5);
            if (frame->second.size() == 5) {
                const std::uint16_t code = (static_cast<std::uint16_t>(
                                                static_cast<std::uint8_t>(frame->second[0]))
                                               << 8)
                    | static_cast<std::uint8_t>(frame->second[1]);
                KAS_TEST_CHECK_IMPL(ctx, code == 1000);
            }
        }
    }

} // namespace

// ============================================================
//  Точка входа
// ============================================================
inline bool runTests()
{
    TestContext ctx;

    // Handshake
    testHandshakeRfcExample(ctx);
    testHandshakeCaseInsensitive(ctx);
    testHandshakeNotGet(ctx);
    testHandshakeNoUpgrade(ctx);
    testHandshakeWrongUpgrade(ctx);
    testHandshakeNoConnectionUpgrade(ctx);
    testHandshakeWrongVersion(ctx);
    testHandshakeNoVersion(ctx);
    testHandshakeNoKey(ctx);
    testHandshakeBadKeyLength(ctx);

    // readFrame
    testReadTextFrame(ctx);
    testReadBinaryFrame(ctx);
    testReadEmptyFrame(ctx);
    testReadPing(ctx);
    testReadClose(ctx);
    testReadExtendedLength126(ctx);
    testReadExtendedLength127(ctx);
    testReadUnmaskedRejected(ctx);
    testReadControlTooLong(ctx);
    testReadControlFragmented(ctx);
    testRoundTripViaClientFrame(ctx);
    testFragmentedReads(ctx);

    // writeFrame
    testWriteTextFrame(ctx);
    testWriteEmptyFrame(ctx);
    testWritePong(ctx);
    testWriteClose(ctx);

    // WebSocketConnection — readMessage
    testConnReadSingleText(ctx);
    testConnReadSingleBinary(ctx);
    testConnFragmentedMessage(ctx);
    testConnAutoPong(ctx);
    testConnAutoClose(ctx);
    testConnEof(ctx);
    testConnTwoMessages(ctx);

    // WebSocketConnection — send
    testConnSendText(ctx);
    testConnSendBinary(ctx);
    testConnPing(ctx);
    testConnClose(ctx);

    testConnFragmentedEmptyFirst(ctx);

    return printSummary("kas::net::http::websocket::tests", ctx);
}

} // namespace kas::net::http::websocket::tests

#endif // KASLIBCXX_NET_HTTP_WEBSOCKET_TESTS