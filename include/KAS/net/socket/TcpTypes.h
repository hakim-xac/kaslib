/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_NET_TCP_TYPES_H
#define KASLIBCXX_NET_TCP_TYPES_H

namespace kas::net::socket {

/**
 * @brief High-level transport selector used by socket wrappers.
 *
 * Unlike the raw AF_/SOCK_/IPPROTO_* parameters, this enum describes the
 * intended transport in a single tag. It is used by factories such as
 * @c SocketServer::make to pick a sensible (domain, type, protocol) triple
 * and, for @ref UNIX, to select AF_UNIX instead of an IP-based family.
 */
enum class TcpTypes {
    TCP, ///< Stream transport over IPv4/IPv6 (SOCK_STREAM).
    UDP, ///< Datagram transport over IPv4/IPv6 (SOCK_DGRAM).
    ICMP, ///< Raw ICMP socket (SOCK_RAW with IPPROTO_ICMP).
    UNIX ///< Unix domain stream socket (AF_UNIX).
};

} // namespace kas::net::socket

#endif