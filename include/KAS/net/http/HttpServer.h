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

#include <KAS/net/socket/ClientConnection.h>
#include <KAS/net/socket/SocketServer.h>
#include <KAS/thread/ThreadPool.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
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
 *       and a worker pool. Call @ref stop to release them. Destruction
 *       without a prior @ref stop is not a substitute for @ref stop.
 */
class HttpServer final {
public:
    /// Endpoint handler type used by HTTP routes.
    using handler_t = HttpRoute::end_point_handler_t;
    /// WebSocket handler type, invoked after a successful handshake.
    using ws_handler_t = std::function<void(WebSocketConnection&)>;
    /// Socket handle type used by the underlying socket server.
    using socket_t = socket::SocketServer::socket_t;
    /// Timeout type for client receive/send operations.
    using client_timeout_ms_t = std::chrono::milliseconds;

    /// Default receive timeout for a client connection.
    constexpr static client_timeout_ms_t DEFAULT_CLIENT_RECEIVE_TIMEOUT_MS { 5000 };
    /// Default send timeout for a client connection.
    constexpr static client_timeout_ms_t DEFAULT_CLIENT_SEND_TIMEOUT_MS { 5000 };

    /**
     * @brief Constructs a server bound to the given source.
     *
     * @param source       Listen address in the form "host:port" (for
     *                     example, "0.0.0.0:8080").
     * @param worker_count Number of worker threads in the pool.
     */
    explicit HttpServer(
        std::string source,
        std::size_t worker_count = 4);

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;
    HttpServer(HttpServer&&) noexcept = delete;
    HttpServer& operator=(HttpServer&&) noexcept = delete;
    ~HttpServer() = default;

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
     * the established connection.
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
     * running, if the listening socket cannot be created (after a previous
     * stop()), if listen() fails, or if the accept task cannot be pushed
     * into the worker pool.
     *
     * Steps:
     *  1. If already running, returns false.
     *  2. If the socket was closed (for example, by a previous stop()),
     *     recreates it from m_source and m_tcp_type, and switches it to
     *     non-blocking mode.
     *  3. Calls listen() on the socket.
     *  4. Starts the worker pool and publishes m_running = true.
     *  5. Pushes the accept loop as a task into the pool. If that fails,
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
     * the accept loop and for all in-flight connection handlers to finish.
     *
     * Safe to call multiple times; subsequent calls after the first have no
     * additional effect.
     */
    void stop();

    /**
     * @brief Reports whether the server is currently running.
     *
     * @return true if @ref start succeeded and @ref stop has not completed.
     */
    [[nodiscard]] bool isRunning() const noexcept;

    /**
     * @brief Returns the listening socket handle.
     *
     * @return The socket handle used by the underlying socket server.
     */
    [[nodiscard]] socket_t getSocketID() const noexcept;

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

    std::string m_source; ///< Listen address ("host:port").
    socket::TcpTypes m_tcp_type = socket::TcpTypes::TCP; ///< TCP type used by the socket server.
    socket::SocketServer m_server; ///< Underlying socket server.
    std::unordered_map<std::string, HttpRoute> m_routes; ///< HTTP routes keyed by exact path.
    std::unordered_map<std::string, ws_handler_t> m_ws_routes; ///< WebSocket routes keyed by exact path.
    kas::ThreadPool m_pool; ///< Worker pool processing accepted connections.
    client_timeout_ms_t m_client_receive_timeout_ms = DEFAULT_CLIENT_RECEIVE_TIMEOUT_MS; ///< Client receive timeout.
    client_timeout_ms_t m_client_send_timeout_ms = DEFAULT_CLIENT_SEND_TIMEOUT_MS; ///< Client send timeout.
    std::optional<std::filesystem::path> m_dir_with_static_files_opt; ///< Static files root, if configured.
    std::atomic<bool> m_running { false }; ///< Whether the server is running.
};

} // namespace kas::net::http

#endif // KASLIBCXX_NET_HTTP_HTTP_SERVER_H