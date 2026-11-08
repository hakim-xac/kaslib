/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_STREAM_LOGGER_TESTS_H
#define KASLIBCXX_STREAM_LOGGER_TESTS_H

#include "v1/tests.h"

namespace kas::stream::logger::tests {
inline bool runTests()
{
    return v1::tests::runTests();
}

} // namespace kas::stream::logger

#endif