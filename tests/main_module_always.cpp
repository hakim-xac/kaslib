// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include <iostream>

#include "convert/tests.h"
#include "expected/tests.h"
#include "functional/tests.h"
#include "utils/tests.h"

int runTests()
{
    bool ok { true };

    ok &= kas::test_expected::runTests();
    ok &= kas::utils::tests::runTests();
    ok &= kas::functional::tests::runTests();
    return ok ? 0 : 1;
}

int main()
{
    return runTests();
}