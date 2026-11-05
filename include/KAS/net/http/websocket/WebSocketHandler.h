/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_NET_HTTP_WEBSOCKET_HANDLER_H
#define KASLIBCXX_NET_HTTP_WEBSOCKET_HANDLER_H

#include "WebSocketConnection.h"

#include <KAS/functional/MoveOnlyFunction.h>

namespace kas::net::http {

/**
 * @brief Handler invoked after a successful WebSocket handshake.
 *
 * The handler owns the per-connection session logic and runs its own loop
 * over the given connection (for example, @c readMessage / @c sendText)
 * until the connection is closed. The reference is valid only for the
 * duration of the call; it must not be stored beyond that.
 *
 * Move-only by design: the handler is meant to be passed by value into
 * the server and consumed when the connection is established.
 */
using ws_handler_t = functional::MoveOnlyFunction<void(WebSocketConnection&)>;

} // namespace kas::net::http

#endif