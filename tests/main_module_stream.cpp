// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include <iostream>

#include "stream/tests.h"

int main()
{
    return kas::stream::tests::runTests() ? 0 : 1;
}
