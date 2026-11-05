// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include "KAS/net/http/websocket/WebSocketHandshake.h"
#include "KAS/convert/base64/base64.h"
#include "KAS/convert/sha1/sha1.h"
#include "KAS/utils/utils.h"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace kas::net::http {

namespace {
    /**
     * @brief Magic GUID from RFC 6455, section 1.3.
     *
     * Concatenated with the client's Sec-WebSocket-Key before hashing:
     * accept = base64(SHA-1(key + WS_GUID)).
     */
    constexpr std::string_view WS_GUID { "258EAFA5-E914-47DA-95CA-C5AB0DC85B11" };
}

//------------------
/**
 * @brief Computes the Sec-WebSocket-Accept value for an upgrade request.
 *
 * Validates that @p request is a correct WebSocket upgrade per RFC 6455,
 * section 4.2.1:
 *  1. the method is GET;
 *  2. the Upgrade header equals "websocket" (case-insensitive);
 *  3. the Connection header contains the "upgrade" token (it may be
 *     part of a comma-separated list, e.g. "keep-alive, Upgrade");
 *  4. Sec-WebSocket-Version is exactly "13";
 *  5. Sec-WebSocket-Key is a base64-encoded 16-byte value, i.e. 22
 *     characters without padding or 24 characters with "==" padding.
 *
 * On success, returns base64(SHA-1(key + GUID)) as the value to put
 * into the Sec-WebSocket-Accept response header.
 *
 * @param request Incoming HTTP request to validate.
 * @return The Sec-WebSocket-Accept value on success, or std::nullopt if
 *         the request is not a valid WebSocket upgrade.
 */
std::optional<std::string>
computeWebSocketAccept(const Request& request)
{
    // 1. Method must be GET.
    if (request.getMethod() != "GET")
        return std::nullopt;

    // 2. Upgrade: websocket (case-insensitive).
    auto upgrade = request.getHeader("Upgrade");
    if (!upgrade || !utils::iequals(*upgrade, "websocket"))
        return std::nullopt;

    // 3. Connection contains the "upgrade" token (may be
    //    "keep-alive, Upgrade").
    auto connection = request.getHeader("Connection");
    if (!connection || !utils::containsToken(*connection, "upgrade"))
        return std::nullopt;

    // 4. Sec-WebSocket-Version must be exactly 13.
    auto version = request.getHeader("Sec-WebSocket-Version");
    if (!version || *version != "13")
        return std::nullopt;

    // 5. Sec-WebSocket-Key is base64 of 16 bytes: 22 characters without
    //    padding, or 24 characters with "==" padding.
    auto key = request.getHeader("Sec-WebSocket-Key");
    if (!key || (key->size() != 22 && key->size() != 24))
        return std::nullopt;

    // 6. accept = base64(SHA-1(key + GUID)).
    std::string concat { *key };
    concat += WS_GUID;

    const auto digest = kas::convert::sha1::v1::hashBinary(std::move(concat));

    return kas::convert::base64::v1::encode(
        std::span<const std::uint8_t> { digest.data(), digest.size() });
}

//------------------

} // namespace kas::net::http