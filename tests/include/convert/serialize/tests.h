/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_CONVERT_SERIALIZE_TESTS_H
#define KASLIBCXX_CONVERT_SERIALIZE_TESTS_H

#include "Deserializer/tests.h"
#include "RawPacket/tests.h"
#include "Serializer/tests.h"

namespace kas::convert::serialize::tests {
inline bool runTests()
{
    bool ok { true };
    ok &= rwp::tests::runTests();
    ok &= serializer::tests::runTests();
    ok &= deserializer::tests::runTests();
    return ok;
}

} // namespace kas::convert::serialize::tests

#endif