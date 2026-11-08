/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_FUNCTIONAL_TESTS_H
#define KASLIBCXX_FUNCTIONAL_TESTS_H

#include "test_func_guard.h"
#include "test_log_time.h"
#include "test_move_only_function.h"
#include "test_safe_data.h"
#include "test_type_guard.h"

namespace kas::functional::tests {

// ============================================================
//  Точка входа
// ============================================================
inline bool runTests()
{
    bool ok { true };
    ok &= mof::tests::runTests();
    ok &= safe_data::tests::runTests();
    ok &= log_time::tests::runTests();
    ok &= func_guard::tests::runTests();
    ok &= type_guard::tests::runTests();
    return ok;
}

}
#endif