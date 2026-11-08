/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_CONVERT_BASE64_TESTS_H
#define KASLIBCXX_CONVERT_BASE64_TESTS_H

#include "v1/tests.h"

namespace kas::convert::base64::tests {
inline bool runTests()
{
    return v1::tests::runTests();
}

} // namespace kas::convert::base64::tests

#endif