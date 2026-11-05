/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_STREAM_LOGGER_V1_TESTS_H
#define KASLIBCXX_STREAM_LOGGER_V1_TESTS_H

#include "tests_file_logger.h"
#include "tests_logger.h"
#include "tests_logger_sync.h"

namespace kas::stream::logger::v1::tests {

inline bool runTests()
{
    bool ok { true };
    ok &= stream_logger::runTests();
    ok &= stream_logger_sync::runTests();
    ok &= stream_file_logger::runTests();
    return ok;
}

} // namespace kas::stream::logger::v1::tests

#endif