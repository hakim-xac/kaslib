// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include <iostream>

#include "convert/tests.h"

int runTests()
{
#if ENABLE_CONVERTED
    return kas::convert::tests::runTests() ? 0 : 1;
#else
    std::cout << "ENABLE_CONVERTED: OFF\n";
    return 0;
#endif
}

int main()
{
    return runTests();
}
