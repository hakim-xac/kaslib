// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include <iostream>

#include "serial/tests.h"

int runTests()
{
#if ENABLE_SERIAL
    return kas::serial::tests::runTests() ? 0 : 1;
#else
    std::cout << "ENABLE_SERIAL: OFF\n";
    return 0;
#endif
}

int main()
{
    return runTests();
}
