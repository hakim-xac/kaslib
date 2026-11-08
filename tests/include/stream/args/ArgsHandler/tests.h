/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_STREAM_ARGS_HANDLER_TESTS_H
#define KASLIBCXX_STREAM_ARGS_HANDLER_TESTS_H

#include "v1/tests.h"

namespace kas::stream::args::args_handler::tests {
inline bool runTests()
{
    return args_handler::v1::tests::runTests();
}

} // namespace kas::stream::input_args::tests

#endif