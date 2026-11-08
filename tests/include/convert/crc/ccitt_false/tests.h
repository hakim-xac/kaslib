/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_CONVERT_CRC_CCITT_FALSE_TESTS_H
#define KASLIBCXX_CONVERT_CRC_CCITT_FALSE_TESTS_H

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "KAS/convert/crc/ccitt_false/crc16.h"
#include "tests/tests.h"

/**
 * @brief Tests for the CRC-16/CCITT-FALSE implementation.
 */
namespace kas::convert::crc::ccitt_false::tests {

namespace {
    using namespace kas::tests;
    using kas::convert::crc::ccitt_false::crc16;

    // ------------------------------------------------------------------
    // Helpers
    // ------------------------------------------------------------------

    /**
     * @brief Converts a string view into a byte vector.
     */
    [[nodiscard]] std::vector<std::uint8_t> bytes(std::string_view s)
    {
        return std::vector<std::uint8_t>(s.begin(), s.end());
    }

    // ==================================================================
    // Reference vectors for CRC-16/CCITT-FALSE
    // poly=0x1021, init=0xFFFF, refin=false, refout=false, xorout=0x0000
    // Reference values: python3 -c "from binascii import crc_hqx; ..."
    // with init=0xFFFF.
    // ==================================================================

    // --- 1. Empty input -> 0xFFFF (init value, no data) ---------------
    inline void emptyInput(TestContext& ctx)
    {
        const std::vector<std::uint8_t> empty;
        KAS_TEST_CHECK_IMPL(ctx, crc16(empty) == 0xFFFF);
    }

    // --- 2. Classic "123456789" -> 0x29B1 -----------------------------
    inline void checkString(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx,
            crc16(bytes("123456789")) == 0x29B1);
    }

    // --- 3. Single byte "A" (0x41) ------------------------------------
    inline void singleByteA(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, crc16(bytes("A")) == 0xB915);
    }

    // --- 4. Single byte "a" (0x61) ------------------------------------
    inline void singleByteLower(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, crc16(bytes("a")) == 0x9D77);
    }

    // --- 5. "Hello" ---------------------------------------------------
    inline void hello(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, crc16(bytes("Hello")) == 0xDADA);
    }

    // --- 6. "The quick brown fox ..." ---------------------------------
    inline void quickBrownFox(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx,
            crc16(bytes("The quick brown fox jumps over the lazy dog"))
                == 0x8FDD);
    }

    // --- 7. Single byte 0x00 ------------------------------------------
    inline void singleZeroByte(TestContext& ctx)
    {
        const std::vector<std::uint8_t> v { 0x00 };
        KAS_TEST_CHECK_IMPL(ctx, crc16(v) == 0xE1F0);
    }

    // --- 8. Single byte 0xFF ------------------------------------------
    inline void singleFFByte(TestContext& ctx)
    {
        const std::vector<std::uint8_t> v { 0xFF };
        KAS_TEST_CHECK_IMPL(ctx, crc16(v) == 0xFF00);
    }

    // --- 9. All 256 byte values in order ------------------------------
    inline void allByteValues(TestContext& ctx)
    {
        std::vector<std::uint8_t> data(256);
        for (std::size_t i = 0; i < data.size(); ++i)
            data[i] = static_cast<std::uint8_t>(i);
        // std::cout << "data: " << crc16(data) << "\n";
        KAS_TEST_CHECK_IMPL(ctx, crc16(data) == 0x3FBD);
    }

    // ==================================================================
    // Properties
    // ==================================================================

    // --- 10. Determinism on long inputs -------------------------------
    inline void deterministic(TestContext& ctx)
    {
        std::vector<std::uint8_t> data(1024);
        for (std::size_t i = 0; i < data.size(); ++i)
            data[i] = static_cast<std::uint8_t>((i * 37 + 7) & 0xFF);

        const std::uint16_t first = crc16(data);
        for (int k = 0; k < 10; ++k)
            KAS_TEST_CHECK_IMPL(ctx, crc16(data) == first);
    }

    // --- 11. Different inputs -> different CRC values -----------------
    inline void distinctInputs(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx,
            crc16(bytes("abc")) != crc16(bytes("abd")));
        KAS_TEST_CHECK_IMPL(ctx,
            crc16(bytes("")) != crc16(bytes(" ")));
        KAS_TEST_CHECK_IMPL(ctx,
            crc16(bytes("A")) != crc16(bytes("a")));
    }

    // --- 12. Byte order matters ---------------------------------------
    inline void orderMatters(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx,
            crc16(bytes("ab")) != crc16(bytes("ba")));
    }

    // --- 13. Input is not mutated -------------------------------------
    inline void inputNotMutated(TestContext& ctx)
    {
        const std::vector<std::uint8_t> data = bytes("unchanged input");
        const auto copy = data;
        (void)crc16(data);
        KAS_TEST_CHECK_IMPL(ctx, data == copy);
    }

    // ==================================================================
    // Span: different sources
    // ==================================================================

    // --- 14. span from std::array - "123456789" -----------------------
    inline void spanFromArray(TestContext& ctx)
    {
        const std::array<std::uint8_t, 9> arr {
            '1', '2', '3', '4', '5', '6', '7', '8', '9'
        };
        KAS_TEST_CHECK_IMPL(ctx,
            crc16(std::span<const std::uint8_t>(arr)) == 0x29B1);
    }

    // --- 15. span from a vector subrange - "123456789" ----------------
    inline void spanSubrange(TestContext& ctx)
    {
        const std::vector<std::uint8_t> full = bytes("x123456789y");
        const std::span<const std::uint8_t> sub(full.data() + 1, 9);
        KAS_TEST_CHECK_IMPL(ctx, crc16(sub) == 0x29B1);
    }

    // --- 16. Two spans with identical bytes -> same CRC ---------------
    inline void sameBytesDifferentSources(TestContext& ctx)
    {
        const std::vector<std::uint8_t> v = bytes("Hello");
        const std::array<std::uint8_t, 5> a { 'H', 'e', 'l', 'l', 'o' };
        KAS_TEST_CHECK_IMPL(ctx,
            crc16(std::span<const std::uint8_t>(v))
                == crc16(std::span<const std::uint8_t>(a)));
    }

    // ==================================================================
    // Robustness against single-byte changes
    // ==================================================================

    // --- 17. Single bit flipped -> CRC changes ------------------------
    inline void singleBitFlip(TestContext& ctx)
    {
        std::vector<std::uint8_t> data = bytes("payload");
        const std::uint16_t original = crc16(data);

        data[0] ^= 0x01;
        KAS_TEST_CHECK_IMPL(ctx, crc16(data) != original);
    }

} // namespace

// ============================================================
// Entry point
// ============================================================

/**
 * @brief Runs all CRC-16/CCITT-FALSE tests and prints a summary.
 */
inline bool runTests()
{
    TestContext ctx;

    emptyInput(ctx);
    checkString(ctx);
    singleByteA(ctx);
    singleByteLower(ctx);
    hello(ctx);
    quickBrownFox(ctx);

    singleZeroByte(ctx);
    singleFFByte(ctx);
    allByteValues(ctx);
    deterministic(ctx);

    distinctInputs(ctx);
    orderMatters(ctx);
    inputNotMutated(ctx);

    spanFromArray(ctx);
    spanSubrange(ctx);
    sameBytesDifferentSources(ctx);

    singleBitFlip(ctx);

    return printSummary("kas::convert::crc::ccitt_false::tests", ctx);
}

} // namespace kas::convert::crc::ccitt_false::tests

#endif