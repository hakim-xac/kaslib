// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include <iostream>

#include "convert/tests.h"

int main()
{
    return kas::convert::tests::runTests() ? 0 : 1;
}
