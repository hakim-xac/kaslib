/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_CONVERT_SHA256_TESTS_H
#define KASLIBCXX_CONVERT_SHA256_TESTS_H

#include "v2/tests.h"

namespace kas::convert::sha256::tests {
inline bool runTests()
{
    return v2::tests::runTests();
}

} // namespace kas::convert::sha256::tests

#endif