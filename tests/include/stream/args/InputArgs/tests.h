/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_STREAM_INPUT_ARGS_TESTS_H
#define KASLIBCXX_STREAM_INPUT_ARGS_TESTS_H

#include "v1/tests.h"

namespace kas::stream::args::input_args::tests {
inline bool runTests()
{
    return input_args::v1::tests::runTests();
}

} // namespace kas::stream::input_args::tests

#endif