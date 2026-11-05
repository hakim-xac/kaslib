/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_UTILS_TESTS_H
#define KASLIBCXX_UTILS_TESTS_H

#include "v5/tests.h"

namespace kas::utils::tests {
inline bool runTests()
{
    return kas::utils::v5::tests::runTests();
}

} // namespace kas::utils::tests

#endif