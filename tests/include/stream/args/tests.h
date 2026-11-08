/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_STREAM_ARGS_TESTS_H
#define KASLIBCXX_STREAM_ARGS_TESTS_H

#include "ArgsHandler/tests.h"
#include "InputArgs/tests.h"

namespace kas::stream::args::tests {
inline bool runTests()
{
    bool ok { true };
    ok &= input_args::tests::runTests();
    ok &= args_handler::tests::runTests();
    return ok;
}

} // namespace kas::stream::tests

#endif