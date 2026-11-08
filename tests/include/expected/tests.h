/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_EXPECTED_TESTS_H
#define KASLIBCXX_EXPECTED_TESTS_H

#include "v3/tests.h"
#include <iostream>

namespace kas::test_expected {
inline bool runTests()
{
    return v3::runTests();
}
}

#endif