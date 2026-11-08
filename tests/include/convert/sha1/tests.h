/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_CONVERT_SHA1_TESTS_H
#define KASLIBCXX_CONVERT_SHA1_TESTS_H

#include "v1/tests.h"

namespace kas::convert::sha1::tests {
inline bool runTests()
{
    return v1::tests::runTests();
}

} // namespace kas::convert::sha1::tests

#endif