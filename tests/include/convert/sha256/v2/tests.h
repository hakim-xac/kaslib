/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_CONVERT_SHA256_V2_TESTS_H
#define KASLIBCXX_CONVERT_SHA256_V2_TESTS_H

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include "KAS/convert/sha256/v2/sha256.h"
#include "tests/tests.h"

namespace kas::convert::sha256::v2::tests {

namespace {
    using namespace kas::tests;
    using kas::convert::sha256::v2::digest;
    using kas::convert::sha256::v2::hash;
    using kas::convert::sha256::v2::hashFile;
    using kas::convert::sha256::v2::hexDigest;

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

    std::string makeTempFilePath()
    {
        static std::atomic<int> counter { 0 };
        const auto name = "kas_sha256_test_" + std::to_string(::getpid())
            + "_" + std::to_string(counter.fetch_add(1));
        return (std::filesystem::temp_directory_path() / name).string();
    }

    TempFileGuard makeTempFile(std::string_view content)
    {
        TempFileGuard g { makeTempFilePath() };
        std::ofstream out(g.path, std::ios::binary);
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        return g;
    }

    [[nodiscard]] std::string hex(const std::array<std::uint8_t, 32>& bytes)
    {
        static constexpr char HEX[] = "0123456789abcdef";
        std::string out;
        out.reserve(64);
        for (std::uint8_t b : bytes) {
            out.push_back(HEX[b >> 4]);
            out.push_back(HEX[b & 0x0F]);
        }
        return out;
    }

    // ==================================================================
    //  hash() - эталонные векторы
    // ==================================================================

    // --- 1. hash - пустая строка -------------------------------------
    inline void hashEmpty(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, hash("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    }

    // --- 2. hash - "abc" ---------------------------------------------
    inline void hashAbc(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, hash("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    }

    // --- 3. hash - "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq" ---
    inline void hashRfcVector(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx,
            hash("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")
                == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    }

    // --- 4. hash - "Hello, World!" -----------------------------------
    inline void hashCommonPhrase(TestContext& ctx)
    {
        // hashlib.sha256(b"Hello, World!").hexdigest()
        KAS_TEST_CHECK_IMPL(ctx, hash("Hello, World!") == "dffd6021bb2bd5b0af676290809ec3a53191dd81c7f70a4b28688a362182986f");
    }

    // --- 5. hash - строка со встроенным '\0' -------------------------
    inline void hashEmbeddedNull(TestContext& ctx)
    {
        std::string s("ab\0cd", 5);
        KAS_TEST_CHECK_IMPL(ctx, hash(s) == "1bd95cf6379b94fd3b6ceb1390b70b822c76442c4bfb8273b941e09d8dfd9b56");
    }

    // ==================================================================
    //  hash() - границы блоков
    // ==================================================================

    // --- 6. hash - 55/56/63/64/65 байт ------------------------------
    inline void hashBlockBoundaries(TestContext& ctx)
    {
        for (std::size_t len : { 55u, 56u, 63u, 64u, 65u,
                 119u, 120u, 127u, 128u, 129u }) {
            const std::string s(len, 'x');
            const std::string h = hash(s);
            KAS_TEST_CHECK_IMPL(ctx, h.size() == 64u);
            KAS_TEST_CHECK_IMPL(ctx, h == hash(s)); // идемпотентность
        }
    }

    // --- 7. hash - 1000 байт 'a' ------------------------------------
    inline void hashLarge(TestContext& ctx)
    {
        // hashlib.sha256(b'a'*1000).hexdigest()
        KAS_TEST_CHECK_IMPL(ctx, hash(std::string(1000, 'a')) == "41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3");
    }

    // ==================================================================
    //  digest() / hexDigest() - согласованность
    // ==================================================================

    // --- 8. digest - 32 байта, hex совпадает с hash ------------------
    inline void digestMatchesHash(TestContext& ctx)
    {
        const std::vector<std::string> lists {
            "", "a", "abc", "Hello, World!",
            std::string(64, 'a'), std::string(1000, 'a')
        };
        for (std::string_view s : lists) {
            KAS_TEST_CHECK_IMPL(ctx, hex(digest(std::string(s))) == hash(std::string(s)));
        }
    }

    // --- 9. digest - размер ровно 32 --------------------------------
    inline void digestSize(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, digest("").size() == 32u);
        KAS_TEST_CHECK_IMPL(ctx, digest("abc").size() == 32u);
    }

    // --- 10. digest - эталонные байты для "abc" ---------------------
    inline void digestAbcBytes(TestContext& ctx)
    {
        // ba 78 16 bf 8f 01 cf ea 41 41 40 de 5d ae 22 23
        // b0 03 61 a3 96 17 7a 9c b4 10 ff 61 f2 00 15 ad
        const std::array<std::uint8_t, 32> expected {
            0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
            0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
            0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
            0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
        };
        KAS_TEST_CHECK_IMPL(ctx, digest("abc") == expected);
    }

    // --- 11. digest - big-endian, а не little-endian ----------------
    inline void digestBigEndian(TestContext& ctx)
    {
        const auto b = digest("abc");
        KAS_TEST_CHECK_IMPL(ctx, b[0] == 0xba);
        KAS_TEST_CHECK_IMPL(ctx, b[3] == 0xbf);
    }

    // --- 12. digest - пустая строка, эталон -------------------------
    inline void digestEmptyBytes(TestContext& ctx)
    {
        const std::array<std::uint8_t, 32> expected {
            0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14,
            0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
            0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
            0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55
        };
        KAS_TEST_CHECK_IMPL(ctx, digest("") == expected);
    }

    // --- 13. hexDigest - 64 символа, lowercase hex ------------------
    inline void hexDigestFormat(TestContext& ctx)
    {
        const std::string h = hexDigest("format check");
        KAS_TEST_CHECK_IMPL(ctx, h.size() == 64u);
        for (char c : h)
            KAS_TEST_CHECK_IMPL(ctx, (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
    }

    // --- 14. hexDigest(string) == hexDigest(span) -------------------
    inline void hexDigestOverloads(TestContext& ctx)
    {
        const std::string s = "consistency";
        const auto* p = reinterpret_cast<const std::uint8_t*>(s.data());
        KAS_TEST_CHECK_IMPL(ctx, hexDigest(s) == hexDigest(std::span<const std::uint8_t>(p, s.size())));
    }

    // ==================================================================
    //  Свойства
    // ==================================================================

    // --- 15. hash - разные входы -> разные хэши ----------------------
    inline void hashDistinct(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, hash("abc") != hash("abd"));
        KAS_TEST_CHECK_IMPL(ctx, hash("a") != hash("A"));
        KAS_TEST_CHECK_IMPL(ctx, hash("") != hash(" "));
    }

    // --- 16. Детерминированность ------------------------------------
    inline void deterministic(TestContext& ctx)
    {
        const std::string s = "repeatable input";
        const std::string first = hash(s);
        for (int i = 0; i < 50; ++i)
            KAS_TEST_CHECK_IMPL(ctx, hash(s) == first);
    }

    // --- 17. hash() не портит вход ----------------------------------
    inline void inputNotMutated(TestContext& ctx)
    {
        const std::string s = "unchanged";
        const std::string copy = s;
        (void)hash(s);
        KAS_TEST_CHECK_IMPL(ctx, s == copy);
    }

    // ==================================================================
    //  hashFile - полный файл
    // ==================================================================

    // --- 18. hashFile - совпадает с hash() --------------------------
    inline void hashFileMatchesHash(TestContext& ctx)
    {
        const auto tf = makeTempFile("Hello, World!");
        const auto file_hash = hashFile(tf.path);
        KAS_TEST_CHECK_IMPL(ctx, file_hash.has_value());
        if (file_hash)
            KAS_TEST_CHECK_IMPL(ctx, *file_hash == hash("Hello, World!"));
    }

    // --- 19. hashFile - пустой файл --------------------------------
    inline void hashFileEmpty(TestContext& ctx)
    {
        const auto tf = makeTempFile("");
        const auto file_hash = hashFile(tf.path);
        KAS_TEST_CHECK_IMPL(ctx, file_hash.has_value());
        if (file_hash)
            KAS_TEST_CHECK_IMPL(ctx, *file_hash == hash(""));
    }

    // --- 20. hashFile - несуществующий файл -> nullopt -------------
    inline void hashFileMissing(TestContext& ctx)
    {
        const auto path = std::filesystem::temp_directory_path()
            / ("kas_sha256_missing_" + std::to_string(::getpid()));
        const auto file_hash = hashFile(path);
        KAS_TEST_CHECK_IMPL(ctx, !file_hash.has_value());
    }

    // --- 21. hashFile - большой файл (1000 байт) -------------------
    inline void hashFileLarge(TestContext& ctx)
    {
        const std::string content(1000, 'a');
        const auto tf = makeTempFile(content);
        const auto file_hash = hashFile(tf.path);
        KAS_TEST_CHECK_IMPL(ctx, file_hash.has_value());
        if (file_hash)
            KAS_TEST_CHECK_IMPL(ctx, *file_hash == hash(content));
    }

    // --- 22. hashFile - бинарные данные с '\0' ---------------------
    inline void hashFileBinary(TestContext& ctx)
    {
        const std::string content("a\0b\0c", 5);
        const auto tf = makeTempFile(content);
        const auto file_hash = hashFile(tf.path);
        KAS_TEST_CHECK_IMPL(ctx, file_hash.has_value());
        if (file_hash)
            KAS_TEST_CHECK_IMPL(ctx, *file_hash == hash(content));
    }

    // ==================================================================
    //  hashFile - диапазон [start, end)
    // ==================================================================

    // --- 23. hashFile - [0, 5) --------------------------------------
    inline void hashFileRangePrefix(TestContext& ctx)
    {
        const auto tf = makeTempFile("Hello, World!");
        const auto file_hash = hashFile(tf.path, 0, 5);
        KAS_TEST_CHECK_IMPL(ctx, file_hash.has_value());
        if (file_hash)
            KAS_TEST_CHECK_IMPL(ctx, *file_hash == hash("Hello"));
    }

    // --- 24. hashFile - [7, 13) -------------------------------------
    inline void hashFileRangeSuffix(TestContext& ctx)
    {
        const auto tf = makeTempFile("Hello, World!");
        const auto file_hash = hashFile(tf.path, 7, 13); // "World!"
        KAS_TEST_CHECK_IMPL(ctx, file_hash.has_value());
        if (file_hash)
            KAS_TEST_CHECK_IMPL(ctx, *file_hash == hash("World!"));
    }

    // --- 25. hashFile - полный диапазон == hashFile без диапазона ---
    inline void hashFileRangeFull(TestContext& ctx)
    {
        const std::string content = "Hello, World!";
        const auto tf = makeTempFile(content);
        const auto file_hash = hashFile(tf.path, 0, content.size());
        KAS_TEST_CHECK_IMPL(ctx, file_hash.has_value());
        if (file_hash)
            KAS_TEST_CHECK_IMPL(ctx, *file_hash == hash(content));
    }

    // --- 26. hashFile - [start, start) -> хеш пустой строки ---------
    inline void hashFileRangeEmpty(TestContext& ctx)
    {
        const auto tf = makeTempFile("Hello");
        const auto file_hash = hashFile(tf.path, 2, 2);
        KAS_TEST_CHECK_IMPL(ctx, file_hash.has_value());
        if (file_hash)
            KAS_TEST_CHECK_IMPL(ctx, *file_hash == hash(""));
    }

    // --- 27. hashFile - start > end -> nullopt ----------------------
    inline void hashFileRangeStartAfterEnd(TestContext& ctx)
    {
        const auto tf = makeTempFile("Hello");
        KAS_TEST_CHECK_IMPL(ctx, !hashFile(tf.path, 3, 1).has_value());
    }

    // --- 28. hashFile - end > file_size -> nullopt ------------------
    inline void hashFileRangeBeyondEof(TestContext& ctx)
    {
        const auto tf = makeTempFile("Hello"); // 5 байт
        KAS_TEST_CHECK_IMPL(ctx, !hashFile(tf.path, 0, 6).has_value());
        KAS_TEST_CHECK_IMPL(ctx, !hashFile(tf.path, 0, 100).has_value());
    }

    // --- 29. hashFile - [0, 0) == пустой файл ----------------------
    inline void hashFileRangeZeroZero(TestContext& ctx)
    {
        const auto tf = makeTempFile("Hello");
        const auto file_hash = hashFile(tf.path, 0, 0);
        KAS_TEST_CHECK_IMPL(ctx, file_hash.has_value());
        if (file_hash)
            KAS_TEST_CHECK_IMPL(ctx, *file_hash == hash(""));
    }

    // --- 30. hashFile - бинарные данные в диапазоне ------------------
    inline void hashFileRangeBinary(TestContext& ctx)
    {
        const std::string content("a\0b\0c", 5);
        const auto tf = makeTempFile(content);
        const auto file_hash = hashFile(tf.path, 1, 4); // "\0b\0"
        KAS_TEST_CHECK_IMPL(ctx, file_hash.has_value());
        if (file_hash)
            KAS_TEST_CHECK_IMPL(ctx, *file_hash == hash(std::string("\0b\0", 3)));
    }
} // namespace

// ============================================================
//  Точка входа
// ============================================================

inline bool runTests()
{
    TestContext ctx;

    hashEmpty(ctx);
    hashAbc(ctx);
    hashRfcVector(ctx);
    hashCommonPhrase(ctx);
    hashEmbeddedNull(ctx);
    hashBlockBoundaries(ctx);
    hashLarge(ctx);

    digestMatchesHash(ctx);
    digestSize(ctx);
    digestAbcBytes(ctx);
    digestBigEndian(ctx);
    digestEmptyBytes(ctx);
    hexDigestFormat(ctx);
    hexDigestOverloads(ctx);

    // hashFile - полный файл
    hashFileMatchesHash(ctx);
    hashFileEmpty(ctx);
    hashFileMissing(ctx);
    hashFileLarge(ctx);
    hashFileBinary(ctx);

    // hashFile - диапазон
    hashFileRangePrefix(ctx);
    hashFileRangeSuffix(ctx);
    hashFileRangeFull(ctx);
    hashFileRangeEmpty(ctx);
    hashFileRangeStartAfterEnd(ctx);
    hashFileRangeBeyondEof(ctx);
    hashFileRangeZeroZero(ctx);
    hashFileRangeBinary(ctx);

    hashDistinct(ctx);
    deterministic(ctx);
    inputNotMutated(ctx);

    return printSummary("kas::convert::sha256::v2::tests", ctx);
}

} // namespace kas::convert::sha256::v2::tests

#endif