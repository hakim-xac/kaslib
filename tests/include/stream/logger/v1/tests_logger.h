/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_STREAM_LOGGER_V1_TESTS_LOGGER_H
#define KASLIBCXX_STREAM_LOGGER_V1_TESTS_LOGGER_H

#include "KAS/stream/logger/v1/Logger.h"
#include "tests/tests.h"

/**
 * @file LoggerTests.h
 * @brief Unit tests for kas::stream::logger::v1::Logger.
 *
 * The tests cover:
 *  - plain print() has no ANSI and ends with a newline;
 *  - multi-argument print() joins values with a space;
 *  - printLog() emits ANSI, ends with "\033[0m\n", and contains the
 *    "(Log)" header and the message after stripping ANSI;
 *  - every level (Trace, Debug, Log, Info, Warning) has a header;
 *  - Error carries a source location (file:line) and a function name;
 *  - Fatal and Exception have their headers;
 *  - level filtering via setLogLevel<Tags...>() emits only the
 *    selected levels;
 *  - setDisabled()/setEnabled() control all level-based output but not
 *    plain print();
 *  - getLogLevel()/setLogLevel(raw) round-trip;
 *  - getEnable()/setEnable() round-trip;
 *  - level 0 passes all records;
 *  - each colored line ends with a reset;
 *  - print() with no arguments writes just a newline;
 *  - a plain print() after a colored one does not leak ANSI;
 *  - setLogLevel(raw) is equivalent to setLogLevel<Tag>();
 *  - two colored lines produce two resets.
 *
 * Test style:
 *  - each test is a free function taking TestContext&;
 *  - KAS_TEST_CHECK_IMPL records a failure but does not abort the test;
 *  - KAS_TEST_REQUIRE_IMPL aborts the current test on failure;
 *  - ANSI sequences are detected and removed with hasAnsi()/stripAnsi();
 *  - line-based checks use splitLines()/endsWithReset()/allDigits().
 *
 * @note The tests assume the following Logger contracts:
 *  - print() is always emitted, regardless of enable/level state;
 *  - printXxx() is suppressed when disabled or filtered out;
 *  - each colored record ends with "\033[0m" before the newline;
 *  - level 0 means "emit all levels".
 */

// ============================================================
//  Logger tests
// ============================================================
namespace kas::stream::logger::v1::tests::stream_logger {
namespace {

    using namespace kas::tests;

    using Logger = kas::stream::logger::v1::Logger;

    /**
     * @brief Verifies that plain print() emits no ANSI and adds a newline.
     */
    inline void withNotANSI(TestContext& ctx)
    {
        std::ostringstream oss;
        Logger log { oss };
        log.print("42");

        KAS_TEST_CHECK_IMPL(ctx, oss.str() == "42\n");
        KAS_TEST_CHECK_IMPL(ctx, !hasAnsi(oss.str()));
    }

    /**
     * @brief Verifies that multi-argument print() joins values with a space.
     */
    inline void printMultipleArgs(TestContext& ctx)
    {
        std::ostringstream oss;
        Logger log { oss };
        log.print("a", 1, 2.5);

        KAS_TEST_CHECK_IMPL(ctx, oss.str() == "a 1 2.5\n");
        KAS_TEST_CHECK_IMPL(ctx, !hasAnsi(oss.str()));
    }

    /**
     * @brief Verifies that printLog() emits ANSI, ends with a reset and
     *        a newline, and contains the "(Log)" header and the message.
     */
    inline void printLogHasAnsiAndReset(TestContext& ctx)
    {
        std::ostringstream oss;
        Logger log { oss };
        log.printLog("hello");

        const std::string raw { oss.str() };
        KAS_TEST_CHECK_IMPL(ctx, hasAnsi(raw));

        KAS_TEST_CHECK_IMPL(ctx, raw.size() >= 5);
        KAS_TEST_CHECK_IMPL(ctx, raw.compare(raw.size() - 5, 5, "\033[0m\n") == 0);
        KAS_TEST_CHECK_IMPL(ctx, raw.back() == '\n');

        const std::string plain { stripAnsi(raw) };
        KAS_TEST_CHECK_IMPL(ctx, plain.find("(Log)") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, plain.find("hello") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, plain.back() == '\n');
    }

    /**
     * @brief Verifies that each simple level emits its own header.
     */
    inline void allLevelsHaveHeaders(TestContext& ctx)
    {
        std::ostringstream oss;
        Logger log { oss };

        log.printTrace("t");
        log.printDebug("d");
        log.printLog("l");
        log.printInfo("i");
        log.printWarn("w");

        const std::string plain { stripAnsi(oss.str()) };
        KAS_TEST_CHECK_IMPL(ctx, plain.find("(Trace)") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, plain.find("(Debug)") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, plain.find("(Log)") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, plain.find("(Info)") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, plain.find("(Warning)") != std::string::npos);
    }

    /**
     * @brief Verifies that Error records include a source location
     *        (file:line) and a function name.
     */
    inline void errorHasLocationAndFunction(TestContext& ctx)
    {
        std::ostringstream oss;
        Logger log { oss };
        log.printError("boom");

        const std::string plain { stripAnsi(oss.str()) };
        KAS_TEST_CHECK_IMPL(ctx, plain.find("(Error)") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, plain.find("boom") != std::string::npos);

        static const std::regex loc_re { R"(\(\s*[^()]*:\s*(\d+)\s*\))" };
        std::smatch m;
        KAS_TEST_REQUIRE_IMPL(ctx, std::regex_search(plain, m, loc_re));
        KAS_TEST_CHECK_IMPL(ctx, allDigits(m[1]));

        KAS_TEST_CHECK_IMPL(ctx, plain.find("->") != std::string::npos);
    }

    /**
     * @brief Verifies the Fatal and Exception headers and messages.
     */
    inline void fatalAndExceptionHaveHeaders(TestContext& ctx)
    {
        std::ostringstream oss;
        Logger log { oss };

        log.printFatal("fatal-msg");
        log.printException("exc-msg");

        const std::string plain { stripAnsi(oss.str()) };
        KAS_TEST_CHECK_IMPL(ctx, plain.find("(Fatal)") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, plain.find("fatal-msg") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, plain.find("(Exception)") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, plain.find("exc-msg") != std::string::npos);
    }

    /**
     * @brief Verifies level filtering via setLogLevel<Tags...>().
     *
     * Only Log and Error should be emitted; the rest must be suppressed.
     */
    inline void levelsFiltering(TestContext& ctx)
    {
        using namespace kas::stream::logger::v1::tags;

        std::ostringstream oss;
        Logger log { oss };
        log.setLogLevel<Log, Error>();

        log.printTrace("MSG_TRACE");
        log.printDebug("MSG_DEBUG");
        log.printLog("MSG_LOG");
        log.printInfo("MSG_INFO");
        log.printWarn("MSG_WARN");
        log.printError("MSG_ERROR");
        log.printFatal("MSG_FATAL");
        log.printException("MSG_EXC");

        const std::string plain { stripAnsi(oss.str()) };
        KAS_TEST_CHECK_IMPL(ctx, plain.find("MSG_TRACE") == std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, plain.find("MSG_DEBUG") == std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, plain.find("MSG_LOG") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, plain.find("MSG_INFO") == std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, plain.find("MSG_WARN") == std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, plain.find("MSG_ERROR") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, plain.find("MSG_FATAL") == std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, plain.find("MSG_EXC") == std::string::npos);
    }

    /**
     * @brief Verifies that setDisabled()/setEnabled() control only
     *        level-based output, not plain print().
     */
    inline void disableEnable(TestContext& ctx)
    {
        std::ostringstream oss;
        Logger log { oss };

        log.setDisabled();
        log.printLog("y");
        log.printError("z");
        KAS_TEST_CHECK_IMPL(ctx, oss.str().empty());

        log.print("x");
        KAS_TEST_CHECK_IMPL(ctx, oss.str() == "x\n");

        log.setEnabled();
        log.printLog("y");
        KAS_TEST_CHECK_IMPL(ctx, oss.str().find("(Log)") != std::string::npos);
    }

    /**
     * @brief Verifies getLogLevel()/setLogLevel(raw) round-trip.
     */
    inline void getSetLogLevel(TestContext& ctx)
    {
        std::ostringstream oss;
        Logger log { oss };

        KAS_TEST_CHECK_IMPL(ctx, log.getLogLevel() == 0);

        log.setLogLevel(static_cast<std::size_t>(
            kas::stream::logger::v1::tags::Warning::code));
        KAS_TEST_CHECK_IMPL(ctx, log.getLogLevel() == 16);
    }

    /**
     * @brief Verifies getEnable()/setEnable() round-trip.
     */
    inline void getSetEnable(TestContext& ctx)
    {
        std::ostringstream oss;
        Logger log { oss };

        KAS_TEST_CHECK_IMPL(ctx, log.getEnable() == true);
        log.setEnable(false);
        KAS_TEST_CHECK_IMPL(ctx, log.getEnable() == false);
        log.setEnable(true);
        KAS_TEST_CHECK_IMPL(ctx, log.getEnable() == true);
    }

    /**
     * @brief Verifies that level 0 means "emit all levels".
     */
    inline void zeroLevelPassesAll(TestContext& ctx)
    {
        std::ostringstream oss;
        Logger log { oss };

        KAS_TEST_CHECK_IMPL(ctx, log.getLogLevel() == 0);

        log.printTrace("MSG_TRACE");
        log.printLog("MSG_LOG");
        log.printError("MSG_ERROR");

        const std::string plain { stripAnsi(oss.str()) };
        KAS_TEST_CHECK_IMPL(ctx, plain.find("MSG_TRACE") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, plain.find("MSG_LOG") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, plain.find("MSG_ERROR") != std::string::npos);
    }

    /**
     * @brief Verifies that every colored line ends with a reset
     *        sequence before the newline.
     */
    inline void eachColoredLineEndsWithReset(TestContext& ctx)
    {
        std::ostringstream oss;
        Logger log { oss };
        log.printLog("one");
        log.printWarn("two");
        log.printInfo("three");
        log.printError("four");

        const auto lines = splitLines(oss.str());
        KAS_TEST_CHECK_IMPL(ctx, lines.size() == 4);
        for (const auto& line : lines) {
            KAS_TEST_CHECK_IMPL(ctx, endsWithReset(line));
        }
    }

    /**
     * @brief Verifies that print() with no arguments writes only a
     *        newline and no ANSI.
     */
    inline void printEmptyGivesNewline(TestContext& ctx)
    {
        std::ostringstream oss;
        Logger log { oss };
        log.print();

        KAS_TEST_CHECK_IMPL(ctx, oss.str() == "\n");
        KAS_TEST_CHECK_IMPL(ctx, !hasAnsi(oss.str()));
    }

    /**
     * @brief Verifies that a plain print() after a colored one does
     *        not leak ANSI codes.
     */
    inline void printDoesNotLeakAnsi(TestContext& ctx)
    {
        std::ostringstream oss;
        Logger log { oss };

        log.printLog("colored");
        const std::size_t after_log = oss.str().size();

        log.print("plain");

        const std::string tail = oss.str().substr(after_log);
        KAS_TEST_CHECK_IMPL(ctx, tail == "plain\n");
        KAS_TEST_CHECK_IMPL(ctx, !hasAnsi(tail));
    }

    /**
     * @brief Verifies that setLogLevel(raw) is equivalent to
     *        setLogLevel<Tag>().
     */
    inline void setLogLevelRawEquivalent(TestContext& ctx)
    {
        std::ostringstream oss;
        Logger log { oss };

        log.setLogLevel(static_cast<std::size_t>(
            kas::stream::logger::v1::tags::Info::code));

        log.printTrace("MSG_TRACE");
        log.printInfo("MSG_INFO");
        log.printError("MSG_ERROR");

        const std::string plain { stripAnsi(oss.str()) };
        KAS_TEST_CHECK_IMPL(ctx, plain.find("MSG_TRACE") == std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, plain.find("MSG_INFO") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, plain.find("MSG_ERROR") == std::string::npos);
    }

    /**
     * @brief Verifies that two colored lines produce exactly two resets.
     */
    inline void twoLinesTwoResets(TestContext& ctx)
    {
        std::ostringstream oss;
        Logger log { oss };
        log.printLog("first");
        log.printLog("second");

        KAS_TEST_CHECK_IMPL(ctx, countSubstr(oss.str(), "\033[0m") == 2);
    }
}

/**
 * @brief Runs all Logger unit tests.
 *
 * Tests are executed in a fixed order: plain output, multi-argument
 * output, colored records, headers, location, filtering, enable
 * toggle, level accessors, ANSI hygiene. The summary is printed via
 * printSummary() at the end.
 */
inline bool runTests()
{
    TestContext ctx;

    withNotANSI(ctx);
    printMultipleArgs(ctx);
    printLogHasAnsiAndReset(ctx);
    allLevelsHaveHeaders(ctx);
    errorHasLocationAndFunction(ctx);
    fatalAndExceptionHaveHeaders(ctx);
    levelsFiltering(ctx);
    disableEnable(ctx);
    getSetLogLevel(ctx);
    getSetEnable(ctx);
    zeroLevelPassesAll(ctx);
    eachColoredLineEndsWithReset(ctx);
    printEmptyGivesNewline(ctx);
    printDoesNotLeakAnsi(ctx);
    setLogLevelRawEquivalent(ctx);
    twoLinesTwoResets(ctx);

    return printSummary("kas::stream::logger::v1::tests::stream_logger", ctx);
}
}

#endif