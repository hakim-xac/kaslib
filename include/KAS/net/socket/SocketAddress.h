/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_NET_SOCKET_SOCKET_ADDRESS_H
#define KASLIBCXX_NET_SOCKET_SOCKET_ADDRESS_H

#include "SocketScoped.h"

#include <arpa/inet.h>
#include <cstdint>
#include <linux/netlink.h>
#include <netinet/in.h>
#include <netpacket/packet.h>
#include <optional>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/un.h>

#ifdef KASLIBCXX_NET_HAS_BLUETOOTH
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#endif

namespace kas::net::socket {

/**
 * @brief Supported socket address families.
 *
 * Mirrors the corresponding AF_* constants and is used by
 * @ref SocketAddress::make to dispatch to the right address parser.
 */
enum class SocketDomain : sa_family_t {
    IPv4 = AF_INET, ///< IPv4 (AF_INET).
    IPv6 = AF_INET6, ///< IPv6 (AF_INET6).
    Unix = AF_UNIX, ///< Unix domain sockets (AF_UNIX).
    Packet = AF_PACKET, ///< Packet sockets (AF_PACKET).
    Netlink = AF_NETLINK, ///< Netlink sockets (AF_NETLINK).
#ifdef KASLIBCXX_NET_HAS_BLUETOOTH
    Bluetooth = AF_BLUETOOTH, ///< Bluetooth (AF_BLUETOOTH).
#endif
};

/**
 * @brief A portable wrapper around sockaddr_storage.
 *
 * Stores a socket address of any supported family in a single
 * sockaddr_storage buffer together with the effective address length, and
 * provides typed accessors (asSockaddrIN, asSockaddrIN6, asSockaddrUN,
 * asSockaddrLL, asSockaddrNL, and optionally asSockaddrRC) as well as
 * length setters for each family.
 *
 * Instances are normally created via the static factory methods
 * (makeIPv4, makeIPv6, makeUnix, makePacket, makeNetlink, and optionally
 * makeBluetooth) or via the generic @ref make / @ref makeSocketAddress
 * dispatchers, all of which parse a textual form into the stored address.
 *
 * @note The typed accessors do not validate the stored family; calling the
 *       wrong accessor is a precondition violation.
 */
class SocketAddress {
public:
    /// Constructs an empty address (all zeros, zero length).
    SocketAddress() = default;

    // --- IPv4/IPv6/Unix ---

    /**
     * @brief Parses an IPv4 address of the form "ip:port".
     * @param ip_port Source string, for example "127.0.0.1:8080".
     * @return Parsed address, or std::nullopt on a parse error.
     */
    [[nodiscard]] static std::optional<SocketAddress> makeIPv4(std::string_view ip_port);

    /**
     * @brief Parses an IPv6 address of the form "[ip]:port" or "ip".
     * @param ip_port Source string.
     * @return Parsed address, or std::nullopt on a parse error.
     */
    [[nodiscard]] static std::optional<SocketAddress> makeIPv6(std::string_view ip_port);

    /**
     * @brief Parses a Unix domain socket path.
     * @param path Filesystem path of the socket.
     * @return Parsed address, or std::nullopt on a parse error.
     */
    [[nodiscard]] static std::optional<SocketAddress> makeUnix(std::string_view path);

    // --- raw / special ---

    /**
     * @brief Parses a packet socket address from an interface name.
     * @param ifname   Network interface name.
     * @param protocol Packet protocol number (0 by default).
     * @return Parsed address, or std::nullopt on a parse error.
     */
    [[nodiscard]] static std::optional<SocketAddress> makePacket(
        std::string_view ifname, std::uint16_t protocol = 0);

    /**
     * @brief Parses a netlink socket address from a PID string.
     * @param pid Netlink PID as text.
     * @return Parsed address, or std::nullopt on a parse error.
     */
    [[nodiscard]] static std::optional<SocketAddress> makeNetlink(std::string_view pid);

#ifdef KASLIBCXX_NET_HAS_BLUETOOTH
    /**
     * @brief Parses a Bluetooth RFCOMM address.
     *
     * Accepted formats: "AA:BB:CC:DD:EE:FF" or
     * "AA:BB:CC:DD:EE:FF:channel".
     *
     * @param addr Source string.
     * @return Parsed address, or std::nullopt on a parse error.
     */
    [[nodiscard]] static std::optional<SocketAddress> makeBluetooth(std::string_view addr);
#endif

    // --- dispatch ---

    /**
     * @brief Parses an address for the given @ref SocketDomain.
     * @param domain Address family.
     * @param source Textual address.
     * @return Parsed address, or std::nullopt on a parse error or an
     *         unsupported domain.
     */
    [[nodiscard]] static std::optional<SocketAddress> make(
        SocketDomain domain, std::string_view source);

    /**
     * @brief Parses an address for the given raw AF_* family value.
     *
     * Same as @ref make but accepts the underlying sa_family_t value, which
     * is convenient when the family comes from SocketScoped.
     *
     * @param domain Raw address family value.
     * @param source Textual address.
     * @return Parsed address, or std::nullopt on a parse error or an
     *         unsupported family.
     */
    [[nodiscard]] static std::optional<SocketAddress> makeSocketAddress(
        SocketScoped::domain_t domain, std::string_view source);

    // --- access ---

    /**
     * @brief Returns the stored address as a generic sockaddr pointer.
     * @return Pointer to the internal storage.
     */
    [[nodiscard]] const sockaddr* asSockaddr() const&;

    /// @brief Returns the stored address as sockaddr_in (IPv4).
    [[nodiscard]] const sockaddr_in* asSockaddrIN() const&;

    /// @brief Returns the stored address as sockaddr_in6 (IPv6).
    [[nodiscard]] const sockaddr_in6* asSockaddrIN6() const&;

    /// @brief Returns the stored address as sockaddr_un (Unix).
    [[nodiscard]] const sockaddr_un* asSockaddrUN() const&;

    /// @brief Returns the stored address as sockaddr_ll (packet).
    [[nodiscard]] const sockaddr_ll* asSockaddrLL() const&;

    /// @brief Returns the stored address as sockaddr_nl (netlink).
    [[nodiscard]] const sockaddr_nl* asSockaddrNL() const&;
#ifdef KASLIBCXX_NET_HAS_BLUETOOTH
    /// @brief Returns the stored address as sockaddr_rc (Bluetooth RFCOMM).
    [[nodiscard]] const sockaddr_rc* asSockaddrRC() const&;
#endif

    /**
     * @brief Returns the effective address length.
     * @return Address length in bytes.
     */
    [[nodiscard]] socklen_t getAddrLen() const&;

    /// @brief Sets the address length to sizeof(sockaddr_in).
    SocketAddress& setSockaddrLenIN() &;
    /// @brief Sets the address length to sizeof(sockaddr_in).
    SocketAddress&& setSockaddrLenIN() &&;
    /// @brief Sets the address length to sizeof(sockaddr_in6).
    SocketAddress& setSockaddrLenIN6() &;
    /// @brief Sets the address length to sizeof(sockaddr_in6).
    SocketAddress&& setSockaddrLenIN6() &&;
    /// @brief Sets the address length to sizeof(sockaddr_un).
    SocketAddress& setSockaddrLenUN() &;
    /// @brief Sets the address length to sizeof(sockaddr_un).
    SocketAddress&& setSockaddrLenUN() &&;
    /// @brief Sets the address length to sizeof(sockaddr_ll).
    SocketAddress& setSockaddrLenLL() &;
    /// @brief Sets the address length to sizeof(sockaddr_ll).
    SocketAddress&& setSockaddrLenLL() &&;
    /// @brief Sets the address length to sizeof(sockaddr_nl).
    SocketAddress& setSockaddrLenNL() &;
    /// @brief Sets the address length to sizeof(sockaddr_nl).
    SocketAddress&& setSockaddrLenNL() &&;
#ifdef KASLIBCXX_NET_HAS_BLUETOOTH
    /// @brief Sets the address length to sizeof(sockaddr_rc).
    SocketAddress& setSockaddrLenRC() &;
    /// @brief Sets the address length to sizeof(sockaddr_rc).
    SocketAddress&& setSockaddrLenRC() &&;
#endif
    /// @brief Sets an explicit address length.
    SocketAddress& setSockaddrLen(socklen_t length) &;
    /// @brief Sets an explicit address length.
    SocketAddress&& setSockaddrLen(socklen_t length) &&;

    /**
     * @brief Replaces the stored address with the given sockaddr_storage.
     * @param storage Source storage.
     * @return Reference to this address (lvalue overload).
     */
    SocketAddress& setSockaddrStorage(sockaddr_storage storage) &;

    /**
     * @brief Replaces the stored address with the given sockaddr_storage.
     * @param storage Source storage.
     * @return Moved reference to this address (rvalue overload).
     */
    SocketAddress&& setSockaddrStorage(sockaddr_storage storage) &&;

    /**
     * @brief Returns a human-readable textual form of the address.
     * @return Address string.
     */
    [[nodiscard]] std::string toString() const;

private:
    sockaddr_storage m_storage { }; ///< Raw address storage.
    socklen_t m_addr_len { }; ///< Effective address length.
};
} // namespace kas::net::socket

#endif