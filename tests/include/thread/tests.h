/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_THREAD_TESTS_H
#define KASLIBCXX_THREAD_TESTS_H

#include "test_async_thread.h"
#include "test_jthread.h"
#include "test_thread.h"
#include "test_thread_pool.h"

namespace kas::thread::tests {
inline bool runTests()
{
    bool ok { true };
    ok &= test_thread::tests::runTests();
    ok &= test_jthread::tests::runTests();
    ok &= test_thread_pool::tests::runTests();
    ok &= test_async_thread::tests::runTests();
    return ok;
}

} // namespace kas::thread::tests

#endif