// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include "KAS/net/socket/ClientConnection.h"
#include "KAS/utils/utils.h"

#include <arpa/inet.h>
#include <chrono>
#include <optional>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>

namespace kas::net::socket {
//------------------

[[nodiscard]]
std::optional<ClientConnection>
ClientConnection::getClientConnection(socket_client_t socket_server)
{
    sockaddr_storage storage { };
    socklen_t client_len { sizeof(storage) };

    socket_client_t client_socket { ::accept(socket_server, reinterpret_cast<sockaddr*>(&storage), &client_len) };
    if (client_socket < 0) [[unlikely]]
        return std::nullopt;

    return ClientConnection {
        client_socket,
        SocketAddress { }
            .setSockaddrStorage(std::move(storage))
            .setSockaddrLen(client_len)
    };
}

//------------------

ClientConnection::ClientConnection(socket_client_t client_socket, SocketAddress client_socket_address)
    : m_connection_time { std::chrono::system_clock::now() }
    , m_client_socket_address { std::move(client_socket_address) }
    , m_client_socket { client_socket }
{
}

//------------------

ClientConnection::ClientConnection(ClientConnection&& other) noexcept
    : m_connection_time { std::move(other.m_connection_time) }
    , m_client_socket_address { std::move(other.m_client_socket_address) }
    , m_client_socket { std::exchange(other.m_client_socket, -1) }
{
}

//------------------

ClientConnection& ClientConnection::operator=(ClientConnection&& other) noexcept
{
    if (std::addressof(other) != this)
        swap(other);

    return *this;
}

//------------------

void ClientConnection::swap(ClientConnection& other) noexcept
{
    std::swap(m_connection_time, other.m_connection_time);
    std::swap(m_client_socket_address, other.m_client_socket_address);
    std::swap(m_client_socket, other.m_client_socket);
}

//------------------

ClientConnection::~ClientConnection()
{
    close();
}

//------------------

void ClientConnection::close()
{
    if (m_client_socket != -1) {
        ::close(m_client_socket);
        m_client_socket = -1;
    }
}

//------------------

[[nodiscard]]
const std::chrono::system_clock::time_point&
ClientConnection::getConnectionTime() const& noexcept
{
    return m_connection_time;
}

//------------------

void ClientConnection::updateConnectionTime()
{
    m_connection_time = std::chrono::system_clock::now();
}

//------------------

[[nodiscard]]
ClientConnection::socket_client_t
ClientConnection::getSocketID() const noexcept
{
    return m_client_socket;
}

//------------------

[[nodiscard]]
const SocketAddress&
ClientConnection::getRemoteAddress() const&
{
    return m_client_socket_address;
}

//------------------
} // namespace kas::net
