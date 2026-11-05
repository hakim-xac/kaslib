// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include <iostream>

#include "stream/tests.h"

int runTests()
{
#if ENABLE_STREAM
    return kas::stream::tests::runTests() ? 0 : 1;
#else
    std::cout << "ENABLE_STREAM: OFF\n";
    return 0;
#endif
}

int main()
{
    return runTests();
}
