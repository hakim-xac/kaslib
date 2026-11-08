// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include <iostream>

#include "json/test.h"

int main()
{
    return kas::json::tests::runTests() ? 0 : 1;
}