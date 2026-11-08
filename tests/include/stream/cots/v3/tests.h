/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_STREAM_COTS_V3_TESTS_H
#define KASLIBCXX_STREAM_COTS_V3_TESTS_H

#include <algorithm>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <syncstream>
#include <thread>
#include <vector>

#include "KAS/stream/cots/v3/ColorOutToStream.h"
#include "tests/tests.h"

/**
 * @file ColorOutToStreamTests.h
 * @brief Unit tests for kas::stream::cots::v3::ColorOutToStream.
 *
 * The tests cover:
 *  - constructor and getOut();
 *  - setDelimiter() in both lvalue and rvalue chains;
 *  - whitespace helpers: ln(), space(), tab();
 *  - color selection via setColor<T>() (Red, Green);
 *  - color property selection via setColorProperty<T>() (Bold);
 *  - combination of color and property (single SGR sequence
 *    "\033[<property>;<color>m");
 *  - colorReset(), reset(), colorPropertyReset();
 *  - printWithOutColor() does not emit ANSI codes;
 *  - print() with no arguments is a no-op;
 *  - setOut() switches the underlying stream;
 *  - flush() forwards to the underlying stream;
 *  - multi-argument print() with a delimiter;
 *  - repeated setColor() emits a new code on the next print();
 *  - a multi-threaded test that serializes writes with a mutex.
 *
 * Test style:
 *  - each test is a free function taking TestContext&;
 *  - KAS_TEST_CHECK_IMPL records a failure but does not abort the test;
 *  - ANSI sequences are compared as exact strings using "\033[" to
 *    avoid depending on the ESC character literal;
 *  - rvalue chains are used where the temporary must live to the end
 *    of the full expression (ColorOutToStream(oss).print(...)).
 *
 * @note The tests assume the following ColorOutToStream contracts:
 *  - color and property codes are emitted lazily, before the first
 *    value of print(), not when setColor()/setColorProperty() is called;
 *  - colorReset() emits "\033[39m";
 *  - reset() emits "\033[0m";
 *  - colorPropertyReset() emits "\033[0m" for the property, then the
 *    color code again on the next print();
 *  - print() with no arguments does not emit color codes.
 */

namespace kas::stream::cots::v3::tests {

namespace {
    using namespace kas::tests;
    using namespace kas::stream::cots::v3;

    // --- 1. Constructor and getOut() --------------------------------
    /**
     * @brief Verifies that getOut() returns the stream passed to the
     *        constructor.
     */
    inline void ctorAndGetOut(TestContext& ctx)
    {
        std::ostringstream oss;
        ColorOutToStream c(oss);

        KAS_TEST_CHECK_IMPL(ctx, &c.getOut() == &oss);
    }

    // --- 2. setDelimiter (lvalue and rvalue) ------------------------
    /**
     * @brief Verifies setDelimiter() on an lvalue and on a temporary.
     *
     * The delimiter is inserted between values by print().
     */
    inline void delimiter(TestContext& ctx)
    {
        {
            std::ostringstream oss;
            ColorOutToStream c(oss);
            c.setDelimiter(", ").print("a", "b", "c");

            KAS_TEST_CHECK_IMPL(ctx, oss.str() == "a, b, c");
        }
        {
            std::ostringstream oss;
            ColorOutToStream { oss }.setDelimiter("|").print("x", "y");

            KAS_TEST_CHECK_IMPL(ctx, oss.str() == "x|y");
        }
    }

    // --- 3. ln(), space(), tab() ------------------------------------
    /**
     * @brief Verifies the whitespace helpers.
     *
     * tab(tabs, spaces) writes tabs * spaces spaces. The default
     * tab(1, 4) writes 4 spaces.
     */
    inline void whitespace(TestContext& ctx)
    {
        {
            std::ostringstream oss;
            ColorOutToStream(oss).print("A").ln().print("B");
            KAS_TEST_CHECK_IMPL(ctx, oss.str() == "A\nB");
        }
        {
            std::ostringstream oss;
            ColorOutToStream(oss).print("A").space().print("B");
            KAS_TEST_CHECK_IMPL(ctx, oss.str() == "A B");
        }
        {
            std::ostringstream oss;
            ColorOutToStream(oss).print("A").tab(2, 4).print("B");
            KAS_TEST_CHECK_IMPL(ctx, oss.str() == "A        B"); // 8 spaces
        }
        {
            std::ostringstream oss;
            ColorOutToStream(oss).print("A").tab().print("B"); // 4 spaces
            KAS_TEST_CHECK_IMPL(ctx, oss.str() == "A    B");
        }
    }

    // --- 4. setColor - Red ------------------------------------------
    /**
     * @brief Verifies that Red emits the SGR 31 code before the value.
     */
    inline void setColorRed(TestContext& ctx)
    {
        std::ostringstream oss;
        ColorOutToStream(oss).setColor<ColorTypes::Red>().print("hello");

        KAS_TEST_CHECK_IMPL(ctx, oss.str() == "\033[31mhello");
    }

    // --- 5. setColor - Green ----------------------------------------
    /**
     * @brief Verifies that Green emits the SGR 32 code before the value.
     */
    inline void setColorGreen(TestContext& ctx)
    {
        std::ostringstream oss;
        ColorOutToStream(oss).setColor<ColorTypes::Green>().print("hi");

        KAS_TEST_CHECK_IMPL(ctx, oss.str() == "\033[32mhi");
    }

    // --- 6. setColorProperty - Bold ---------------------------------
    /**
     * @brief Verifies that Bold emits the SGR 1 code before the value.
     */
    inline void setColorPropertyBold(TestContext& ctx)
    {
        std::ostringstream oss;
        ColorOutToStream(oss).setColorProperty<ColorProperties::Bold>().print("B");

        KAS_TEST_CHECK_IMPL(ctx, oss.str() == "\033[1mB");
    }

    // --- 7. color + property -> ESC[p;cm ----------------------------
    /**
     * @brief Verifies that color and property are combined into a
     *        single SGR sequence: "\033[<property>;<color>m".
     */
    inline void colorAndProperty(TestContext& ctx)
    {
        std::ostringstream oss;
        ColorOutToStream(oss)
            .setColorProperty<ColorProperties::Bold>()
            .setColor<ColorTypes::Red>()
            .print("X");

        KAS_TEST_CHECK_IMPL(ctx, oss.str() == "\033[1;31mX");
    }

    // --- 8. colorReset() - resets color to Default ("39") -----------
    /**
     * @brief Verifies colorReset() emits "\033[39m" (reset foreground).
     */
    inline void colorReset(TestContext& ctx)
    {
        std::ostringstream oss;
        ColorOutToStream(oss)
            .setColor<ColorTypes::Red>()
            .print("R")
            .colorReset()
            .print("D");

        KAS_TEST_CHECK_IMPL(ctx, oss.str() == "\033[31mR\033[39mD");
    }

    // --- 9. reset() - resets both color and properties --------------
    /**
     * @brief Verifies reset() emits "\033[0m" (full SGR reset).
     */
    inline void reset(TestContext& ctx)
    {
        std::ostringstream oss;
        ColorOutToStream(oss)
            .setColorProperty<ColorProperties::Bold>()
            .setColor<ColorTypes::Blue>()
            .print("B")
            .reset()
            .print("X");

        KAS_TEST_CHECK_IMPL(ctx, oss.str() == "\033[1;34mB\033[0mX");
    }

    // --- 10. colorPropertyReset() - only properties, color kept ----
    /**
     * @brief Verifies that colorPropertyReset() emits a reset for the
     *        property but keeps the color, which is re-emitted on the
     *        next print().
     */
    inline void colorPropertyReset(TestContext& ctx)
    {
        std::ostringstream oss;
        ColorOutToStream(oss)
            .setColorProperty<ColorProperties::Italic>()
            .setColor<ColorTypes::Cyan>()
            .print("I")
            .colorPropertyReset()
            .print("X");

        KAS_TEST_CHECK_IMPL(ctx, oss.str() == "\033[3;36mI\033[0m\033[36mX");
    }

    // --- 11. printWithOutColor - no ANSI ---------------------------
    /**
     * @brief Verifies that printWithOutColor() writes values without
     *        emitting color codes, and that a later print() still
     *        emits the pending color.
     */
    inline void printWithOutColor(TestContext& ctx)
    {
        std::ostringstream oss;
        ColorOutToStream(oss)
            .setColor<ColorTypes::Red>()
            .printWithOutColor("plain")
            .print("colored");

        KAS_TEST_CHECK_IMPL(ctx, oss.str() == "plain\033[31mcolored");
    }

    // --- 12. print() with no arguments - no-op ---------------------
    /**
     * @brief Pins the contract that print() with no arguments does not
     *        emit color codes.
     */
    inline void printEmpty(TestContext& ctx)
    {
        std::ostringstream oss;
        ColorOutToStream(oss).print().print("A");

        KAS_TEST_CHECK_IMPL(ctx, oss.str() == "A");
    }

    // --- 13. setOut - switches the stream --------------------------
    /**
     * @brief Verifies that setOut() redirects subsequent writes to the
     *        new stream while keeping the earlier output in the old
     *        stream.
     */
    inline void setOut(TestContext& ctx)
    {
        std::ostringstream a, b;
        ColorOutToStream c(a);
        c.print("first").setOut(b).print("second");

        KAS_TEST_CHECK_IMPL(ctx, a.str() == "first");
        KAS_TEST_CHECK_IMPL(ctx, b.str() == "second");
    }

    // --- 14. flush - forwards to the stream ------------------------
    /**
     * @brief Verifies that flush() forwards to the underlying stream.
     *
     * ostringstream has no observable flush side effect, so the test
     * only checks that data written before flush() is present.
     */
    inline void flush(TestContext& ctx)
    {
        std::ostringstream oss;
        ColorOutToStream c(oss);
        c.print("x").flush();

        KAS_TEST_CHECK_IMPL(ctx, oss.str() == "x");
    }

    // --- 15. multi-argument print with a delimiter -----------------
    /**
     * @brief Verifies multi-argument print() with mixed value types
     *        and a custom delimiter.
     */
    inline void printMultipleArgs(TestContext& ctx)
    {
        std::ostringstream oss;
        ColorOutToStream(oss)
            .setDelimiter(" ")
            .setColor<ColorTypes::Yellow>()
            .print("a", 1, 2.5, "b");

        KAS_TEST_CHECK_IMPL(ctx, oss.str() == "\033[33ma 1 2.5 b");
    }

    // --- 16. repeated setColor - new code on next print ------------
    /**
     * @brief Verifies that selecting a new color after a print()
     *        emits the new code before the next value.
     */
    inline void repeatedSetColor(TestContext& ctx)
    {
        std::ostringstream oss;
        ColorOutToStream(oss)
            .setColor<ColorTypes::Red>()
            .print("R")
            .setColor<ColorTypes::Green>()
            .print("G");

        KAS_TEST_CHECK_IMPL(ctx, oss.str() == "\033[31mR\033[32mG");
    }

    // --- 17. multi-threaded use ------------------------------------
    /**
     * @brief Verifies that ColorOutToStream can be used from multiple
     *        threads when the caller serializes access.
     *
     * The test uses a mutex to serialize whole chains; ColorOutToStream
     * itself is not thread-safe, and the contract is that the caller
     * provides the locking. The test checks that both thread names and
     * both color codes appear in the output, and that a reset sequence
     * is present.
     */
    inline void multithreaded(TestContext& ctx)
    {
        std::ostringstream oss;
        {
            std::mutex mx { };
            auto worker = [&]<ColorTypes C>(const std::string& name) {
                std::lock_guard lk { mx };
                ColorOutToStream(oss)
                    .template setColor<C>()
                    .print(name)
                    .reset()
                    .ln()
                    .flush();
            };

            std::thread t1([&] { worker.template operator()<ColorTypes::Red>("T1"); });
            std::thread t2([&] { worker.template operator()<ColorTypes::Green>("T2"); });
            t1.join();
            t2.join();
        }

        const std::string s { oss.str() };

        KAS_TEST_CHECK_IMPL(ctx, s.find("T1") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, s.find("T2") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, s.find("\033[31m") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, s.find("\033[32m") != std::string::npos);

        // Each line should end with a reset "\033[0m"
        // (after stripping '\n' - use splitLines as in the logger tests).
        // At minimum, check that both resets are present.
        KAS_TEST_CHECK_IMPL(ctx, s.find("\033[0m") != std::string::npos);
    }

    inline void setColorNone(TestContext& ctx)
    {
        std::ostringstream oss;
        ColorOutToStream(oss).setColor<ColorTypes::None>().print("x");
        KAS_TEST_CHECK_IMPL(ctx, oss.str() == "x"); // no ANSI
    }

    inline void getOutAfterSetOut(TestContext& ctx)
    {
        std::ostringstream a, b;
        ColorOutToStream c(a);
        c.setOut(b);
        KAS_TEST_CHECK_IMPL(ctx, std::addressof(c.getOut()) == std::addressof(b));
    }
}

// ============================================================
//  Entry point
// ============================================================
/**
 * @brief Runs all ColorOutToStream unit tests.
 *
 * Tests are executed in a fixed order: construction, delimiter,
 * whitespace, colors, properties, reset variants, stream control,
 * multi-argument print, repeated color selection, multi-threaded
 * usage. The summary is printed via printSummary() at the end.
 */
inline bool runTests()
{
    TestContext ctx;

    ctorAndGetOut(ctx);
    delimiter(ctx);
    whitespace(ctx);
    setColorRed(ctx);
    setColorGreen(ctx);
    setColorPropertyBold(ctx);
    colorAndProperty(ctx);
    colorReset(ctx);
    reset(ctx);
    colorPropertyReset(ctx);
    printWithOutColor(ctx);
    printEmpty(ctx);
    setOut(ctx);
    flush(ctx);
    printMultipleArgs(ctx);
    repeatedSetColor(ctx);
    multithreaded(ctx);
    setColorNone(ctx);
    getOutAfterSetOut(ctx);

    return printSummary("kas::stream::cots::v3::tests", ctx);
}

} // namespace kas::stream::cots::v3::tests

#endif