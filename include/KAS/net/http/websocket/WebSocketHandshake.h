/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_NET_HTTP_WEBSOCKET_HANDSHAKE_H
#define KASLIBCXX_NET_HTTP_WEBSOCKET_HANDSHAKE_H

#include <KAS/net/http/Request.h>

#include <optional>
#include <string>

namespace kas::net::http {

/**
 * @brief Computes the Sec-WebSocket-Accept value for an upgrade request.
 *
 * Validates that the given request is a correct WebSocket upgrade
 * (RFC 6455, section 4.2.1): checks the Upgrade / Connection headers, the
 * Sec-WebSocket-Version, and the presence of Sec-WebSocket-Key. On success,
 * returns the value to put into the Sec-WebSocket-Accept response header,
 * computed as base64(SHA-1(key + GUID)).
 *
 * @param request Incoming HTTP request to validate.
 * @return The Sec-WebSocket-Accept value on success, or std::nullopt if the
 *         request is not a WebSocket upgrade, uses an unsupported version,
 *         or lacks the required key.
 */
[[nodiscard]] std::optional<std::string> computeWebSocketAccept(const Request& request);

} // namespace kas::net::http

#endif