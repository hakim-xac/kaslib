// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include "KAS/net/socket/SocketClient.h"
#include "KAS/net/general.h"
#include "KAS/net/socket/SocketAddress.h"
#include "KAS/utils/utils.h"

#include <algorithm>
#include <array>
#include <sys/time.h>
#include <utility>
#include <vector>

namespace kas::net::socket {

//------------------

[[nodiscard]]
std::optional<SocketClient>
SocketClient::make(
    std::string_view source,
    domain_t domain,
    type_t type,
    protocol_t protocol)
{
    auto sc {
        SocketClient { source, domain, type, protocol }
            .connect()
    };
    if (!sc.isConnected()) [[unlikely]]
        return std::nullopt;

    return sc;
}

//------------------

SocketClient::SocketClient(
    std::string_view source,
    domain_t domain,
    type_t type,
    protocol_t protocol)
    : m_socket { domain, type, protocol }
    , m_connection_time { std::chrono::system_clock::now() }
    , m_source { source }
    , m_socket_address_opt { }
    , m_domain { domain }
{
}

//------------------

[[nodiscard]] SocketClient::socket_t
SocketClient::getSocketID() const noexcept
{
    return m_socket.getSocketID();
}

//------------------

[[nodiscard]] bool
SocketClient::isSocketCreated() const noexcept
{
    return m_socket.isSocketCreated();
}

//------------------

[[nodiscard]] std::string_view
SocketClient::getSource() const& noexcept
{
    return m_source;
}

//------------------

[[nodiscard]] bool
SocketClient::isConnected() const noexcept
{
    return m_socket_address_opt.has_value();
}

//------------------

[[nodiscard]]
const std::chrono::system_clock::time_point&
SocketClient::getConnectionTime() const& noexcept
{
    return m_connection_time;
}

//------------------

void SocketClient::init()
{
    if (!m_socket.isSocketCreated()) [[unlikely]]
        return;

    auto addr_opt { SocketAddress::makeSocketAddress(m_domain, m_source) };
    if (!addr_opt) [[unlikely]]
        return;

    if (::connect(m_socket.getSocketID(), addr_opt->asSockaddr(), addr_opt->getAddrLen()) != 0) [[unlikely]]
        return;

    m_socket_address_opt.emplace(std::move(*addr_opt));
}

//------------------

SocketClient& SocketClient::connect() &
{
    init();
    return *this;
}

//------------------

SocketClient&& SocketClient::connect() &&
{
    init();
    return std::move(*this);
}

//------------------

[[nodiscard]]
kas::expected_type<std::vector<std::uint8_t>, general::ReadError>
SocketClient::readBytes(std::size_t need_bytes) const
{
    if (!m_socket.isSocketCreated())
        return kas::unexpected<general::ReadError> { general::ReadError::Error };

    if (need_bytes > 0)
        return general::readExactBytes(m_socket.getSocketID(), need_bytes);

    if (m_socket.getSocketType() == SOCK_DGRAM)
        return kas::unexpected<general::ReadError> { general::ReadError::Error };

    return general::readAllBytes(m_socket.getSocketID());
}

//------------------

[[nodiscard]] std::optional<std::vector<std::uint8_t>>
SocketClient::readDatagram() const
{
    constexpr std::size_t MAX_BUFFER_LENGTH { 65536 }; // UDP max
    std::vector<std::uint8_t> buffer(MAX_BUFFER_LENGTH);

    const ssize_t read_bytes = ::recv(
        m_socket.getSocketID(), buffer.data(), buffer.size(), MSG_NOSIGNAL);

    if (read_bytes <= 0) [[unlikely]]
        return std::nullopt;

    buffer.resize(static_cast<std::size_t>(read_bytes));
    return buffer;
}

//------------------

[[nodiscard]]
bool SocketClient::sendBytes(std::span<const std::uint8_t> data) const noexcept
{
    if (!m_socket.isSocketCreated()) [[unlikely]]
        return false;

    return general::sendBytes(m_socket.getSocketID(), data);
}

//------------------

[[nodiscard]] std::optional<std::string>
SocketClient::readString(size_t need_bytes) const
{
    auto result_opt = readBytes(need_bytes);
    if (!result_opt) [[unlikely]]
        return std::nullopt;

    return utils::bytesAsString(*result_opt);
}

//------------------

[[nodiscard]]
bool SocketClient::sendString(std::string text) const noexcept
{
    return sendBytes(std::span<const std::uint8_t> {
        reinterpret_cast<const std::uint8_t*>(text.data()),
        text.size() });
}

//------------------

const SocketClient&
SocketClient::setSendTimeoutMs(std::chrono::milliseconds send_timeout_ms) const&
{
    general::setSendTimeoutMs(m_socket.getSocketID(), send_timeout_ms);
    return *this;
}

//------------------

const SocketClient&
SocketClient::setReceiveTimeoutMs(std::chrono::milliseconds receive_timeout_ms) const&
{
    general::setReceiveTimeoutMs(m_socket.getSocketID(), receive_timeout_ms);
    return *this;
}

//------------------

[[nodiscard]]
const std::optional<SocketAddress>&
SocketClient::getSocketAddress() const&
{
    return m_socket_address_opt;
}

//------------------
} // namespace kas::net