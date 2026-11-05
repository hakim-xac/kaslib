/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_CONVERT_BASE64_V1_TESTS_H
#define KASLIBCXX_CONVERT_BASE64_V1_TESTS_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "KAS/convert/base64/v1/base64.h"
#include "tests/tests.h"

/**
 * @brief Tests for the Base64 v1 encoder and decoder.
 */
namespace kas::convert::base64::v1::tests {

namespace {
    using namespace kas::tests;
    using kas::convert::base64::v1::decode;
    using kas::convert::base64::v1::encode;

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

    /**
     * @brief Converts a byte vector into a string.
     */
    [[nodiscard]] std::string asString(const std::vector<std::uint8_t>& v)
    {
        return std::string(v.begin(), v.end());
    }

    // ==================================================================
    // encode
    // ==================================================================

    // --- 1. encode - empty input --------------------------------------
    inline void encodeEmpty(TestContext& ctx)
    {
        const std::vector<std::uint8_t> empty;
        KAS_TEST_CHECK_IMPL(ctx, encode(empty).empty());
    }

    // --- 2. encode - one byte ("f" -> "Zg==") -------------------------
    inline void encodeOneByte(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, encode(bytes("f")) == "Zg==");
    }

    // --- 3. encode - two bytes ("fo" -> "Zm8=") -----------------------
    inline void encodeTwoBytes(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, encode(bytes("fo")) == "Zm8=");
    }

    // --- 4. encode - three bytes ("foo" -> "Zm9v") --------------------
    inline void encodeThreeBytes(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, encode(bytes("foo")) == "Zm9v");
    }

    // --- 5. encode - RFC 4648 section 10 test vectors -----------------
    inline void encodeRfcVectors(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, encode(bytes("")) == "");
        KAS_TEST_CHECK_IMPL(ctx, encode(bytes("f")) == "Zg==");
        KAS_TEST_CHECK_IMPL(ctx, encode(bytes("fo")) == "Zm8=");
        KAS_TEST_CHECK_IMPL(ctx, encode(bytes("foo")) == "Zm9v");
        KAS_TEST_CHECK_IMPL(ctx, encode(bytes("foob")) == "Zm9vYg==");
        KAS_TEST_CHECK_IMPL(ctx, encode(bytes("fooba")) == "Zm9vYmE=");
        KAS_TEST_CHECK_IMPL(ctx, encode(bytes("foobar")) == "Zm9vYmFy");
    }

    // --- 6. encode - common phrases -----------------------------------
    inline void encodeCommonPhrases(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx,
            encode(bytes("The quick brown fox jumps over the lazy dog"))
                == "VGhlIHF1aWNrIGJyb3duIGZveCBqdW1wcyBvdmVyIHRoZSBsYXp5IGRvZw==");

        KAS_TEST_CHECK_IMPL(ctx,
            encode(bytes("Hello, World!"))
                == "SGVsbG8sIFdvcmxkIQ==");
    }

    // --- 7. encode - binary data with all values 0..255 ---------------
    inline void encodeAllByteValues(TestContext& ctx)
    {
        std::vector<std::uint8_t> data(256);
        for (std::size_t i = 0; i < data.size(); ++i)
            data[i] = static_cast<std::uint8_t>(i);

        const std::string b64 = encode(data);
        KAS_TEST_CHECK_IMPL(ctx, b64.size() == 344u);
        KAS_TEST_CHECK_IMPL(ctx, b64.substr(b64.size() - 2) == "==");
    }

    // --- 8. encode - length multiple of 3 -> no '=' padding -----------
    inline void encodeNoPaddingWhenMultipleOf3(TestContext& ctx)
    {
        const std::string b64 = encode(bytes("abcabc")); // 6 bytes
        KAS_TEST_CHECK_IMPL(ctx, b64.find('=') == std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, b64 == "YWJjYWJj");
    }

    // --- 9. encode - exactly one '=' when remainder is 2 --------------
    inline void encodeSinglePad(TestContext& ctx)
    {
        const std::string b64 = encode(bytes("ab")); // 2 bytes
        KAS_TEST_CHECK_IMPL(ctx, b64.size() == 4u);
        KAS_TEST_CHECK_IMPL(ctx, b64.back() == '=');
        KAS_TEST_CHECK_IMPL(ctx, b64[b64.size() - 2] != '=');
    }

    // --- 10. encode - exactly two '=' when remainder is 1 -------------
    inline void encodeDoublePad(TestContext& ctx)
    {
        const std::string b64 = encode(bytes("a")); // 1 byte
        KAS_TEST_CHECK_IMPL(ctx, b64.size() == 4u);
        KAS_TEST_CHECK_IMPL(ctx, b64.substr(b64.size() - 2) == "==");
    }

    // ==================================================================
    // decode - success cases
    // ==================================================================

    // --- 11. decode - empty string -> empty vector --------------------
    inline void decodeEmpty(TestContext& ctx)
    {
        const auto r = decode("");
        KAS_TEST_CHECK_IMPL(ctx, r.has_value());
        if (r)
            KAS_TEST_CHECK_IMPL(ctx, r.value().empty());
    }

    // --- 12. decode - RFC 4648 section 10 vectors ---------------------
    inline void decodeRfcVectors(TestContext& ctx)
    {
        auto check = [&](const char* in, std::string_view expected) {
            const auto r = decode(in);
            KAS_TEST_CHECK_IMPL(ctx, r.has_value());
            if (r)
                KAS_TEST_CHECK_IMPL(ctx, asString(r.value()) == expected);
        };

        check("Zg==", "f");
        check("Zm8=", "fo");
        check("Zm9v", "foo");
        check("Zm9vYg==", "foob");
        check("Zm9vYmE=", "fooba");
        check("Zm9vYmFy", "foobar");
    }

    // --- 13. decode - common phrase -----------------------------------
    inline void decodeCommonPhrase(TestContext& ctx)
    {
        const auto r = decode("SGVsbG8sIFdvcmxkIQ==");
        KAS_TEST_CHECK_IMPL(ctx, r.has_value());
        if (r)
            KAS_TEST_CHECK_IMPL(ctx, asString(r.value()) == "Hello, World!");
    }

    // ==================================================================
    // round-trip
    // ==================================================================

    // --- 14. round-trip - all 256 byte values -------------------------
    inline void roundTripAllBytes(TestContext& ctx)
    {
        std::vector<std::uint8_t> data(256);
        for (std::size_t i = 0; i < data.size(); ++i)
            data[i] = static_cast<std::uint8_t>(i);

        const auto r = decode(encode(data));
        KAS_TEST_CHECK_IMPL(ctx, r.has_value());
        if (r)
            KAS_TEST_CHECK_IMPL(ctx, r.value() == data);
    }

    // --- 15. round-trip - lengths 0..32 -------------------------------
    inline void roundTripLengths(TestContext& ctx)
    {
        for (std::size_t len = 0; len <= 32; ++len) {
            std::vector<std::uint8_t> data(len);
            for (std::size_t i = 0; i < len; ++i)
                data[i] = static_cast<std::uint8_t>((i * 37 + len) & 0xFF);

            const auto r = decode(encode(data));
            KAS_TEST_CHECK_IMPL(ctx, r.has_value());
            if (r)
                KAS_TEST_CHECK_IMPL(ctx, r.value() == data);
        }
    }

    // --- 16. round-trip - large pseudo-random data, 1000 bytes --------
    inline void roundTripLargeRandom(TestContext& ctx)
    {
        std::vector<std::uint8_t> data(1000);
        for (std::size_t i = 0; i < data.size(); ++i)
            data[i] = static_cast<std::uint8_t>((i * 131 + 7) & 0xFF);

        const auto r = decode(encode(data));
        KAS_TEST_CHECK_IMPL(ctx, r.has_value());
        if (r)
            KAS_TEST_CHECK_IMPL(ctx, r.value() == data);
    }

    // ==================================================================
    // decode - error cases
    // ==================================================================

    // --- 17. decode - invalid character -------------------------------
    inline void decodeInvalidChar(TestContext& ctx)
    {
        const auto r = decode("Zm9v!");
        KAS_TEST_CHECK_IMPL(ctx, r.has_error());
        KAS_TEST_CHECK_IMPL(ctx, !r.has_value());
    }

    // --- 18. decode - whitespace inside input -------------------------
    inline void decodeWhitespace(TestContext& ctx)
    {
        const auto r = decode("Zm9 v");
        KAS_TEST_CHECK_IMPL(ctx, r.has_error());
    }

    // --- 19. decode - length not a multiple of 4 ----------------------
    inline void decodeBadLength(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, decode("Z").has_error());
        KAS_TEST_CHECK_IMPL(ctx, decode("Zm").has_error());
        KAS_TEST_CHECK_IMPL(ctx, decode("Zm9").has_error());
        KAS_TEST_CHECK_IMPL(ctx, decode("Zm9vY").has_error());
    }

    // --- 20. decode - invalid padding ---------------------------------
    inline void decodeBadPadding(TestContext& ctx)
    {
        // '=' not at the end
        KAS_TEST_CHECK_IMPL(ctx, decode("Zg=a").has_error());
        // three '=' in a row
        KAS_TEST_CHECK_IMPL(ctx, decode("Zg===").has_error());
        // a single '=' in a 4-character block is only allowed at the end
        KAS_TEST_CHECK_IMPL(ctx, decode("=Zg=").has_error());
    }

    // --- 21. decode - padding in the wrong place ----------------------
    inline void decodePadInMiddle(TestContext& ctx)
    {
        // "Zg==" is valid, but "Zg==Zg==" has extra data after padding
        KAS_TEST_CHECK_IMPL(ctx, decode("Zg==Zg==").has_error());
    }

    // --- 22. decode - non-printable bytes in input --------------------
    inline void decodeControlChars(TestContext& ctx)
    {
        const std::string s = std::string("Zm9v") + '\0';
        KAS_TEST_CHECK_IMPL(ctx, decode(s).has_error());
    }

    // --- 23. decode - value() throws on error -------------------------
    inline void decodeErrorValueThrows(TestContext& ctx)
    {
        const auto r = decode("!");
        KAS_TEST_CHECK_IMPL(ctx, r.has_error());

        if (!r) {
            bool caught = false;
            try {
                (void)r.value();
            } catch (const std::exception&) {
                caught = true;
            }
            KAS_TEST_CHECK_IMPL(ctx, caught);
        }
    }

    // --- 24. decode - error() is accessible on error ------------------
    inline void decodeErrorMessage(TestContext& ctx)
    {
        const auto r = decode("!");
        KAS_TEST_CHECK_IMPL(ctx, r.has_error());

        if (!r) {
            // value() must throw on error
            bool threwOnValue = false;
            try {
                (void)r.value();
            } catch (const std::exception&) {
                threwOnValue = true;
            }
            KAS_TEST_CHECK_IMPL(ctx, threwOnValue);

            // error() must not throw on error - it returns E
            const auto& _err = r.error();
            (void)_err;
        }
    }

    // ==================================================================
    // decode - lenient / variant cases (pin down the behavior)
    // ==================================================================

    // --- 25. decode - full block without padding ----------------------
    inline void decodeFullBlockWithNoPad(TestContext& ctx)
    {
        // 4 characters without '=' form a valid block
        const auto r = decode("Zm9v");
        KAS_TEST_CHECK_IMPL(ctx, r.has_value());
        if (r)
            KAS_TEST_CHECK_IMPL(ctx, asString(r.value()) == "foo");
    }

    // --- 26. decode - character case sensitivity ----------------------
    inline void decodeCaseSensitivity(TestContext& ctx)
    {
        // Base64 is case-sensitive: "Zm9v" -> "foo", "zm9v" -> a different result.
        const auto lower = decode("Zm9v");
        KAS_TEST_CHECK_IMPL(ctx, lower.has_value());
        if (lower)
            KAS_TEST_CHECK_IMPL(ctx, asString(lower.value()) == "foo");

        // 'z' is a valid base64 character (index 51), so "zm9v" must decode
        // successfully, but to a different byte sequence than "foo".
        const auto alt = decode("zm9v");
        KAS_TEST_CHECK_IMPL(ctx, alt.has_value());
        if (alt) {
            KAS_TEST_CHECK_IMPL(ctx, asString(alt.value()) != "foo");
            KAS_TEST_CHECK_IMPL(ctx,
                alt.value() == std::vector<std::uint8_t>({ 0xCE, 0x6F, 0x6F }));
        }
    }
} // namespace

// ============================================================
// Entry point
// ============================================================

/**
 * @brief Runs all Base64 v1 tests and prints a summary.
 */
inline bool runTests()
{
    TestContext ctx;

    // encode
    encodeEmpty(ctx);
    encodeOneByte(ctx);
    encodeTwoBytes(ctx);
    encodeThreeBytes(ctx);
    encodeRfcVectors(ctx);
    encodeCommonPhrases(ctx);
    encodeAllByteValues(ctx);
    encodeNoPaddingWhenMultipleOf3(ctx);
    encodeSinglePad(ctx);
    encodeDoublePad(ctx);

    // decode - success
    decodeEmpty(ctx);
    decodeRfcVectors(ctx);
    decodeCommonPhrase(ctx);

    // round-trip
    roundTripAllBytes(ctx);
    roundTripLengths(ctx);
    roundTripLargeRandom(ctx);

    // decode - errors
    decodeInvalidChar(ctx);
    decodeWhitespace(ctx);
    decodeBadLength(ctx);
    decodeBadPadding(ctx);
    decodePadInMiddle(ctx);
    decodeControlChars(ctx);
    decodeErrorValueThrows(ctx);
    decodeErrorMessage(ctx);

    // decode - variant cases
    decodeFullBlockWithNoPad(ctx);
    decodeCaseSensitivity(ctx);

    return printSummary("kas::convert::base64::v1::tests", ctx);
}

} // namespace kas::convert::base64::v1::tests

#endif