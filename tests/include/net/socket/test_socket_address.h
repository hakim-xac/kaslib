/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_NET_SOCKET_SOCKET_ADDRESS_TESTS_H
#define KASLIBCXX_NET_SOCKET_SOCKET_ADDRESS_TESTS_H

#include <arpa/inet.h>
#include <cstring>
#include <netinet/in.h>
#include <optional>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>

#include "KAS/net/socket/SocketAddress.h"
#include "tests/tests.h"

namespace kas::net::socket::socket_address::tests {

namespace {
    using namespace kas::tests;
    using kas::net::socket::SocketAddress;
    using kas::net::socket::SocketDomain;

    // ==================================================================
    // default ctor
    // ==================================================================

    /// Default-constructed address has zero length and AF_UNSPEC family.
    inline void defaultCtor(TestContext& ctx)
    {
        SocketAddress a;
        KAS_TEST_CHECK_IMPL(ctx, a.getAddrLen() == 0u);
        KAS_TEST_CHECK_IMPL(ctx, a.asSockaddr() != nullptr);
        KAS_TEST_CHECK_IMPL(ctx, a.asSockaddr()->sa_family == AF_UNSPEC);
    }

    // ==================================================================
    // makeIPv4
    // ==================================================================

    /// "127.0.0.1:8080" parses into AF_INET with the loopback address and port 8080.
    inline void makeIPv4WithPort(TestContext& ctx)
    {
        const auto opt = SocketAddress::makeIPv4("127.0.0.1:8080");
        KAS_TEST_CHECK_IMPL(ctx, opt.has_value());
        if (!opt)
            return;

        const auto* sin = opt->asSockaddrIN();
        KAS_TEST_CHECK_IMPL(ctx, sin->sin_family == AF_INET);
        KAS_TEST_CHECK_IMPL(ctx, sin->sin_addr.s_addr == htonl(INADDR_LOOPBACK));
        KAS_TEST_CHECK_IMPL(ctx, ntohs(sin->sin_port) == 8080);
    }

    /// Missing port yields std::nullopt.
    inline void makeIPv4NoPort(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, !SocketAddress::makeIPv4("127.0.0.1").has_value());
    }

    /// Non-numeric port yields std::nullopt.
    inline void makeIPv4BadPort(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, !SocketAddress::makeIPv4("127.0.0.1:bad").has_value());
    }

    /// Port greater than 65535 yields std::nullopt.
    inline void makeIPv4PortTooBig(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, !SocketAddress::makeIPv4("127.0.0.1:70000").has_value());
    }

    /// Port 0 is accepted.
    inline void makeIPv4PortZero(TestContext& ctx)
    {
        const auto opt = SocketAddress::makeIPv4("127.0.0.1:0");
        KAS_TEST_CHECK_IMPL(ctx, opt.has_value());
        if (!opt)
            return;
        KAS_TEST_CHECK_IMPL(ctx, ntohs(opt->asSockaddrIN()->sin_port) == 0);
    }

    /// "0.0.0.0" maps to INADDR_ANY.
    inline void makeIPv4Any(TestContext& ctx)
    {
        const auto opt = SocketAddress::makeIPv4("0.0.0.0:80");
        KAS_TEST_CHECK_IMPL(ctx, opt.has_value());
        if (!opt)
            return;
        KAS_TEST_CHECK_IMPL(ctx, opt->asSockaddrIN()->sin_addr.s_addr == INADDR_ANY);
    }

    /// "*" is accepted as a wildcard address (INADDR_ANY).
    inline void makeIPv4Star(TestContext& ctx)
    {
        const auto opt = SocketAddress::makeIPv4("*:80");
        KAS_TEST_CHECK_IMPL(ctx, opt.has_value());
        if (!opt)
            return;
        KAS_TEST_CHECK_IMPL(ctx, opt->asSockaddrIN()->sin_addr.s_addr == INADDR_ANY);
    }

    /// Empty IP before ':' maps to INADDR_ANY with the given port.
    inline void makeIPv4EmptyIP(TestContext& ctx)
    {
        const auto opt = SocketAddress::makeIPv4(":80");
        KAS_TEST_CHECK_IMPL(ctx, opt.has_value());
        if (!opt)
            return;
        KAS_TEST_CHECK_IMPL(ctx, opt->asSockaddrIN()->sin_addr.s_addr == INADDR_ANY);
        KAS_TEST_CHECK_IMPL(ctx, ntohs(opt->asSockaddrIN()->sin_port) == 80);
    }

    /// Invalid IP yields std::nullopt.
    inline void makeIPv4InvalidIp(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, !SocketAddress::makeIPv4("not-an-ip:80").has_value());
    }

    /// Extra ':' yields std::nullopt.
    inline void makeIPv4ExtraColon(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, !SocketAddress::makeIPv4("1.2.3.4:5:6").has_value());
    }

    // ==================================================================
    // makeUnix
    // ==================================================================

    /// "/tmp/test.sock" parses into AF_UNIX with the matching sun_path.
    inline void makeUnix(TestContext& ctx)
    {
        const auto opt = SocketAddress::makeUnix("/tmp/test.sock");
        KAS_TEST_CHECK_IMPL(ctx, opt.has_value());
        if (!opt)
            return;

        const auto* sun = opt->asSockaddrUN();
        KAS_TEST_CHECK_IMPL(ctx, sun->sun_family == AF_UNIX);
        KAS_TEST_CHECK_IMPL(ctx, std::strcmp(sun->sun_path, "/tmp/test.sock") == 0);
    }

    // ==================================================================
    // makeIPv6 - "[ip]:port"
    // ==================================================================

    /// "[::1]:8080" parses into AF_INET6 loopback with port 8080.
    inline void makeIPv6Basic(TestContext& ctx)
    {
        const auto opt = SocketAddress::makeIPv6("[::1]:8080");
        KAS_TEST_CHECK_IMPL(ctx, opt.has_value());
        if (!opt)
            return;

        const auto* sin6 = opt->asSockaddrIN6();
        KAS_TEST_CHECK_IMPL(ctx, sin6->sin6_family == AF_INET6);
        KAS_TEST_CHECK_IMPL(ctx, ntohs(sin6->sin6_port) == 8080);
        KAS_TEST_CHECK_IMPL(ctx, IN6_IS_ADDR_LOOPBACK(&sin6->sin6_addr));
    }

    /// "[::]:80" maps to the unspecified (any) IPv6 address.
    inline void makeIPv6Any(TestContext& ctx)
    {
        const auto opt = SocketAddress::makeIPv6("[::]:80");
        KAS_TEST_CHECK_IMPL(ctx, opt.has_value());
        if (!opt)
            return;
        KAS_TEST_CHECK_IMPL(ctx,
            IN6_IS_ADDR_UNSPECIFIED(&opt->asSockaddrIN6()->sin6_addr));
    }

    /// A full-form IPv6 address with a port parses correctly.
    inline void makeIPv6FullForm(TestContext& ctx)
    {
        const auto opt = SocketAddress::makeIPv6("[2001:db8::1]:443");
        KAS_TEST_CHECK_IMPL(ctx, opt.has_value());
        if (!opt)
            return;
        KAS_TEST_CHECK_IMPL(ctx, ntohs(opt->asSockaddrIN6()->sin6_port) == 443);
    }

    /// Without brackets, the ip:port split is impossible -> nullopt.
    inline void makeIPv6NoBrackets(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, !SocketAddress::makeIPv6("::1:8080").has_value());
    }

    /// "[::1]" without ":port" yields nullopt.
    inline void makeIPv6NoPort(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, !SocketAddress::makeIPv6("[::1]").has_value());
    }

    /// Non-numeric or out-of-range port yields nullopt.
    inline void makeIPv6BadPort(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, !SocketAddress::makeIPv6("[::1]:bad").has_value());
        KAS_TEST_CHECK_IMPL(ctx, !SocketAddress::makeIPv6("[::1]:70000").has_value());
    }

    /// Invalid IPv6 address yields nullopt.
    inline void makeIPv6BadAddr(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, !SocketAddress::makeIPv6("[not::ip]:80").has_value());
    }

    /// Empty string yields nullopt.
    inline void makeIPv6Empty(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, !SocketAddress::makeIPv6("").has_value());
    }

    /// Extra ':' after the port yields nullopt.
    inline void makeIPv6ExtraColon(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, !SocketAddress::makeIPv6("[::1]:80:90").has_value());
    }

    // ==================================================================
    // makePacket - interface
    // ==================================================================

    /// "lo" resolves to AF_PACKET with a positive ifindex.
    inline void makePacketLoopback(TestContext& ctx)
    {
        // "lo" is always present on Linux.
        const auto opt = SocketAddress::makePacket("lo");
        KAS_TEST_CHECK_IMPL(ctx, opt.has_value());
        if (!opt)
            return;

        const auto* ll = opt->asSockaddrLL();
        KAS_TEST_CHECK_IMPL(ctx, ll->sll_family == AF_PACKET);
        KAS_TEST_CHECK_IMPL(ctx, ll->sll_ifindex > 0);
    }

    /// "lo:0x0800" sets the protocol (ETH_P_IP).
    inline void makePacketWithProtocol(TestContext& ctx)
    {
        const auto opt = SocketAddress::makePacket("lo:0x0800");
        KAS_TEST_CHECK_IMPL(ctx, opt.has_value());
        if (!opt)
            return;
        KAS_TEST_CHECK_IMPL(ctx, opt->asSockaddrLL()->sll_family == AF_PACKET);
        KAS_TEST_CHECK_IMPL(ctx,
            ntohs(opt->asSockaddrLL()->sll_protocol) == 0x0800);
    }

    /// "lo:" with an empty protocol yields nullopt.
    inline void makePacketEmptyProtocol(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, !SocketAddress::makePacket("lo:").has_value());
    }

    /// A decimal protocol "lo:2048" is accepted and equals 0x0800.
    inline void makePacketProtocolDecimal(TestContext& ctx)
    {
        const auto opt = SocketAddress::makePacket("lo:2048");
        KAS_TEST_CHECK_IMPL(ctx, opt.has_value());
        if (!opt)
            return;
        KAS_TEST_CHECK_IMPL(ctx,
            ntohs(opt->asSockaddrLL()->sll_protocol) == 0x0800);
    }

    /// Uppercase "0X" hex prefix is accepted.
    inline void makePacketProtocolHexUpper(TestContext& ctx)
    {
        const auto opt = SocketAddress::makePacket("lo:0X0800");
        KAS_TEST_CHECK_IMPL(ctx, opt.has_value());
        if (!opt)
            return;
        KAS_TEST_CHECK_IMPL(ctx,
            ntohs(opt->asSockaddrLL()->sll_protocol) == 0x0800);
    }

    /// "1:0x0800" sets ifindex 1 and protocol 0x0800.
    inline void makePacketIndexWithProtocol(TestContext& ctx)
    {
        const auto opt = SocketAddress::makePacket("1:0x0800");
        KAS_TEST_CHECK_IMPL(ctx, opt.has_value());
        if (!opt)
            return;
        KAS_TEST_CHECK_IMPL(ctx, opt->asSockaddrLL()->sll_ifindex == 1);
        KAS_TEST_CHECK_IMPL(ctx,
            ntohs(opt->asSockaddrLL()->sll_protocol) == 0x0800);
    }

    /// "1" sets ifindex 1 (usually "lo" on Linux).
    inline void makePacketByIndex(TestContext& ctx)
    {
        const auto opt = SocketAddress::makePacket("1");
        KAS_TEST_CHECK_IMPL(ctx, opt.has_value());
        if (!opt)
            return;
        KAS_TEST_CHECK_IMPL(ctx, opt->asSockaddrLL()->sll_ifindex == 1);
    }

    /// Unknown interface name yields nullopt.
    inline void makePacketNoSuchIface(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx,
            !SocketAddress::makePacket("no_such_if_zzz").has_value());
    }

    /// Empty string yields nullopt.
    inline void makePacketEmpty(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, !SocketAddress::makePacket("").has_value());
    }

    /// Non-hex, non-numeric protocol yields nullopt.
    inline void makePacketBadProtocol(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx,
            !SocketAddress::makePacket("lo:notahex").has_value());
    }

    // ==================================================================
    // makeNetlink - pid
    // ==================================================================

    /// "0" is the kernel netlink PID.
    inline void makeNetlinkKernel(TestContext& ctx)
    {
        const auto opt = SocketAddress::makeNetlink("0");
        KAS_TEST_CHECK_IMPL(ctx, opt.has_value());
        if (!opt)
            return;
        KAS_TEST_CHECK_IMPL(ctx, opt->asSockaddrNL()->nl_family == AF_NETLINK);
        KAS_TEST_CHECK_IMPL(ctx, opt->asSockaddrNL()->nl_pid == 0);
    }

    /// A custom numeric PID is stored as-is.
    inline void makeNetlinkCustomPid(TestContext& ctx)
    {
        const auto opt = SocketAddress::makeNetlink("12345");
        KAS_TEST_CHECK_IMPL(ctx, opt.has_value());
        if (!opt)
            return;
        KAS_TEST_CHECK_IMPL(ctx, opt->asSockaddrNL()->nl_pid == 12345);
    }

    /// Empty string yields nullopt.
    inline void makeNetlinkEmpty(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, !SocketAddress::makeNetlink("").has_value());
    }

    /// Non-numeric PID yields nullopt.
    inline void makeNetlinkInvalid(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, !SocketAddress::makeNetlink("abc").has_value());
    }

    // ==================================================================
    // makeBluetooth (only if BlueZ)
    // ==================================================================

#ifdef KASLIBCXX_NET_HAS_BLUETOOTH

    /// A MAC without channel yields rc_channel == 0.
    inline void makeBluetoothMac(TestContext& ctx)
    {
        const auto opt = SocketAddress::makeBluetooth("AA:BB:CC:DD:EE:FF");
        KAS_TEST_CHECK_IMPL(ctx, opt.has_value());
        if (!opt)
            return;
        KAS_TEST_CHECK_IMPL(ctx, opt->asSockaddrRC()->rc_family == AF_BLUETOOTH);
        KAS_TEST_CHECK_IMPL(ctx, opt->asSockaddrRC()->rc_channel == 0);
    }

    /// A MAC with ":channel" sets rc_channel.
    inline void makeBluetoothMacChannel(TestContext& ctx)
    {
        const auto opt = SocketAddress::makeBluetooth("AA:BB:CC:DD:EE:FF:1");
        KAS_TEST_CHECK_IMPL(ctx, opt.has_value());
        if (!opt)
            return;
        KAS_TEST_CHECK_IMPL(ctx, opt->asSockaddrRC()->rc_channel == 1);
    }

    /// Invalid or empty input yields nullopt.
    inline void makeBluetoothInvalid(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, !SocketAddress::makeBluetooth("bad").has_value());
        KAS_TEST_CHECK_IMPL(ctx, !SocketAddress::makeBluetooth("").has_value());
    }

#endif // KASLIBCXX_NET_HAS_BLUETOOTH

    // ==================================================================
    // makeByEnum - other domains
    // ==================================================================

    /// make(SocketDomain::IPv6, ...) dispatches to the IPv6 parser.
    inline void makeByEnumIPv6(TestContext& ctx)
    {
        const auto opt = SocketAddress::make(SocketDomain::IPv6, "[::1]:80");
        KAS_TEST_CHECK_IMPL(ctx, opt.has_value());
        if (!opt)
            return;
        KAS_TEST_CHECK_IMPL(ctx, opt->asSockaddrIN6()->sin6_family == AF_INET6);
    }

    /// make(SocketDomain::Packet, ...) dispatches to the packet parser.
    inline void makeByEnumPacket(TestContext& ctx)
    {
        const auto opt = SocketAddress::make(SocketDomain::Packet, "lo");
        KAS_TEST_CHECK_IMPL(ctx, opt.has_value());
        if (!opt)
            return;
        KAS_TEST_CHECK_IMPL(ctx, opt->asSockaddrLL()->sll_family == AF_PACKET);
    }

    /// make(SocketDomain::Netlink, ...) dispatches to the netlink parser.
    inline void makeByEnumNetlink(TestContext& ctx)
    {
        const auto opt = SocketAddress::make(SocketDomain::Netlink, "0");
        KAS_TEST_CHECK_IMPL(ctx, opt.has_value());
        if (!opt)
            return;
        KAS_TEST_CHECK_IMPL(ctx, opt->asSockaddrNL()->nl_family == AF_NETLINK);
    }

    // ==================================================================
    // setSockaddrLenIN6 / LL / NL / RC
    // ==================================================================

    /// setSockaddrLenIN6 sets the length to sizeof(sockaddr_in6).
    inline void setLenIN6(TestContext& ctx)
    {
        SocketAddress a;
        a.setSockaddrLenIN6();
        KAS_TEST_CHECK_IMPL(ctx, a.getAddrLen() == sizeof(sockaddr_in6));
    }

    /// setSockaddrLenLL sets the length to sizeof(sockaddr_ll).
    inline void setLenLL(TestContext& ctx)
    {
        SocketAddress a;
        a.setSockaddrLenLL();
        KAS_TEST_CHECK_IMPL(ctx, a.getAddrLen() == sizeof(sockaddr_ll));
    }

    /// setSockaddrLenNL sets the length to sizeof(sockaddr_nl).
    inline void setLenNL(TestContext& ctx)
    {
        SocketAddress a;
        a.setSockaddrLenNL();
        KAS_TEST_CHECK_IMPL(ctx, a.getAddrLen() == sizeof(sockaddr_nl));
    }

#ifdef KASLIBCXX_NET_HAS_BLUETOOTH
    /// setSockaddrLenRC sets the length to sizeof(sockaddr_rc).
    inline void setLenRC(TestContext& ctx)
    {
        SocketAddress a;
        a.setSockaddrLenRC();
        KAS_TEST_CHECK_IMPL(ctx, a.getAddrLen() == sizeof(sockaddr_rc));
    }
#endif

    // ==================================================================
    // getAddrLen - IPv6/Packet/Netlink
    // ==================================================================

    /// Address length for an IPv6 address equals sizeof(sockaddr_in6).
    inline void addrLenIPv6(TestContext& ctx)
    {
        const auto opt = SocketAddress::makeIPv6("[::1]:80");
        KAS_TEST_CHECK_IMPL(ctx, opt.has_value());
        if (!opt)
            return;
        KAS_TEST_CHECK_IMPL(ctx, opt->getAddrLen() == sizeof(sockaddr_in6));
    }

    /// Address length for a packet address equals sizeof(sockaddr_ll).
    inline void addrLenPacket(TestContext& ctx)
    {
        const auto opt = SocketAddress::makePacket("lo");
        KAS_TEST_CHECK_IMPL(ctx, opt.has_value());
        if (!opt)
            return;
        KAS_TEST_CHECK_IMPL(ctx, opt->getAddrLen() == sizeof(sockaddr_ll));
    }

    /// Address length for a netlink address equals sizeof(sockaddr_nl).
    inline void addrLenNetlink(TestContext& ctx)
    {
        const auto opt = SocketAddress::makeNetlink("0");
        KAS_TEST_CHECK_IMPL(ctx, opt.has_value());
        if (!opt)
            return;
        KAS_TEST_CHECK_IMPL(ctx, opt->getAddrLen() == sizeof(sockaddr_nl));
    }

    // ==================================================================
    // asSockaddr consistency - IPv6 / LL / NL
    // ==================================================================

    /// asSockaddr() and asSockaddrIN6() point to the same storage.
    inline void asSockaddrConsistentIPv6(TestContext& ctx)
    {
        const auto opt = SocketAddress::makeIPv6("[::1]:80");
        KAS_TEST_CHECK_IMPL(ctx, opt.has_value());
        if (!opt)
            return;
        KAS_TEST_CHECK_IMPL(ctx,
            reinterpret_cast<const void*>(opt->asSockaddr())
                == reinterpret_cast<const void*>(opt->asSockaddrIN6()));
    }

    /// Empty Unix path yields nullopt.
    inline void makeUnixEmpty(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, !SocketAddress::makeUnix("").has_value());
    }

    /// Overly long Unix path (200 chars) yields nullopt.
    inline void makeUnixTooLong(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx,
            !SocketAddress::makeUnix(std::string(200, 'x')).has_value());
    }

    /// A Unix path of exactly the maximum length is accepted.
    inline void makeUnixExactMax(TestContext& ctx)
    {
        constexpr std::size_t max_len = sizeof(sockaddr_un::sun_path) - 1;
        const auto opt = SocketAddress::makeUnix(std::string(max_len, 'x'));
        KAS_TEST_CHECK_IMPL(ctx, opt.has_value());
    }

    // ==================================================================
    // make(SocketDomain, source) - enum dispatch
    // ==================================================================

    /// make(SocketDomain::IPv4, ...) dispatches to the IPv4 parser.
    inline void makeByEnumIPv4(TestContext& ctx)
    {
        const auto opt = SocketAddress::make(SocketDomain::IPv4, "127.0.0.1:8080");
        KAS_TEST_CHECK_IMPL(ctx, opt.has_value());
        if (!opt)
            return;
        KAS_TEST_CHECK_IMPL(ctx, opt->asSockaddrIN()->sin_family == AF_INET);
        KAS_TEST_CHECK_IMPL(ctx, ntohs(opt->asSockaddrIN()->sin_port) == 8080);
    }

    /// make(SocketDomain::Unix, ...) dispatches to the Unix parser.
    inline void makeByEnumUnix(TestContext& ctx)
    {
        const auto opt = SocketAddress::make(SocketDomain::Unix, "/tmp/x.sock");
        KAS_TEST_CHECK_IMPL(ctx, opt.has_value());
        if (!opt)
            return;
        KAS_TEST_CHECK_IMPL(ctx, opt->asSockaddrUN()->sun_family == AF_UNIX);
    }

    // ==================================================================
    // makeSocketAddress(domain_t, source) - legacy, still works
    // ==================================================================

    /// makeSocketAddress(AF_INET, ...) succeeds.
    inline void makeSocketAddressIPv4(TestContext& ctx)
    {
        const auto opt = SocketAddress::makeSocketAddress(AF_INET, "127.0.0.1:80");
        KAS_TEST_CHECK_IMPL(ctx, opt.has_value());
    }

    /// makeSocketAddress(AF_UNIX, ...) succeeds.
    inline void makeSocketAddressUnix(TestContext& ctx)
    {
        const auto opt = SocketAddress::makeSocketAddress(AF_UNIX, "/tmp/x.sock");
        KAS_TEST_CHECK_IMPL(ctx, opt.has_value());
    }

    /// makeSocketAddress(AF_INET6, ...) is currently unsupported -> nullopt.
    inline void makeSocketAddressUnsupported(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx,
            !SocketAddress::makeSocketAddress(AF_INET6, "::1").has_value());
    }

    // ==================================================================
    // getAddrLen
    // ==================================================================

    /// Address length for an IPv4 address equals sizeof(sockaddr_in).
    inline void addrLenIPv4(TestContext& ctx)
    {
        const auto opt = SocketAddress::makeIPv4("127.0.0.1:80");
        KAS_TEST_CHECK_IMPL(ctx, opt.has_value());
        if (!opt)
            return;
        KAS_TEST_CHECK_IMPL(ctx, opt->getAddrLen() == sizeof(sockaddr_in));
    }

    /// Address length for a Unix address equals sizeof(sockaddr_un).
    inline void addrLenUnix(TestContext& ctx)
    {
        const auto opt = SocketAddress::makeUnix("/tmp/x.sock");
        KAS_TEST_CHECK_IMPL(ctx, opt.has_value());
        if (!opt)
            return;
        KAS_TEST_CHECK_IMPL(ctx, opt->getAddrLen() == sizeof(sockaddr_un));
    }

    // ==================================================================
    // setSockaddrLen / setSockaddrStorage
    // ==================================================================

    /// setSockaddrLenIN sets the length to sizeof(sockaddr_in).
    inline void setLenIN(TestContext& ctx)
    {
        SocketAddress a;
        a.setSockaddrLenIN();
        KAS_TEST_CHECK_IMPL(ctx, a.getAddrLen() == sizeof(sockaddr_in));
    }

    /// setSockaddrLenUN sets the length to sizeof(sockaddr_un).
    inline void setLenUN(TestContext& ctx)
    {
        SocketAddress a;
        a.setSockaddrLenUN();
        KAS_TEST_CHECK_IMPL(ctx, a.getAddrLen() == sizeof(sockaddr_un));
    }

    /// setSockaddrLen stores an explicit length.
    inline void setLenCustom(TestContext& ctx)
    {
        SocketAddress a;
        a.setSockaddrLen(42);
        KAS_TEST_CHECK_IMPL(ctx, a.getAddrLen() == 42u);
    }

    /// setSockaddrLen returns a reference to the same object.
    inline void setLenReturnsSelf(TestContext& ctx)
    {
        SocketAddress a;
        SocketAddress& ref = a.setSockaddrLen(10);
        KAS_TEST_CHECK_IMPL(ctx, &ref == &a);
    }

    /// setSockaddrStorage copies the raw storage verbatim.
    inline void setStorage(TestContext& ctx)
    {
        sockaddr_storage storage { };
        auto* sin = reinterpret_cast<sockaddr_in*>(&storage);
        sin->sin_family = AF_INET;
        sin->sin_port = htons(12345);
        sin->sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        SocketAddress a;
        a.setSockaddrStorage(storage);

        const auto* result = a.asSockaddrIN();
        KAS_TEST_CHECK_IMPL(ctx, result->sin_family == AF_INET);
        KAS_TEST_CHECK_IMPL(ctx, ntohs(result->sin_port) == 12345);
        KAS_TEST_CHECK_IMPL(ctx, result->sin_addr.s_addr == htonl(INADDR_LOOPBACK));
    }

    // ==================================================================
    // asSockaddr consistency
    // ==================================================================

    /// asSockaddr() and asSockaddrIN() point to the same storage.
    inline void asSockaddrConsistent(TestContext& ctx)
    {
        const auto opt = SocketAddress::makeIPv4("127.0.0.1:80");
        KAS_TEST_CHECK_IMPL(ctx, opt.has_value());
        if (!opt)
            return;

        const auto* sa = opt->asSockaddr();
        const auto* sin = opt->asSockaddrIN();
        KAS_TEST_CHECK_IMPL(ctx,
            reinterpret_cast<const void*>(sa) == reinterpret_cast<const void*>(sin));
    }

    // ==================================================================
    // Type properties
    // ==================================================================
    static_assert(std::is_default_constructible_v<SocketAddress>);
    static_assert(std::is_copy_constructible_v<SocketAddress>);
    static_assert(std::is_copy_assignable_v<SocketAddress>);
    static_assert(std::is_move_constructible_v<SocketAddress>);
    static_assert(std::is_move_assignable_v<SocketAddress>);

} // namespace

// ============================================================
// Entry point
// ============================================================

/**
 * @brief Runs all SocketAddress tests.
 *
 * Executes every test case defined in this header against a shared
 * TestContext and prints a summary for the
 * "kas::net::socket::socket_address::tests" suite.
 */
inline bool runTests()
{
    TestContext ctx;

    defaultCtor(ctx);

    makeIPv4WithPort(ctx);
    makeIPv4NoPort(ctx);
    makeIPv4BadPort(ctx);
    makeIPv4PortTooBig(ctx);
    makeIPv4PortZero(ctx);
    makeIPv4Any(ctx);
    makeIPv4Star(ctx);
    makeIPv4EmptyIP(ctx);
    makeIPv4InvalidIp(ctx);
    makeIPv4ExtraColon(ctx);

    makeUnix(ctx);
    makeUnixEmpty(ctx);
    makeUnixTooLong(ctx);
    makeUnixExactMax(ctx);

    makeIPv6Basic(ctx);
    makeIPv6Any(ctx);
    makeIPv6FullForm(ctx);
    makeIPv6NoBrackets(ctx);
    makeIPv6NoPort(ctx);
    makeIPv6BadPort(ctx);
    makeIPv6BadAddr(ctx);
    makeIPv6Empty(ctx);
    makeIPv6ExtraColon(ctx);

    makePacketLoopback(ctx);
    makePacketEmptyProtocol(ctx);
    makePacketProtocolDecimal(ctx);
    makePacketProtocolHexUpper(ctx);
    makePacketIndexWithProtocol(ctx);
    makePacketWithProtocol(ctx);
    makePacketByIndex(ctx);
    makePacketNoSuchIface(ctx);
    makePacketEmpty(ctx);
    makePacketBadProtocol(ctx);

    makeNetlinkKernel(ctx);
    makeNetlinkCustomPid(ctx);
    makeNetlinkEmpty(ctx);
    makeNetlinkInvalid(ctx);

#ifdef KASLIBCXX_NET_HAS_BLUETOOTH
    makeBluetoothMac(ctx);
    makeBluetoothMacChannel(ctx);
    makeBluetoothInvalid(ctx);
#endif

    makeByEnumIPv4(ctx);
    makeByEnumUnix(ctx);
    makeByEnumIPv6(ctx);
    makeByEnumPacket(ctx);
    makeByEnumNetlink(ctx);

    makeSocketAddressIPv4(ctx);
    makeSocketAddressUnix(ctx);
    makeSocketAddressUnsupported(ctx);

    addrLenIPv4(ctx);
    addrLenUnix(ctx);
    addrLenIPv6(ctx);
    addrLenPacket(ctx);
    addrLenNetlink(ctx);

    setLenIN(ctx);
    setLenUN(ctx);
    setLenIN6(ctx);
    setLenLL(ctx);
    setLenNL(ctx);
#ifdef KASLIBCXX_NET_HAS_BLUETOOTH
    setLenRC(ctx);
#endif
    setLenCustom(ctx);
    setLenReturnsSelf(ctx);

    setStorage(ctx);

    asSockaddrConsistent(ctx);
    asSockaddrConsistentIPv6(ctx);

    return printSummary("kas::net::socket::socket_address::tests", ctx);
}

} // namespace kas::net::socket::socket_address::tests

#endif