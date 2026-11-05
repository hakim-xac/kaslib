/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_NET_SOCKET_TESTS_H
#define KASLIBCXX_NET_SOCKET_TESTS_H

#include "test_client_connection.h"
#include "test_socket_address.h"
#include "test_socket_client.h"
#include "test_socket_scoped.h"
#include "test_socket_server.h"

namespace kas::net::socket::tests {
inline bool runTests()
{
    bool ok { true };
    ok &= socket_scoped::tests::runTests();
    ok &= socket_address::tests::runTests();
    ok &= socket_client::tests::runTests();
    ok &= socket_server::tests::runTests();
    ok &= client_connection::tests::runTests();
    return ok;
}

} // namespace kas::stream::tests

#endif