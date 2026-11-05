// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include "KAS/net/socket/SocketScoped.h"

#include <unistd.h>
#include <utility>

namespace kas::net::socket {
//------------------

[[nodiscard]]
std::optional<SocketScoped>
SocketScoped::makeSocketScoped(domain_t domain, type_t type, protocol_t protocol)
{
    SocketScoped res { domain, type, protocol };
    return res.isSocketCreated()
        ? std::optional { std::move(res) }
        : std::nullopt;
}

//------------------

SocketScoped::SocketScoped(domain_t domain, type_t type, protocol_t protocol)
    : m_socket_domain { domain }
    , m_socket_type { type }
    , m_socket_protocol { protocol }
    , m_socket { ::socket(domain, type, protocol) }
{
}

//------------------

void SocketScoped::closeSocket() noexcept
{
    if (m_socket != -1) {
        ::close(m_socket);
        m_socket = -1;
    }
}

//------------------

SocketScoped::~SocketScoped()
{
    closeSocket();
}

//------------------

SocketScoped::SocketScoped(SocketScoped&& other) noexcept
    : m_socket { std::exchange(other.m_socket, -1) }
{
}

//------------------

SocketScoped& SocketScoped::operator=(SocketScoped&& other) noexcept
{
    if (std::addressof(other) != this)
        std::swap(m_socket, other.m_socket);

    return *this;
}

//------------------

[[nodiscard]]
int32_t
SocketScoped::getSocketID() const noexcept
{
    return m_socket;
}

//------------------

[[nodiscard]]
SocketScoped::type_t
SocketScoped::getSocketType() const noexcept
{
    return m_socket_type;
}

//------------------

[[nodiscard]]
SocketScoped::protocol_t
SocketScoped::getSocketProtocol() const noexcept
{
    return m_socket_protocol;
}

//------------------

[[nodiscard]]
SocketScoped::domain_t
SocketScoped::getSocketDomain() const noexcept
{
    return m_socket_domain;
}

//------------------

[[nodiscard]]
bool SocketScoped::isSocketCreated() const noexcept
{
    return m_socket != -1;
}

//------------------
} // namespace kas::net