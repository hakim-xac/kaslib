/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_NET_HTTP_TESTS_H
#define KASLIBCXX_NET_HTTP_TESTS_H

#include "test_http_range.h"
#include "test_http_response.h"
#include "test_http_route.h"
#include "test_http_server.h"
#include "test_request.h"
#include "websocket/tests.h"

namespace kas::net::http::tests {
inline bool runTests()
{
    bool ok { true };
    ok &= http_response::tests::runTests();
    ok &= test_request::tests::runTests();
    ok &= http_route::tests::runTests();
    ok &= test_server::tests::runTests();
    ok &= http_range::tests::runTests();
    ok &= websocket::tests::runTests();
    return ok;
}

} // namespace kas::net::http::tests

#endif