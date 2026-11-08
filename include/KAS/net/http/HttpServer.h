/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_NET_HTTP_HTTP_SERVER_H
#define KASLIBCXX_NET_HTTP_HTTP_SERVER_H

#include "HttpRoute.h"
#include "Methods.h"
#include "Request.h"
#include "Response.h"
#include "StaticFile.h"
#include "websocket/WebSocketHandler.h"

#include <KAS/functional/MoveOnlyFunction.h>
#include <KAS/net/socket/ClientConnection.h>
#include <KAS/net/socket/SocketServer.h>
#include <KAS/thread/ThreadPool.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace kas::net::http {

/**
 * @brief Multi-threaded HTTP/WebSocket server.
 *
 * Accepts TCP connections, dispatches each connection to a worker from an
 * internal thread pool, and serves requests using a fixed set of routes:
 * plain HTTP routes (see @ref route) and WebSocket routes (see
 * @ref routeWebSocket). Requests that do not match any route may optionally
 * be served from a static directory (see @ref setDirWithStaticFiles).
 *
 * Typical lifecycle:
 * @code
 * HttpServer server("127.0.0.1:8080", 4);
 * server.route("/ping", HttpRoute{}.setMethod<Methods::GET>()
 *                                  .setHandler([](const Request&, Response&) { ... }));
 * server.start();
 * // ...
 * server.stop();
 * @endcode
 *
 * The class is non-copyable and non-movable; it must be created and kept
 * in a stable location (for example, on the stack of the thread that owns
 * the server loop).
 *
 * @note After @ref start returns true, the server owns a listening socket
 *       and a worker pool. Call @ref stop to release them. The destructor
 *       calls stop() as a safety net, but relying on it is not a
 *       substitute for an explicit stop(): the destructor cannot be
 *       called from a signal handler and does not give the caller a
 *       chance to observe the shutdown.
 *
 * @par Thread safety
 * start(), stop(), wait() and isRunning() may be called from different
 * threads. stop() is idempotent and may be called concurrently with
 * start() (in which case start() fails and the server stays stopped).
 * The route registration methods are not synchronized and must be called
 * before start() or from a single thread.
 *
 * @par WebSocket ping/pong
 * For WebSocket sessions the server runs a heartbeat: it sends a Ping
 * every @ref setWebSocketPingInterval and closes the connection if no
 * Pong (or any inbound frame) arrives within @ref setWebSocketPongTimeout.
 * Setting the ping interval to zero disables the heartbeat and leaves the
 * connection in blocking I/O mode.
 */
class HttpServer final {
public:
    /// Endpoint handler type used by HTTP routes.
    using handler_t = HttpRoute::end_point_handler_t;
    /// WebSocket handler type, invoked after a successful handshake.
    using ws_handler_t = kas::functional::MoveOnlyFunction<void(WebSocketConnection&)>;
    /// Socket handle type used by the underlying socket server.
    using socket_t = socket::SocketServer::socket_t;
    /// Timeout type for client receive/send operations.
    using client_timeout_ms_t = std::chrono::milliseconds;

    /// Default receive timeout for a client connection.
    constexpr static client_timeout_ms_t DEFAULT_CLIENT_RECEIVE_TIMEOUT_MS { 5000 };
    /// Default send timeout for a client connection.
    constexpr static client_timeout_ms_t DEFAULT_CLIENT_SEND_TIMEOUT_MS { 5000 };

    /// WebSocket ping interval (0 disables server-initiated pings).
    using ws_ping_interval_t = std::chrono::milliseconds;
    /// How long to wait for a Pong after a Ping before closing the session.
    using ws_pong_timeout_t = std::chrono::milliseconds;

    /// Default: ping every 30 seconds.
    constexpr static ws_ping_interval_t DEFAULT_WS_PING_INTERVAL_MS { 30'000 };
    /// Default: close if no Pong within 10 seconds.
    constexpr static ws_pong_timeout_t DEFAULT_WS_PONG_TIMEOUT_MS { 10'000 };

    /**
     * @brief Constructs a server bound to the given source.
     *
     * The server does not observe any stop token; use @ref stop to stop
     * it. This constructor is equivalent to passing an empty
     * std::stop_token{} to the three-argument constructor.
     *
     * @param source       Listen address in the form "host:port" (for
     *                     example, "0.0.0.0:8080") or a Unix socket path
     *                     starting with '/'.
     * @param worker_count Number of worker threads in the pool.
     */
    explicit HttpServer(std::string source, std::size_t worker_count = 4);

    /**
     * @brief Constructs a server that stops when @p stop_token requests it.
     *
     * If @p stop_token has stop_possible() == true, a stop callback is
     * registered that calls stop() as soon as request_stop() is invoked on
     * the corresponding std::stop_source. This makes it possible to stop
     * the server from outside (for example, from a signal-handling thread
     * that forwards a signal to a stop_source).
     *
     * If @p stop_token is an empty token (stop_possible() == false), the
     * server behaves exactly like the two-argument constructor.
     *
     * A stop request that arrives before @ref start returns makes start()
     * fail: the server is never started in that case.
     *
     * @param source       Listen address ("host:port") or a Unix socket
     *                     path starting with '/'.
     * @param worker_count Number of worker threads in the pool.
     * @param stop_token   Token to observe for a stop request.
     */
    explicit HttpServer(std::string source, std::size_t worker_count, std::stop_token stop_token);

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;
    HttpServer(HttpServer&&) noexcept = delete;
    HttpServer& operator=(HttpServer&&) noexcept = delete;

    /**
     * @brief Stops the server if it is still running.
     *
     * Equivalent to calling stop(); provided so that a forgotten stop()
     * does not leave the accept loop running against a destroyed object.
     */
    ~HttpServer();

    /**
     * @brief Registers an HTTP route for an exact path.
     *
     * Re-registering the same path replaces the previously registered route.
     *
     * @param path      Exact request path.
     * @param http_route Route to register.
     * @return Reference to this server (lvalue overload).
     */
    HttpServer& route(std::string path, HttpRoute http_route) &;

    /**
     * @brief Registers an HTTP route for an exact path.
     *
     * @param path      Exact request path.
     * @param http_route Route to register.
     * @return Moved reference to this server (rvalue overload).
     */
    HttpServer&& route(std::string path, HttpRoute http_route) &&;

    /**
     * @brief Registers a WebSocket route for an exact path.
     *
     * After a successful handshake on this path, @p handler is invoked with
     * the established connection. The handler is expected to run the session
     * loop itself (for example, by repeatedly calling readMessage() and
     * replying via sendText()). While the handler runs, the server runs the
     * ping/pong heartbeat in a background thread.
     *
     * @param path    Exact request path.
     * @param handler WebSocket session handler.
     * @return Reference to this server (lvalue overload).
     */
    HttpServer& routeWebSocket(std::string path, ws_handler_t handler) &;

    /**
     * @brief Registers a WebSocket route for an exact path.
     *
     * @param path    Exact request path.
     * @param handler WebSocket session handler.
     * @return Moved reference to this server (rvalue overload).
     */
    HttpServer&& routeWebSocket(std::string path, ws_handler_t handler) &&;

    /**
     * @brief Sets the receive timeout for client connections.
     *
     * @param timeout_ms Timeout value.
     * @return Reference to this server.
     */
    HttpServer& setClientReceiveTimeoutMs(client_timeout_ms_t timeout_ms) &;

    /**
     * @brief Sets the send timeout for client connections.
     *
     * @param timeout_ms Timeout value.
     * @return Reference to this server.
     */
    HttpServer& setClientSendTimeoutMs(client_timeout_ms_t timeout_ms) &;

    /**
     * @brief Sets the root directory for static files.
     *
     * If set, requests that do not match any entry in the HTTP route table
     * or the WebSocket route table are served as files from this directory.
     *
     * @param static_dir Directory with static files.
     * @return Reference to this server (lvalue overload).
     */
    HttpServer& setDirWithStaticFiles(std::filesystem::path static_dir) &;

    /**
     * @brief Sets the root directory for static files.
     *
     * @param static_dir Directory with static files.
     * @return Moved reference to this server (rvalue overload).
     */
    HttpServer&& setDirWithStaticFiles(std::filesystem::path static_dir) &&;

    /**
     * @brief Starts the server: creates the socket, binds, listens, and runs the pool.
     *
     * Returns false (and leaves the server stopped) if it is already
     * running, if the stop token has already requested a stop, if the
     * listening socket cannot be created (after a previous stop()), if
     * listen() fails, or if the accept task cannot be pushed into the
     * worker pool.
     *
     * Steps:
     *  1. If already running, returns false.
     *  2. If the stop token has already requested a stop, returns false.
     *  3. If the socket was closed (for example, by a previous stop()),
     *     recreates it from m_source and m_tcp_type, and switches it to
     *     non-blocking mode.
     *  4. Calls listen() on the socket.
     *  5. Starts the worker pool and publishes m_running = true.
     *  6. Re-checks the stop token after publishing m_running; if a stop
     *     was requested in the window between steps 2 and 5, stops the
     *     server and returns false.
     *  7. Pushes the accept loop as a task into the pool. If that fails,
     *     rolls back (m_running = false, pool stopped) and returns false.
     *
     * @return true if the server started successfully, false otherwise.
     */
    [[nodiscard]] bool start();

    /**
     * @brief Stops the server: stops accepting new connections and drains the pool.
     *
     * Sets m_running to false (idempotent: if the server was not running,
     * returns immediately) and closes the listening socket. Closing the
     * socket makes acceptLoop exit: accept() on a closed descriptor fails
     * with EBADF, and the loop re-checks m_running before continuing.
     *
     * Then waits for the worker pool (stop + join), which also waits for
     * the accept loop and for all in-flight connection handlers to finish,
     * and finally wakes all threads blocked in wait().
     *
     * Safe to call multiple times and from multiple threads; subsequent
     * calls after the first have no additional effect.
     */
    void stop();

    /**
     * @brief Blocks the calling thread until the server stops.
     *
     * Returns when m_running becomes false: either because stop() was
     * called (from another thread or via the stop token), or because the
     * accept loop terminated on its own.
     *
     * Does not busy-wait: the calling thread is parked on a condition
     * variable and is woken by stop(). Safe to call only after start()
     * returned true; calling it on a server that is not running returns
     * immediately.
     *
     * @note If you need to stop the server from another thread, call
     *       stop() and then wait() (or just stop()); wait() alone does not
     *       stop anything.
     */
    void wait();

    /**
     * @brief Reports whether the server is currently running.
     *
     * @return true if @ref start succeeded and @ref stop has not completed.
     */
    [[nodiscard]] bool isRunning() const noexcept;

    /**
     * @brief Returns the listening socket handle.
     *
     * The handle is meaningful only while the server is running (between a
     * successful start() and stop()). Before start() and after stop() the
     * value is implementation-defined and must not be used.
     */
    [[nodiscard]] socket_t getSocketID() const noexcept;

    /// Sets the WebSocket ping interval (0 disables server-initiated pings).
    HttpServer& setWebSocketPingInterval(ws_ping_interval_t interval) &;

    /// Sets how long to wait for a Pong after a Ping.
    HttpServer& setWebSocketPongTimeout(ws_pong_timeout_t timeout) &;

private:
    /**
     * @brief Accept loop: waits for incoming connections and dispatches them to the pool.
     *
     * Uses poll(2) on the listening socket with a short timeout so that
     * m_running is re-checked periodically and stop() does not have to wait
     * for a blocking accept(). When the socket is reported readable,
     * accepts a client via ClientConnection::getClientConnection() and
     * hands it to the worker pool, which runs handleConnection() on it.
     *
     * The loop exits when m_running becomes false. Transient errors
     * (EINTR, EAGAIN/EWOULDBLOCK) are retried; other accept() failures are
     * also retried, but only while the server is still running.
     */
    void acceptLoop();

    /**
     * @brief Runs the request loop for a single client connection.
     *
     * Configures receive and send timeouts on the socket (when the
     * descriptor is valid), then repeatedly processes requests via
     * processRequest() while both of the following hold:
     *  - the server is still running (m_running is true);
     *  - processRequest() reports that the connection may be reused
     *    (keep-alive).
     *
     * The loop exits when either condition becomes false, which means the
     * connection should be closed by the caller.
     *
     * @param connection Client connection to serve.
     */
    void handleConnection(kas::net::socket::ClientConnection& connection);

    /// Attempts to serve a request from the static files directory.
    bool tryServeStaticFile(std::string_view endpoint, Response& response) const;

    /**
     * @brief Processes a single HTTP request on an established connection.
     *
     * Steps:
     *  1. Reads and parses the request via Request::make(). Returns false
     *     on parse/read failure (the connection should be closed).
     *  2. If the request path matches a registered WebSocket route,
     *     handleWebSocketIfMatched() performs the upgrade (or sends a 400)
     *     and runs the session handler. In that case the connection is
     *     finished after the WebSocket session ends, so false is returned.
     *  3. Otherwise, builds a Response via buildRouteResponse() (which
     *     dispatches to the matched HTTP route or falls back to static
     *     files).
     *  4. Adds "Accept-Ranges: bytes" for 200 responses and applies the
     *     Range request header (206 / multipart / 416), if present.
     *  5. Sets the Connection header according to the request's keep-alive
     *     flag and sends the response. For HEAD requests only the headers
     *     are sent (the body is skipped).
     *
     * @param connection Client connection carrying the request and the
     *                   socket to write the response to.
     * @return true if the connection may be reused (keep-alive and the
     *         response was sent successfully), false otherwise.
     */
    bool processRequest(kas::net::socket::ClientConnection& connection);

    /// Builds a Response by dispatching the request to the matching route.
    [[nodiscard]] Response buildRouteResponse(const Request& request) const;

    /**
     * @brief Runs a WebSocket session: heartbeat + user handler.
     *
     * Starts a background heartbeat thread (when the ping interval is
     * non-zero) that sends a Ping every m_ws_ping_interval and closes the
     * socket if no inbound frame arrives within m_ws_pong_timeout. Then
     * invokes @p handler, which is expected to run the session loop itself
     * and return when the session ends. After the handler returns, the
     * heartbeat is stopped, a best-effort Close frame is sent, and the
     * socket is shut down.
     *
     * @param ws      WebSocket connection wrapping the client socket.
     * @param handler Session handler (owns the read/write loop).
     */
    void runWebSocketSession(WebSocketConnection& ws, const ws_handler_t& handler);

    std::string m_source; ///< Listen address ("host:port") or Unix socket path.
    socket::TcpTypes m_tcp_type = socket::TcpTypes::TCP; ///< TCP type used by the socket server.
    socket::SocketServer m_server; ///< Underlying socket server.
    std::unordered_map<std::string, HttpRoute> m_routes; ///< HTTP routes keyed by exact path.
    std::unordered_map<std::string, ws_handler_t> m_ws_routes; ///< WebSocket routes keyed by exact path.
    kas::ThreadPool m_pool; ///< Worker pool processing accepted connections.
    client_timeout_ms_t m_client_receive_timeout_ms = DEFAULT_CLIENT_RECEIVE_TIMEOUT_MS; ///< Client receive timeout.
    client_timeout_ms_t m_client_send_timeout_ms = DEFAULT_CLIENT_SEND_TIMEOUT_MS; ///< Client send timeout.
    ws_ping_interval_t m_ws_ping_interval { DEFAULT_WS_PING_INTERVAL_MS }; ///< WebSocket ping interval.
    ws_pong_timeout_t m_ws_pong_timeout { DEFAULT_WS_PONG_TIMEOUT_MS }; ///< WebSocket pong timeout.
    std::optional<std::filesystem::path> m_dir_with_static_files_opt; ///< Static files root, if configured.
    std::atomic<bool> m_running { false }; ///< Whether the server is running.

    std::mutex m_wait_mx { }; ///< Protects m_wait_cv and the predicate in wait().
    std::condition_variable_any m_wait_cv { }; ///< Wakes threads blocked in wait() when the server stops.
    std::condition_variable_any m_ws_cv { }; ///< Wakes heartbeat threads when the server stops.

    std::stop_token m_stop_token { }; ///< External stop token; empty if no external stop is configured.
    std::optional<std::stop_callback<functional::MoveOnlyFunction<void()>>> m_stop_cb; ///< Calls stop() on request_stop().
};

} // namespace kas::net::http

#endif // KASLIBCXX_NET_HTTP_HTTP_SERVER_H