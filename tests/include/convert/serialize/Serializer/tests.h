/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_CONVERT_SERIALIZE_SERIALIZER_TESTS_H
#define KASLIBCXX_CONVERT_SERIALIZE_SERIALIZER_TESTS_H

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "KAS/convert/serialize/RawPacket/RawPacket.h"
#include "KAS/convert/serialize/Serializer/Serializer.h"
#include "tests/tests.h"

namespace kas::convert::serialize::serializer::tests {

namespace {
    using namespace kas::tests;
    using kas::convert::serialize::Serializer;

    // ==================================================================
    //  Тестовые InfoType - по одному на каждый value_type
    // ==================================================================

    struct InfoU8 {
        using value_type = std::uint8_t;
        using code_type = std::uint8_t;
        static constexpr code_type code = 0x01;
    };

    struct InfoU16 {
        using value_type = std::uint16_t;
        using code_type = std::uint8_t;
        static constexpr code_type code = 0x02;
    };

    struct InfoU32 {
        using value_type = std::uint32_t;
        using code_type = std::uint8_t;
        static constexpr code_type code = 0x03;
    };

    struct InfoU64 {
        using value_type = std::uint64_t;
        using code_type = std::uint8_t;
        static constexpr code_type code = 0x04;
    };

    struct InfoString {
        using value_type = std::string;
        using code_type = std::uint8_t;
        static constexpr code_type code = 0x10;
    };

    struct InfoVec {
        using value_type = std::vector<std::uint8_t>;
        using code_type = std::uint8_t;
        static constexpr code_type code = 0x11;
    };

    // ==================================================================
    //  SerializeType-обёртка (удовлетворяет as_serialize_type)
    // ==================================================================

    template <typename Info>
    struct Wrap {
        using info_type = Info;
        using value_type = typename Info::value_type; // требование концепта
        value_type value;
    };

    // ==================================================================
    //  1. Пустой Serializer
    // ==================================================================
    inline void emptySerializer(TestContext& ctx)
    {
        Serializer<> s;
        KAS_TEST_CHECK_IMPL(ctx, s.getCountElems() == 0u);
        KAS_TEST_CHECK_IMPL(ctx, s.getRaw().empty());
    }

    // ==================================================================
    //  2. addSerializeType<InfoU8> - [code][value]
    // ==================================================================
    inline void addU8(TestContext& ctx)
    {
        Serializer<> s;
        s.addSerializeType<InfoU8>(std::uint8_t { 0xAB });

        KAS_TEST_CHECK_IMPL(ctx, s.getCountElems() == 1u);
        const std::vector<std::uint8_t> expected { 0x01, 0xAB };
        KAS_TEST_CHECK_IMPL(ctx, s.getRaw() == expected);
    }

    // ==================================================================
    //  3. addSerializeType<InfoU16> - BE
    // ==================================================================
    inline void addU16(TestContext& ctx)
    {
        Serializer<> s;
        s.addSerializeType<InfoU16>(std::uint16_t { 0x1234 });

        const std::vector<std::uint8_t> expected { 0x02, 0x12, 0x34 };
        KAS_TEST_CHECK_IMPL(ctx, s.getRaw() == expected);
    }

    // ==================================================================
    //  4. addSerializeType<InfoU32> - BE
    // ==================================================================
    inline void addU32(TestContext& ctx)
    {
        Serializer<> s;
        s.addSerializeType<InfoU32>(std::uint32_t { 0x12345678 });

        const std::vector<std::uint8_t> expected {
            0x03, 0x12, 0x34, 0x56, 0x78
        };
        KAS_TEST_CHECK_IMPL(ctx, s.getRaw() == expected);
    }

    // ==================================================================
    //  5. addSerializeType<InfoU64> - BE
    // ==================================================================
    inline void addU64(TestContext& ctx)
    {
        Serializer<> s;
        s.addSerializeType<InfoU64>(std::uint64_t { 0x0123456789ABCDEFull });

        const std::vector<std::uint8_t> expected {
            0x04, 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF
        };
        KAS_TEST_CHECK_IMPL(ctx, s.getRaw() == expected);
    }

    // ==================================================================
    //  6. Цепочка на lvalue
    // ==================================================================
    inline void chainLvalue(TestContext& ctx)
    {
        Serializer<> s;
        s.addSerializeType<InfoU8>(std::uint8_t { 0xAA })
            .addSerializeType<InfoU16>(std::uint16_t { 0xBBCC })
            .addSerializeType<InfoU32>(std::uint32_t { 0xDDEEFF00 });

        KAS_TEST_CHECK_IMPL(ctx, s.getCountElems() == 3u);

        const std::vector<std::uint8_t> expected {
            0x01, 0xAA,
            0x02, 0xBB, 0xCC,
            0x03, 0xDD, 0xEE, 0xFF, 0x00
        };
        KAS_TEST_CHECK_IMPL(ctx, s.getRaw() == expected);
    }

    // ==================================================================
    //  7. Цепочка на rvalue (временный Serializer)
    // ==================================================================
    inline void chainRvalue(TestContext& ctx)
    {
        const std::vector<std::uint8_t> raw {
            Serializer<> { }
                .addSerializeType<InfoU8>(std::uint8_t { 0x01 })
                .addSerializeType<InfoU16>(std::uint16_t { 0x0203 })
                .getRaw()
        };

        const std::vector<std::uint8_t> expected {
            0x01, 0x01,
            0x02, 0x02, 0x03
        };
        KAS_TEST_CHECK_IMPL(ctx, raw == expected);
    }

    // ==================================================================
    //  8. getCountElems растёт на каждом addSerializeType
    // ==================================================================
    inline void countElems(TestContext& ctx)
    {
        Serializer<> s;
        KAS_TEST_CHECK_IMPL(ctx, s.getCountElems() == 0u);
        s.addSerializeType<InfoU8>(std::uint8_t { 0 });
        KAS_TEST_CHECK_IMPL(ctx, s.getCountElems() == 1u);
        s.addSerializeType<InfoU8>(std::uint8_t { 0 });
        KAS_TEST_CHECK_IMPL(ctx, s.getCountElems() == 2u);
        s.addSerializeType<InfoU32>(std::uint32_t { 0 });
        KAS_TEST_CHECK_IMPL(ctx, s.getCountElems() == 3u);
    }

    // ==================================================================
    //  9. addSerializeType(SerializeType&&) - обёртка с info_type/value
    // ==================================================================
    inline void addSerializeTypeObject(TestContext& ctx)
    {
        Serializer<> s;
        s.addSerializeType(Wrap<InfoU16> { 0x1234 });

        const std::vector<std::uint8_t> expected { 0x02, 0x12, 0x34 };
        KAS_TEST_CHECK_IMPL(ctx, s.getRaw() == expected);
        KAS_TEST_CHECK_IMPL(ctx, s.getCountElems() == 1u);
    }

    // ==================================================================
    //  10. Строка: код + длина (uint64 BE) + данные
    // ==================================================================
    inline void addString(TestContext& ctx)
    {
        Serializer<> s;
        s.addSerializeType<InfoString>(std::string { "hi" });

        const std::vector<std::uint8_t> expected {
            0x10,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
            'h', 'i'
        };
        KAS_TEST_CHECK_IMPL(ctx, s.getRaw() == expected);
    }

    // ==================================================================
    //  11. Пустая строка - код + длина 0
    // ==================================================================
    inline void addEmptyString(TestContext& ctx)
    {
        Serializer<> s;
        s.addSerializeType<InfoString>(std::string { });

        const std::vector<std::uint8_t> expected {
            0x10,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };
        KAS_TEST_CHECK_IMPL(ctx, s.getRaw() == expected);
    }

    // ==================================================================
    //  12. std::string_view - тот же формат
    // ==================================================================
    inline void addStringView(TestContext& ctx)
    {
        Serializer<> s;
        s.addSerializeType<InfoString>(std::string_view { "hi" });

        const std::vector<std::uint8_t> expected {
            0x10,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
            'h', 'i'
        };
        KAS_TEST_CHECK_IMPL(ctx, s.getRaw() == expected);
    }

    // ==================================================================
    //  13. vector<uint8_t>: код + длина (uint64 BE) + данные
    // ==================================================================
    inline void addVector(TestContext& ctx)
    {
        Serializer<> s;
        s.addSerializeType<InfoVec>(std::vector<std::uint8_t> { 0xDE, 0xAD });

        const std::vector<std::uint8_t> expected {
            0x11,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
            0xDE, 0xAD
        };
        KAS_TEST_CHECK_IMPL(ctx, s.getRaw() == expected);
    }

    // ==================================================================
    //  14. Пустой вектор - код + длина 0
    // ==================================================================
    inline void addEmptyVector(TestContext& ctx)
    {
        Serializer<> s;
        s.addSerializeType<InfoVec>(std::vector<std::uint8_t> { });

        const std::vector<std::uint8_t> expected {
            0x11,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };
        KAS_TEST_CHECK_IMPL(ctx, s.getRaw() == expected);
    }

    // ==================================================================
    //  15. getRaw() const& - возвращает ссылку на внутренний буфер
    // ==================================================================
    inline void getRawRef(TestContext& ctx)
    {
        Serializer<> s;
        s.addSerializeType<InfoU8>(std::uint8_t { 0x01 });
        const auto& ref = s.getRaw();
        KAS_TEST_CHECK_IMPL(ctx, &ref == &s.getRaw());
    }

    // ==================================================================
    //  16. getRaw() && - перемещает буфер
    // ==================================================================
    inline void getRawMove(TestContext& ctx)
    {
        Serializer<> s;
        s.addSerializeType<InfoU8>(std::uint8_t { 0xAA });
        std::vector<std::uint8_t> raw = std::move(s).getRaw();

        const std::vector<std::uint8_t> expected { 0x01, 0xAA };
        KAS_TEST_CHECK_IMPL(ctx, raw == expected);
    }

    // ==================================================================
    //  17. Смешанные типы
    // ==================================================================
    inline void mixedTypes(TestContext& ctx)
    {
        Serializer<> s;
        s.addSerializeType<InfoU8>(std::uint8_t { 0xFF })
            .addSerializeType<InfoU16>(std::uint16_t { 0x1234 })
            .addSerializeType<InfoString>(std::string { "ab" })
            .addSerializeType<InfoU32>(std::uint32_t { 0xDEADBEEF });

        const std::vector<std::uint8_t> expected {
            0x01, 0xFF,
            0x02, 0x12, 0x34,
            0x10,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
            'a', 'b',
            0x03, 0xDE, 0xAD, 0xBE, 0xEF
        };
        KAS_TEST_CHECK_IMPL(ctx, s.getRaw() == expected);
        KAS_TEST_CHECK_IMPL(ctx, s.getCountElems() == 4u);
    }

    // ==================================================================
    //  18. getRaw() не меняет getCountElems
    // ==================================================================
    inline void getRawDoesNotChangeCount(TestContext& ctx)
    {
        Serializer<> s;
        s.addSerializeType<InfoU8>(std::uint8_t { 0xAA });
        (void)s.getRaw();
        (void)s.getRaw();
        KAS_TEST_CHECK_IMPL(ctx, s.getCountElems() == 1u);
    }

    // ==================================================================
    //  19. Формат читается обратно по байтам
    // ==================================================================
    inline void rawIsParseable(TestContext& ctx)
    {
        Serializer<> s;
        s.addSerializeType<InfoU8>(std::uint8_t { 0xAA })
            .addSerializeType<InfoU16>(std::uint16_t { 0xBBCC });

        const auto& raw = s.getRaw();
        KAS_TEST_CHECK_IMPL(ctx, raw.size() == 5u);

        KAS_TEST_CHECK_IMPL(ctx, raw[0] == 0x01);
        KAS_TEST_CHECK_IMPL(ctx, raw[1] == 0xAA);
        KAS_TEST_CHECK_IMPL(ctx, raw[2] == 0x02);
        KAS_TEST_CHECK_IMPL(ctx, raw[3] == 0xBB);
        KAS_TEST_CHECK_IMPL(ctx, raw[4] == 0xCC);
    }

    // ==================================================================
    //  20. Детерминизм
    // ==================================================================
    inline void deterministic(TestContext& ctx)
    {
        Serializer<> s;
        s.addSerializeType<InfoU32>(std::uint32_t { 0x12345678 })
            .addSerializeType<InfoString>(std::string { "payload" });

        const auto first = s.getRaw();
        for (int i = 0; i < 10; ++i)
            KAS_TEST_CHECK_IMPL(ctx, s.getRaw() == first);
    }

    // ==================================================================
    //  21. Конструктор с variadic-параметрами (SerializeType)
    // ==================================================================
    inline void constructWithProps(TestContext& ctx)
    {
        Serializer<> s {
            Wrap<InfoU8> { 0xAA },
            Wrap<InfoU16> { 0xBBCC }
        };

        const std::vector<std::uint8_t> expected {
            0x01, 0xAA,
            0x02, 0xBB, 0xCC
        };
        KAS_TEST_CHECK_IMPL(ctx, s.getRaw() == expected);
        KAS_TEST_CHECK_IMPL(ctx, s.getCountElems() == 2u);
    }

} // namespace

// ============================================================
//  Точка входа
// ============================================================

inline bool runTests()
{
    TestContext ctx;

    emptySerializer(ctx);
    addU8(ctx);
    addU16(ctx);
    addU32(ctx);
    addU64(ctx);
    chainLvalue(ctx);
    chainRvalue(ctx);
    countElems(ctx);
    addSerializeTypeObject(ctx);
    addString(ctx);
    addEmptyString(ctx);
    addStringView(ctx);
    addVector(ctx);
    addEmptyVector(ctx);
    getRawRef(ctx);
    getRawMove(ctx);
    mixedTypes(ctx);
    getRawDoesNotChangeCount(ctx);
    rawIsParseable(ctx);
    deterministic(ctx);
    constructWithProps(ctx);

    return printSummary("kas::convert::serialize::serializer::tests", ctx);
}

} // namespace kas::convert::serialize::serializer::tests

#endif