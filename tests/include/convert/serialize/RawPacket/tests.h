/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_CONVERT_SERIALIZE_RAW_PACKET_TESTS_H
#define KASLIBCXX_CONVERT_SERIALIZE_RAW_PACKET_TESTS_H

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "KAS/convert/serialize/RawPacket/RawPacket.h"
#include "tests/tests.h"

namespace kas::convert::serialize::rwp::tests {

namespace {
    using namespace kas::tests;
    using kas::convert::serialize::RawPacket;

    [[nodiscard]] std::vector<std::uint8_t> bytes(std::string_view s)
    {
        return std::vector<std::uint8_t>(s.begin(), s.end());
    }

    // ==================================================================
    //  Конструктор / reserve / resize
    // ==================================================================

    // --- 1. Пустой по умолчанию --------------------------------------
    inline void defaultConstruct(TestContext& ctx)
    {
        const RawPacket p;
        KAS_TEST_CHECK_IMPL(ctx, !p.hasData());
        KAS_TEST_CHECK_IMPL(ctx, p.size() == 0u);
        KAS_TEST_CHECK_IMPL(ctx, p.toRawBytes().empty());
    }

    // --- 2. reserve не меняет size -----------------------------------
    inline void reserveDoesNotChangeSize(TestContext& ctx)
    {
        RawPacket p;
        p.reserve(100);
        KAS_TEST_CHECK_IMPL(ctx, p.size() == 0u);
        KAS_TEST_CHECK_IMPL(ctx, !p.hasData());
    }

    // --- 3. resize(0) на пустом - no-op ------------------------------
    inline void resizeZero(TestContext& ctx)
    {
        RawPacket p;
        p.resize(0);
        KAS_TEST_CHECK_IMPL(ctx, p.size() == 0u);
    }

    // --- 4. resize увеличивает с нулями ------------------------------
    inline void resizeUpZeros(TestContext& ctx)
    {
        RawPacket p;
        p.resize(5);
        KAS_TEST_CHECK_IMPL(ctx, p.size() == 5u);
        KAS_TEST_CHECK_IMPL(ctx, p.toRawBytes() == std::vector<std::uint8_t>(5, 0));
    }

    // --- 5. resize уменьшает ----------------------------------------
    inline void resizeDown(TestContext& ctx)
    {
        RawPacket p;
        p.putArr(bytes("abcdefgh"));
        p.resize(3);
        KAS_TEST_CHECK_IMPL(ctx, p.size() == 3u);
        KAS_TEST_CHECK_IMPL(ctx, p.toRawBytes() == bytes("abc"));
    }

    // ==================================================================
    //  put8
    // ==================================================================

    // --- 6. put8 -----------------------------------------------------
    inline void put8One(TestContext& ctx)
    {
        RawPacket p;
        p.put8(0xAB);
        KAS_TEST_CHECK_IMPL(ctx, p.size() == 1u);
        KAS_TEST_CHECK_IMPL(ctx, p.data()[0] == 0xAB);
    }

    // ==================================================================
    //  put16_be / put16_le
    // ==================================================================

    // --- 7. put16_be - старший байт первым --------------------------
    inline void put16Be(TestContext& ctx)
    {
        RawPacket p;
        p.put16_be(0x1234);
        KAS_TEST_CHECK_IMPL(ctx, p.size() == 2u);
        KAS_TEST_CHECK_IMPL(ctx, p.data()[0] == 0x12);
        KAS_TEST_CHECK_IMPL(ctx, p.data()[1] == 0x34);
    }

    // --- 8. put16_le - младший байт первым -------------------------
    inline void put16Le(TestContext& ctx)
    {
        RawPacket p;
        p.put16_le(0x1234);
        KAS_TEST_CHECK_IMPL(ctx, p.size() == 2u);
        KAS_TEST_CHECK_IMPL(ctx, p.data()[0] == 0x34);
        KAS_TEST_CHECK_IMPL(ctx, p.data()[1] == 0x12);
    }

    // ==================================================================
    //  put32_be / put32_le
    // ==================================================================

    // --- 9. put32_be ------------------------------------------------
    inline void put32Be(TestContext& ctx)
    {
        RawPacket p;
        p.put32_be(0x12345678);
        KAS_TEST_CHECK_IMPL(ctx, p.size() == 4u);
        KAS_TEST_CHECK_IMPL(ctx, p.data()[0] == 0x12);
        KAS_TEST_CHECK_IMPL(ctx, p.data()[1] == 0x34);
        KAS_TEST_CHECK_IMPL(ctx, p.data()[2] == 0x56);
        KAS_TEST_CHECK_IMPL(ctx, p.data()[3] == 0x78);
    }

    // --- 10. put32_le -----------------------------------------------
    inline void put32Le(TestContext& ctx)
    {
        RawPacket p;
        p.put32_le(0x12345678);
        KAS_TEST_CHECK_IMPL(ctx, p.size() == 4u);
        KAS_TEST_CHECK_IMPL(ctx, p.data()[0] == 0x78);
        KAS_TEST_CHECK_IMPL(ctx, p.data()[1] == 0x56);
        KAS_TEST_CHECK_IMPL(ctx, p.data()[2] == 0x34);
        KAS_TEST_CHECK_IMPL(ctx, p.data()[3] == 0x12);
    }

    // ==================================================================
    //  put64_be / put64_le
    // ==================================================================

    // --- 11. put64_be -----------------------------------------------
    inline void put64Be(TestContext& ctx)
    {
        RawPacket p;
        p.put64_be(0x0123456789ABCDEFull);
        KAS_TEST_CHECK_IMPL(ctx, p.size() == 8u);
        const std::vector<std::uint8_t> expected {
            0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF
        };
        KAS_TEST_CHECK_IMPL(ctx, p.toRawBytes() == expected);
    }

    // --- 12. put64_le -----------------------------------------------
    inline void put64Le(TestContext& ctx)
    {
        RawPacket p;
        p.put64_le(0x0123456789ABCDEFull);
        KAS_TEST_CHECK_IMPL(ctx, p.size() == 8u);
        const std::vector<std::uint8_t> expected {
            0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01
        };
        KAS_TEST_CHECK_IMPL(ctx, p.toRawBytes() == expected);
    }

    // --- 13. BE и LE на одних данных различаются --------------------
    inline void beVsLeDistinct(TestContext& ctx)
    {
        RawPacket be;
        be.put32_be(0x11223344);
        RawPacket le;
        le.put32_le(0x11223344);

        KAS_TEST_CHECK_IMPL(ctx, be.toRawBytes() != le.toRawBytes());
        KAS_TEST_CHECK_IMPL(ctx, (be.toRawBytes() == std::vector<std::uint8_t> { 0x11, 0x22, 0x33, 0x44 }));
        KAS_TEST_CHECK_IMPL(ctx, (le.toRawBytes() == std::vector<std::uint8_t> { 0x44, 0x33, 0x22, 0x11 }));
    }

    // ==================================================================
    //  putArr / putVec / putRawPacket
    // ==================================================================

    // --- 14. putArr -------------------------------------------------
    inline void putArrBasic(TestContext& ctx)
    {
        RawPacket p;
        p.putArr(bytes("hello"));
        KAS_TEST_CHECK_IMPL(ctx, p.toRawBytes() == bytes("hello"));
    }

    // --- 15. putArr пустой - no-op ---------------------------------
    inline void putArrEmpty(TestContext& ctx)
    {
        RawPacket p;
        const std::vector<std::uint8_t> empty;
        p.putArr(empty);
        KAS_TEST_CHECK_IMPL(ctx, p.size() == 0u);
    }

    // --- 16. putVec - перемещает (в пустой буфер) ------------------
    inline void putVecMove(TestContext& ctx)
    {
        RawPacket p;
        std::vector<std::uint8_t> v = bytes("world");
        p.putVec(std::move(v));
        KAS_TEST_CHECK_IMPL(ctx, p.toRawBytes() == bytes("world"));
    }

    // --- 17. putVec в непустой буфер - дописывает -----------------
    inline void putVecAppend(TestContext& ctx)
    {
        RawPacket p;
        p.putArr(bytes("abc"));
        p.putVec(bytes("def"));
        KAS_TEST_CHECK_IMPL(ctx, p.toRawBytes() == bytes("abcdef"));
    }

    // --- 18. putRawPacket (const&) - копирует ----------------------
    inline void putRawPacketCopy(TestContext& ctx)
    {
        RawPacket src;
        src.putArr(bytes("abc"));
        RawPacket dst;
        dst.putRawPacket(src);
        KAS_TEST_CHECK_IMPL(ctx, dst.toRawBytes() == bytes("abc"));
        KAS_TEST_CHECK_IMPL(ctx, src.toRawBytes() == bytes("abc")); // src цел
    }

    // --- 19. putRawPacket (&&) - перемещает ------------------------
    inline void putRawPacketMove(TestContext& ctx)
    {
        RawPacket src;
        src.putArr(bytes("xyz"));
        RawPacket dst;
        dst.putRawPacket(std::move(src));
        KAS_TEST_CHECK_IMPL(ctx, dst.toRawBytes() == bytes("xyz"));
    }

    // --- 20. self-append копии - no-op ----------------------------
    inline void selfAppendCopy(TestContext& ctx)
    {
        RawPacket p;
        p.putArr(bytes("abc"));
        p.putRawPacket(p); // self, const&
        KAS_TEST_CHECK_IMPL(ctx, p.toRawBytes() == bytes("abc")); // не удвоилось
    }

    // --- 21. self-append перемещения - no-op ----------------------
    inline void selfAppendMove(TestContext& ctx)
    {
        RawPacket p;
        p.putArr(bytes("abc"));
        p.putRawPacket(std::move(p)); // self, &&
        KAS_TEST_CHECK_IMPL(ctx, p.toRawBytes() == bytes("abc"));
    }

    // ==================================================================
    //  update8
    // ==================================================================

    // --- 22. update8 в границах -----------------------------------
    inline void update8InRange(TestContext& ctx)
    {
        RawPacket p;
        p.putArr(bytes("abcd"));
        KAS_TEST_CHECK_IMPL(ctx, p.update8(0xFF, 1));
        KAS_TEST_CHECK_IMPL(ctx, p.data()[1] == 0xFF);
    }

    // --- 23. update8 за границей -> false, данные не тронуты ------
    inline void update8OutOfRange(TestContext& ctx)
    {
        RawPacket p;
        p.putArr(bytes("abc"));
        const auto before = p.toRawBytes();
        KAS_TEST_CHECK_IMPL(ctx, !p.update8(0xFF, 3));
        KAS_TEST_CHECK_IMPL(ctx, !p.update8(0xFF, 100));
        KAS_TEST_CHECK_IMPL(ctx, p.toRawBytes() == before);
    }

    // ==================================================================
    //  update16_be / update16_le
    // ==================================================================

    // --- 24. update16_be в границах -------------------------------
    inline void update16BeInRange(TestContext& ctx)
    {
        RawPacket p;
        p.putArr(bytes("abcd"));
        KAS_TEST_CHECK_IMPL(ctx, p.update16_be(0x1234, 1));
        KAS_TEST_CHECK_IMPL(ctx, p.data()[1] == 0x12);
        KAS_TEST_CHECK_IMPL(ctx, p.data()[2] == 0x34);
        KAS_TEST_CHECK_IMPL(ctx, p.data()[0] == 'a'); // не тронуто
        KAS_TEST_CHECK_IMPL(ctx, p.data()[3] == 'd'); // не тронуто
    }

    // --- 25. update16_le в границах -------------------------------
    inline void update16LeInRange(TestContext& ctx)
    {
        RawPacket p;
        p.putArr(bytes("abcd"));
        KAS_TEST_CHECK_IMPL(ctx, p.update16_le(0x1234, 1));
        KAS_TEST_CHECK_IMPL(ctx, p.data()[1] == 0x34);
        KAS_TEST_CHECK_IMPL(ctx, p.data()[2] == 0x12);
    }

    // --- 26. update16_be атомарен при провале ---------------------
    inline void update16BeAtomic(TestContext& ctx)
    {
        RawPacket p;
        p.putArr(bytes("abc")); // size 3
        const auto before = p.toRawBytes();
        // offset 2: только 1 байт свободен, 2 не помещаются
        KAS_TEST_CHECK_IMPL(ctx, !p.update16_be(0x1234, 2));
        KAS_TEST_CHECK_IMPL(ctx, p.toRawBytes() == before); // не тронуто
    }

    // --- 27. update16_le атомарен при провале ---------------------
    inline void update16LeAtomic(TestContext& ctx)
    {
        RawPacket p;
        p.putArr(bytes("abc"));
        const auto before = p.toRawBytes();
        KAS_TEST_CHECK_IMPL(ctx, !p.update16_le(0x1234, 2));
        KAS_TEST_CHECK_IMPL(ctx, p.toRawBytes() == before);
    }

    // ==================================================================
    //  update32_be / update32_le
    // ==================================================================

    // --- 28. update32_be в границах -------------------------------
    inline void update32BeInRange(TestContext& ctx)
    {
        RawPacket p;
        p.putArr(std::vector<std::uint8_t>(8, 0));
        KAS_TEST_CHECK_IMPL(ctx, p.update32_be(0xDEADBEEF, 2));
        KAS_TEST_CHECK_IMPL(ctx, p.data()[2] == 0xDE);
        KAS_TEST_CHECK_IMPL(ctx, p.data()[3] == 0xAD);
        KAS_TEST_CHECK_IMPL(ctx, p.data()[4] == 0xBE);
        KAS_TEST_CHECK_IMPL(ctx, p.data()[5] == 0xEF);
    }

    // --- 29. update32_le в границах -------------------------------
    inline void update32LeInRange(TestContext& ctx)
    {
        RawPacket p;
        p.putArr(std::vector<std::uint8_t>(8, 0));
        KAS_TEST_CHECK_IMPL(ctx, p.update32_le(0xDEADBEEF, 2));
        KAS_TEST_CHECK_IMPL(ctx, p.data()[2] == 0xEF);
        KAS_TEST_CHECK_IMPL(ctx, p.data()[3] == 0xBE);
        KAS_TEST_CHECK_IMPL(ctx, p.data()[4] == 0xAD);
        KAS_TEST_CHECK_IMPL(ctx, p.data()[5] == 0xDE);
    }

    // --- 30. update32_be атомарен при провале ---------------------
    inline void update32BeAtomic(TestContext& ctx)
    {
        RawPacket p;
        p.putArr(bytes("abc")); // size 3
        const auto before = p.toRawBytes();
        KAS_TEST_CHECK_IMPL(ctx, !p.update32_be(0x12345678, 0));
        KAS_TEST_CHECK_IMPL(ctx, p.toRawBytes() == before);
    }

    // --- 31. update32_le атомарен при провале ---------------------
    inline void update32LeAtomic(TestContext& ctx)
    {
        RawPacket p;
        p.putArr(bytes("abc"));
        const auto before = p.toRawBytes();
        KAS_TEST_CHECK_IMPL(ctx, !p.update32_le(0x12345678, 0));
        KAS_TEST_CHECK_IMPL(ctx, p.toRawBytes() == before);
    }

    // ==================================================================
    //  update64_be / update64_le
    // ==================================================================

    // --- 32. update64_be в границах -------------------------------
    inline void update64BeInRange(TestContext& ctx)
    {
        RawPacket p;
        p.putArr(std::vector<std::uint8_t>(16, 0));
        KAS_TEST_CHECK_IMPL(ctx, p.update64_be(0x0102030405060708ull, 4));
        KAS_TEST_CHECK_IMPL(ctx, p.data()[4] == 0x01);
        KAS_TEST_CHECK_IMPL(ctx, p.data()[5] == 0x02);
        KAS_TEST_CHECK_IMPL(ctx, p.data()[10] == 0x07);
        KAS_TEST_CHECK_IMPL(ctx, p.data()[11] == 0x08);
    }

    // --- 33. update64_le в границах -------------------------------
    inline void update64LeInRange(TestContext& ctx)
    {
        RawPacket p;
        p.putArr(std::vector<std::uint8_t>(16, 0));
        KAS_TEST_CHECK_IMPL(ctx, p.update64_le(0x0102030405060708ull, 4));
        KAS_TEST_CHECK_IMPL(ctx, p.data()[4] == 0x08);
        KAS_TEST_CHECK_IMPL(ctx, p.data()[5] == 0x07);
        KAS_TEST_CHECK_IMPL(ctx, p.data()[10] == 0x02);
        KAS_TEST_CHECK_IMPL(ctx, p.data()[11] == 0x01);
    }

    // --- 34. update64_be атомарен при провале ---------------------
    inline void update64BeAtomic(TestContext& ctx)
    {
        RawPacket p;
        p.putArr(bytes("abcdefgh")); // size 8, но нужно 8 + offset 1 = 9
        const auto before = p.toRawBytes();
        KAS_TEST_CHECK_IMPL(ctx, !p.update64_be(0, 1));
        KAS_TEST_CHECK_IMPL(ctx, p.toRawBytes() == before);
    }

    // --- 35. update64_le атомарен при провале ---------------------
    inline void update64LeAtomic(TestContext& ctx)
    {
        RawPacket p;
        p.putArr(bytes("abcdefgh"));
        const auto before = p.toRawBytes();
        KAS_TEST_CHECK_IMPL(ctx, !p.update64_le(0, 1));
        KAS_TEST_CHECK_IMPL(ctx, p.toRawBytes() == before);
    }

    // ==================================================================
    //  updateRange
    // ==================================================================

    // --- 36. updateRange в границах -------------------------------
    inline void updateRangeInRange(TestContext& ctx)
    {
        RawPacket p;
        p.putArr(bytes("abcdefgh"));
        KAS_TEST_CHECK_IMPL(ctx, p.updateRange(bytes("XYZ"), 2));
        KAS_TEST_CHECK_IMPL(ctx, p.toRawBytes() == bytes("abXYZfgh"));
    }

    // --- 37. updateRange ровно в конец ----------------------------
    inline void updateRangeExactEnd(TestContext& ctx)
    {
        RawPacket p;
        p.putArr(bytes("abc"));
        KAS_TEST_CHECK_IMPL(ctx, p.updateRange(bytes("XY"), 1));
        KAS_TEST_CHECK_IMPL(ctx, p.toRawBytes() == bytes("aXY"));
    }

    // --- 38. updateRange за границей -> false, данные целы ---------
    inline void updateRangeOutOfRange(TestContext& ctx)
    {
        RawPacket p;
        p.putArr(bytes("abc"));
        const auto before = p.toRawBytes();
        KAS_TEST_CHECK_IMPL(ctx, !p.updateRange(bytes("WXYZ"), 0));
        KAS_TEST_CHECK_IMPL(ctx, !p.updateRange(bytes("W"), 3));
        KAS_TEST_CHECK_IMPL(ctx, p.toRawBytes() == before);
    }

    // --- 39. updateRange: offset + size не переполняется ----------
    inline void updateRangeOverflow(TestContext& ctx)
    {
        RawPacket p;
        p.putArr(bytes("abc"));
        const auto before = p.toRawBytes();
        // offset = SIZE_MAX - не должен падать, должен вернуть false
        KAS_TEST_CHECK_IMPL(ctx,
            !p.updateRange(bytes("X"),
                std::numeric_limits<std::size_t>::max()));
        KAS_TEST_CHECK_IMPL(ctx, p.toRawBytes() == before);
    }

    // ==================================================================
    //  Fluent-цепочки
    // ==================================================================

    // --- 40. lvalue-цепочка ---------------------------------------
    inline void lvalueChain(TestContext& ctx)
    {
        RawPacket p;
        p.put8(0x01)
            .put16_be(0x0203)
            .put32_be(0x04050607)
            .put8(0x08);

        const std::vector<std::uint8_t> expected {
            0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08
        };
        KAS_TEST_CHECK_IMPL(ctx, p.toRawBytes() == expected);
    }

    // --- 41. rvalue-цепочка (временный объект) --------------------
    inline void rvalueChain(TestContext& ctx)
    {
        const std::vector<std::uint8_t> raw {
            RawPacket { }
                .put8(0xAA)
                .put16_be(0xBBCC)
                .toRawBytes()
        };

        KAS_TEST_CHECK_IMPL(ctx, (raw == std::vector<std::uint8_t> { 0xAA, 0xBB, 0xCC }));
    }

    // ==================================================================
    //  toRawBytes: lvalue - ссылка, rvalue - перемещение
    // ==================================================================

    // --- 42. toRawBytes() const& возвращает ссылку ----------------
    inline void toRawBytesRef(TestContext& ctx)
    {
        RawPacket p;
        p.putArr(bytes("abc"));
        const auto& ref = p.toRawBytes();
        KAS_TEST_CHECK_IMPL(ctx, &ref == &p.toRawBytes());
    }

    // --- 43. toRawBytes() && перемещает ---------------------------
    inline void toRawBytesMove(TestContext& ctx)
    {
        RawPacket p;
        p.putArr(bytes("xyz"));
        std::vector<std::uint8_t> v = std::move(p).toRawBytes();
        KAS_TEST_CHECK_IMPL(ctx, v == bytes("xyz"));
    }

    // ==================================================================
    //  Геттеры / свойства
    // ==================================================================

    // --- 44. hasData после put ------------------------------------
    inline void hasDataAfterPut(TestContext& ctx)
    {
        RawPacket p;
        KAS_TEST_CHECK_IMPL(ctx, !p.hasData());
        p.put8(0);
        KAS_TEST_CHECK_IMPL(ctx, p.hasData());
    }

    // --- 45. size соответствует числу put* ----------------------
    inline void sizeMatchesPuts(TestContext& ctx)
    {
        RawPacket p;
        p.put8(0);
        KAS_TEST_CHECK_IMPL(ctx, p.size() == 1u);
        p.put16_be(0);
        KAS_TEST_CHECK_IMPL(ctx, p.size() == 3u);
        p.put32_be(0);
        KAS_TEST_CHECK_IMPL(ctx, p.size() == 7u);
        p.put64_be(0);
        KAS_TEST_CHECK_IMPL(ctx, p.size() == 15u);
    }

    // --- 46. data() указывает на непрерывный буфер --------------
    inline void dataContiguous(TestContext& ctx)
    {
        RawPacket p;
        p.putArr(bytes("hello"));
        KAS_TEST_CHECK_IMPL(ctx, p.data() == p.toRawBytes().data());
        KAS_TEST_CHECK_IMPL(ctx, std::string(reinterpret_cast<const char*>(p.data()), p.size()) == "hello");
    }

    // --- 47. round-trip: put + update + toRawBytes ---------------
    inline void roundTrip(TestContext& ctx)
    {
        RawPacket p;
        p.put8(0x01);
        p.put16_be(0x0203);
        p.put32_be(0x04050607);
        KAS_TEST_CHECK_IMPL(ctx, p.update8(0xFF, 0));

        const std::vector<std::uint8_t> expected {
            0xFF, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
        };
        KAS_TEST_CHECK_IMPL(ctx, p.toRawBytes() == expected);
    }

    // --- 48. Детерминизм ----------------------------------------
    inline void deterministic(TestContext& ctx)
    {
        RawPacket p;
        p.putArr(bytes("repeat"));
        const auto first = p.toRawBytes();
        for (int i = 0; i < 10; ++i)
            KAS_TEST_CHECK_IMPL(ctx, p.toRawBytes() == first);
    }

    // --- 49. Копия RawPacket независима -------------------------
    inline void copyIndependence(TestContext& ctx)
    {
        RawPacket a;
        a.putArr(bytes("abc"));
        RawPacket b = a;
        b.put8(0xFF);
        KAS_TEST_CHECK_IMPL(ctx, a.toRawBytes() == bytes("abc"));
        KAS_TEST_CHECK_IMPL(ctx, b.toRawBytes() == bytes("abc\xFF"));
    }

} // namespace

// ============================================================
//  Точка входа
// ============================================================

inline bool runTests()
{
    TestContext ctx;

    defaultConstruct(ctx);
    reserveDoesNotChangeSize(ctx);
    resizeZero(ctx);
    resizeUpZeros(ctx);
    resizeDown(ctx);

    put8One(ctx);
    put16Be(ctx);
    put16Le(ctx);
    put32Be(ctx);
    put32Le(ctx);
    put64Be(ctx);
    put64Le(ctx);
    beVsLeDistinct(ctx);

    putArrBasic(ctx);
    putArrEmpty(ctx);
    putVecMove(ctx);
    putVecAppend(ctx);
    putRawPacketCopy(ctx);
    putRawPacketMove(ctx);
    selfAppendCopy(ctx);
    selfAppendMove(ctx);

    update8InRange(ctx);
    update8OutOfRange(ctx);

    update16BeInRange(ctx);
    update16LeInRange(ctx);
    update16BeAtomic(ctx);
    update16LeAtomic(ctx);

    update32BeInRange(ctx);
    update32LeInRange(ctx);
    update32BeAtomic(ctx);
    update32LeAtomic(ctx);

    update64BeInRange(ctx);
    update64LeInRange(ctx);
    update64BeAtomic(ctx);
    update64LeAtomic(ctx);

    updateRangeInRange(ctx);
    updateRangeExactEnd(ctx);
    updateRangeOutOfRange(ctx);
    updateRangeOverflow(ctx);

    lvalueChain(ctx);
    rvalueChain(ctx);

    toRawBytesRef(ctx);
    toRawBytesMove(ctx);

    hasDataAfterPut(ctx);
    sizeMatchesPuts(ctx);
    dataContiguous(ctx);

    roundTrip(ctx);
    deterministic(ctx);
    copyIndependence(ctx);

    return printSummary("kas::convert::serialize::rwp::tests", ctx);
}

} // namespace kas::convert::serialize::rwp::tests

#endif