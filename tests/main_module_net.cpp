// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include <iostream>

#include "net/tests.h"

int runTests()
{
#if ENABLE_NET
    return kas::net::tests::runTests() ? 0 : 1;
#else
    std::cout << "ENABLE_NET: OFF\n";
    return 0;
#endif
}

int main()
{
    return runTests();
}
