/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_STREAM_LOGGER_V1_TESTS_LOGGER_SYNC_H
#define KASLIBCXX_STREAM_LOGGER_V1_TESTS_LOGGER_SYNC_H

#include "KAS/stream/logger/v1/LoggerSync.h"
#include "KAS/utils/utils.h"

#include "tests/tests.h"

/**
 * @file LoggerSyncTests.h
 * @brief Unit tests for kas::stream::logger::v1::LoggerSync.
 *
 * The tests cover:
 *  - basic writes: plain print() and a colored printLog();
 *  - level filtering via setLogLevel<Tags...>();
 *  - setDisabled()/setEnabled() suppress level-based output but not
 *    plain print();
 *  - getLogLevel()/getEnable() round-trip;
 *  - each colored line ends with a reset sequence;
 *  - printError() carries a source location and a function name;
 *  - plain print() emits no ANSI;
 *  - concurrent logging from multiple threads does not interleave
 *    records: every line matches "T <t> L <i>" exactly.
 *  - flush() forwards to the underlying stream;
 *  - concurrent colored logging does not interleave records and does
 *    not race in time formatting (meant to be run under TSan).
 *
 * Test style:
 *  - each test is a free function taking TestContext&;
 *  - KAS_TEST_CHECK_IMPL records a failure but does not abort the test;
 *  - ANSI sequences are detected and removed with hasAnsi()/stripAnsi();
 *  - line-based checks use splitLines()/endsWithReset();
 *  - the multi-threaded test uses a static_assert to pin that the
 *    tested type is LoggerSync, not Logger.
 *
 * @note The tests assume the following LoggerSync contracts:
 *  - print() is always emitted, regardless of enable/level state;
 *  - printXxx() is suppressed when disabled or filtered out;
 *  - each colored record ends with "\033[0m" before the newline;
 *  - records written through the same LoggerSync are not interleaved,
 *    because the internal mutex is held for the whole record.
 */

// ============================================================
//  LoggerSync tests
// ============================================================

namespace kas::stream::logger::v1::tests::stream_logger_sync {
namespace {

    using namespace kas::tests;

    using Logger = kas::stream::logger::v1::LoggerSync;

    /**
     * @brief Verifies basic writes: plain text and a colored record.
     */
    inline void writeAndCheck(TestContext& ctx)
    {
        std::ostringstream sink;
        Logger log { sink };

        log.print("42");
        log.printLog("hello");

        KAS_TEST_CHECK_IMPL(ctx, sink.str().find("42") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, sink.str().find("hello") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, hasAnsi(sink.str()));

        const std::string plain { stripAnsi(sink.str()) };
        KAS_TEST_CHECK_IMPL(ctx, plain.find("(Log)") != std::string::npos);
    }

    // --- level filtering --------------------------------------------
    /**
     * @brief Verifies that setLogLevel<Log>() suppresses other levels.
     */
    inline void levelsFiltering(TestContext& ctx)
    {
        using namespace kas::stream::logger::v1::tags;

        std::ostringstream sink;
        Logger log { sink };

        log.setLogLevel<Log>();

        log.printTrace("MSG_TRACE");
        log.printLog("MSG_LOG");
        log.printInfo("MSG_INFO");

        const std::string plain { stripAnsi(sink.str()) };
        KAS_TEST_CHECK_IMPL(ctx, plain.find("MSG_TRACE") == std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, plain.find("MSG_LOG") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, plain.find("MSG_INFO") == std::string::npos);
    }

    // --- setDisabled on the sync logger -----------------------------
    /**
     * @brief Verifies that setDisabled() suppresses level-based output
     *        but not plain print().
     *
     * Pins the contract that print() ignores the enable flag.
     */
    inline void disableEnable(TestContext& ctx)
    {
        std::ostringstream sink;
        Logger log { sink };

        log.setDisabled();
        log.printLog("MSG_HIDDEN");
        log.printError("MSG_HIDDEN_2");
        KAS_TEST_CHECK_IMPL(ctx, sink.str().empty());

        log.print("MSG_RAW"); // print ignores setDisabled - by design
        KAS_TEST_CHECK_IMPL(ctx, sink.str().find("MSG_RAW") != std::string::npos);

        log.setEnabled();
        log.printLog("MSG_VISIBLE");
        KAS_TEST_CHECK_IMPL(ctx, sink.str().find("MSG_VISIBLE") != std::string::npos);
    }

    // --- get/set log level and enable -------------------------------
    /**
     * @brief Verifies getLogLevel()/setLogLevel(raw) and
     *        getEnable()/setEnable() round-trip.
     */
    inline void getSetState(TestContext& ctx)
    {
        std::ostringstream sink;
        Logger log { sink };

        KAS_TEST_CHECK_IMPL(ctx, log.getLogLevel() == 0);
        KAS_TEST_CHECK_IMPL(ctx, log.getEnable() == true);

        log.setLogLevel(static_cast<std::size_t>(
            kas::stream::logger::v1::tags::Error::code));
        KAS_TEST_CHECK_IMPL(ctx, log.getLogLevel() == 32);

        log.setEnable(false);
        KAS_TEST_CHECK_IMPL(ctx, log.getEnable() == false);
    }

    // --- each colored line ends with a reset ------------------------
    /**
     * @brief Verifies that every colored record ends with a reset
     *        sequence before the newline.
     */
    inline void eachColoredLineEndsWithReset(TestContext& ctx)
    {
        std::ostringstream sink;
        Logger log { sink };

        log.printLog("one");
        log.printWarn("two");
        log.printError("three");

        const auto lines = splitLines(sink.str());
        KAS_TEST_CHECK_IMPL(ctx, lines.size() == 3);
        for (const auto& line : lines) {
            KAS_TEST_CHECK_IMPL(ctx, endsWithReset(line));
        }
    }

    // --- printError has file:line -> function -----------------------
    /**
     * @brief Verifies that printError() output includes the "(Error)"
     *        header, the message, and the "->" function separator.
     */
    inline void errorHasLocationAndFunction(TestContext& ctx)
    {
        std::ostringstream sink;
        Logger log { sink };

        log.printError("boom");

        const std::string plain { stripAnsi(sink.str()) };
        KAS_TEST_CHECK_IMPL(ctx, plain.find("(Error)") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, plain.find("boom") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, plain.find("->") != std::string::npos);
    }

    // --- print emits no ANSI ----------------------------------------
    /**
     * @brief Verifies that plain print() writes the message plus a
     *        newline and no ANSI codes.
     */
    inline void printPlainHasNoAnsi(TestContext& ctx)
    {
        std::ostringstream sink;
        Logger log { sink };

        log.print("plain-msg");

        KAS_TEST_CHECK_IMPL(ctx, sink.str() == "plain-msg\n");
        KAS_TEST_CHECK_IMPL(ctx, !hasAnsi(sink.str()));
    }

    // --- multi-threading: records are not interleaved --------------
    /**
     * @brief Verifies that concurrent logging through the same
     *        LoggerSync does not interleave records.
     *
     * kThreads threads each write kLines records of the form
     * "T <t> L <i>". After joining, every line in the output must
     * match that exact shape, and the total count must equal
     * kThreads * kLines. Any interleaving would break the regex match.
     *
     * @note A static_assert pins that the tested type is LoggerSync,
     *       not Logger (which has no internal mutex).
     */
    inline void multithreadNoInterleave(TestContext& ctx)
    {
        std::ostringstream sink;
        Logger log { sink };

        static_assert(std::is_same_v<
                          decltype(log),
                          kas::stream::logger::v1::LoggerSync>,
            "log is not LoggerSync<std::ostream>");

        constexpr int kThreads = 8;
        constexpr int kLines = 100;

        std::vector<std::thread> threads;
        threads.reserve(kThreads);

        for (int t = 0; t < kThreads; ++t) {
            threads.emplace_back([&log, t] {
                for (int i = 0; i < kLines; ++i)
                    log.print("T", t, "L", i);
            });
        }
        for (auto& th : threads)
            th.join();

        static const std::regex re { R"(^T (\d+) L (\d+)$)" };

        std::size_t count = 0;
        bool all_match = true;

        for (const auto& line : splitLines(stripAnsi(sink.str()))) {
            if (line.empty())
                continue;
            ++count;

            std::smatch m;
            if (!std::regex_match(line, m, re)) {
                all_match = false;
                std::cerr << utils::stringify("    unexpected line: \"", line, "\"\n");
                continue;
            }
            const int t = std::stoi(m[1]);
            const int i = std::stoi(m[2]);
            KAS_TEST_CHECK_IMPL(ctx, t >= 0 && t < kThreads);
            KAS_TEST_CHECK_IMPL(ctx, i >= 0 && i < kLines);
        }

        KAS_TEST_CHECK_IMPL(ctx, all_match);
        KAS_TEST_CHECK_IMPL(ctx,
            count == static_cast<std::size_t>(kThreads * kLines));
    }

    /**
     * @brief Verifies that flush() forwards to the underlying stream
     *        without disturbing prior output.
     *
     * ostringstream has no observable flush side effect, so the test
     * only checks that data written before flush() is still present
     * and correctly terminated.
     */
    inline void flushWorks(TestContext& ctx)
    {
        std::ostringstream sink;
        Logger log { sink };
        log.print("x");
        log.flush();
        KAS_TEST_CHECK_IMPL(ctx, sink.str() == "x\n");
    }

    /**
     * @brief Verifies that concurrent colored logging does not
     *        interleave records and does not race in time formatting.
     *
     * kThreads threads each write kLines printLog() records of the
     * form "[time] (Log) T <t> L <i>". After joining, every line must
     * match that shape (after stripping ANSI), and the total count
     * must equal kThreads * kLines.
     *
     * This test is intended to be run under ThreadSanitizer: printLog()
     * formats the current time in LoggerBaseInterface before taking
     * the LoggerSync mutex, so a non-thread-safe localtime() would be
     * reported as a data race here even though the stream itself is
     * protected.
     */
    inline void multithreadColoredNoInterleave(TestContext& ctx)
    {
        std::ostringstream sink;
        Logger log { sink };

        constexpr int kThreads = 4;
        constexpr int kLines = 50;

        std::vector<std::thread> threads;
        threads.reserve(kThreads);
        for (int t = 0; t < kThreads; ++t) {
            threads.emplace_back([&log, t] {
                for (int i = 0; i < kLines; ++i)
                    log.printLog("T", t, "L", i);
            });
        }
        for (auto& th : threads)
            th.join();

        static const std::regex re { R"(^\[[^\]]*\] \(Log\) T (\d+) L (\d+)$)" };

        std::size_t count = 0;
        bool all_match = true;

        for (const auto& line : splitLines(stripAnsi(sink.str()))) {
            if (line.empty())
                continue;
            ++count;

            std::smatch m;
            if (!std::regex_match(line, m, re)) {
                all_match = false;
                std::cerr << utils::stringify("    unexpected line: \"", line, "\"\n");
                continue;
            }
            const int t = std::stoi(m[1]);
            const int i = std::stoi(m[2]);
            KAS_TEST_CHECK_IMPL(ctx, t >= 0 && t < kThreads);
            KAS_TEST_CHECK_IMPL(ctx, i >= 0 && i < kLines);
        }

        KAS_TEST_CHECK_IMPL(ctx, all_match);
        KAS_TEST_CHECK_IMPL(ctx,
            count == static_cast<std::size_t>(kThreads * kLines));
    }
}

/**
 * @brief Runs all LoggerSync unit tests.
 *
 * Tests are executed in a fixed order: basic writes, filtering,
 * enable toggle, level accessors, ANSI hygiene, location, and the
 * multi-threaded no-interleave check. The summary is printed via
 * printSummary() at the end.
 */
inline bool runTests()
{
    TestContext ctx;

    writeAndCheck(ctx);
    levelsFiltering(ctx);
    disableEnable(ctx);
    getSetState(ctx);
    eachColoredLineEndsWithReset(ctx);
    errorHasLocationAndFunction(ctx);
    printPlainHasNoAnsi(ctx);
    multithreadNoInterleave(ctx);

    flushWorks(ctx);
    multithreadColoredNoInterleave(ctx);

    return printSummary("kas::stream::logger::v1::tests::stream_logger_sync", ctx);
}
}

#endif