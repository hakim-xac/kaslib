// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include "KAS/net/socket/SocketServer.h"
#include "KAS/net/general.h"
#include "KAS/net/socket/ClientConnection.h"
#include "KAS/net/socket/SocketAddress.h"

#include <algorithm>
#include <arpa/inet.h>
#include <bit>
#include <cstdio>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <functional>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>
#include <utility>

namespace kas::net::socket {
//------------------

[[nodiscard]]
std::optional<SocketServer>
SocketServer::make(std::string source, TcpTypes tcp_type)
{
    std::optional<SocketServer> res;
    switch (tcp_type) {
    case TcpTypes::TCP: {
        res.emplace(std::move(source), AF_INET, SOCK_STREAM, IPPROTO_TCP);
        break;
    }
    case TcpTypes::UDP: {
        res.emplace(std::move(source), AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        break;
    }
    case TcpTypes::ICMP: {
        res.emplace(std::move(source), AF_INET, SOCK_RAW, IPPROTO_ICMP);
        break;
    }
    case TcpTypes::UNIX: {
        res.emplace(std::move(source), AF_UNIX, SOCK_STREAM, IPPROTO_IP);
        break;
    }
    default:
        return std::nullopt;
    }

    if (!res) [[unlikely]]
        return std::nullopt;

    if (!res->isSocketCreated()) [[unlikely]]
        return std::nullopt;

    return res;
}

//------------------

SocketServer::SocketServer(
    std::string source,
    socket_domain_t domain,
    socket_type_t type,
    socket_protocol_t protocol,
    client_receive_timeout_ms_t client_receive_timeout_ms,
    client_send_timeout_ms_t client_send_timeout_ms)
    : m_socket_scoped { domain, type, protocol }
    , m_source { std::move(source) }
    , m_domain { domain }
    , m_client_receive_timeout_ms { client_receive_timeout_ms }
    , m_client_send_timeout_ms { client_send_timeout_ms }
{
}

//------------------

[[nodiscard]] SocketServer::socket_t
SocketServer::getSocketID() const noexcept
{
    return m_socket_scoped.getSocketID();
}

//------------------

[[nodiscard]] bool
SocketServer::isSocketCreated() const noexcept
{
    return m_socket_scoped.isSocketCreated();
}

//------------------

[[nodiscard]] std::string_view
SocketServer::getIpAddress() const noexcept
{
    return m_source;
}

//------------------

SocketServer& SocketServer::setClientReceiveTimeoutMs(std::chrono::milliseconds timeout_ms) &
{
    m_client_receive_timeout_ms = timeout_ms;
    return *this;
}

//------------------

SocketServer& SocketServer::setClientSendTimeoutMs(std::chrono::milliseconds timeout_ms) &
{
    m_client_send_timeout_ms = timeout_ms;
    return *this;
}

//------------------

SocketServer& SocketServer::setBlocking() &
{
    if (m_socket_scoped.isSocketCreated()) {

        const auto socket { m_socket_scoped.getSocketID() };

        const int socket_flags { ::fcntl(socket, F_GETFL, 0) };
        if (socket_flags >= 0)
            (void)::fcntl(socket, F_SETFL, socket_flags & ~O_NONBLOCK);
    }
    return *this;
}

//------------------

SocketServer& SocketServer::setNoBlocking() &
{
    if (m_socket_scoped.isSocketCreated()) {

        const auto socket { m_socket_scoped.getSocketID() };

        const int socket_flags { ::fcntl(socket, F_GETFL, 0) };
        if (socket_flags >= 0)
            (void)::fcntl(socket, F_SETFL, socket_flags | O_NONBLOCK);
    }

    return *this;
}

//------------------

[[nodiscard]] bool
SocketServer::listen(int32_t max_listen) noexcept
{
    if (!m_socket_scoped.isSocketCreated()) [[unlikely]]
        return false;

    if (m_domain == AF_INET) {
        // Indicates that the socket can be bound unless there is an active listening socket bound to the address.
        int one { 1 };
        if (setsockopt(m_socket_scoped.getSocketID(), SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0)
            return false;
    }

    auto addr_opt { SocketAddress::makeSocketAddress(m_domain, m_source) };
    if (!addr_opt) [[unlikely]]
        return false;

    if (m_domain == AF_UNIX) {
        const auto* un_addr { addr_opt->asSockaddrUN() };
        unlink(un_addr->sun_path);
    }

    if (::bind(m_socket_scoped.getSocketID(), addr_opt->asSockaddr(), addr_opt->getAddrLen()) < 0) [[unlikely]]
        return false;

    if (m_domain == AF_UNIX)
        chmod(m_source.c_str(), 0666);

    if (::listen(m_socket_scoped.getSocketID(), max_listen) < 0) [[unlikely]]
        return false;

    return true;
}

//------------------

[[nodiscard]]
kas::expected_type<std::vector<std::uint8_t>, general::ReadError>
SocketServer::readBytes() const
{
    return general::readAllBytes(m_socket_scoped.getSocketID());
}

//------------------

void SocketServer::closeSocket() noexcept
{
    m_socket_scoped.closeSocket();
}

//------------------

[[nodiscard]]
const std::string&
SocketServer::getSource() const& noexcept
{
    return m_source;
}

//------------------
} // namespace kas::net