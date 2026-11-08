/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_NET_SOCKET_SOCKET_SCOPED_H
#define KASLIBCXX_NET_SOCKET_SOCKET_SCOPED_H

#include <netinet/in.h>
#include <optional>
#include <sys/socket.h>

namespace kas::net::socket {

/**
 * @brief RAII owner of a POSIX socket handle.
 *
 * Creates a socket in the constructor (via ::socket) and closes it in the
 * destructor, unless it was released earlier with @ref closeSocket. The
 * class is move-only: copying would duplicate the handle and lead to
 * double-close, so copy operations are deleted.
 *
 * The created socket is not connected or bound; callers are expected to
 * perform bind/connect/listen/accept themselves using @ref getSocketID.
 *
 * @note The class is not thread-safe: concurrent calls to methods of the
 *       same instance from different threads are not allowed without
 *       external synchronization.
 */
class SocketScoped final {
public:
    //------------------
    /// Socket handle type.
    using socket_t = int32_t;
    /// Address family type (raw AF_* value).
    using domain_t = sa_family_t;
    /// Socket type (for example, SOCK_STREAM or SOCK_DGRAM).
    using type_t = int32_t;
    /// Protocol type (for example, IPPROTO_IP or IPPROTO_TCP).
    using protocol_t = int32_t;

    /// Default address family (AF_INET).
    constexpr static domain_t DEFAULT_SOCKET_DOMAIN = AF_INET;
    /// Default socket type (SOCK_STREAM).
    constexpr static type_t DEFAULT_SOCKET_TYPE = SOCK_STREAM;
    /// Default protocol (IPPROTO_IP).
    constexpr static protocol_t DEFAULT_SOCKET_PROTOCOL = IPPROTO_IP;

    //------------------

    /**
     * @brief Creates a socket and wraps it in SocketScoped.
     *
     * Unlike the constructor, this factory reports failure via std::nullopt
     * instead of exposing an invalid handle.
     *
     * @param domain   Address family.
     * @param type     Socket type.
     * @param protocol Socket protocol.
     * @return The created wrapper, or std::nullopt if ::socket failed.
     */
    [[nodiscard]] static std::optional<SocketScoped> makeSocketScoped(
        domain_t domain = DEFAULT_SOCKET_DOMAIN,
        type_t type = DEFAULT_SOCKET_TYPE,
        protocol_t protocol = DEFAULT_SOCKET_PROTOCOL);

    /**
     * @brief Creates a socket with the given parameters.
     *
     * On failure the internal handle is left invalid; check
     * @ref isSocketCreated to detect this case. Prefer @ref makeSocketScoped
     * if error reporting via std::optional is desired.
     *
     * @param domain   Address family.
     * @param type     Socket type.
     * @param protocol Socket protocol.
     */
    explicit SocketScoped(
        domain_t domain = DEFAULT_SOCKET_DOMAIN,
        type_t type = DEFAULT_SOCKET_TYPE,
        protocol_t protocol = DEFAULT_SOCKET_PROTOCOL);
    //------------------

    /// Closes the owned socket if it is still open.
    ~SocketScoped();
    //------------------

    SocketScoped(const SocketScoped&) = delete;
    SocketScoped& operator=(const SocketScoped&) = delete;

    /**
     * @brief Move constructor: transfers ownership of the socket handle.
     * @param other Source wrapper; left without an owned socket.
     */
    SocketScoped(SocketScoped&& other) noexcept;

    /**
     * @brief Move assignment: closes the current socket (if any) and takes
     *        over the handle from @p other.
     * @param other Source wrapper.
     * @return Reference to this wrapper.
     */
    SocketScoped& operator=(SocketScoped&&) noexcept;
    //------------------

    /**
     * @brief Returns the owned socket handle.
     * @return Socket handle (may be invalid if the socket was not created
     *         or has been closed/moved away).
     */
    [[nodiscard]] socket_t getSocketID() const noexcept;

    /**
     * @brief Returns the socket type passed at construction.
     * @return Socket type.
     */
    [[nodiscard]] type_t getSocketType() const noexcept;

    /**
     * @brief Returns the socket protocol passed at construction.
     * @return Socket protocol.
     */
    [[nodiscard]] protocol_t getSocketProtocol() const noexcept;

    /**
     * @brief Returns the socket domain passed at construction.
     * @return Address family.
     */
    [[nodiscard]] domain_t getSocketDomain() const noexcept;
    //------------------

    /**
     * @brief Closes the owned socket immediately.
     *
     * Safe to call multiple times; subsequent calls have no additional
     * effect.
     */
    void closeSocket() noexcept;
    //------------------

    /**
     * @brief Reports whether the owned socket handle is valid.
     * @return true if the socket was created and not yet closed/moved.
     */
    [[nodiscard]] bool isSocketCreated() const noexcept;
    //------------------

private:
    domain_t m_socket_domain; ///< Address family.
    type_t m_socket_type; ///< Socket type.
    protocol_t m_socket_protocol; ///< Socket protocol.
    socket_t m_socket; ///< Owned socket handle.
};

} // namespace kas::net::socket

#endif