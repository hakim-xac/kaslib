/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_NET_HTTP_HTTP_RANGE_TESTS_H
#define KASLIBCXX_NET_HTTP_HTTP_RANGE_TESTS_H

#include "KAS/net/http/HttpRange.h"
#include "tests/tests.h"

#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace kas::net::http::http_range::tests {

namespace {
    using namespace kas::tests;
    using kas::net::http::buildMultipartByteranges;
    using kas::net::http::ByteRange;
    using kas::net::http::makeBoundary;
    using kas::net::http::parseRangeHeader;
    using kas::net::http::RangeSpec;
    using kas::net::http::resolveByteRange;
    using kas::net::http::resolveByteRanges;

    // ============================================================
    // parseRangeHeader - valid headers
    // ============================================================

    /// Test 1: single range "bytes=0-99" is parsed as start=0, end=99.
    inline void testParseSingle(TestContext& ctx)
    {
        auto s = parseRangeHeader("bytes=0-99");
        KAS_TEST_CHECK_IMPL(ctx, s.has_value());
        if (!s)
            return;
        KAS_TEST_CHECK_IMPL(ctx, s->valid);
        KAS_TEST_CHECK_IMPL(ctx, s->unit == "bytes");
        KAS_TEST_CHECK_IMPL(ctx, s->ranges.size() == 1);
        KAS_TEST_CHECK_IMPL(ctx, s->ranges[0].start == 0);
        KAS_TEST_CHECK_IMPL(ctx, s->ranges[0].end == 99);
    }

    /// Test 2: open-ended range "bytes=200-" has start=200 and no end.
    inline void testParseOpenEnd(TestContext& ctx)
    {
        auto s = parseRangeHeader("bytes=200-");
        KAS_TEST_CHECK_IMPL(ctx, s.has_value());
        if (!s)
            return;
        KAS_TEST_CHECK_IMPL(ctx, s->valid);
        KAS_TEST_CHECK_IMPL(ctx, s->ranges.size() == 1);
        KAS_TEST_CHECK_IMPL(ctx, s->ranges[0].start == 200);
        KAS_TEST_CHECK_IMPL(ctx, !s->ranges[0].end.has_value());
    }

    /// Test 3: suffix range "bytes=-500" has no start and end=500.
    inline void testParseSuffix(TestContext& ctx)
    {
        auto s = parseRangeHeader("bytes=-500");
        KAS_TEST_CHECK_IMPL(ctx, s.has_value());
        if (!s)
            return;
        KAS_TEST_CHECK_IMPL(ctx, s->valid);
        KAS_TEST_CHECK_IMPL(ctx, s->ranges.size() == 1);
        KAS_TEST_CHECK_IMPL(ctx, !s->ranges[0].start.has_value());
        KAS_TEST_CHECK_IMPL(ctx, s->ranges[0].end == 500);
    }

    /// Test 4: multiple ranges "bytes=0-99,200-,300-400" parse into 3 entries.
    inline void testParseMultiple(TestContext& ctx)
    {
        auto s = parseRangeHeader("bytes=0-99,200-,300-400");
        KAS_TEST_CHECK_IMPL(ctx, s.has_value());
        if (!s)
            return;
        KAS_TEST_CHECK_IMPL(ctx, s->valid);
        KAS_TEST_CHECK_IMPL(ctx, s->ranges.size() == 3);
        KAS_TEST_CHECK_IMPL(ctx, s->ranges[0].start == 0 && s->ranges[0].end == 99);
        KAS_TEST_CHECK_IMPL(ctx, s->ranges[1].start == 200 && !s->ranges[1].end);
        KAS_TEST_CHECK_IMPL(ctx, s->ranges[2].start == 300 && s->ranges[2].end == 400);
    }

    /// Test 5: optional whitespace around range parts is tolerated.
    inline void testParseOws(TestContext& ctx)
    {
        auto s = parseRangeHeader("bytes=0-99, 200-299");
        KAS_TEST_CHECK_IMPL(ctx, s.has_value());
        if (!s)
            return;
        KAS_TEST_CHECK_IMPL(ctx, s->valid);
        KAS_TEST_CHECK_IMPL(ctx, s->ranges.size() == 2);
    }

    /// Test 6: single-byte range "bytes=5-5" yields start=end=5.
    inline void testParseSingleByte(TestContext& ctx)
    {
        auto s = parseRangeHeader("bytes=5-5");
        KAS_TEST_CHECK_IMPL(ctx, s.has_value());
        if (!s)
            return;
        KAS_TEST_CHECK_IMPL(ctx, s->valid);
        KAS_TEST_CHECK_IMPL(ctx, s->ranges.size() == 1);
        KAS_TEST_CHECK_IMPL(ctx, s->ranges[0].start == 5);
        KAS_TEST_CHECK_IMPL(ctx, s->ranges[0].end == 5);
    }

    // ============================================================
    // parseRangeHeader - invalid headers
    // ============================================================

    /// Test 7: empty string yields nullopt (no '=').
    inline void testParseEmpty(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, !parseRangeHeader("").has_value());
    }

    /// Test 8: unit other than "bytes" (including different case) yields nullopt.
    inline void testParseWrongUnit(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, !parseRangeHeader("items=0-1").has_value());
        KAS_TEST_CHECK_IMPL(ctx, !parseRangeHeader("Bytes=0-1").has_value());
    }

    /// Test 9: "bytes=" with no ranges yields a spec with valid == false.
    inline void testParseNoRanges(TestContext& ctx)
    {
        auto s = parseRangeHeader("bytes=");
        KAS_TEST_CHECK_IMPL(ctx, s.has_value());
        KAS_TEST_CHECK_IMPL(ctx, !s->valid);
    }

    /// Test 10: "bytes=-" (neither start nor end) yields valid == false.
    inline void testParseBareDash(TestContext& ctx)
    {
        auto s = parseRangeHeader("bytes=-");
        KAS_TEST_CHECK_IMPL(ctx, s.has_value());
        KAS_TEST_CHECK_IMPL(ctx, !s->valid);
    }

    /// Test 11: "bytes=5-3" (start > end) yields valid == false.
    inline void testParseReversed(TestContext& ctx)
    {
        auto s = parseRangeHeader("bytes=5-3");
        KAS_TEST_CHECK_IMPL(ctx, s.has_value());
        KAS_TEST_CHECK_IMPL(ctx, !s->valid);
    }

    /// Test 12: "bytes=abc-1" (non-numeric) yields valid == false.
    inline void testParseNonNumeric(TestContext& ctx)
    {
        auto s = parseRangeHeader("bytes=abc-1");
        KAS_TEST_CHECK_IMPL(ctx, s.has_value());
        KAS_TEST_CHECK_IMPL(ctx, !s->valid);
    }

    /// Test 13: trailing empty part "bytes=0-99," yields valid == false.
    inline void testParseTrailingComma(TestContext& ctx)
    {
        auto s = parseRangeHeader("bytes=0-99,");
        KAS_TEST_CHECK_IMPL(ctx, s.has_value());
        KAS_TEST_CHECK_IMPL(ctx, !s->valid);
    }

    /// Test 14: empty part in the middle "bytes=0-99,,200-299" yields valid == false.
    inline void testParseDoubleComma(TestContext& ctx)
    {
        auto s = parseRangeHeader("bytes=0-99,,200-299");
        KAS_TEST_CHECK_IMPL(ctx, s.has_value());
        KAS_TEST_CHECK_IMPL(ctx, !s->valid);
    }

    // ============================================================
    // resolveByteRange - single range
    // ============================================================

    /// Test 15: "0-99" with total=1000 resolves to (0, 100).
    inline void testResolveInclusive(TestContext& ctx)
    {
        ByteRange r { 0, 99 };
        auto res = resolveByteRange(r, 1000);
        KAS_TEST_CHECK_IMPL(ctx, res.has_value());
        KAS_TEST_CHECK_IMPL(ctx, res->first == 0);
        KAS_TEST_CHECK_IMPL(ctx, res->second == 100);
    }

    /// Test 16: "900-" with total=1000 resolves to (900, 100).
    inline void testResolveOpenEnd(TestContext& ctx)
    {
        ByteRange r { 900, std::nullopt };
        auto res = resolveByteRange(r, 1000);
        KAS_TEST_CHECK_IMPL(ctx, res.has_value());
        KAS_TEST_CHECK_IMPL(ctx, res->first == 900);
        KAS_TEST_CHECK_IMPL(ctx, res->second == 100);
    }

    /// Test 17: "-100" with total=1000 resolves to (900, 100).
    inline void testResolveSuffix(TestContext& ctx)
    {
        ByteRange r { std::nullopt, 100 };
        auto res = resolveByteRange(r, 1000);
        KAS_TEST_CHECK_IMPL(ctx, res.has_value());
        KAS_TEST_CHECK_IMPL(ctx, res->first == 900);
        KAS_TEST_CHECK_IMPL(ctx, res->second == 100);
    }

    /// Test 18: suffix larger than the body "-2000" with total=1000 yields the whole body.
    inline void testResolveSuffixTooBig(TestContext& ctx)
    {
        ByteRange r { std::nullopt, 2000 };
        auto res = resolveByteRange(r, 1000);
        KAS_TEST_CHECK_IMPL(ctx, res.has_value());
        KAS_TEST_CHECK_IMPL(ctx, res->first == 0);
        KAS_TEST_CHECK_IMPL(ctx, res->second == 1000);
    }

    /// Test 19: start equal to total "1000-" with total=1000 yields nullopt.
    inline void testResolveStartAtTotal(TestContext& ctx)
    {
        ByteRange r { 1000, std::nullopt };
        KAS_TEST_CHECK_IMPL(ctx, !resolveByteRange(r, 1000).has_value());
    }

    /// Test 20: end beyond total "0-2000" with total=1000 is clamped to the body size.
    inline void testResolveEndTooBig(TestContext& ctx)
    {
        ByteRange r { 0, 2000 };
        auto res = resolveByteRange(r, 1000);
        KAS_TEST_CHECK_IMPL(ctx, res.has_value());
        KAS_TEST_CHECK_IMPL(ctx, res->first == 0);
        KAS_TEST_CHECK_IMPL(ctx, res->second == 1000);
    }

    /// Test 21: total=0 yields nullopt.
    inline void testResolveEmptyBody(TestContext& ctx)
    {
        ByteRange r { 0, 99 };
        KAS_TEST_CHECK_IMPL(ctx, !resolveByteRange(r, 0).has_value());
    }

    /// Test 22: suffix zero "-0" yields nullopt.
    inline void testResolveSuffixZero(TestContext& ctx)
    {
        ByteRange r { std::nullopt, 0 };
        KAS_TEST_CHECK_IMPL(ctx, !resolveByteRange(r, 1000).has_value());
    }

    /// Test 23: "999-999" with total=1000 resolves to (999, 1).
    inline void testResolveLastByte(TestContext& ctx)
    {
        ByteRange r { 999, 999 };
        auto res = resolveByteRange(r, 1000);
        KAS_TEST_CHECK_IMPL(ctx, res.has_value());
        KAS_TEST_CHECK_IMPL(ctx, res->first == 999);
        KAS_TEST_CHECK_IMPL(ctx, res->second == 1);
    }

    // ============================================================
    // resolveByteRanges - sets of ranges
    // ============================================================

    /// Test 24: a single range yields a one-element vector.
    inline void testResolveSetSingle(TestContext& ctx)
    {
        auto s = parseRangeHeader("bytes=0-99");
        KAS_TEST_CHECK_IMPL(ctx, s.has_value());
        if (!s)
            return;

        auto res = resolveByteRanges(*s, 1000);
        KAS_TEST_CHECK_IMPL(ctx, res.has_value());
        KAS_TEST_CHECK_IMPL(ctx, res->size() == 1);
        KAS_TEST_CHECK_IMPL(ctx, (*res)[0].first == 0);
        KAS_TEST_CHECK_IMPL(ctx, (*res)[0].second == 100);
    }

    /// Test 25: two ranges yield a two-element vector in the original order.
    inline void testResolveSetTwo(TestContext& ctx)
    {
        auto s = parseRangeHeader("bytes=0-99,200-299");
        KAS_TEST_CHECK_IMPL(ctx, s.has_value());
        if (!s)
            return;

        auto res = resolveByteRanges(*s, 1000);
        KAS_TEST_CHECK_IMPL(ctx, res.has_value());
        KAS_TEST_CHECK_IMPL(ctx, res->size() == 2);
        KAS_TEST_CHECK_IMPL(ctx, (*res)[0].first == 0);
        KAS_TEST_CHECK_IMPL(ctx, (*res)[0].second == 100);
        KAS_TEST_CHECK_IMPL(ctx, (*res)[1].first == 200);
        KAS_TEST_CHECK_IMPL(ctx, (*res)[1].second == 100);
    }

    /// Test 26: one unsatisfiable and one satisfiable range keep only the latter.
    inline void testResolveSetPartial(TestContext& ctx)
    {
        auto s = parseRangeHeader("bytes=2000-,0-99");
        KAS_TEST_CHECK_IMPL(ctx, s.has_value());
        if (!s)
            return;

        auto res = resolveByteRanges(*s, 1000);
        KAS_TEST_CHECK_IMPL(ctx, res.has_value());
        KAS_TEST_CHECK_IMPL(ctx, res->size() == 1);
        KAS_TEST_CHECK_IMPL(ctx, (*res)[0].first == 0);
        KAS_TEST_CHECK_IMPL(ctx, (*res)[0].second == 100);
    }

    /// Test 27: all ranges unsatisfiable yields nullopt (server responds 416).
    inline void testResolveSetNone(TestContext& ctx)
    {
        auto s = parseRangeHeader("bytes=2000-,3000-");
        KAS_TEST_CHECK_IMPL(ctx, s.has_value());
        if (!s)
            return;

        KAS_TEST_CHECK_IMPL(ctx, !resolveByteRanges(*s, 1000).has_value());
    }

    /// Test 28: a range covering the whole body yields { {0, total} }.
    /// The caller (HttpServer) decides between 200 and 206: if ranges is
    /// { {0, total} }, the server may respond with the full body (200).
    inline void testResolveSetFull(TestContext& ctx)
    {
        auto s = parseRangeHeader("bytes=0-999");
        KAS_TEST_CHECK_IMPL(ctx, s.has_value());
        if (!s)
            return;

        auto res = resolveByteRanges(*s, 1000);
        KAS_TEST_CHECK_IMPL(ctx, res.has_value());
        if (!res)
            return;

        KAS_TEST_CHECK_IMPL(ctx, res->size() == 1);
        KAS_TEST_CHECK_IMPL(ctx, (*res)[0].first == 0);
        KAS_TEST_CHECK_IMPL(ctx, (*res)[0].second == 1000);
    }

    /// Test 29: total=0 yields nullopt.
    inline void testResolveSetEmpty(TestContext& ctx)
    {
        auto s = parseRangeHeader("bytes=0-99");
        KAS_TEST_CHECK_IMPL(ctx, s.has_value());
        if (!s)
            return;

        KAS_TEST_CHECK_IMPL(ctx, !resolveByteRanges(*s, 0).has_value());
    }

    /// Test 30: a spec with valid == false yields nullopt.
    inline void testResolveSetInvalidSpec(TestContext& ctx)
    {
        RangeSpec spec;
        spec.valid = false;
        KAS_TEST_CHECK_IMPL(ctx, !resolveByteRanges(spec, 1000).has_value());
    }

    // ============================================================
    // buildMultipartByteranges
    // ============================================================

    /// Test 31: basic multipart structure (boundaries, headers, payloads, closing boundary).
    inline void testMultipartBasic(TestContext& ctx)
    {
        const std::string body = "0123456789";
        const std::vector<std::pair<std::size_t, std::size_t>> ranges {
            { 0, 3 }, // "012"
            { 7, 3 } // "789"
        };

        const auto mp = buildMultipartByteranges(
            "BOUND", body, ranges, "text/plain", body.size());

        KAS_TEST_CHECK_IMPL(ctx, mp.find("--BOUND\r\n") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, mp.find("Content-Type: text/plain\r\n") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, mp.find("Content-Range: bytes 0-2/10\r\n") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, mp.find("Content-Range: bytes 7-9/10\r\n") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, mp.find("012") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, mp.find("789") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, mp.find("--BOUND--\r\n") != std::string::npos);
    }

    /// Test 32: empty content_type omits the Content-Type line.
    inline void testMultipartNoContentType(TestContext& ctx)
    {
        const std::string body = "abcdef";
        const std::vector<std::pair<std::size_t, std::size_t>> ranges { { 0, 2 } };

        const auto mp = buildMultipartByteranges(
            "B", body, ranges, "", body.size());

        KAS_TEST_CHECK_IMPL(ctx, mp.find("Content-Type:") == std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, mp.find("Content-Range: bytes 0-1/6\r\n") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, mp.find("ab") != std::string::npos);
    }

    /// Test 33: a single range in multipart.
    inline void testMultipartSingleRange(TestContext& ctx)
    {
        const std::string body = "hello";
        const std::vector<std::pair<std::size_t, std::size_t>> ranges { { 1, 3 } };

        const auto mp = buildMultipartByteranges(
            "X", body, ranges, "text/plain", body.size());

        KAS_TEST_CHECK_IMPL(ctx, mp.find("Content-Range: bytes 1-3/5\r\n") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, mp.find("ell") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, mp.find("--X--\r\n") != std::string::npos);
    }

    /// Test 34: an empty ranges vector produces only the closing boundary.
    inline void testMultipartEmpty(TestContext& ctx)
    {
        const std::vector<std::pair<std::size_t, std::size_t>> ranges { };
        const auto mp = buildMultipartByteranges(
            "B", "abc", ranges, "text/plain", 3);

        KAS_TEST_CHECK_IMPL(ctx, mp == "--B--\r\n");
    }

    // ============================================================
    // makeBoundary
    // ============================================================

    /// Test 35: boundary is a 32-character lowercase hex string without CR/LF.
    inline void testMakeBoundary(TestContext& ctx)
    {
        const auto b = makeBoundary();
        KAS_TEST_CHECK_IMPL(ctx, b.size() == 32);

        for (char c : b) {
            const bool is_hex = (c >= '0' && c <= '9')
                || (c >= 'a' && c <= 'f');
            KAS_TEST_CHECK_IMPL(ctx, is_hex);
        }
    }

    /// Test 36: two calls produce different boundary values (probabilistically).
    inline void testMakeBoundaryUnique(TestContext& ctx)
    {
        const auto b1 = makeBoundary();
        const auto b2 = makeBoundary();
        KAS_TEST_CHECK_IMPL(ctx, b1 != b2);
    }

    // ============================================================
    // compile-time checks
    // ============================================================

    /// Test 37: static_assert checks of the public function return types.
    inline void testTypes(TestContext& ctx)
    {
        (void)ctx;

        static_assert(std::is_same_v<
            decltype(parseRangeHeader(std::string_view { })),
            std::optional<RangeSpec>>);

        static_assert(std::is_same_v<
            decltype(resolveByteRange(ByteRange { }, 0)),
            std::optional<std::pair<std::size_t, std::size_t>>>);

        static_assert(std::is_same_v<
            decltype(resolveByteRanges(RangeSpec { }, 0)),
            std::optional<std::vector<std::pair<std::size_t, std::size_t>>>>);

        static_assert(std::is_same_v<
            decltype(buildMultipartByteranges(
                std::string_view { },
                std::string_view { },
                std::vector<std::pair<std::size_t, std::size_t>> { },
                std::string_view { },
                0)),
            std::string>);

        static_assert(std::is_same_v<decltype(makeBoundary()), std::string>);
    }

} // namespace

// ============================================================
// Entry point
// ============================================================

/**
 * @brief Runs all HttpRange tests.
 *
 * Executes every test case defined in this header against a shared
 * TestContext and prints a summary for the "kas::net::http::http_range::tests"
 * suite.
 */
inline bool runTests()
{
    TestContext ctx;

    // parseRangeHeader - valid
    testParseSingle(ctx);
    testParseOpenEnd(ctx);
    testParseSuffix(ctx);
    testParseMultiple(ctx);
    testParseOws(ctx);
    testParseSingleByte(ctx);

    // parseRangeHeader - invalid
    testParseEmpty(ctx);
    testParseWrongUnit(ctx);
    testParseNoRanges(ctx);
    testParseBareDash(ctx);
    testParseReversed(ctx);
    testParseNonNumeric(ctx);
    testParseTrailingComma(ctx);
    testParseDoubleComma(ctx);

    // resolveByteRange - single
    testResolveInclusive(ctx);
    testResolveOpenEnd(ctx);
    testResolveSuffix(ctx);
    testResolveSuffixTooBig(ctx);
    testResolveStartAtTotal(ctx);
    testResolveEndTooBig(ctx);
    testResolveEmptyBody(ctx);
    testResolveSuffixZero(ctx);
    testResolveLastByte(ctx);

    // resolveByteRanges - sets
    testResolveSetSingle(ctx);
    testResolveSetTwo(ctx);
    testResolveSetPartial(ctx);
    testResolveSetNone(ctx);
    testResolveSetFull(ctx);
    testResolveSetEmpty(ctx);
    testResolveSetInvalidSpec(ctx);

    // buildMultipartByteranges
    testMultipartBasic(ctx);
    testMultipartNoContentType(ctx);
    testMultipartSingleRange(ctx);
    testMultipartEmpty(ctx);

    // makeBoundary
    testMakeBoundary(ctx);
    testMakeBoundaryUnique(ctx);

    // compile-time
    testTypes(ctx);

    return printSummary("kas::net::http::http_range::tests", ctx);
}

} // namespace kas::net::http::http_range::tests

#endif // KASLIBCXX_NET_HTTP_HTTP_RANGE_TESTS_H