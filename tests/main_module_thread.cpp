// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include <iostream>

#include "thread/tests.h"

int main()
{
    return kas::thread::tests::runTests() ? 0 : 1;
}
