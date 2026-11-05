/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_CONVERT_CRC_TESTS_H
#define KASLIBCXX_CONVERT_CRC_TESTS_H

#include "ccitt_false/tests.h"

namespace kas::convert::crc::tests {
inline bool runTests()
{
    return ccitt_false::tests::runTests();
}

} // namespace kas::convert::crc::tests

#endif