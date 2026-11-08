/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_SERIAL_TESTS_H
#define KASLIBCXX_SERIAL_TESTS_H

#include "test_file_handler_scoped.h"
#include "test_serial.h"

namespace kas::serial::tests {

// ============================================================
//  Точка входа
// ============================================================
inline bool runTests()
{
    bool ok { true };
    ok &= test_serial::tests::runTests();
    ok &= test_fhs::tests::runTests();
    return ok;
}

}
#endif