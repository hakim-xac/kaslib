// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include "KAS/net/socket/SocketAddress.h"
#include "KAS/utils/utils.h"

#include <algorithm>
#include <cstring>
#include <net/if.h> // if_nametoindex
#include <utility>

namespace kas::net::socket {

namespace {

    /**
     * @brief Formats an IPv4 socket address as "ip:port".
     *
     * Uses inet_ntop to render the address and appends the port in
     * host byte order.
     *
     * @param addr IPv4 address.
     * @return "ip:port", or "<invalid-ipv4>" if inet_ntop fails.
     */
    std::string formatIPv4(const sockaddr_in& addr)
    {
        char ip[INET_ADDRSTRLEN] { };
        if (::inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip)) == nullptr)
            return "<invalid-ipv4>";

        const auto port = ntohs(addr.sin_port);
        return std::string { ip } + ':' + std::to_string(port);
    }

    /**
     * @brief Formats an IPv6 socket address as "[ip]:port[%scope]".
     *
     * Uses inet_ntop to render the address, wraps it in brackets, and
     * appends the port in host byte order. If the scope id is non-zero
     * (link-local addresses), "%<scope_id>" is appended.
     *
     * @param addr IPv6 address.
     * @return "[ip]:port" (optionally with "%scope"), or
     *         "<invalid-ipv6>" if inet_ntop fails.
     */
    std::string formatIPv6(const sockaddr_in6& addr)
    {
        char ip[INET6_ADDRSTRLEN] { };
        if (::inet_ntop(AF_INET6, &addr.sin6_addr, ip, sizeof(ip)) == nullptr)
            return "<invalid-ipv6>";

        const auto port = ntohs(addr.sin6_port);
        std::string result { '[' };
        result += ip;
        result += "]:" + std::to_string(port);

        if (addr.sin6_scope_id != 0)
            result += "%" + std::to_string(addr.sin6_scope_id);

        return result;
    }

    /**
     * @brief Formats a Unix-domain socket address as its path.
     *
     * sun_path may not be null-terminated, so the length is bounded by
     * strnlen to the size of the field.
     *
     * @param addr Unix-domain address.
     * @return The path stored in sun_path.
     */
    std::string formatUnix(const sockaddr_un& addr)
    {
        // sun_path may not be null-terminated, so bound the length.
        const auto len = ::strnlen(addr.sun_path, sizeof(addr.sun_path));
        return std::string { addr.sun_path, len };
    }

    /**
     * @brief Formats a packet socket address.
     *
     * The result has the form:
     *     ifindex=<N>,proto=0x<pppp>,halen=<H>[,addr=xx:xx:...]
     * where proto is the (network-order) protocol printed in hex and
     * addr lists the first sll_halen bytes of sll_addr.
     *
     * @param addr Packet socket address.
     * @return The formatted string.
     */
    std::string formatPacket(const sockaddr_ll& addr)
    {
        std::string result { "ifindex=" };
        result += std::to_string(addr.sll_ifindex);
        result += ",proto=0x";

        char proto[8] { };
        std::snprintf(proto, sizeof(proto), "%04x", ntohs(addr.sll_protocol));
        result += proto;

        result += ",halen=" + std::to_string(addr.sll_halen);
        if (addr.sll_halen > 0) {
            result += ",addr=";
            for (unsigned i = 0; i < addr.sll_halen; ++i) {
                char byte[4] { };
                std::snprintf(byte, sizeof(byte), "%02x", addr.sll_addr[i]);
                result += byte;
                if (i + 1 < addr.sll_halen)
                    result += ':';
            }
        }
        return result;
    }

    /**
     * @brief Formats a netlink socket address.
     *
     * The result has the form:
     *     pid=<N>,groups=0x<gggggggg>
     * where groups is printed in hex (zero-padded to 8 digits).
     *
     * @param addr Netlink socket address.
     * @return The formatted string.
     */
    std::string formatNetlink(const sockaddr_nl& addr)
    {
        std::string result { "pid=" };
        result += std::to_string(addr.nl_pid);
        result += ",groups=0x";

        char groups[16] { };
        std::snprintf(groups, sizeof(groups), "%08x", addr.nl_groups);
        result += groups;
        return result;
    }

#ifdef KASLIBCXX_NET_HAS_BLUETOOTH
    /**
     * @brief Formats a Bluetooth RFCOMM socket address as "mac:channel".
     *
     * Uses ba2str to render the BD address; the channel is appended in
     * decimal form.
     *
     * @param addr Bluetooth RFCOMM address.
     * @return "mac:channel", or "<invalid-bt>" if ba2str fails.
     */
    std::string formatBluetooth(const sockaddr_rc& addr)
    {
        char mac[18] { };
        if (::ba2str(&addr.rc_bdaddr, mac) < 0)
            return "<invalid-bt>";

        return std::string { mac } + ':' + std::to_string(addr.rc_channel);
    }
#endif
}

//------------------

std::optional<SocketAddress>
SocketAddress::make(SocketDomain domain, std::string_view source)
{
    switch (domain) {
    case SocketDomain::IPv4:
        return makeIPv4(source);
    case SocketDomain::IPv6:
        return makeIPv6(source);
    case SocketDomain::Unix:
        return makeUnix(source);
    case SocketDomain::Packet:
        return makePacket(source);
    case SocketDomain::Netlink:
        return makeNetlink(source);
#ifdef KASLIBCXX_NET_HAS_BLUETOOTH
    case SocketDomain::Bluetooth:
        return makeBluetooth(source);
#endif
    }
    return std::nullopt;
}

//------------------

std::string
SocketAddress::toString() const
{
    switch (m_storage.ss_family) {
    case AF_INET:
        return formatIPv4(*asSockaddrIN());
    case AF_INET6:
        return formatIPv6(*asSockaddrIN6());
    case AF_UNIX:
        return formatUnix(*asSockaddrUN());
    case AF_PACKET:
        return formatPacket(*asSockaddrLL());
    case AF_NETLINK:
        return formatNetlink(*asSockaddrNL());
#ifdef KASLIBCXX_NET_HAS_BLUETOOTH
    case AF_BLUETOOTH:
        return formatBluetooth(*asSockaddrRC());
#endif
    default:
        return "<unknown-family:" + std::to_string(m_storage.ss_family) + ">";
    }
}

//------------------

std::optional<SocketAddress>
SocketAddress::makeSocketAddress(SocketScoped::domain_t domain, std::string_view source)
{
    switch (domain) {
    case AF_INET:
        return makeIPv4(source);
    case AF_INET6:
        return makeIPv6(source);
    case AF_UNIX:
        return makeUnix(source);
    case AF_PACKET:
        return makePacket(source);
    case AF_NETLINK:
        return makeNetlink(source);
#ifdef KASLIBCXX_NET_HAS_BLUETOOTH
    case AF_BLUETOOTH:
        return makeBluetooth(source);
#endif
    }
    return std::nullopt;
}

//------------------

[[nodiscard]]
std::optional<SocketAddress>
SocketAddress::makeIPv4(std::string_view ip_port)
{
    const auto colon_pos = ip_port.find(':');
    if (colon_pos == std::string_view::npos) [[unlikely]]
        return std::nullopt;

    const std::string_view ip_str { ip_port.substr(0, colon_pos) };
    const std::string_view port_str { ip_port.substr(colon_pos + 1) };

    if (port_str.find(':') != std::string_view::npos) [[unlikely]]
        return std::nullopt;

    const auto port_exp = utils::stringToInteger<std::uint16_t>(port_str);
    if (!port_exp) [[unlikely]]
        return std::nullopt;

    sockaddr_storage storage { };
    auto* addr = reinterpret_cast<sockaddr_in*>(&storage);

    addr->sin_family = AF_INET;
    addr->sin_port = htons(port_exp.value());

    if (ip_str.empty() || ip_str == "0.0.0.0" || ip_str == "*") {
        addr->sin_addr.s_addr = INADDR_ANY;
    } else {

        const std::string ip_owned { ip_str };
        in_addr ip { };
        if (::inet_pton(AF_INET, ip_owned.c_str(), &ip) != 1) [[unlikely]]
            return std::nullopt;
        addr->sin_addr = ip;
    }

    return SocketAddress { }
        .setSockaddrStorage(std::move(storage))
        .setSockaddrLenIN();
}

//------------------

[[nodiscard]]
std::optional<SocketAddress>
SocketAddress::makeUnix(std::string_view path)
{
    if (path.empty()) [[unlikely]]
        return std::nullopt;

    constexpr std::size_t max_len = sizeof(sockaddr_un::sun_path) - 1;
    if (path.length() > max_len) [[unlikely]]
        return std::nullopt;

    sockaddr_storage storage { };
    auto* addr = reinterpret_cast<sockaddr_un*>(&storage);

    addr->sun_family = AF_UNIX;
    std::copy_n(path.begin(), path.length(), addr->sun_path);
    if (path.front() != '\0')
        addr->sun_path[path.length()] = '\0';

    return SocketAddress { }
        .setSockaddrStorage(std::move(storage))
        .setSockaddrLenUN();
}

//------------------

[[nodiscard]]
const sockaddr*
SocketAddress::asSockaddr() const&
{
    return reinterpret_cast<const sockaddr*>(&m_storage);
}

//------------------

[[nodiscard]]
const sockaddr_in*
SocketAddress::asSockaddrIN() const&
{
    return reinterpret_cast<const sockaddr_in*>(&m_storage);
}

//------------------

[[nodiscard]]
const sockaddr_un*
SocketAddress::asSockaddrUN() const&
{
    return reinterpret_cast<const sockaddr_un*>(&m_storage);
}

//------------------

[[nodiscard]]
socklen_t
SocketAddress::getAddrLen() const&
{
    return m_addr_len;
}

//------------------

SocketAddress& SocketAddress::setSockaddrLenIN() &
{
    return setSockaddrLen(sizeof(sockaddr_in));
}

//------------------

SocketAddress&& SocketAddress::setSockaddrLenIN() &&
{
    setSockaddrLen(sizeof(sockaddr_in));
    return std::move(*this);
}

//------------------

SocketAddress& SocketAddress::setSockaddrLenUN() &
{
    return setSockaddrLen(sizeof(sockaddr_un));
}

//------------------

SocketAddress&& SocketAddress::setSockaddrLenUN() &&
{
    setSockaddrLen(sizeof(sockaddr_un));
    return std::move(*this);
}

//------------------

SocketAddress& SocketAddress::setSockaddrLen(socklen_t length) &
{
    m_addr_len = length;
    return *this;
}
//------------------

SocketAddress&& SocketAddress::setSockaddrLen(socklen_t length) &&
{
    setSockaddrLen(length);
    return std::move(*this);
}

//-----------------

SocketAddress& SocketAddress::setSockaddrStorage(sockaddr_storage storage) &
{
    m_storage = std::move(storage);
    return *this;
}

//------------------

SocketAddress&& SocketAddress::setSockaddrStorage(sockaddr_storage storage) &&
{
    setSockaddrStorage(std::move(storage));
    return std::move(*this);
}

//------------------

[[nodiscard]]
std::optional<SocketAddress>
SocketAddress::makeIPv6(std::string_view ip_port)
{
    // [addr]:port
    if (ip_port.empty() || ip_port.front() != '[') [[unlikely]]
        return std::nullopt;

    const auto close_bracket = ip_port.rfind(']');
    if (close_bracket == std::string_view::npos) [[unlikely]]
        return std::nullopt;

    if (close_bracket + 2 >= ip_port.size()) [[unlikely]]
        return std::nullopt;

    if (ip_port[close_bracket + 1] != ':') [[unlikely]]
        return std::nullopt;

    std::string_view ip_str = ip_port.substr(1, close_bracket - 1);
    std::string_view port_str = ip_port.substr(close_bracket + 2);

    if (port_str.find(':') != std::string_view::npos) [[unlikely]]
        return std::nullopt;

    const auto port_exp = utils::stringToInteger<std::uint16_t>(port_str);
    if (!port_exp) [[unlikely]]
        return std::nullopt;

    // scope id - "fe80::1%eth0"
    std::uint32_t scope_id = 0;
    const auto scope_pos = ip_str.find('%');
    if (scope_pos != std::string_view::npos) {
        const std::string ifname(ip_str.substr(scope_pos + 1));
        scope_id = ::if_nametoindex(ifname.c_str());
        if (scope_id == 0) [[unlikely]]
            return std::nullopt;
        ip_str = ip_str.substr(0, scope_pos);
    }

    sockaddr_storage storage { };
    auto* addr = reinterpret_cast<sockaddr_in6*>(&storage);

    addr->sin6_family = AF_INET6;
    addr->sin6_port = htons(port_exp.value());
    addr->sin6_scope_id = scope_id;

    const std::string ip_owned(ip_str);
    if (::inet_pton(AF_INET6, ip_owned.c_str(), &addr->sin6_addr) != 1) [[unlikely]]
        return std::nullopt;

    return SocketAddress { }
        .setSockaddrStorage(std::move(storage))
        .setSockaddrLenIN6();
}

//------------------

[[nodiscard]]
std::optional<SocketAddress>
SocketAddress::makePacket(std::string_view ifname, std::uint16_t protocol)
{
    if (ifname.empty()) [[unlikely]]
        return std::nullopt;

    // optional ":protocol" (hex or dec)
    std::string_view name = ifname;
    const auto colon_pos = ifname.find(':');
    if (colon_pos != std::string_view::npos) {
        name = ifname.substr(0, colon_pos);
        std::string_view proto_str = ifname.substr(colon_pos + 1);

        if (proto_str.empty()) [[unlikely]]
            return std::nullopt;

        // hex prefix "0x"/"0X" -> base 16, otherwise base 10
        int proto_base = 10;
        if (proto_str.size() > 2
            && proto_str[0] == '0'
            && (proto_str[1] == 'x' || proto_str[1] == 'X')) {
            proto_base = 16;
            proto_str.remove_prefix(2);
        }

        const auto proto_exp = utils::stringToInteger<std::uint16_t>(proto_str, proto_base);
        if (!proto_exp) [[unlikely]]
            return std::nullopt;
        protocol = proto_exp.value();
    }

    // ifindex: either a number or an interface name
    std::uint32_t ifindex = 0;
    const auto idx_exp = utils::stringToInteger<std::uint32_t>(name);
    if (idx_exp) {
        ifindex = idx_exp.value();
    } else {
        const std::string name_owned(name);
        ifindex = ::if_nametoindex(name_owned.c_str());
    }
    if (ifindex == 0) [[unlikely]]
        return std::nullopt;

    sockaddr_storage storage { };
    auto* addr = reinterpret_cast<sockaddr_ll*>(&storage);

    addr->sll_family = AF_PACKET;
    addr->sll_protocol = htons(protocol);
    addr->sll_ifindex = static_cast<int>(ifindex);
    // other fields (sll_hatype, sll_pkttype, sll_halen, sll_addr) - 0

    return SocketAddress { }
        .setSockaddrStorage(std::move(storage))
        .setSockaddrLenLL();
}

//------------------

[[nodiscard]]
std::optional<SocketAddress>
SocketAddress::makeNetlink(std::string_view pid)
{
    if (pid.empty()) [[unlikely]]
        return std::nullopt;

    const auto pid_exp = utils::stringToInteger<std::uint32_t>(pid);
    if (!pid_exp) [[unlikely]]
        return std::nullopt;

    sockaddr_storage storage { };
    auto* addr = reinterpret_cast<sockaddr_nl*>(&storage);

    addr->nl_family = AF_NETLINK;
    addr->nl_pid = pid_exp.value();
    addr->nl_groups = 0; // unicast; for multicast - a separate parameter

    return SocketAddress { }
        .setSockaddrStorage(std::move(storage))
        .setSockaddrLenNL();
}

//------------------

#ifdef KASLIBCXX_NET_HAS_BLUETOOTH
[[nodiscard]]
std::optional<SocketAddress>
SocketAddress::makeBluetooth(std::string_view addr)
{
    if (addr.empty()) [[unlikely]]
        return std::nullopt;

    std::string_view bdaddr_str = addr;
    std::uint8_t channel = 0;

    // Explicit channel separator: "AA:BB:CC:DD:EE:FF#1"
    const auto hash_pos = addr.rfind('#');
    if (hash_pos != std::string_view::npos) {
        const auto ch_str = addr.substr(hash_pos + 1);
        if (ch_str.empty()) [[unlikely]]
            return std::nullopt;
        const auto ch_exp = utils::stringToInteger<std::uint8_t>(ch_str);
        if (!ch_exp) [[unlikely]]
            return std::nullopt;
        channel = ch_exp.value();
        bdaddr_str = addr.substr(0, hash_pos);
    }

    const std::string bdaddr_owned(bdaddr_str);
    bdaddr_t bdaddr { };
    if (::str2ba(bdaddr_owned.c_str(), &bdaddr) != 0) [[unlikely]]
        return std::nullopt;

    sockaddr_storage storage { };
    auto* addr_rc = reinterpret_cast<sockaddr_rc*>(&storage);
    addr_rc->rc_family = AF_BLUETOOTH;
    addr_rc->rc_bdaddr = bdaddr;
    addr_rc->rc_channel = channel;

    return SocketAddress { }
        .setSockaddrStorage(std::move(storage))
        .setSockaddrLenRC();
}
#endif

//------------------

[[nodiscard]] const sockaddr_in6*
SocketAddress::asSockaddrIN6() const&
{
    return reinterpret_cast<const sockaddr_in6*>(&m_storage);
}

//------------------

[[nodiscard]] const sockaddr_ll*
SocketAddress::asSockaddrLL() const&
{
    return reinterpret_cast<const sockaddr_ll*>(&m_storage);
}

//------------------

[[nodiscard]] const sockaddr_nl*
SocketAddress::asSockaddrNL() const&
{
    return reinterpret_cast<const sockaddr_nl*>(&m_storage);
}

//------------------

#ifdef KASLIBCXX_NET_HAS_BLUETOOTH
[[nodiscard]] const sockaddr_rc*
SocketAddress::asSockaddrRC() const&
{
    return reinterpret_cast<const sockaddr_rc*>(&m_storage);
}

//------------------
#endif

SocketAddress& SocketAddress::setSockaddrLenIN6() &
{
    return setSockaddrLen(sizeof(sockaddr_in6));
}

//------------------

SocketAddress&& SocketAddress::setSockaddrLenIN6() &&
{
    return std::move(setSockaddrLen(sizeof(sockaddr_in6)));
}

//------------------

SocketAddress& SocketAddress::setSockaddrLenLL() &
{
    return setSockaddrLen(sizeof(sockaddr_ll));
}

//------------------

SocketAddress&& SocketAddress::setSockaddrLenLL() &&
{
    return std::move(setSockaddrLen(sizeof(sockaddr_ll)));
}

//------------------

SocketAddress& SocketAddress::setSockaddrLenNL() &
{
    return setSockaddrLen(sizeof(sockaddr_nl));
}

//------------------

SocketAddress&& SocketAddress::setSockaddrLenNL() &&
{
    return std::move(setSockaddrLen(sizeof(sockaddr_nl)));
}

//------------------

#ifdef KASLIBCXX_NET_HAS_BLUETOOTH
SocketAddress& SocketAddress::setSockaddrLenRC() &
{
    return setSockaddrLen(sizeof(sockaddr_rc));
}

//------------------

SocketAddress&& SocketAddress::setSockaddrLenRC() &&
{
    return std::move(setSockaddrLen(sizeof(sockaddr_rc)));
}

//------------------
#endif

} // namespace kas::net