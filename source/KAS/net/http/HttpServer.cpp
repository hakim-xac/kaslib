// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include "KAS/net/http/HttpServer.h"
#include "KAS/net/general.h"
#include "KAS/net/http/HttpRange.h"
#include "KAS/net/http/ResponseTypes.h"
#include "KAS/net/http/websocket/WebSocketConnection.h"
#include "KAS/net/http/websocket/WebSocketHandshake.h"
#include "KAS/net/socket/SocketServer.h"

#include <cerrno>
#include <cstring>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>

namespace kas::net::http {
namespace {

    /**
     * @brief Builds an "Allow: ..." header value from the given methods.
     *
     * Joins the method names with ", " in the order they appear in
     * @p methods. The result is suitable for the Allow response header.
     *
     * @param methods Allowed HTTP methods.
     * @return Comma-separated list of method names (empty if @p methods
     *         is empty).
     */
    std::string makeAllowHeader(const std::vector<std::string_view>& methods)
    {
        std::string out;
        for (std::size_t i = 0; i < methods.size(); ++i) {
            if (i > 0)
                out += ", ";
            out += methods[i];
        }
        return out;
    }

    /**
     * @brief Creates a SocketServer from a source string and a TCP type.
     *
     * Throws std::runtime_error if SocketServer::make() fails (for
     * example, on an invalid source or socket creation error).
     *
     * @param source   Listen address: a Unix socket path (starts with
     *                 '/') or a "host:port" TCP address.
     * @param tcp_type Transport type passed to SocketServer::make().
     * @return The constructed SocketServer.
     */
    socket::SocketServer makeServer(std::string source, socket::TcpTypes tcp_type)
    {
        auto opt = socket::SocketServer::make(std::move(source), tcp_type);
        if (!opt)
            throw std::runtime_error { "HttpServer: failed to create SocketServer" };
        return std::move(*opt);
    }

    // ---------------------------------------------------------------------
    //  Response helpers
    // ---------------------------------------------------------------------

    /**
     * @brief Builds a 101 Switching Protocols response for a WebSocket upgrade.
     *
     * Sets the Upgrade, Connection, and Sec-WebSocket-Accept headers
     * required by RFC 6455, section 4.2.2.
     *
     * @param accept Value of the Sec-WebSocket-Accept header.
     * @return The 101 response.
     */
    Response makeWebSocketUpgradeResponse(std::string_view accept)
    {
        Response resp;
        resp.setResponseCode<response_types::SwitchingProtocols>();
        resp.setHeader("Upgrade", "websocket");
        resp.setHeader("Connection", "Upgrade");
        resp.setHeader("Sec-WebSocket-Accept", accept);
        return resp;
    }

    /**
     * @brief Builds a 400 Bad Request response with a plain-text message.
     *
     * @param message Body of the response.
     * @return The 400 response.
     */
    Response makeBadRequestResponse(std::string_view message)
    {
        Response resp;
        resp.setResponseCode<response_types::BadRequest>()
            .setContentType<ContentTypes::TXT>()
            .setBody(std::string { message });
        return resp;
    }

    /**
     * @brief Builds a 404 Not Found response with a plain-text body.
     */
    Response makeNotFoundResponse()
    {
        Response resp;
        resp.setResponseCode<response_types::NotFound>()
            .setContentType<ContentTypes::TXT>()
            .setBody("Not Found");
        return resp;
    }

    /**
     * @brief Builds a 405 Method Not Allowed response.
     *
     * Adds an Allow header listing @p allowed_methods when the list is
     * not empty.
     *
     * @param allowed_methods Methods allowed by the matched route.
     * @return The 405 response.
     */
    Response makeMethodNotAllowedResponse(
        const std::vector<std::string_view>& allowed_methods)
    {
        Response resp;
        resp.setResponseCode<response_types::MethodNotAllowed>()
            .setContentType<ContentTypes::TXT>()
            .setBody("Method Not Allowed");
        if (!allowed_methods.empty())
            resp.setHeader("Allow", makeAllowHeader(allowed_methods));
        return resp;
    }

    /**
     * @brief Builds a 500 Internal Server Error response with a plain-text body.
     */
    Response makeInternalServerErrorResponse()
    {
        Response resp;
        resp.setResponseCode<response_types::InternalServerError>()
            .setContentType<ContentTypes::TXT>()
            .setBody("Internal Server Error");
        return resp;
    }

    // ---------------------------------------------------------------------
    //  WebSocket
    // ---------------------------------------------------------------------

    /**
     * @brief Handles a request if its path matches a registered WebSocket route.
     *
     * If @p request.getPath() is not in @p ws_routes, returns false and
     * does nothing, so the caller can continue with normal HTTP routing.
     *
     * If the path matches:
     *  - when computeWebSocketAccept() succeeds, sends a 101 Switching
     *    Protocols response (headers only), overrides the socket timeouts
     *    with the WebSocket-specific ones, constructs a
     *    WebSocketConnection around @p fd, and invokes @p run_session with
     *    the connection and the route's handler; @p run_session is
     *    expected to run the whole session and return when it ends;
     *  - otherwise sends a 400 Bad Request response indicating that a
     *    WebSocket upgrade is required.
     *
     * In both cases returns true: the request is considered handled,
     * and the caller must not produce another HTTP response. Keep-alive
     * has no meaning here: the connection is closed after the WebSocket
     * session ends.
     *
     * @tparam WsRoutes  Map type: path -> ws_handler_t.
     * @tparam RunSession Callable invoked as run_session(ws, handler).
     * @param request      Incoming HTTP request.
     * @param fd           Socket file descriptor.
     * @param ws_routes    Registered WebSocket routes.
     * @param run_session  Session runner.
     * @param recv_timeout Receive timeout to install for the WS session.
     * @param send_timeout Send timeout to install for the WS session.
     * @return true if the request matched a WebSocket route and was
     *         handled (upgrade or 400), false otherwise.
     */
    template <typename WsRoutes, typename RunSession>
    bool handleWebSocketIfMatched(
        const Request& request,
        int fd,
        const WsRoutes& ws_routes,
        RunSession&& run_session,
        std::chrono::milliseconds recv_timeout,
        std::chrono::milliseconds send_timeout)
    {
        const auto ws_it = ws_routes.find(request.getPath());
        if (ws_it == ws_routes.end())
            return false;

        if (auto accept = computeWebSocketAccept(request)) {
            Response resp = makeWebSocketUpgradeResponse(*accept);
            if (!resp.sendTo(fd, /*skip_body=*/true))
                return true; // already responded (or error) - close the connection

            // The HTTP keep-alive timeout does not apply to a WebSocket
            // session; override it with the WS-specific one (or 0 = blocking).
            (void)general::setReceiveTimeoutMs(fd, recv_timeout);
            (void)general::setSendTimeoutMs(fd, send_timeout);

            WebSocketConnection ws { fd };
            run_session(ws, ws_it->second);
            return true;
        }

        // Not an upgrade -> 400.
        Response resp = makeBadRequestResponse("WebSocket upgrade required");
        (void)resp.sendTo(fd);
        return true;
    }

    // ---------------------------------------------------------------------
    //  Routes
    // ---------------------------------------------------------------------

    /**
     * @brief Applies the result of HttpRoute::call() to @p response.
     *
     * On success, does nothing (the handler has already populated
     * @p response). On error, maps the route error to a response:
     *  - ERROR_METHOD_NOT_ALLOWED -> 405 with an Allow header;
     *  - ERROR_NO_HANDLER         -> 500;
     *  - any other error string   -> 400 with that string as the body.
     *
     * @param route   Route that was called (used for its allowed methods).
     * @param result  Result returned by route.call().
     * @param response Response to populate on error.
     */
    void applyRouteResult(
        const HttpRoute& route,
        expected_string<void>&& result,
        Response& response)
    {
        if (result.has_value())
            return;

        const std::string& err = result.error();

        if (err == HttpRoute::ERROR_METHOD_NOT_ALLOWED) {
            response = makeMethodNotAllowedResponse(route.getAllowedMethods());
        } else if (err == HttpRoute::ERROR_NO_HANDLER) {
            response = makeInternalServerErrorResponse();
        } else {
            response = makeBadRequestResponse(err);
        }
    }

    // ---------------------------------------------------------------------
    //  Range
    // ---------------------------------------------------------------------

    /**
     * @brief Applies a single byte range to a string body.
     *
     * Replaces the response body with the substring [start, start + len).
     *
     * @param response Response whose body is a string.
     * @param start    Start offset in the body.
     * @param len      Number of bytes to keep.
     */
    void applyStringSingleRange(
        Response& response,
        std::size_t start,
        std::size_t len)
    {
        std::string sliced = response.getBody().substr(start, len);
        response.setBody(std::move(sliced));
    }

    /**
     * @brief Applies a single byte range to a file body.
     *
     * Updates the file window in place, without reading the file into
     * memory: the new window starts at the file's base offset plus
     * @p start and spans @p len bytes.
     *
     * @param response Response whose body is a file.
     * @param start    Start offset relative to the current file window.
     * @param len      Number of bytes in the new window.
     */
    void applyFileSingleRange(
        Response& response,
        std::size_t start,
        std::size_t len)
    {
        const std::size_t base = response.getFileStartOffset();
        response.setFileRange(base + start, len);
    }

    /**
     * @brief Applies multiple byte ranges to a string body (multipart).
     *
     * Builds a multipart/byteranges body with a fresh random boundary,
     * sets the response status to 206 Partial Content, and sets the
     * Content-Type (with the boundary) and Content-Range headers.
     *
     * @param response Response whose body is a string.
     * @param ranges   Resolved (start, length) ranges.
     * @param total    Total size of the original body.
     */
    void applyStringMultiRange(
        Response& response,
        const std::vector<std::pair<std::size_t, std::size_t>>& ranges,
        std::size_t total)
    {
        const auto boundary = makeBoundary();
        const std::string content_type { response.getContentType() };

        std::string multipart = buildMultipartByteranges(
            boundary, response.getBody(), ranges, content_type, total);

        response.setResponseCode<response_types::PartialContent>();
        response.setHeader("Content-Type",
            "multipart/byteranges; boundary=" + boundary);
        response.setHeader("Content-Range",
            "bytes 0-" + std::to_string(total - 1) + "/" + std::to_string(total));
        response.setBody(std::move(multipart));
    }

    /**
     * @brief Applies a single byte range: 206, Content-Range, sliced body.
     *
     * Works for both string and file bodies: the file case updates the
     * file window in place, the string case replaces the body with the
     * requested slice.
     *
     * @param response Response to modify.
     * @param start    Start offset.
     * @param len      Number of bytes in the range.
     * @param total    Total size of the original body.
     */
    void applySingleRange(
        Response& response,
        std::size_t start,
        std::size_t len,
        std::size_t total)
    {
        response.setResponseCode<response_types::PartialContent>();
        response.setHeader("Content-Range",
            "bytes " + std::to_string(start) + "-"
                + std::to_string(start + len - 1) + "/" + std::to_string(total));

        if (response.hasFileBody())
            applyFileSingleRange(response, start, len);
        else
            applyStringSingleRange(response, start, len);
    }

    /**
     * @brief Turns the response into 416 Range Not Satisfiable.
     *
     * Sets the status to 416, adds "Content-Range: bytes * / <total>"
     * (without spaces around the slash), and clears the body.
     *
     * @param response Response to modify.
     * @param total    Total size of the original body.
     */
    void applyRangeNotSatisfiable(Response& response, std::size_t total)
    {
        response.setResponseCode<response_types::RangeNotSatisfiable>();
        response.setHeader("Content-Range", "bytes */" + std::to_string(total));
        response.clearBody();
    }

    /**
     * @brief Applies a Range request header to a 200 response, if present.
     *
     * Does nothing when:
     *  - the response status is not 200;
     *  - the response already has a Content-Range header;
     *  - the request has no Range header;
     *  - the Range header is syntactically invalid (the header is
     *    ignored and the full 200 response is kept, per RFC 9110);
     *  - the resolved ranges fully cover the body (the response stays
     *    200 with the full body).
     *
     * Otherwise:
     *  - a single satisfiable range yields 206 with a Content-Range
     *    header and a sliced body (string or file);
     *  - multiple satisfiable ranges on a string body yield a 206
     *    multipart/byteranges response;
     *  - multiple ranges on a file body are ignored (RFC 9110, section
     *    14.2 permits this), and the response stays 200;
     *  - no satisfiable ranges yield 416 Range Not Satisfiable.
     *
     * @param request  Incoming request (source of the Range header).
     * @param response Response to modify.
     */
    void applyRangeIfRequested(
        const Request& request,
        Response& response)
    {
        if (response.getStatusCode() != 200
            || response.hasHeader("Content-Range"))
            return;

        const auto range_str = request.getHeader("Range");
        if (!range_str)
            return;

        const auto spec = parseRangeHeader(*range_str);
        if (!spec || !spec->valid)
            return; // malformed Range -> ignore, keep 200

        const std::size_t total = response.getBodySize();

        const auto resolved = resolveByteRanges(*spec, total);
        if (!resolved) {
            applyRangeNotSatisfiable(response, total);
            return;
        }

        const auto& ranges = *resolved;

        // Full coverage -> keep the whole 200 response.
        if (ranges.size() == 1
            && ranges[0].first == 0
            && ranges[0].second == total)
            return;

        if (ranges.size() == 1) {
            const auto [start, len] = ranges.front();
            applySingleRange(response, start, len, total);
        } else if (response.hasFileBody()) {
            // Multi-range for a file: RFC 9110, section 14.2 allows ignoring it.
        } else {
            applyStringMultiRange(response, ranges, total);
        }
    }

    // ---------------------------------------------------------------------
    //  Miscellaneous
    // ---------------------------------------------------------------------

    /**
     * @brief Adds "Accept-Ranges: bytes" to a 200 response if not present.
     *
     * Only 200 responses are touched; other statuses are left alone.
     * An existing Accept-Ranges header is preserved.
     *
     * @param response Response to modify.
     */
    void setAcceptRangesIfNeeded(Response& response)
    {
        if (response.getStatusCode() == 200
            && !response.hasHeader("Accept-Ranges"))
            response.setHeader("Accept-Ranges", "bytes");
    }

    /**
     * @brief Sets the Connection header according to @p keep_alive.
     *
     * @param response   Response to modify.
     * @param keep_alive true -> "keep-alive", false -> "close".
     */
    void setConnectionHeader(Response& response, bool keep_alive)
    {
        response.setHeader("Connection", keep_alive ? "keep-alive" : "close");
    }

} // namespace

//------------------

HttpServer::HttpServer(std::string source, std::size_t worker_count)
    : HttpServer { std::move(source), worker_count, std::stop_token { } }
{
}

HttpServer::HttpServer(std::string source,
    std::size_t worker_count,
    std::stop_token stop_token)
    : m_source { source }
    , m_tcp_type { (!source.empty() && source.front() == '/')
            ? socket::TcpTypes::UNIX
            : socket::TcpTypes::TCP }
    , m_server { makeServer(source, m_tcp_type) }
    , m_pool { worker_count }
    , m_stop_token { std::move(stop_token) }
{
    m_server.setNoBlocking();

    if (m_stop_token.stop_possible()) {
        m_stop_cb.emplace(m_stop_token, [this] { stop(); });
    }
}

//------------------

HttpServer::~HttpServer()
{
    stop();
}

//------------------

HttpServer& HttpServer::route(std::string path, HttpRoute http_route) &
{
    m_routes.insert_or_assign(std::move(path), std::move(http_route));
    return *this;
}

//------------------

HttpServer&& HttpServer::route(std::string path, HttpRoute http_route) &&
{
    m_routes.insert_or_assign(std::move(path), std::move(http_route));
    return std::move(*this);
}

//------------------

HttpServer& HttpServer::routeWebSocket(std::string path, ws_handler_t handler) &
{
    m_ws_routes.insert_or_assign(std::move(path), std::move(handler));
    return *this;
}

//------------------

HttpServer&& HttpServer::routeWebSocket(std::string path, ws_handler_t handler) &&
{
    m_ws_routes.insert_or_assign(std::move(path), std::move(handler));
    return std::move(*this);
}

//------------------

bool HttpServer::start()
{
    if (m_running.load(std::memory_order_acquire))
        return false;

    if (m_stop_token.stop_requested())
        return false;

    // If the socket is closed (after stop()), recreate it.
    if (!m_server.isSocketCreated()) {
        auto opt = socket::SocketServer::make(m_source, m_tcp_type);
        if (!opt)
            return false;
        m_server = std::move(*opt);
        m_server.setNoBlocking();
    }

    if (!m_server.listen())
        return false;

    m_pool.start();
    m_running.store(true, std::memory_order_release);

    // Re-check the token: a request_stop() may have arrived between the
    // check above and the m_running store. If so, stop() was already
    // called by the stop callback, but at that moment m_running was still
    // false, so it returned without doing anything. Stop now that
    // m_running is visible.
    if (m_stop_token.stop_requested()) {
        stop();
        return false;
    }

    const bool ok = m_pool.push_task([this] { acceptLoop(); });
    if (!ok) {
        m_running.store(false, std::memory_order_release);
        m_pool.stop();
        return false;
    }

    return true;
}

//------------------

void HttpServer::stop()
{
    if (!m_running.exchange(false, std::memory_order_acq_rel))
        return;
    m_server.closeSocket();
    m_ws_cv.notify_all();
    m_pool.wait();
    m_wait_cv.notify_all();
}

//------------------

bool HttpServer::isRunning() const noexcept
{
    return m_running.load(std::memory_order_acquire);
}

//------------------

HttpServer::socket_t HttpServer::getSocketID() const noexcept
{
    return m_server.getSocketID();
}

//------------------

void HttpServer::acceptLoop()
{
    const auto server_fd = m_server.getSocketID();

    while (m_running.load(std::memory_order_acquire)) {
        // Wait for the listening socket to become readable (POLLIN) with
        // a timeout, so that m_running is re-checked periodically.
        pollfd pfd { server_fd, POLLIN, 0 };
        const int rc = ::poll(&pfd, 1, 100); // 100 ms

        if (rc < 0) {
            if (errno == EINTR)
                continue;
            if (!m_running.load(std::memory_order_acquire))
                break;
            continue;
        }
        if (rc == 0)
            continue; // timeout - re-check m_running

        // Socket is ready - accept() will not return EAGAIN.
        auto connection_opt = kas::net::socket::ClientConnection::getClientConnection(server_fd);
        if (!connection_opt) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                continue;
            if (errno == EINTR)
                continue;
            if (!m_running.load(std::memory_order_acquire))
                break;
            continue;
        }

        // Hand the connection to the pool. ClientConnection is
        // move-only, which is fine here.
        m_pool.push_task(
            [this, conn = std::move(*connection_opt)]() mutable {
                handleConnection(conn);
            });
    }
}

//------------------

HttpServer& HttpServer::setClientReceiveTimeoutMs(client_timeout_ms_t timeout_ms) &
{
    m_client_receive_timeout_ms = timeout_ms;
    return *this;
}

//------------------

HttpServer& HttpServer::setClientSendTimeoutMs(client_timeout_ms_t timeout_ms) &
{
    m_client_send_timeout_ms = timeout_ms;
    return *this;
}

//------------------

HttpServer& HttpServer::setWebSocketPingInterval(ws_ping_interval_t interval) &
{
    m_ws_ping_interval = interval;
    return *this;
}

//------------------

HttpServer& HttpServer::setWebSocketPongTimeout(ws_pong_timeout_t timeout) &
{
    m_ws_pong_timeout = timeout;
    return *this;
}

//------------------

void HttpServer::handleConnection(kas::net::socket::ClientConnection& connection)
{
    const int fd = connection.getSocketID();
    if (fd >= 0) {
        general::setReceiveTimeoutMs(fd, m_client_receive_timeout_ms);
        general::setSendTimeoutMs(fd, m_client_send_timeout_ms);
    }

    // While keep-alive and the server is running, read the next request.
    while (m_running.load(std::memory_order_acquire)) {
        if (!processRequest(connection))
            break;
    }
}

//------------------

bool HttpServer::processRequest(kas::net::socket::ClientConnection& connection)
{
    auto request_opt = Request::make(connection);
    if (!request_opt)
        return false;

    const int fd = connection.getSocketID();
    Request& request = request_opt.value();

    // 1. WebSocket (handled here, terminates the connection).
    if (handleWebSocketIfMatched(
            request, fd, m_ws_routes,
            [this](WebSocketConnection& ws, const ws_handler_t& handler) {
                runWebSocketSession(ws, handler);
            },
            /* recv_timeout = */ std::chrono::milliseconds { 0 }, // blocking; heartbeat handles liveness
            /* send_timeout = */ m_client_send_timeout_ms))
        return false;

    // 2. Regular request: route or static file.
    Response response = buildRouteResponse(request);

    // 3. Range.
    setAcceptRangesIfNeeded(response);
    applyRangeIfRequested(request, response);

    // 4. Send.
    const bool keep_alive = request.isKeepAlive();
    setConnectionHeader(response, keep_alive);

    const bool is_head = (request.getMethod() == "HEAD");
    if (!response.sendTo(fd, is_head))
        return false;

    return keep_alive;
}

//------------------

void HttpServer::runWebSocketSession(WebSocketConnection& ws, const ws_handler_t& handler)
{
    const auto fd = ws.fd();
    std::atomic<bool> stop { false };

    // Heartbeat thread: wait for the ping interval, or until the server
    // stops or the session ends; on wakeup, either ping or shutdown().
    kas::Thread heartbeat;
    if (m_ws_ping_interval.count() > 0) {
        heartbeat = kas::Thread([&, interval = m_ws_ping_interval, pong_timeout = m_ws_pong_timeout] {
            std::mutex mx;
            std::unique_lock lock { mx };

            while (!stop.load(std::memory_order_acquire)
                && m_running.load(std::memory_order_acquire)) {

                // Sleep for the ping interval, but wake up early if the
                // server stops or the session ends.
                const bool interrupted = m_ws_cv.wait_for(lock, interval, [&] {
                    return stop.load(std::memory_order_acquire)
                        || !m_running.load(std::memory_order_acquire);
                });
                if (interrupted) {
                    ::shutdown(fd, SHUT_RDWR);
                    break;
                }

                const auto before = ws.lastActivity();
                if (!ws.ping()) {
                    ::shutdown(fd, SHUT_RDWR);
                    break;
                }

                // Wait up to pong_timeout for any inbound frame.
                const auto deadline = std::chrono::steady_clock::now() + pong_timeout;
                while (std::chrono::steady_clock::now() < deadline) {
                    if (ws.lastActivity() > before)
                        break;
                    if (stop.load(std::memory_order_acquire)
                        || !m_running.load(std::memory_order_acquire))
                        break;
                    std::this_thread::sleep_for(std::chrono::milliseconds { 50 });
                }

                if (!m_running.load(std::memory_order_acquire)
                    || stop.load(std::memory_order_acquire)) {
                    ::shutdown(fd, SHUT_RDWR);
                    break;
                }

                if (ws.lastActivity() <= before
                    && std::chrono::steady_clock::now() >= deadline) {
                    // No response within pong_timeout -> close the socket.
                    ::shutdown(fd, SHUT_RDWR);
                    break;
                }
            }
        });
    }

    // The user handler owns the session: it reads and writes messages
    // itself (for example, by looping on ws.readMessage()). We only
    // provide the heartbeat and make sure the socket is unblocked when
    // the handler returns.
    handler(ws);

    stop.store(true, std::memory_order_release);
    m_ws_cv.notify_all(); // wake the heartbeat if it is still waiting
    if (heartbeat.joinable())
        heartbeat.join();

    // Best-effort Close frame, then make sure the fd is unblocked.
    (void)ws.close(1001); // 1001 = "going away"
    ::shutdown(fd, SHUT_RDWR);
}

//------------------

Response HttpServer::buildRouteResponse(const Request& request) const
{
    const auto it = m_routes.find(request.getPath());
    if (it == m_routes.end()) {
        Response response;
        if (tryServeStaticFile(request.getPath(), response))
            return response;
        return makeNotFoundResponse();
    }

    Response response;
    const HttpRoute& route = it->second;
    auto result = route.call(request, response);
    applyRouteResult(route, std::move(result), response);
    return response;
}

//------------------

bool HttpServer::tryServeStaticFile(std::string_view endpoint, Response& response) const
{
    if (!m_dir_with_static_files_opt)
        return false;

    auto file_path_opt = tryResolveStaticPath(endpoint, *m_dir_with_static_files_opt);
    if (!file_path_opt)
        return false;

    response.setResponseCode<response_types::OK>();
    response.setFileBody(*file_path_opt);
    response.setHeader("Accept-Ranges", "bytes");
    return true;
}

//------------------

HttpServer& HttpServer::setDirWithStaticFiles(std::filesystem::path static_dir) &
{
    m_dir_with_static_files_opt.emplace(std::move(static_dir));
    return *this;
}

//------------------

HttpServer&& HttpServer::setDirWithStaticFiles(std::filesystem::path static_dir) &&
{
    m_dir_with_static_files_opt.emplace(std::move(static_dir));
    return std::move(*this);
}

//------------------

void HttpServer::wait()
{
    std::unique_lock lock { m_wait_mx };
    m_wait_cv.wait(lock, [this] {
        return !m_running.load(std::memory_order_acquire);
    });
}

//------------------

} // namespace kas::net::http