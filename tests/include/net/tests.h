/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_NET_TESTS_H
#define KASLIBCXX_NET_TESTS_H

#include "http/tests.h"
#include "socket/tests.h"
#include "test_general.h"

namespace kas::net::tests {
inline bool runTests()
{
    bool ok { true };
    ok &= socket::tests::runTests();
    ok &= http::tests::runTests();
    ok &= test_general::tests::runTests();
    return ok;
}

} // namespace kas::stream::tests

#endif