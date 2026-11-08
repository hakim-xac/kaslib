// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include <iostream>

#include "net/tests.h"

int main()
{
    return kas::net::tests::runTests() ? 0 : 1;
}
