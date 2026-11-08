/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_STREAM_TESTS_H
#define KASLIBCXX_STREAM_TESTS_H

#include "args/tests.h"
#include "cots/tests.h"
#include "logger/tests.h"

namespace kas::stream::tests {
inline bool runTests()
{
    bool ok { true };

    ok &= cots::tests::runTests();
    ok &= logger::tests::runTests();
    ok &= args::tests::runTests();
    return ok;
}

} // namespace kas::stream::tests

#endif