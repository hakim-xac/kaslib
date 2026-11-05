/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_CONVERT_MD5_V3_TESTS_H
#define KASLIBCXX_CONVERT_MD5_V3_TESTS_H

#include "KAS/convert/md5/v3/md5.h"
#include "tests/tests.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

/**
 * @brief Tests for the MD5 v3 hashing utilities.
 */
namespace kas::convert::md5::v3::tests {
namespace {

    using namespace kas::tests;
    namespace md5 = kas::convert::md5::v3;

    /**
     * @brief RAII guard that deletes a temporary file on destruction.
     */
    struct TempFileGuard {
        std::filesystem::path path;

        explicit TempFileGuard(std::string p)
            : path(std::move(p))
        {
        }

        TempFileGuard(const TempFileGuard&) = delete;
        TempFileGuard& operator=(const TempFileGuard&) = delete;
        TempFileGuard(TempFileGuard&&) = default;
        TempFileGuard& operator=(TempFileGuard&&) = default;

        ~TempFileGuard()
        {
            if (!path.empty()) {
                std::error_code ec;
                std::filesystem::remove(path, ec);
            }
        }
    };

    /**
     * @brief Builds a unique temporary file path (PID + atomic counter).
     */
    std::string makeTempFilePath()
    {
        static std::atomic<int> counter { 0 };
        const auto name = "kas_md5_v3_test_" + std::to_string(::getpid())
            + "_" + std::to_string(counter.fetch_add(1));
        return (std::filesystem::temp_directory_path() / name).string();
    }

    /**
     * @brief Creates a temporary file with the given content.
     */
    TempFileGuard makeTempFile(std::string_view content)
    {
        TempFileGuard g { makeTempFilePath() };
        std::ofstream out(g.path, std::ios::binary);
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        return g;
    }

    // ============================================================
    // Test: known RFC 1321 vectors (hashString)
    // ============================================================
    void testRfc1321Vectors(TestContext& ctx)
    {
        struct Vec {
            std::string input;
            std::string expected;
        };
        const std::vector<Vec> vectors = {
            { "", "d41d8cd98f00b204e9800998ecf8427e" },
            { "a", "0cc175b9c0f1b6a831c399e269772661" },
            { "abc", "900150983cd24fb0d6963f7d28e17f72" },
            { "message digest", "f96b697d7cb7938d525a2f31aaf161d0" },
            { "abcdefghijklmnopqrstuvwxyz", "c3fcd3d76192e4007dfb496cca67e13b" },
            { "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
                "d174ab98d277d9f5a5611c2c9f419d9f" },
            { "12345678901234567890123456789012345678901234567890123456789012345678901234567890",
                "57edf4a22be3c955ac49da2e2107b67a" },
        };
        for (const auto& v : vectors) {
            KAS_TEST_CHECK_IMPL(ctx, md5::hashString(v.input) == v.expected);
        }
    }

    // ============================================================
    // Test: output format - 32 lowercase hex characters
    // ============================================================
    void testOutputFormat(TestContext& ctx)
    {
        const std::string h = md5::hashString("abc");
        KAS_TEST_REQUIRE_IMPL(ctx, h.size() == 32);

        bool allHex = true;
        for (char c : h) {
            const bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
            if (!ok)
                allHex = false;
        }
        KAS_TEST_CHECK_IMPL(ctx, allHex);
    }

    // ============================================================
    // Test: determinism
    // ============================================================
    void testDeterminism(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx,
            md5::hashString("hello world") == md5::hashString("hello world"));
    }

    // ============================================================
    // Test: different inputs produce different hashes
    // ============================================================
    void testDifferentInputsDifferentHashes(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, md5::hashString("abc") != md5::hashString("abd"));
        KAS_TEST_CHECK_IMPL(ctx, md5::hashString("abc") != md5::hashString("ABC"));
        KAS_TEST_CHECK_IMPL(ctx, md5::hashString("") != md5::hashString(" "));
    }

    // ============================================================
    // Test: block boundaries - 55/56/63/64/65/127/128
    // ============================================================
    void testBlockBoundaries(TestContext& ctx)
    {
        const std::vector<std::size_t> lens = { 1, 55, 56, 63, 64, 65, 127, 128 };
        for (std::size_t n : lens) {
            const std::string s(n, 'x');
            const std::string h1 = md5::hashString(s);
            const std::string h2 = md5::hashString(s);
            KAS_TEST_CHECK_IMPL(ctx, h1.size() == 32);
            KAS_TEST_CHECK_IMPL(ctx, h1 == h2);
        }
    }

    // ============================================================
    // Test: binary data with null bytes (hashBytes(ptr, size))
    // ============================================================
    void testBinaryWithNullBytes(TestContext& ctx)
    {
        const std::uint8_t raw[] = { 'a', 0, 'b', 0, 'c', 0 };
        const std::string h = md5::hashBytes(raw, sizeof(raw));
        KAS_TEST_CHECK_IMPL(ctx, h.size() == 32);
        // reference: printf 'a\0b\0c\0' | md5sum
        KAS_TEST_CHECK_IMPL(ctx, h == "ce1473cf80c6b3fda8e3dfc006adc315");
    }

    // ============================================================
    // Test: hashBytes(span) matches hashBytes(ptr, size)
    // ============================================================
    void testHashBytesOverloadsAgree(TestContext& ctx)
    {
        const std::uint8_t data[] = { 0x00, 0x01, 0x02, 0x7f, 0x80, 0xff, 0x00 };
        const std::size_t n = sizeof(data);

        const std::string viaPtr = md5::hashBytes(data, n);
        const std::string viaSpan = md5::hashBytes(std::span<const std::uint8_t>(data, n));

        KAS_TEST_CHECK_IMPL(ctx, viaPtr == viaSpan);
        KAS_TEST_CHECK_IMPL(ctx, viaPtr.size() == 32);
    }

    // ============================================================
    // Test: empty span equals MD5 of empty input
    // ============================================================
    void testEmptySpan(TestContext& ctx)
    {
        const std::uint8_t dummy = 0;
        const std::string h = md5::hashBytes(std::span<const std::uint8_t>(&dummy, 0));
        KAS_TEST_CHECK_IMPL(ctx, h == "d41d8cd98f00b204e9800998ecf8427e");
    }

    // ============================================================
    // Test: hashFile - whole file matches hashString
    // ============================================================
    void testHashFileFull(TestContext& ctx)
    {
        const auto tf = makeTempFile("abc");
        const auto h = md5::hashFile(tf.path);
        KAS_TEST_CHECK_IMPL(ctx, h.has_value());
        if (h) {
            KAS_TEST_CHECK_IMPL(ctx, *h == md5::hashString("abc"));
            KAS_TEST_CHECK_IMPL(ctx, *h == "900150983cd24fb0d6963f7d28e17f72");
        }
    }

    // ============================================================
    // Test: hashFile - missing file -> nullopt
    // ============================================================
    void testHashFileMissing(TestContext& ctx)
    {
        const auto path = std::filesystem::temp_directory_path()
            / "kas_md5_v3_definitely_missing_file.bin";
        std::error_code ec;
        std::filesystem::remove(path, ec); // just in case

        const auto h = md5::hashFile(path);
        KAS_TEST_CHECK_IMPL(ctx, !h.has_value());
    }

    // ============================================================
    // Test: hashFile - partial range [start_bytes, end_bytes)
    // ============================================================
    void testHashFileRange(TestContext& ctx)
    {
        const auto tf = makeTempFile("0123456789");
        const auto h = md5::hashFile(tf.path, 2, 5);
        KAS_TEST_CHECK_IMPL(ctx, h.has_value());
        if (h)
            KAS_TEST_CHECK_IMPL(ctx, *h == md5::hashString("234"));
    }

    // ============================================================
    // Test: hashFile - empty range (start == end) -> MD5 of empty
    // ============================================================
    void testHashFileEmptyRange(TestContext& ctx)
    {
        const auto tf = makeTempFile("0123456789");
        const auto h = md5::hashFile(tf.path, 5, 5);
        KAS_TEST_CHECK_IMPL(ctx, h.has_value());
        if (h)
            KAS_TEST_CHECK_IMPL(ctx, *h == "d41d8cd98f00b204e9800998ecf8427e");
    }

    // ============================================================
    // Test: tests.h helpers applied to the hash output
    // ============================================================
    void testHelperCompat(TestContext& ctx)
    {
        const std::string h = md5::hashString("abc");
        KAS_TEST_CHECK_IMPL(ctx, !hasAnsi(h));
        KAS_TEST_CHECK_IMPL(ctx, stripAnsi(h) == h);
        KAS_TEST_CHECK_IMPL(ctx, splitLines(h).size() == 1);
        KAS_TEST_CHECK_IMPL(ctx, countSubstr(h, "0") <= 32);
        KAS_TEST_CHECK_IMPL(ctx, endsWithReset(h) == false);
        KAS_TEST_CHECK_IMPL(ctx, !allDigits(h));
    }

    // ============================================================
    // hashFile - empty file
    // ============================================================
    void testHashFileEmpty(TestContext& ctx)
    {
        const auto tf = makeTempFile("");
        const auto h = md5::hashFile(tf.path);
        KAS_TEST_CHECK_IMPL(ctx, h.has_value());
        if (h)
            KAS_TEST_CHECK_IMPL(ctx, *h == md5::hashString(""));
    }

    // ============================================================
    // hashFile - large file (1000 bytes)
    // ============================================================
    void testHashFileLarge(TestContext& ctx)
    {
        const std::string content(1000, 'a');
        const auto tf = makeTempFile(content);
        const auto h = md5::hashFile(tf.path);
        KAS_TEST_CHECK_IMPL(ctx, h.has_value());
        if (h)
            KAS_TEST_CHECK_IMPL(ctx, *h == md5::hashString(content));
    }

    // ============================================================
    // hashFile - binary data containing '\0'
    // ============================================================
    void testHashFileBinary(TestContext& ctx)
    {
        const std::string content("a\0b\0c", 5);
        const auto tf = makeTempFile(content);
        const auto h = md5::hashFile(tf.path);
        KAS_TEST_CHECK_IMPL(ctx, h.has_value());
        if (h)
            KAS_TEST_CHECK_IMPL(ctx, *h == md5::hashString(content));
    }

    // ============================================================
    // hashFile(range) - full range equals no-range call
    // ============================================================
    void testHashFileRangeFull(TestContext& ctx)
    {
        const std::string content = "Hello, World!";
        const auto tf = makeTempFile(content);

        const auto full = md5::hashFile(tf.path);
        const auto ranged = md5::hashFile(tf.path, 0, content.size());

        KAS_TEST_CHECK_IMPL(ctx, full.has_value());
        KAS_TEST_CHECK_IMPL(ctx, ranged.has_value());
        if (full && ranged)
            KAS_TEST_CHECK_IMPL(ctx, *full == *ranged);
    }

    // ============================================================
    // hashFile(range) - start > end -> nullopt
    // ============================================================
    void testHashFileRangeStartAfterEnd(TestContext& ctx)
    {
        const auto tf = makeTempFile("Hello");
        KAS_TEST_CHECK_IMPL(ctx, !md5::hashFile(tf.path, 3, 1).has_value());
    }

    // ============================================================
    // hashFile(range) - end > file_size -> nullopt
    // ============================================================
    void testHashFileRangeBeyondEof(TestContext& ctx)
    {
        const auto tf = makeTempFile("Hello"); // 5 bytes
        KAS_TEST_CHECK_IMPL(ctx, !md5::hashFile(tf.path, 0, 6).has_value());
        KAS_TEST_CHECK_IMPL(ctx, !md5::hashFile(tf.path, 0, 100).has_value());
    }

    // ============================================================
    // hashFile(range) - [0, 0) equals empty file
    // ============================================================
    void testHashFileRangeZeroZero(TestContext& ctx)
    {
        const auto tf = makeTempFile("Hello");
        const auto h = md5::hashFile(tf.path, 0, 0);
        KAS_TEST_CHECK_IMPL(ctx, h.has_value());
        if (h)
            KAS_TEST_CHECK_IMPL(ctx, *h == md5::hashString(""));
    }

    // ============================================================
    // hashFile(range) - binary data within the range
    // ============================================================
    void testHashFileRangeBinary(TestContext& ctx)
    {
        const std::string content("a\0b\0c", 5);
        const auto tf = makeTempFile(content);
        const auto h = md5::hashFile(tf.path, 1, 4); // "\0b\0"
        KAS_TEST_CHECK_IMPL(ctx, h.has_value());
        if (h)
            KAS_TEST_CHECK_IMPL(ctx, *h == md5::hashString(std::string("\0b\0", 3)));
    }
}

// ============================================================
// Runner
// ============================================================
/**
 * @brief Runs all MD5 v3 tests and prints a summary.
 */
inline bool runTests()
{
    TestContext ctx;

    // hashString / hashBytes
    testRfc1321Vectors(ctx);
    testOutputFormat(ctx);
    testDeterminism(ctx);
    testDifferentInputsDifferentHashes(ctx);
    testBlockBoundaries(ctx);
    testBinaryWithNullBytes(ctx);
    testHashBytesOverloadsAgree(ctx);
    testEmptySpan(ctx);

    // hashFile - whole file
    testHashFileFull(ctx);
    testHashFileMissing(ctx);
    testHashFileEmpty(ctx);
    testHashFileLarge(ctx);
    testHashFileBinary(ctx);

    // hashFile - range
    testHashFileRange(ctx);
    testHashFileEmptyRange(ctx);
    testHashFileRangeFull(ctx);
    testHashFileRangeStartAfterEnd(ctx);
    testHashFileRangeBeyondEof(ctx);
    testHashFileRangeZeroZero(ctx);
    testHashFileRangeBinary(ctx);

    // tests.h helpers
    testHelperCompat(ctx);

    return printSummary("kas::convert::md5::v3::tests", ctx);
}

}

#endif