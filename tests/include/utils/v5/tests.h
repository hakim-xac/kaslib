/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_UTILS_V5_TESTS_H
#define KASLIBCXX_UTILS_V5_TESTS_H

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "KAS/utils/v5/utils.h"
#include "tests/tests.h"

/**
 * @brief Tests for kas::utils::v5 utility functions.
 *
 * Covers the public API of utils/v5/utils.h:
 *  - splitString (vector, index, callable, delimiter-callable overloads);
 *  - ltrim / rtrim / trim;
 *  - stringify / joinStr / joinString;
 *  - isRange / clamp;
 *  - hexToInteger / stringToInteger / intToString;
 *  - integerToHEX / isStrAsDigit;
 *  - stringToBytes / bytesAsString;
 *  - fileToBytesArray / byteArrayToFile;
 *  - fileGetContents / stringToFile;
 *  - exec;
 *  - convertTimePointToString / systemTimeAsString / stringToSystemTimePoint /
 *    fileTimeToString;
 *  - randomString;
 *  - checkRangeElemAsInteger;
 *  - printRawData;
 *  - containsToken / iequals.
 */
namespace kas::utils::v5::tests {

namespace {
    using namespace kas::tests;

    // ------------------------------------------------------------------
    // Temporary file (RAII)
    // ------------------------------------------------------------------

    /**
     * @brief Creates a temporary file with the given content and removes it
     *        on destruction.
     */
    class TempFile {
    public:
        explicit TempFile(std::string name, std::string_view content,
            bool binary = false)
            : m_path(std::filesystem::temp_directory_path() / std::move(name))
        {
            std::ofstream out(m_path,
                binary ? (std::ios::binary | std::ios::out) : std::ios::out);
            out.write(content.data(), static_cast<std::streamsize>(content.size()));
        }
        ~TempFile()
        {
            std::error_code ec;
            std::filesystem::remove(m_path, ec);
        }
        TempFile(const TempFile&) = delete;
        TempFile& operator=(const TempFile&) = delete;

        [[nodiscard]] const std::filesystem::path& path() const { return m_path; }
        [[nodiscard]] std::string name() const { return m_path.string(); }

    private:
        std::filesystem::path m_path;
    };

    // ==================================================================
    // splitString (vector)
    // ==================================================================

    /// "a,b,c" split by ',' yields exactly three tokens in order.
    inline void splitVectorBasic(TestContext& ctx)
    {
        const auto v = splitString("a,b,c", ",");
        KAS_TEST_CHECK_IMPL(ctx, v.size() == 3u);
        KAS_TEST_CHECK_IMPL(ctx, v[0] == "a");
        KAS_TEST_CHECK_IMPL(ctx, v[1] == "b");
        KAS_TEST_CHECK_IMPL(ctx, v[2] == "c");
    }

    /// Empty tokens (including the trailing one) are skipped.
    inline void splitVectorSkipsEmpty(TestContext& ctx)
    {
        const auto v = splitString("a,,b,", ",");
        KAS_TEST_CHECK_IMPL(ctx, v.size() == 2u);
        KAS_TEST_CHECK_IMPL(ctx, v[0] == "a");
        KAS_TEST_CHECK_IMPL(ctx, v[1] == "b");
    }

    /// Empty delimiter -> empty vector (no hang).
    inline void splitVectorEmptyDelimiter(TestContext& ctx)
    {
        const auto v = splitString("abc", "");
        KAS_TEST_CHECK_IMPL(ctx, v.empty());
    }

    /// Empty input -> empty vector.
    inline void splitVectorEmptyInput(TestContext& ctx)
    {
        const auto v = splitString("", ",");
        KAS_TEST_CHECK_IMPL(ctx, v.empty());
    }

    // ==================================================================
    // splitString (index)
    // ==================================================================

    /// Indexes 0..2 return "a", "b", "c" for "a,b,c".
    inline void splitIndexBasic(TestContext& ctx)
    {
        const auto v0 = splitString("a,b,c", ",", 0);
        const auto v1 = splitString("a,b,c", ",", 1);
        const auto v2 = splitString("a,b,c", ",", 2);
        KAS_TEST_CHECK_IMPL(ctx, v0.has_value() && *v0 == "a");
        KAS_TEST_CHECK_IMPL(ctx, v1.has_value() && *v1 == "b");
        KAS_TEST_CHECK_IMPL(ctx, v2.has_value() && *v2 == "c");
    }

    /// Index counts only non-empty tokens: "a,,b" -> index 0 = "a", index 1 = "b".
    inline void splitIndexSkipsEmpty(TestContext& ctx)
    {
        const auto v0 = splitString("a,,b", ",", 0);
        const auto v1 = splitString("a,,b", ",", 1);
        KAS_TEST_CHECK_IMPL(ctx, v0.has_value() && *v0 == "a");
        KAS_TEST_CHECK_IMPL(ctx, v1.has_value() && *v1 == "b");
    }

    /// Out-of-range index returns an error.
    inline void splitIndexOutOfRange(TestContext& ctx)
    {
        const auto v = splitString("a,b", ",", 5);
        KAS_TEST_CHECK_IMPL(ctx, v.has_error());
    }

    /// Empty input returns an error.
    inline void splitIndexEmptyInput(TestContext& ctx)
    {
        const auto v = splitString("", ",", 0);
        KAS_TEST_CHECK_IMPL(ctx, v.has_error());
    }

    // ==================================================================
    // splitString (callable)
    // ==================================================================

    /// Callable receives each non-empty token in order.
    inline void splitCallableBasic(TestContext& ctx)
    {
        std::vector<std::string_view> parts;
        splitString("x;y;z", ";",
            [&parts](std::string_view s) { parts.push_back(s); });

        KAS_TEST_CHECK_IMPL(ctx, parts.size() == 3u);
        KAS_TEST_CHECK_IMPL(ctx, parts[0] == "x");
        KAS_TEST_CHECK_IMPL(ctx, parts[1] == "y");
        KAS_TEST_CHECK_IMPL(ctx, parts[2] == "z");
    }

    /// Empty delimiter -> callable is never invoked (no hang).
    inline void splitCallableEmptyDelimiter(TestContext& ctx)
    {
        int calls = 0;
        splitString("abc", "",
            [&calls](std::string_view) { ++calls; });
        KAS_TEST_CHECK_IMPL(ctx, calls == 0);
    }

    // ==================================================================
    // splitString (delimiter-callable + func-callable)
    // ==================================================================

    /// Delimiter callable returns a set of chars; both ',' and ';' split.
    inline void splitDelimCallableBasic(TestContext& ctx)
    {
        std::vector<std::string_view> parts;
        splitString("a,b;c", [] { return std::string_view { ",;" }; }, [&parts](std::string_view s) { parts.push_back(s); });

        KAS_TEST_CHECK_IMPL(ctx, parts.size() == 3u);
        KAS_TEST_CHECK_IMPL(ctx, parts[0] == "a");
        KAS_TEST_CHECK_IMPL(ctx, parts[1] == "b");
        KAS_TEST_CHECK_IMPL(ctx, parts[2] == "c");
    }

    /// Empty input -> func callable is never invoked.
    inline void splitDelimCallableEmptyStr(TestContext& ctx)
    {
        int calls = 0;
        splitString("", [] { return std::string_view { "," }; }, [&calls](std::string_view) { ++calls; });
        KAS_TEST_CHECK_IMPL(ctx, calls == 0);
    }

    /// Empty delimiter set -> no-op (no hang).
    inline void splitDelimCallableEmptyDelim(TestContext& ctx)
    {
        int calls = 0;
        splitString("abc", [] { return std::string_view { }; }, [&calls](std::string_view) { ++calls; });
        KAS_TEST_CHECK_IMPL(ctx, calls == 0);
    }

    /// Empty tokens are skipped by the delimiter-callable overload too.
    inline void splitDelimCallableSkipsEmpty(TestContext& ctx)
    {
        std::vector<std::string_view> parts;
        splitString("a,,b", [] { return std::string_view { "," }; }, [&parts](std::string_view s) { parts.push_back(s); });

        KAS_TEST_CHECK_IMPL(ctx, parts.size() == 2u);
        KAS_TEST_CHECK_IMPL(ctx, parts[0] == "a");
        KAS_TEST_CHECK_IMPL(ctx, parts[1] == "b");
    }

    // ==================================================================
    // ltrim / rtrim / trim
    // ==================================================================

    /// ltrim removes leading whitespace including \t and \n.
    inline void ltrimBasic(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, ltrim("  hello") == "hello");
        KAS_TEST_CHECK_IMPL(ctx, ltrim("hello") == "hello");
        KAS_TEST_CHECK_IMPL(ctx, ltrim("") == "");
        KAS_TEST_CHECK_IMPL(ctx, ltrim("   ") == "");
        KAS_TEST_CHECK_IMPL(ctx, ltrim("\t\n hi") == "hi");
    }

    /// rtrim removes trailing whitespace including \r and \n.
    inline void rtrimBasic(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, rtrim("hello  ") == "hello");
        KAS_TEST_CHECK_IMPL(ctx, rtrim("hello") == "hello");
        KAS_TEST_CHECK_IMPL(ctx, rtrim("") == "");
        KAS_TEST_CHECK_IMPL(ctx, rtrim("   ") == "");
        KAS_TEST_CHECK_IMPL(ctx, rtrim("hi \r\n") == "hi");
    }

    /// trim removes both ends.
    inline void trimBasic(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, trim("  hello  ") == "hello");
        KAS_TEST_CHECK_IMPL(ctx, trim("hello") == "hello");
        KAS_TEST_CHECK_IMPL(ctx, trim("   ") == "");
    }

    /// Custom delimiter trims only that character.
    inline void trimCustomDelimiter(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, ltrim<','>(",,hello") == "hello");
        KAS_TEST_CHECK_IMPL(ctx, rtrim<','>("hello,,") == "hello");
        KAS_TEST_CHECK_IMPL(ctx, trim<','>(",,hello,,") == "hello");
    }

    // ==================================================================
    // stringify / joinStr / joinString
    // ==================================================================

    /// stringify concatenates without separator; empty pack -> empty string.
    inline void stringifyBasic(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, stringify("a", "b", "c") == "abc");
        KAS_TEST_CHECK_IMPL(ctx, stringify() == "");
        KAS_TEST_CHECK_IMPL(ctx, stringify("hello") == "hello");
    }

    /// joinStr inserts separator between arguments (including single-arg case).
    inline void joinStrBasic(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, joinStr(",", "a", "b", "c") == "a,b,c");
        KAS_TEST_CHECK_IMPL(ctx, joinStr(",", "a") == "a");
    }

    /// joinString joins non-empty elements of a range with a delimiter.
    inline void joinStringBasic(TestContext& ctx)
    {
        const std::vector<std::string_view> parts { "a", "b", "c" };
        const auto s = joinString(parts.begin(), parts.end(), ",");
        KAS_TEST_CHECK_IMPL(ctx, s == "a,b,c");
    }

    /// joinString skips empty elements (no doubled delimiters).
    inline void joinStringSkipsEmpty(TestContext& ctx)
    {
        const std::vector<std::string_view> parts { "a", "", "b", "", "c" };
        const auto s = joinString(parts.begin(), parts.end(), ",");
        KAS_TEST_CHECK_IMPL(ctx, s == "a,b,c");
    }

    /// Empty range -> empty string.
    inline void joinStringEmptyRange(TestContext& ctx)
    {
        const std::vector<std::string_view> parts;
        const auto s = joinString(parts.begin(), parts.end(), ",");
        KAS_TEST_CHECK_IMPL(ctx, s.empty());
    }

    /// All-empty range -> empty string.
    inline void joinStringAllEmpty(TestContext& ctx)
    {
        const std::vector<std::string_view> parts { "", "", "" };
        const auto s = joinString(parts.begin(), parts.end(), ",");
        KAS_TEST_CHECK_IMPL(ctx, s.empty());
    }

    /// Single non-empty element -> the element itself, no delimiter.
    inline void joinStringSingle(TestContext& ctx)
    {
        const std::vector<std::string_view> parts { "only" };
        const auto s = joinString(parts.begin(), parts.end(), ",");
        KAS_TEST_CHECK_IMPL(ctx, s == "only");
    }

    /// joinString also works over std::string elements.
    inline void joinStringFromStrings(TestContext& ctx)
    {
        const std::vector<std::string> parts { "x", "y", "z" };
        const auto s = joinString(parts.begin(), parts.end(), "-");
        KAS_TEST_CHECK_IMPL(ctx, s == "x-y-z");
    }

    // ==================================================================
    // isRange
    // ==================================================================

    /// isRange is inclusive on both ends.
    inline void isRangeBasic(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, isRange(5, 1, 10));
        KAS_TEST_CHECK_IMPL(ctx, isRange(1, 1, 10));
        KAS_TEST_CHECK_IMPL(ctx, isRange(10, 1, 10));
        KAS_TEST_CHECK_IMPL(ctx, !isRange(0, 1, 10));
        KAS_TEST_CHECK_IMPL(ctx, !isRange(11, 1, 10));
    }

    // ==================================================================
    // hexToInteger / stringToInteger / intToString
    // ==================================================================

    /// hexToInteger accepts "ff", "0xFF", and "0Xff".
    inline void hexToIntegerBasic(TestContext& ctx)
    {
        const auto a = hexToInteger<std::uint32_t>("ff");
        const auto b = hexToInteger<std::uint32_t>("0xFF");
        const auto c = hexToInteger<std::uint32_t>("0Xff");
        KAS_TEST_CHECK_IMPL(ctx, a.has_value() && *a == 0xFFu);
        KAS_TEST_CHECK_IMPL(ctx, b.has_value() && *b == 0xFFu);
        KAS_TEST_CHECK_IMPL(ctx, c.has_value() && *c == 0xFFu);
    }

    /// Empty, non-hex, and trailing-junk inputs are rejected.
    inline void hexToIntegerInvalid(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, !hexToInteger<std::uint32_t>("").has_value());
        KAS_TEST_CHECK_IMPL(ctx, !hexToInteger<std::uint32_t>("zz").has_value());
        KAS_TEST_CHECK_IMPL(ctx, !hexToInteger<std::uint32_t>("ffg").has_value());
    }

    /// stringToInteger parses positive and negative values.
    inline void stringToIntegerBasic(TestContext& ctx)
    {
        const auto a = stringToInteger<std::int32_t>("42");
        const auto b = stringToInteger<std::int32_t>("-7");
        KAS_TEST_CHECK_IMPL(ctx, a.has_value() && *a == 42);
        KAS_TEST_CHECK_IMPL(ctx, b.has_value() && *b == -7);
    }

    /// Base 16 is supported.
    inline void stringToIntegerBase16(TestContext& ctx)
    {
        const auto a = stringToInteger<std::uint32_t>("ff", 16);
        KAS_TEST_CHECK_IMPL(ctx, a.has_value() && *a == 0xFFu);
    }

    /// Empty, non-numeric, and trailing-junk inputs are rejected.
    inline void stringToIntegerInvalid(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, !stringToInteger<std::int32_t>("").has_value());
        KAS_TEST_CHECK_IMPL(ctx, !stringToInteger<std::int32_t>("abc").has_value());
        KAS_TEST_CHECK_IMPL(ctx, !stringToInteger<std::int32_t>("12x").has_value());
    }

    /// intToString round-trips through stringToInteger.
    inline void intToStringBasic(TestContext& ctx)
    {
        const auto s0 = intToString(0);
        const auto s1 = intToString(42);
        const auto s2 = intToString(-7);
        const auto s3 = intToString(std::int64_t { -9223372036854775807LL });

        KAS_TEST_CHECK_IMPL(ctx, s0.has_value() && *s0 == "0");
        KAS_TEST_CHECK_IMPL(ctx, s1.has_value() && *s1 == "42");
        KAS_TEST_CHECK_IMPL(ctx, s2.has_value() && *s2 == "-7");
        KAS_TEST_CHECK_IMPL(ctx, s3.has_value() && *s3 == "-9223372036854775807");

        // Round-trip back.
        const auto back = stringToInteger<std::int64_t>(*s3);
        KAS_TEST_CHECK_IMPL(ctx, back.has_value() && *back == -9223372036854775807LL);
    }

    // ==================================================================
    // integerToHEX / isStrAsDigit
    // ==================================================================

    /// integerToHEX respects prefix, width, and case.
    inline void integerToHEXBasic(TestContext& ctx)
    {
        // (num, is_upper_case, is_prefix, max_setw)
        KAS_TEST_CHECK_IMPL(ctx, integerToHEX(0xFF, false, false, 4) == "00ff");
        KAS_TEST_CHECK_IMPL(ctx, integerToHEX(0xFF, false, true, 4) == "0x00ff");
        KAS_TEST_CHECK_IMPL(ctx, integerToHEX(0xFF, true, false, 4) == "00FF");
        KAS_TEST_CHECK_IMPL(ctx, integerToHEX(0xFF, true, true, 4) == "0x00FF");
    }

    /// Values longer than the requested width are not truncated.
    inline void integerToHEXNoTruncation(TestContext& ctx)
    {
        // (num, is_upper_case = false, is_prefix = false, max_setw = 4)
        KAS_TEST_CHECK_IMPL(ctx, integerToHEX(0xDEADBEEF, false, false, 4) == "deadbeef");
    }

    /// Zero is formatted correctly with and without width.
    inline void integerToHEXZero(TestContext& ctx)
    {
        // (num, is_upper_case, is_prefix, max_setw)
        KAS_TEST_CHECK_IMPL(ctx, integerToHEX(0, false, false, 1) == "0");
        KAS_TEST_CHECK_IMPL(ctx, integerToHEX(0, false, false, 4) == "0000");
        KAS_TEST_CHECK_IMPL(ctx, integerToHEX(0, false, true, 4) == "0x0000");
    }

    /// isStrAsDigit accepts empty and digit-only strings, rejects others.
    inline void isStrAsDigitBasic(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, isStrAsDigit("0123456789"));
        KAS_TEST_CHECK_IMPL(ctx, isStrAsDigit(""));
        KAS_TEST_CHECK_IMPL(ctx, !isStrAsDigit("12a"));
        KAS_TEST_CHECK_IMPL(ctx, !isStrAsDigit(" "));
    }

    // ==================================================================
    // stringToBytes / bytesAsString
    // ==================================================================

    /// Round-trip of a plain ASCII string through bytes and back.
    inline void stringToBytesRoundTrip(TestContext& ctx)
    {
        const std::string s = "hello";
        const auto bytes = stringToBytes(s);
        KAS_TEST_CHECK_IMPL(ctx, bytes.size() == s.size());
        KAS_TEST_CHECK_IMPL(ctx, bytes[0] == 'h');
        KAS_TEST_CHECK_IMPL(ctx, bytes[4] == 'o');
        KAS_TEST_CHECK_IMPL(ctx, bytesAsString(bytes) == s);
    }

    /// Empty input -> empty vector; empty span -> empty string.
    inline void stringToBytesEmpty(TestContext& ctx)
    {
        const auto bytes = stringToBytes("");
        KAS_TEST_CHECK_IMPL(ctx, bytes.empty());
        KAS_TEST_CHECK_IMPL(ctx, bytesAsString({ }).empty());
    }

    /// Embedded NULs are preserved by both functions.
    inline void stringToBytesWithNull(TestContext& ctx)
    {
        const std::string s("a\0b", 3);
        const auto bytes = stringToBytes(s);
        KAS_TEST_CHECK_IMPL(ctx, bytes.size() == 3u);
        KAS_TEST_CHECK_IMPL(ctx, bytes[0] == 'a');
        KAS_TEST_CHECK_IMPL(ctx, bytes[1] == 0);
        KAS_TEST_CHECK_IMPL(ctx, bytes[2] == 'b');
        KAS_TEST_CHECK_IMPL(ctx, bytesAsString(bytes) == s);
    }

    // ==================================================================
    // Files (bytes) - round-trip
    // ==================================================================

    /// byteArrayToFile then fileToBytesArray preserves bytes exactly.
    inline void fileBytesRoundTrip(TestContext& ctx)
    {
        const std::vector<std::uint8_t> data { 0xDE, 0xAD, 0x00, 0xBE, 0xEF };
        TempFile f("kas_v5_bytes.bin", "", true);

        const auto wr = byteArrayToFile(f.name(), std::span<const std::uint8_t>(data));
        KAS_TEST_CHECK_IMPL(ctx, wr.has_value() && *wr);

        const auto rd = fileToBytesArray(f.name());
        KAS_TEST_CHECK_IMPL(ctx, rd.has_value());
        if (rd)
            KAS_TEST_CHECK_IMPL(ctx, *rd == data);
    }

    /// An empty file yields an empty vector.
    inline void fileBytesEmpty(TestContext& ctx)
    {
        TempFile f("kas_v5_empty.bin", "", true);
        const auto rd = fileToBytesArray(f.name());
        KAS_TEST_CHECK_IMPL(ctx, rd.has_value());
        if (rd)
            KAS_TEST_CHECK_IMPL(ctx, rd->empty());
    }

    /// Missing file -> error.
    inline void fileBytesMissing(TestContext& ctx)
    {
        const auto rd = fileToBytesArray("definitely_no_such_file_zzz.bin");
        KAS_TEST_CHECK_IMPL(ctx, !rd.has_value());
    }

    // ==================================================================
    // Files (string) - round-trip
    // ==================================================================

    /// stringToFile then fileGetContents preserves text exactly.
    inline void fileStringRoundTrip(TestContext& ctx)
    {
        const std::string content = "hello, world";
        TempFile f("kas_v5_text.txt", content);

        const auto wr = stringToFile(f.name(), content);
        KAS_TEST_CHECK_IMPL(ctx, wr.has_value() && *wr);

        const auto rd = fileGetContents(f.name());
        KAS_TEST_CHECK_IMPL(ctx, rd.has_value());
        if (rd)
            KAS_TEST_CHECK_IMPL(ctx, *rd == content);
    }

    /// fileGetContents overloads (const char*, std::string, path) all work.
    inline void fileGetContentsOverloads(TestContext& ctx)
    {
        const std::string content = "abc";
        TempFile f("kas_v5_overloads.txt", content);

        const auto from_char = fileGetContents(f.name().c_str());
        const auto from_str = fileGetContents(f.name());
        const auto from_path = fileGetContents(std::filesystem::path { f.name() });

        KAS_TEST_CHECK_IMPL(ctx, from_char.has_value() && *from_char == content);
        KAS_TEST_CHECK_IMPL(ctx, from_str.has_value() && *from_str == content);
        KAS_TEST_CHECK_IMPL(ctx, from_path.has_value() && *from_path == content);
    }

    /// Missing file -> error for all overloads.
    inline void fileStringMissing(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, !fileGetContents("definitely_no_such_file_zzz.txt").has_value());
        KAS_TEST_CHECK_IMPL(ctx, !fileGetContents(std::string { "definitely_no_such_file_zzz.txt" }).has_value());
        KAS_TEST_CHECK_IMPL(ctx, !fileGetContents(std::filesystem::path { "definitely_no_such_file_zzz.txt" }).has_value());
    }

    // ==================================================================
    // Processes
    // ==================================================================

    /// exec captures stdout and strips one trailing '\n'.
    inline void execSuccess(TestContext& ctx)
    {
        const auto r = exec("echo hello");
        KAS_TEST_CHECK_IMPL(ctx, r.has_value());
        if (r)
            KAS_TEST_CHECK_IMPL(ctx, *r == "hello");
    }

    /// Non-zero exit status -> error.
    inline void execNonZeroExit(TestContext& ctx)
    {
        const auto r = exec("exit 1");
        KAS_TEST_CHECK_IMPL(ctx, !r.has_value());
    }

    // ==================================================================
    // Time
    // ==================================================================

    /// Invalid date string -> nullopt.
    inline void stringToSystemTimePointInvalid(TestContext& ctx)
    {
        const auto r = stringToSystemTimePoint("not a date");
        KAS_TEST_CHECK_IMPL(ctx, !r.has_value());
    }

    /// Parsing a valid date yields a time point in a reasonable range.
    inline void stringToSystemTimePointBasic(TestContext& ctx)
    {
        // Without an offset (UTC+0 is not guaranteed - mktime depends on
        // the system TZ). Only check that parsing succeeds and the result
        // is in a reasonable range (year 2020..2100).
        const auto r = stringToSystemTimePoint("2024-01-15 12:00:00");
        KAS_TEST_CHECK_IMPL(ctx, r.has_value());
        if (r) {
            const auto t = std::chrono::system_clock::to_time_t(*r);
            KAS_TEST_CHECK_IMPL(ctx, t > 1577836800 && t < 4102444800);
        }
    }

    /// convertTimePointToString formats a valid time point into a non-empty string.
    inline void convertTimePointToStringBasic(TestContext& ctx)
    {
        using namespace std::chrono;
        const auto tp = system_clock::from_time_t(0); // 1970-01-01 UTC
        const auto s = convertTimePointToString(tp, "%Y");
        KAS_TEST_CHECK_IMPL(ctx, s.has_value());
        // Depending on TZ, the year may be 1969 or 1970 - do not pin it.
        if (s)
            KAS_TEST_CHECK_IMPL(ctx, !s->empty());
    }

    /// systemTimeAsString is a thin wrapper over convertTimePointToString.
    inline void systemTimeAsStringBasic(TestContext& ctx)
    {
        using namespace std::chrono;
        const auto tp = system_clock::from_time_t(0);
        const auto ok = systemTimeAsString(tp, "%Y");
        KAS_TEST_CHECK_IMPL(ctx, !ok.empty());

        // Invalid format -> empty string.
        const auto bad = systemTimeAsString(tp, "");
        KAS_TEST_CHECK_IMPL(ctx, bad.empty());
    }

    /// fileTimeToString returns a decimal seconds-since-epoch string.
    inline void fileTimeToStringBasic(TestContext& ctx)
    {
        std::error_code ec;
        const auto now = std::filesystem::file_time_type::clock::now();
        const auto s = fileTimeToString(now);
        KAS_TEST_CHECK_IMPL(ctx, !s.empty());
        for (char c : s) {
            if (c < '0' || c > '9') {
                KAS_TEST_CHECK_IMPL(ctx, false);
                return;
            }
        }
    }

    // ==================================================================
    // randomString
    // ==================================================================

    /// randomString returns the requested length of alphanumeric characters.
    inline void randomStringBasic(TestContext& ctx)
    {
        const auto s = randomString(32);
        KAS_TEST_CHECK_IMPL(ctx, s.size() == 32u);
        for (char c : s)
            KAS_TEST_CHECK_IMPL(ctx,
                (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'));
    }

    /// Length 0 -> empty string.
    inline void randomStringEmpty(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, randomString(0).empty());
    }

    // ==================================================================
    // checkRangeElemAsInteger
    // ==================================================================

    /// All-digit elements parse as int32.
    inline void checkRangeAllDigits(TestContext& ctx)
    {
        const std::vector<std::string_view> parts { "1", "22", "333" };
        KAS_TEST_CHECK_IMPL(ctx,
            checkRangeElemAsInteger<std::int32_t>(parts.begin(), parts.end()));
    }

    /// A single invalid element makes the whole range fail.
    inline void checkRangeWithInvalid(TestContext& ctx)
    {
        const std::vector<std::string_view> parts { "1", "abc", "3" };
        KAS_TEST_CHECK_IMPL(ctx,
            !checkRangeElemAsInteger<std::int32_t>(parts.begin(), parts.end()));
    }

    /// Empty range -> true (vacuous).
    inline void checkRangeEmpty(TestContext& ctx)
    {
        const std::vector<std::string_view> parts;
        KAS_TEST_CHECK_IMPL(ctx,
            checkRangeElemAsInteger<std::int32_t>(parts.begin(), parts.end()));
    }

    /// Negative values are accepted for signed target types.
    inline void checkRangeNegative(TestContext& ctx)
    {
        const std::vector<std::string_view> parts { "-1", "-42", "0" };
        KAS_TEST_CHECK_IMPL(ctx,
            checkRangeElemAsInteger<std::int32_t>(parts.begin(), parts.end()));
    }

    /// Out-of-range value for int32 -> false.
    inline void checkRangeOverflow(TestContext& ctx)
    {
        const std::vector<std::string_view> parts { "1", "999999999999" };
        KAS_TEST_CHECK_IMPL(ctx,
            !checkRangeElemAsInteger<std::int32_t>(parts.begin(), parts.end()));
    }

    // ==================================================================
    // printRawData
    // ==================================================================

    /// Two bytes per line: "de ad\nbe ef\n".
    inline void printRawDataBasic(TestContext& ctx)
    {
        const std::vector<std::uint8_t> data { 0xDE, 0xAD, 0xBE, 0xEF };
        std::ostringstream oss;
        printRawData(oss, std::span<const std::uint8_t>(data), 2);

        KAS_TEST_CHECK_IMPL(ctx, oss.str() == "de ad\nbe ef\n");
    }

    /// One byte per line.
    inline void printRawDataOnePerLine(TestContext& ctx)
    {
        const std::vector<std::uint8_t> data { 0x01, 0x02 };
        std::ostringstream oss;
        printRawData(oss, std::span<const std::uint8_t>(data), 1);
        KAS_TEST_CHECK_IMPL(ctx, oss.str() == "01\n02\n");
    }

    /// Empty span still emits a trailing newline.
    inline void printRawDataEmpty(TestContext& ctx)
    {
        std::ostringstream oss;
        printRawData(oss, std::span<const std::uint8_t> { }, 8);
        KAS_TEST_CHECK_IMPL(ctx, oss.str() == "\n");
    }

    /// Stream flags are restored after printing.
    inline void printRawDataRestoresFlags(TestContext& ctx)
    {
        std::ostringstream oss;
        const auto before = oss.flags();
        const std::vector<std::uint8_t> data { 0xFF };
        printRawData(oss, std::span<const std::uint8_t>(data), 8);
        KAS_TEST_CHECK_IMPL(ctx, oss.flags() == before);
    }

    /// Single-digit values are zero-padded to two hex chars.
    inline void printRawDataPadsZero(TestContext& ctx)
    {
        const std::vector<std::uint8_t> data { 0x05 };
        std::ostringstream oss;
        printRawData(oss, std::span<const std::uint8_t>(data), 8);
        KAS_TEST_CHECK_IMPL(ctx, oss.str() == "05\n");
    }

    // ==================================================================
    // containsToken / iequals
    // ==================================================================

    /// Exact token matches inside a comma-separated header (case-insensitive).
    inline void containsTokenBasic(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, containsToken("keep-alive, close", "close"));
        KAS_TEST_CHECK_IMPL(ctx, containsToken("keep-alive, close", "CLOSE"));
        KAS_TEST_CHECK_IMPL(ctx, containsToken(" keep-alive , close ", "keep-alive"));
        KAS_TEST_CHECK_IMPL(ctx, !containsToken("keep-alive, close", "closed"));
        KAS_TEST_CHECK_IMPL(ctx, !containsToken("", "close"));
    }

    /// iequals folds ASCII case only.
    inline void iequalsBasic(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, iequals("Hello", "hello"));
        KAS_TEST_CHECK_IMPL(ctx, iequals("", ""));
        KAS_TEST_CHECK_IMPL(ctx, !iequals("Hello", "HellO!"));
        KAS_TEST_CHECK_IMPL(ctx, !iequals("a", "ab"));
    }

} // namespace

// ============================================================
// Entry point
// ============================================================

/**
 * @brief Runs all utils v5 tests and prints a summary.
 */
inline bool runTests()
{
    TestContext ctx;

    // splitString (vector)
    splitVectorBasic(ctx);
    splitVectorSkipsEmpty(ctx);
    splitVectorEmptyDelimiter(ctx);
    splitVectorEmptyInput(ctx);

    // splitString (index)
    splitIndexBasic(ctx);
    splitIndexSkipsEmpty(ctx);
    splitIndexOutOfRange(ctx);
    splitIndexEmptyInput(ctx);

    // splitString (callable)
    splitCallableBasic(ctx);
    splitCallableEmptyDelimiter(ctx);

    // splitString (delimiter-callable + func-callable)
    splitDelimCallableBasic(ctx);
    splitDelimCallableEmptyStr(ctx);
    splitDelimCallableEmptyDelim(ctx);
    splitDelimCallableSkipsEmpty(ctx);

    // ltrim / rtrim / trim
    ltrimBasic(ctx);
    rtrimBasic(ctx);
    trimBasic(ctx);
    trimCustomDelimiter(ctx);

    // stringify / joinStr / joinString
    stringifyBasic(ctx);
    joinStrBasic(ctx);
    joinStringBasic(ctx);
    joinStringSkipsEmpty(ctx);
    joinStringEmptyRange(ctx);
    joinStringAllEmpty(ctx);
    joinStringSingle(ctx);
    joinStringFromStrings(ctx);

    // isRange
    isRangeBasic(ctx);

    // hexToInteger / stringToInteger / intToString
    hexToIntegerBasic(ctx);
    hexToIntegerInvalid(ctx);
    stringToIntegerBasic(ctx);
    stringToIntegerBase16(ctx);
    stringToIntegerInvalid(ctx);
    intToStringBasic(ctx);

    // integerToHEX / isStrAsDigit
    integerToHEXBasic(ctx);
    integerToHEXNoTruncation(ctx);
    integerToHEXZero(ctx);
    isStrAsDigitBasic(ctx);

    // stringToBytes / bytesAsString
    stringToBytesRoundTrip(ctx);
    stringToBytesEmpty(ctx);
    stringToBytesWithNull(ctx);

    // Files (bytes)
    fileBytesRoundTrip(ctx);
    fileBytesEmpty(ctx);
    fileBytesMissing(ctx);

    // Files (string)
    fileStringRoundTrip(ctx);
    fileGetContentsOverloads(ctx);
    fileStringMissing(ctx);

    // Processes
    execSuccess(ctx);
    execNonZeroExit(ctx);

    // Time
    stringToSystemTimePointInvalid(ctx);
    stringToSystemTimePointBasic(ctx);
    convertTimePointToStringBasic(ctx);
    systemTimeAsStringBasic(ctx);
    fileTimeToStringBasic(ctx);

    // randomString
    randomStringBasic(ctx);
    randomStringEmpty(ctx);

    // checkRangeElemAsInteger
    checkRangeAllDigits(ctx);
    checkRangeWithInvalid(ctx);
    checkRangeEmpty(ctx);
    checkRangeNegative(ctx);
    checkRangeOverflow(ctx);

    // printRawData
    printRawDataBasic(ctx);
    printRawDataOnePerLine(ctx);
    printRawDataEmpty(ctx);
    printRawDataRestoresFlags(ctx);
    printRawDataPadsZero(ctx);

    // containsToken / iequals
    containsTokenBasic(ctx);
    iequalsBasic(ctx);

    return printSummary("kas::utils::v5::tests", ctx);
}

} // namespace kas::utils::v5::tests

#endif // KASLIBCXX_UTILS_V5_TESTS_H