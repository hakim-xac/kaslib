// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include <iostream>

#include "serial/tests.h"

int main()
{
    return kas::serial::tests::runTests() ? 0 : 1;
}
