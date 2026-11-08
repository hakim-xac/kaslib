/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_CONVERT_TESTS_H
#define KASLIBCXX_CONVERT_TESTS_H

#include "base64/tests.h"
#include "crc/tests.h"
#include "jwt/tests.h"
#include "md5/tests.h"
#include "serialize/tests.h"
#include "sha1/tests.h"
#include "sha256/tests.h"

namespace kas::convert::tests {
inline bool runTests()
{
    bool ok { true };
    ok &= md5::tests::runTests();
    ok &= base64::tests::runTests();
    ok &= sha1::tests::runTests();
    ok &= sha256::tests::runTests();
    ok &= jwt::tests::runTests();
    ok &= crc::tests::runTests();
    ok &= serialize::tests::runTests();
    return ok;
}
}
#endif