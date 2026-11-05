/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_CONVERT_SERIALIZE_DESERIALIZER_TESTS_H
#define KASLIBCXX_CONVERT_SERIALIZE_DESERIALIZER_TESTS_H

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "KAS/convert/serialize/Deserializer/Deserializer.h"
#include "KAS/convert/serialize/Serializer/Serializer.h"
#include "tests/tests.h"

namespace kas::convert::serialize::deserializer::tests {
namespace {

    using namespace kas::tests;
    using kas::convert::serialize::Deserializer;
    using kas::convert::serialize::Serializer;
    namespace types = kas::convert::serialize::types;

    // ==================================================================
    //  1. Пустой буфер -> nullopt
    // ==================================================================
    inline void emptyBuffer(TestContext& ctx)
    {
        const std::vector<std::uint8_t> empty;
        KAS_TEST_CHECK_IMPL(ctx, !Deserializer::getDeserializer(empty).has_value());
    }

    // ==================================================================
    //  2. Битый буфер: неполный SizeType (1 байт)
    // ==================================================================
    inline void truncatedSizeType(TestContext& ctx)
    {
        const std::vector<std::uint8_t> bad { 0x00 };
        KAS_TEST_CHECK_IMPL(ctx, !Deserializer::getDeserializer(bad).has_value());
    }

    // ==================================================================
    //  3. Неизвестный код типа (0xFFFF)
    // ==================================================================
    inline void unknownCode(TestContext& ctx)
    {
        const std::vector<std::uint8_t> bad { 0xFF, 0xFF };
        KAS_TEST_CHECK_IMPL(ctx, !Deserializer::getDeserializer(bad).has_value());
    }

    // ==================================================================
    //  4. Round-trip: u8, u16, string
    // ==================================================================
    inline void roundTripBasic(TestContext& ctx)
    {
        Serializer<> s;
        s.addSerializeType<types::u8>(std::uint8_t { 0xAA })
            .addSerializeType<types::u16>(std::uint16_t { 0xBBCC })
            .addSerializeType<types::string>(std::string { "hi" });

        const auto ds = Deserializer::getDeserializer(s.getRaw());
        KAS_TEST_CHECK_IMPL(ctx, ds.has_value());
        if (!ds)
            return;

        KAS_TEST_CHECK_IMPL(ctx, ds->getCountElems() == 3u);

        const auto v0 = ds->getElements<std::uint8_t>(0);
        const auto v1 = ds->getElements<std::uint16_t>(1);
        const auto v2 = ds->getElements<std::string>(2);

        KAS_TEST_CHECK_IMPL(ctx, v0.has_value());
        KAS_TEST_CHECK_IMPL(ctx, v1.has_value());
        KAS_TEST_CHECK_IMPL(ctx, v2.has_value());

        if (v0)
            KAS_TEST_CHECK_IMPL(ctx, *v0 == 0xAA);
        if (v1)
            KAS_TEST_CHECK_IMPL(ctx, *v1 == 0xBBCC);
        if (v2)
            KAS_TEST_CHECK_IMPL(ctx, *v2 == "hi");
    }

    // ==================================================================
    //  5. Round-trip: все скаляры
    // ==================================================================
    inline void roundTripAllScalars(TestContext& ctx)
    {
        Serializer<> s;
        s.addSerializeType<types::u8>(std::uint8_t { 0x01 })
            .addSerializeType<types::i8>(std::int8_t { -1 })
            .addSerializeType<types::u16>(std::uint16_t { 0x1234 })
            .addSerializeType<types::i16>(std::int16_t { -2 })
            .addSerializeType<types::u32>(std::uint32_t { 0xDEADBEEF })
            .addSerializeType<types::i32>(std::int32_t { -3 })
            .addSerializeType<types::u64>(std::uint64_t { 0x0123456789ABCDEFull })
            .addSerializeType<types::i64>(std::int64_t { -4 });

        const auto ds = Deserializer::getDeserializer(s.getRaw());
        KAS_TEST_CHECK_IMPL(ctx, ds.has_value());
        if (!ds)
            return;

        KAS_TEST_CHECK_IMPL(ctx, ds->getCountElems() == 8u);

        const auto u8v = ds->getElements<std::uint8_t>(0);
        const auto i8v = ds->getElements<std::int8_t>(1);
        const auto u16v = ds->getElements<std::uint16_t>(2);
        const auto i16v = ds->getElements<std::int16_t>(3);
        const auto u32v = ds->getElements<std::uint32_t>(4);
        const auto i32v = ds->getElements<std::int32_t>(5);
        const auto u64v = ds->getElements<std::uint64_t>(6);
        const auto i64v = ds->getElements<std::int64_t>(7);

        KAS_TEST_CHECK_IMPL(ctx, u8v.has_value());
        KAS_TEST_CHECK_IMPL(ctx, i8v.has_value());
        KAS_TEST_CHECK_IMPL(ctx, u16v.has_value());
        KAS_TEST_CHECK_IMPL(ctx, i16v.has_value());
        KAS_TEST_CHECK_IMPL(ctx, u32v.has_value());
        KAS_TEST_CHECK_IMPL(ctx, i32v.has_value());
        KAS_TEST_CHECK_IMPL(ctx, u64v.has_value());
        KAS_TEST_CHECK_IMPL(ctx, i64v.has_value());

        if (u8v)
            KAS_TEST_CHECK_IMPL(ctx, *u8v == 0x01);
        if (i8v)
            KAS_TEST_CHECK_IMPL(ctx, *i8v == -1);
        if (u16v)
            KAS_TEST_CHECK_IMPL(ctx, *u16v == 0x1234);
        if (i16v)
            KAS_TEST_CHECK_IMPL(ctx, *i16v == -2);
        if (u32v)
            KAS_TEST_CHECK_IMPL(ctx, *u32v == 0xDEADBEEF);
        if (i32v)
            KAS_TEST_CHECK_IMPL(ctx, *i32v == -3);
        if (u64v)
            KAS_TEST_CHECK_IMPL(ctx, *u64v == 0x0123456789ABCDEFull);
        if (i64v)
            KAS_TEST_CHECK_IMPL(ctx, *i64v == -4);
    }

    // ==================================================================
    //  6. Round-trip: signed-граничные значения
    // ==================================================================
    inline void signedValuesRoundTrip(TestContext& ctx)
    {
        Serializer<> s;
        s.addSerializeType<types::i8>(std::int8_t { -128 })
            .addSerializeType<types::i8>(std::int8_t { 127 })
            .addSerializeType<types::i16>(std::int16_t { -32768 })
            .addSerializeType<types::i16>(std::int16_t { 32767 })
            .addSerializeType<types::i32>(std::int32_t { -2147483647 - 1 })
            .addSerializeType<types::i32>(std::int32_t { 2147483647 });

        const auto ds = Deserializer::getDeserializer(s.getRaw());
        KAS_TEST_CHECK_IMPL(ctx, ds.has_value());
        if (!ds)
            return;

        KAS_TEST_CHECK_IMPL(ctx, ds->getCountElems() == 6u);

        const auto v0 = ds->getElements<std::int8_t>(0);
        const auto v1 = ds->getElements<std::int8_t>(1);
        const auto v2 = ds->getElements<std::int16_t>(2);
        const auto v3 = ds->getElements<std::int16_t>(3);
        const auto v4 = ds->getElements<std::int32_t>(4);
        const auto v5 = ds->getElements<std::int32_t>(5);

        if (v0)
            KAS_TEST_CHECK_IMPL(ctx, *v0 == -128);
        if (v1)
            KAS_TEST_CHECK_IMPL(ctx, *v1 == 127);
        if (v2)
            KAS_TEST_CHECK_IMPL(ctx, *v2 == -32768);
        if (v3)
            KAS_TEST_CHECK_IMPL(ctx, *v3 == 32767);
        if (v4)
            KAS_TEST_CHECK_IMPL(ctx, *v4 == (-2147483647 - 1));
        if (v5)
            KAS_TEST_CHECK_IMPL(ctx, *v5 == 2147483647);
    }

    // ==================================================================
    //  7. Round-trip: raw_data (vector<uint8_t>)
    // ==================================================================
    inline void roundTripRawData(TestContext& ctx)
    {
        Serializer<> s;
        s.addSerializeType<types::raw_data>(
            std::vector<std::uint8_t> { 0xDE, 0xAD, 0xBE, 0xEF });

        const auto ds = Deserializer::getDeserializer(s.getRaw());
        KAS_TEST_CHECK_IMPL(ctx, ds.has_value());
        if (!ds)
            return;

        const auto v = ds->getElements<std::vector<std::uint8_t>>(0);
        KAS_TEST_CHECK_IMPL(ctx, v.has_value());
        if (v) {
            const std::vector<std::uint8_t> expected { 0xDE, 0xAD, 0xBE, 0xEF };
            KAS_TEST_CHECK_IMPL(ctx, *v == expected);
        }
    }

    // ==================================================================
    //  8. Round-trip: пустая строка
    // ==================================================================
    inline void emptyStringRoundTrip(TestContext& ctx)
    {
        Serializer<> s;
        s.addSerializeType<types::string>(std::string { });

        const auto ds = Deserializer::getDeserializer(s.getRaw());
        KAS_TEST_CHECK_IMPL(ctx, ds.has_value());
        if (!ds)
            return;

        const auto v = ds->getElements<std::string>(0);
        KAS_TEST_CHECK_IMPL(ctx, v.has_value());
        if (v)
            KAS_TEST_CHECK_IMPL(ctx, v->empty());
    }

    // ==================================================================
    //  9. Round-trip: пустой vector<uint8_t>
    // ==================================================================
    inline void emptyRawDataRoundTrip(TestContext& ctx)
    {
        Serializer<> s;
        s.addSerializeType<types::raw_data>(std::vector<std::uint8_t> { });

        const auto ds = Deserializer::getDeserializer(s.getRaw());
        KAS_TEST_CHECK_IMPL(ctx, ds.has_value());
        if (!ds)
            return;

        const auto v = ds->getElements<std::vector<std::uint8_t>>(0);
        KAS_TEST_CHECK_IMPL(ctx, v.has_value());
        if (v)
            KAS_TEST_CHECK_IMPL(ctx, v->empty());
    }

    // ==================================================================
    //  10. getElements: индекс за границей -> nullopt
    // ==================================================================
    inline void getElementsOutOfRange(TestContext& ctx)
    {
        Serializer<> s;
        s.addSerializeType<types::u8>(std::uint8_t { 0x01 });
        const auto ds = Deserializer::getDeserializer(s.getRaw());
        KAS_TEST_CHECK_IMPL(ctx, ds.has_value());
        if (!ds)
            return;

        KAS_TEST_CHECK_IMPL(ctx, !ds->getElements<std::uint8_t>(100).has_value());
        KAS_TEST_CHECK_IMPL(ctx, !ds->getElements<std::uint8_t>(1).has_value());
    }

    // ==================================================================
    //  11. getElements: неверный тип -> nullopt
    // ==================================================================
    inline void getElementsWrongType(TestContext& ctx)
    {
        Serializer<> s;
        s.addSerializeType<types::u8>(std::uint8_t { 0x01 });
        const auto ds = Deserializer::getDeserializer(s.getRaw());
        KAS_TEST_CHECK_IMPL(ctx, ds.has_value());
        if (!ds)
            return;

        KAS_TEST_CHECK_IMPL(ctx, !ds->getElements<std::uint16_t>(0).has_value());
        KAS_TEST_CHECK_IMPL(ctx, !ds->getElements<std::string>(0).has_value());
        KAS_TEST_CHECK_IMPL(ctx, !ds->getElements<std::vector<std::uint8_t>>(0).has_value());
    }

    // ==================================================================
    //  12. Битый буфер: длина строки больше оставшихся данных
    // ==================================================================
    inline void stringLengthOverflow(TestContext& ctx)
    {
        // [code=9 (uint16 BE)][len=100 (uint64 BE)][только 2 байта данных]
        const std::vector<std::uint8_t> bad {
            0x00, 0x09,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x64,
            'h', 'i'
        };
        KAS_TEST_CHECK_IMPL(ctx, !Deserializer::getDeserializer(bad).has_value());
    }

    // ==================================================================
    //  13. Битый буфер: длина raw_data больше оставшихся данных
    // ==================================================================
    inline void rawDataLengthOverflow(TestContext& ctx)
    {
        // [code=10 (uint16 BE)][len=1000 (uint64 BE)][только 3 байта данных]
        const std::vector<std::uint8_t> bad {
            0x00, 0x0A,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xE8,
            0xAA, 0xBB, 0xCC
        };
        KAS_TEST_CHECK_IMPL(ctx, !Deserializer::getDeserializer(bad).has_value());
    }

    // ==================================================================
    //  14. Битый буфер: обрезанное значение скаляра
    // ==================================================================
    inline void truncatedScalarValue(TestContext& ctx)
    {
        // [code=u32 (uint16 BE)][только 2 байта значения вместо 4]
        const std::vector<std::uint8_t> bad {
            0x00, 0x05,
            0x12, 0x34
        };
        KAS_TEST_CHECK_IMPL(ctx, !Deserializer::getDeserializer(bad).has_value());
    }

    // ==================================================================
    //  15. Детерминизм: повторный парсинг того же буфера
    // ==================================================================
    inline void deterministic(TestContext& ctx)
    {
        Serializer<> s;
        s.addSerializeType<types::u16>(std::uint16_t { 0x1234 })
            .addSerializeType<types::string>(std::string { "payload" });

        const auto raw = s.getRaw();

        const auto first = Deserializer::getDeserializer(raw);
        KAS_TEST_CHECK_IMPL(ctx, first.has_value());
        if (!first)
            return;

        for (int i = 0; i < 10; ++i) {
            const auto again = Deserializer::getDeserializer(raw);
            KAS_TEST_CHECK_IMPL(ctx, again.has_value());
            if (again)
                KAS_TEST_CHECK_IMPL(ctx, again->getCountElems() == first->getCountElems());
        }
    }

    // ==================================================================
    //  16. getCountElems корректен после парсинга
    // ==================================================================
    inline void countElems(TestContext& ctx)
    {
        Serializer<> s;
        s.addSerializeType<types::u8>(std::uint8_t { 0 })
            .addSerializeType<types::u16>(std::uint16_t { 0 })
            .addSerializeType<types::u32>(std::uint32_t { 0 })
            .addSerializeType<types::u64>(std::uint64_t { 0 })
            .addSerializeType<types::string>(std::string { "x" })
            .addSerializeType<types::raw_data>(std::vector<std::uint8_t> { 0x01 });

        const auto ds = Deserializer::getDeserializer(s.getRaw());
        KAS_TEST_CHECK_IMPL(ctx, ds.has_value());
        if (ds)
            KAS_TEST_CHECK_IMPL(ctx, ds->getCountElems() == 6u);
    }

} // namespace

// ============================================================
//  Точка входа
// ============================================================

inline bool runTests()
{
    TestContext ctx;

    emptyBuffer(ctx);
    truncatedSizeType(ctx);
    unknownCode(ctx);

    roundTripBasic(ctx);
    roundTripAllScalars(ctx);
    signedValuesRoundTrip(ctx);
    roundTripRawData(ctx);
    emptyStringRoundTrip(ctx);
    emptyRawDataRoundTrip(ctx);

    getElementsOutOfRange(ctx);
    getElementsWrongType(ctx);

    stringLengthOverflow(ctx);
    rawDataLengthOverflow(ctx);
    truncatedScalarValue(ctx);

    deterministic(ctx);
    countElems(ctx);

    return printSummary("kas::convert::serialize::deserializer::tests", ctx);
}

} // namespace kas::convert::serialize::deserializer::tests

#endif