// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include <iostream>

#include "thread/tests.h"

int runTests()
{
#if ENABLE_THREAD
    return kas::thread::tests::runTests() ? 0 : 1;
#else
    std::cout << "ENABLE_THREAD: OFF\n";
    return 0;
#endif
}

int main()
{
    return runTests();
}
