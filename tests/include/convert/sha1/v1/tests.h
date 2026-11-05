/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_CONVERT_SHA1_V1_TESTS_H
#define KASLIBCXX_CONVERT_SHA1_V1_TESTS_H

#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "KAS/convert/sha1/v1/sha1.h"
#include "tests/tests.h"

namespace kas::convert::sha1::v1::tests {

namespace {
    using namespace kas::tests;
    using kas::convert::sha1::v1::hash;
    using kas::convert::sha1::v1::hashBinary;
    using kas::convert::sha1::v1::hashFile;

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
        const auto name = "kas_sha1_test_" + std::to_string(::getpid())
            + "_" + std::to_string(counter.fetch_add(1));
        return (std::filesystem::temp_directory_path() / name).string();
    }

    // Создаёт файл с содержимым; возвращает путь.
    TempFileGuard makeTempFile(std::string_view content)
    {
        TempFileGuard g { makeTempFilePath() };
        std::ofstream out(g.path, std::ios::binary);
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        return g;
    }

    // ------------------------------------------------------------------
    // Хелперы
    // ------------------------------------------------------------------

    [[nodiscard]] std::string hex(const std::array<std::uint8_t, 20>& bytes)
    {
        static constexpr char HEX[] = "0123456789abcdef";
        std::string out;
        out.reserve(bytes.size() * 2);
        for (std::uint8_t b : bytes) {
            out.push_back(HEX[b >> 4]);
            out.push_back(HEX[b & 0x0F]);
        }
        return out;
    }

    // ==================================================================
    //  hash() - базовые векторы
    // ==================================================================

    // --- 1. hash - пустая строка -------------------------------------
    inline void hashEmpty(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx,
            hash("") == "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    }

    // --- 2. hash - один символ ---------------------------------------
    inline void hashSingleChar(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx,
            hash("a") == "86f7e437faa5a7fce15d1ddcb9eaeaea377667b8");
    }

    // --- 3. hash - классический "abc" --------------------------------
    inline void hashAbc(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx,
            hash("abc") == "a9993e364706816aba3e25717850c26c9cd0d89d");
    }

    // --- 4. hash - RFC 3174 §7.3, вектор 1 ---------------------------
    inline void hashRfcVector1(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx,
            hash("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")
                == "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
    }

    // --- 5. hash - RFC 3174 §7.3, вектор 2 ---------------------------
    inline void hashRfcVector2(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx,
            hash("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
                 "hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu")
                == "a49b2446a02c645bf419f995b67091253a04a259");
    }

    // --- 6. hash - "Hello, World!" -----------------------------------
    inline void hashCommonPhrase(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx,
            hash("Hello, World!")
                == "0a0a9f2a6772942557ab5355d76af442f8f65e01");
    }

    // --- 7. hash - строка со встроенным '\0' -------------------------
    inline void hashEmbeddedNull(TestContext& ctx)
    {
        // hash принимает std::string by value - размер сохраняется,
        // '\0' внутри не обрезает строку.
        std::string s("ab\0cd", 5);
        KAS_TEST_CHECK_IMPL(ctx,
            hash(s) == "cf76bf81edbf067731a98c6b7d2eb28824d38027");
    }

    // ==================================================================
    //  hash() - границы блоков (55/56/63/64/65 байт и т.п.)
    // ==================================================================

    // --- 8. hash - 55 байт 'a' (граница паддинга) --------------------
    inline void hashLen55(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx,
            hash(std::string(55, 'a'))
                == "c1c8bbdc22796e28c0e15163d20899b65621d65a");
    }

    // --- 9. hash - 56 байт 'a' (паддинг уходит в новый блок) ---------
    inline void hashLen56(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx,
            hash(std::string(56, 'a'))
                == "c2db330f6083854c99d4b5bfb6e8f29f201be699");
    }

    // --- 10. hash - 63 байта 'a' ------------------------------------
    inline void hashLen63(TestContext& ctx)
    {
        const std::string h = hash(std::string(63, 'a'));
        KAS_TEST_CHECK_IMPL(ctx, h.size() == 40u);
        KAS_TEST_CHECK_IMPL(ctx, h == hash(std::string(63, 'a'))); // детерминизм
    }

    // --- 11. hash - 64 байта 'a' (ровно один блок) -------------------
    inline void hashLen64(TestContext& ctx)
    {
        const std::string h = hash(std::string(64, 'a'));
        KAS_TEST_CHECK_IMPL(ctx, h.size() == 40u);
    }

    // --- 12. hash - 65 байт 'a' (блок + 1) --------------------------
    inline void hashLen65(TestContext& ctx)
    {
        const std::string h = hash(std::string(65, 'a'));
        KAS_TEST_CHECK_IMPL(ctx, h.size() == 40u);
    }

    // --- 13. hash - 119/120/127/128/129 байт ------------------------
    inline void hashBlockBoundaries(TestContext& ctx)
    {
        for (std::size_t len : { 55u, 56u, 63u, 64u, 65u,
                 119u, 120u, 127u, 128u, 129u }) {
            const std::string s(len, 'x');
            const std::string h = hash(s);
            KAS_TEST_CHECK_IMPL(ctx, h.size() == 40u);
            KAS_TEST_CHECK_IMPL(ctx, h == hash(s)); // идемпотентность
        }
    }

    // ==================================================================
    //  hash() - свойства
    // ==================================================================

    // --- 14. hash - 40 символов, lowercase hex ----------------------
    inline void hashFormat(TestContext& ctx)
    {
        const std::string h = hash("format check");
        KAS_TEST_CHECK_IMPL(ctx, h.size() == 40u);
        for (char c : h)
            KAS_TEST_CHECK_IMPL(ctx, (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
    }

    // --- 15. hash - разные входы -> разные хэши ----------------------
    inline void hashDistinct(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, hash("abc") != hash("abd"));
        KAS_TEST_CHECK_IMPL(ctx, hash("a") != hash("A"));
        KAS_TEST_CHECK_IMPL(ctx, hash("") != hash(" "));
    }

    // --- 16. hash - 1000 байт 'a' -----------------------------------
    inline void hashLarge(TestContext& ctx)
    {
        // Эталон: hashlib.sha1(b'a'*1000).hexdigest()
        KAS_TEST_CHECK_IMPL(ctx,
            hash(std::string(1000, 'a'))
                == "291e9a6c66994949b57ba5e650361e98fc36b1ba");
    }

    // ==================================================================
    //  hashBinary() - согласованность с hash()
    // ==================================================================

    // --- 17. hashBinary - hex совпадает с hash() --------------------
    inline void hashBinaryMatchesHash(TestContext& ctx)
    {
        const std::vector<std::string> lines {
            "", "a", "abc", "Hello, World!",
            std::string(64, 'a'), std::string(1000, 'a')
        };
        for (std::string_view s : lines) {
            KAS_TEST_CHECK_IMPL(ctx, hex(hashBinary(std::string(s))) == hash(std::string(s)));
        }
    }

    // --- 18. hashBinary - размер ровно 20 ---------------------------
    inline void hashBinarySize(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, hashBinary("").size() == 20u);
        KAS_TEST_CHECK_IMPL(ctx, hashBinary("abc").size() == 20u);
    }

    // --- 19. hashBinary - эталонные байты для "abc" -----------------
    inline void hashBinaryAbc(TestContext& ctx)
    {
        // SHA-1("abc") = a9 99 3e 36 47 06 81 6a ba 3e
        //                25 71 78 50 c2 6c 9c d0 d8 9d
        const std::array<std::uint8_t, 20> expected {
            0xa9, 0x99, 0x3e, 0x36, 0x47, 0x06, 0x81, 0x6a, 0xba, 0x3e,
            0x25, 0x71, 0x78, 0x50, 0xc2, 0x6c, 0x9c, 0xd0, 0xd8, 0x9d
        };
        KAS_TEST_CHECK_IMPL(ctx, hashBinary("abc") == expected);
    }

    // --- 20. hashBinary - big-endian, а не little-endian ------------
    inline void hashBinaryBigEndian(TestContext& ctx)
    {
        // Для "abc" первый байт - 0xa9 (старший байт h0 = 0xa9993e36).
        // Если бы реализация писала little-endian, первый байт был бы 0x36.
        const auto bytes = hashBinary("abc");
        KAS_TEST_CHECK_IMPL(ctx, bytes[0] == 0xa9);
        KAS_TEST_CHECK_IMPL(ctx, bytes[3] == 0x36);
    }

    // --- 21. hashBinary - пустая строка -----------------------------
    inline void hashBinaryEmpty(TestContext& ctx)
    {
        const std::array<std::uint8_t, 20> expected {
            0xda, 0x39, 0xa3, 0xee, 0x5e, 0x6b, 0x4b, 0x0d, 0x32, 0x55,
            0xbf, 0xef, 0x95, 0x60, 0x18, 0x90, 0xaf, 0xd8, 0x07, 0x09
        };
        KAS_TEST_CHECK_IMPL(ctx, hashBinary("") == expected);
    }

    // ==================================================================
    //  Свойства / стресс
    // ==================================================================

    // --- 22. Детерминированность при повторных вызовах --------------
    inline void deterministic(TestContext& ctx)
    {
        const std::string s = "repeatable input";
        const std::string first = hash(s);
        for (int i = 0; i < 50; ++i)
            KAS_TEST_CHECK_IMPL(ctx, hash(s) == first);
    }

    // --- 23. hash() не портит вход (передаётся by value) -------------
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

    // --- 24. hashFile - совпадает с hash() --------------------------
    inline void hashFileMatchesHash(TestContext& ctx)
    {
        const auto tf = makeTempFile("Hello, World!");
        const auto file_hash = hashFile(tf.path);
        KAS_TEST_CHECK_IMPL(ctx, file_hash.has_value());
        if (file_hash)
            KAS_TEST_CHECK_IMPL(ctx, *file_hash == hash("Hello, World!"));
    }

    // --- 25. hashFile - пустой файл --------------------------------
    inline void hashFileEmpty(TestContext& ctx)
    {
        const auto tf = makeTempFile("");
        const auto file_hash = hashFile(tf.path);
        KAS_TEST_CHECK_IMPL(ctx, file_hash.has_value());
        if (file_hash)
            KAS_TEST_CHECK_IMPL(ctx, *file_hash == hash(""));
    }

    // --- 26. hashFile - несуществующий файл -> nullopt -------------
    inline void hashFileMissing(TestContext& ctx)
    {
        const auto path = std::filesystem::temp_directory_path()
            / ("kas_sha1_missing_" + std::to_string(::getpid()));
        const auto file_hash = hashFile(path);
        KAS_TEST_CHECK_IMPL(ctx, !file_hash.has_value());
    }

    // --- 27. hashFile - большой файл (1000 байт) -------------------
    inline void hashFileLarge(TestContext& ctx)
    {
        const std::string content(1000, 'a');
        const auto tf = makeTempFile(content);
        const auto file_hash = hashFile(tf.path);
        KAS_TEST_CHECK_IMPL(ctx, file_hash.has_value());
        if (file_hash)
            KAS_TEST_CHECK_IMPL(ctx, *file_hash == hash(content));
    }

    // --- 28. hashFile - бинарные данные с '\0' ---------------------
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

    // --- 29. hashFile - [0, 3) --------------------------------------
    inline void hashFileRangePrefix(TestContext& ctx)
    {
        const auto tf = makeTempFile("Hello, World!");
        const auto file_hash = hashFile(tf.path, 0, 5);
        KAS_TEST_CHECK_IMPL(ctx, file_hash.has_value());
        if (file_hash)
            KAS_TEST_CHECK_IMPL(ctx, *file_hash == hash("Hello"));
    }

    // --- 30. hashFile - [7, 12) -------------------------------------
    inline void hashFileRangeSuffix(TestContext& ctx)
    {
        const auto tf = makeTempFile("Hello, World!");
        const auto file_hash = hashFile(tf.path, 7, 13); // "World!"
        KAS_TEST_CHECK_IMPL(ctx, file_hash.has_value());
        if (file_hash)
            KAS_TEST_CHECK_IMPL(ctx, *file_hash == hash("World!"));
    }

    // --- 31. hashFile - полный диапазон == hashFile без диапазона ---
    inline void hashFileRangeFull(TestContext& ctx)
    {
        const std::string content = "Hello, World!";
        const auto tf = makeTempFile(content);
        const auto file_hash = hashFile(tf.path, 0, content.size());
        KAS_TEST_CHECK_IMPL(ctx, file_hash.has_value());
        if (file_hash)
            KAS_TEST_CHECK_IMPL(ctx, *file_hash == hash(content));
    }

    // --- 32. hashFile - [start, start) -> хеш пустой строки ---------
    inline void hashFileRangeEmpty(TestContext& ctx)
    {
        const auto tf = makeTempFile("Hello");
        const auto file_hash = hashFile(tf.path, 2, 2);
        KAS_TEST_CHECK_IMPL(ctx, file_hash.has_value());
        if (file_hash)
            KAS_TEST_CHECK_IMPL(ctx, *file_hash == hash(""));
    }

    // --- 33. hashFile - start > end -> nullopt ----------------------
    inline void hashFileRangeStartAfterEnd(TestContext& ctx)
    {
        const auto tf = makeTempFile("Hello");
        KAS_TEST_CHECK_IMPL(ctx, !hashFile(tf.path, 3, 1).has_value());
    }

    // --- 34. hashFile - end > file_size -> nullopt ------------------
    inline void hashFileRangeBeyondEof(TestContext& ctx)
    {
        const auto tf = makeTempFile("Hello"); // 5 байт
        KAS_TEST_CHECK_IMPL(ctx, !hashFile(tf.path, 0, 6).has_value());
        KAS_TEST_CHECK_IMPL(ctx, !hashFile(tf.path, 0, 100).has_value());
    }

    // --- 35. hashFile - [0, 0) == пустой файл ----------------------
    inline void hashFileRangeZeroZero(TestContext& ctx)
    {
        const auto tf = makeTempFile("Hello");
        const auto file_hash = hashFile(tf.path, 0, 0);
        KAS_TEST_CHECK_IMPL(ctx, file_hash.has_value());
        if (file_hash)
            KAS_TEST_CHECK_IMPL(ctx, *file_hash == hash(""));
    }

    // --- 36. hashFile - бинарные данные в диапазоне ------------------
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

    // hash - базовые
    hashEmpty(ctx);
    hashSingleChar(ctx);
    hashAbc(ctx);
    hashRfcVector1(ctx);
    hashRfcVector2(ctx);
    hashCommonPhrase(ctx);
    hashEmbeddedNull(ctx);

    // hash - границы блоков
    hashLen55(ctx);
    hashLen56(ctx);
    hashLen63(ctx);
    hashLen64(ctx);
    hashLen65(ctx);
    hashBlockBoundaries(ctx);

    // hash - свойства
    hashFormat(ctx);
    hashDistinct(ctx);
    hashLarge(ctx);

    // hashBinary
    hashBinaryMatchesHash(ctx);
    hashBinarySize(ctx);
    hashBinaryAbc(ctx);
    hashBinaryBigEndian(ctx);
    hashBinaryEmpty(ctx);

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

    // свойства
    deterministic(ctx);
    inputNotMutated(ctx);

    return printSummary("kas::convert::sha1::v1::tests", ctx);
}

} // namespace kas::convert::sha1::v1::tests

#endif