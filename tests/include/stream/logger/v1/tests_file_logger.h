/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_STREAM_LOGGER_V1_TESTS_FILE_LOGGER_H
#define KASLIBCXX_STREAM_LOGGER_V1_TESTS_FILE_LOGGER_H

#include "KAS/stream/logger/v1/FileLogger.h"
#include "KAS/utils/utils.h"

#include "tests/tests.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

/**
 * @file FileLoggerTests.h
 * @brief Unit tests for kas::stream::logger::v1::FileLogger.
 *
 * The tests cover:
 *  - construction creates the file and missing parent directories;
 *  - getFilename() returns the path passed to the constructor;
 *  - plain print() writes text without ANSI;
 *  - colored printLog() writes ANSI, the "(Log)" header and a reset;
 *  - every simple level emits its own header;
 *  - printError() carries a source location and a function name;
 *  - level filtering via setLogLevel<Tags...>();
 *  - setDisabled()/setEnabled() suppress level-based output but not
 *    plain print();
 *  - getLogLevel()/getEnable()/getMaxFilesize() accessors;
 *  - flush() actually writes data to disk;
 *  - clearLogFile() truncates the file and logging continues;
 *  - setFilename() switches the target file;
 *  - size-based rotation truncates the file when it reaches
 *    max_filesize;
 *  - concurrent logging from multiple threads does not interleave
 *    records.
 *
 * Test style:
 *  - each test is a free function taking TestContext&;
 *  - KAS_TEST_CHECK_IMPL records a failure but does not abort the test;
 *  - KAS_TEST_REQUIRE_IMPL aborts the current test on failure;
 *  - temporary files are created with makeTempFile() and removed by
 *    TempFileGuard (RAII);
 *  - file contents are read with readFile() and checked with
 *    hasAnsi()/stripAnsi()/splitLines().
 *
 * @note The tests assume the following FileLogger contracts:
 *  - the constructor creates missing parent directories;
 *  - print() ignores setDisabled(), while printXxx() respects it;
 *  - each colored record ends with "\033[0m" before the newline;
 *  - flush() makes previously written data visible on disk;
 *  - clearLogFile() truncates the file but keeps it open;
 *  - setFilename() flushes and closes the old file, then opens the
 *    new one in append mode.
 *
 * @warning rotatesWhenExceedsMaxSize is currently expected to FAIL
 *          because size-based rotation is not wired in. It is kept as
 *          a red test to signal the missing feature.
 */

namespace kas::stream::logger::v1::tests::stream_file_logger {
namespace {

    // ============================================================
    //  Helpers for temporary files
    // ============================================================

    /**
     * @brief Builds a unique path in the system temporary directory.
     *
     * Uniqueness comes from the steady-clock tick count plus an atomic
     * counter, so concurrent tests do not collide.
     *
     * @param prefix File name prefix.
     * @return Path to a not-yet-created file.
     */
    std::filesystem::path makeTempFile(const std::string& prefix)
    {
        static std::atomic<std::size_t> counter { 0 };
        const auto now = std::chrono::steady_clock::now().time_since_epoch().count();

        std::ostringstream name;
        name << prefix << '_' << now << '_' << counter.fetch_add(1);
        return std::filesystem::temp_directory_path() / name.str();
    }

    /**
     * @brief Reads the whole file into a string.
     * @param path File to read.
     * @return File contents, or an empty string if the file is missing.
     */
    std::string readFile(const std::filesystem::path& path)
    {
        std::ifstream in { path, std::ios::binary };
        if (!in.is_open())
            return { };
        std::ostringstream oss;
        oss << in.rdbuf();
        return oss.str();
    }

    /**
     * @brief Removes a file on scope exit (RAII).
     */
    struct TempFileGuard {
        std::filesystem::path path; ///< File to remove.
        ~TempFileGuard()
        {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
    };

    using namespace kas::tests;
    using namespace kas::stream::logger::v1;

    // ============================================================
    //  Tests
    // ============================================================

    // --- 1. Constructor opens the file and creates directories ------
    /**
     * @brief Verifies that the constructor creates missing parent
     *        directories and opens the file for writing.
     */
    inline void ctorCreatesFileAndDirs(TestContext& ctx)
    {
        const auto dir = std::filesystem::temp_directory_path()
            / ("kas_file_logger_test_dir_"
                + std::to_string(
                    std::chrono::steady_clock::now().time_since_epoch().count()));
        const auto file = dir / "nested" / "log.txt";

        TempFileGuard guard_file { file };
        // Remove the whole directory tree after the test.
        struct DirGuard {
            std::filesystem::path d;
            ~DirGuard()
            {
                std::error_code ec;
                std::filesystem::remove_all(d, ec);
            }
        } guard_dir { dir };

        FileLogger log { file };
        log.print("hello");
        log.flush();

        KAS_TEST_CHECK_IMPL(ctx, std::filesystem::exists(file));
        KAS_TEST_CHECK_IMPL(ctx, readFile(file).find("hello") != std::string::npos);
    }

    // --- 2. getFilename returns the constructor path ----------------
    /**
     * @brief Verifies that getFilename() returns the path passed to
     *        the constructor.
     */
    inline void getFilenameReturnsPath(TestContext& ctx)
    {
        const auto file = makeTempFile("kas_file_logger_getname");
        TempFileGuard guard { file };

        FileLogger log { file };

        KAS_TEST_CHECK_IMPL(ctx, log.getFilename() == file);
    }

    // --- 3. print writes plain text without ANSI --------------------
    /**
     * @brief Verifies that plain print() writes the message plus a
     *        newline and no ANSI codes.
     */
    inline void printPlainText(TestContext& ctx)
    {
        const auto file = makeTempFile("kas_file_logger_print");
        TempFileGuard guard { file };

        FileLogger log { file };
        log.print("42");
        log.flush();

        const std::string content { readFile(file) };

        KAS_TEST_CHECK_IMPL(ctx, content == "42\n");
        KAS_TEST_CHECK_IMPL(ctx, !hasAnsi(content));
    }

    // --- 4. printLog writes ANSI, "(Log)" header and reset ----------
    /**
     * @brief Verifies that printLog() writes ANSI, the "(Log)" header,
     *        the message, and ends with a reset before the newline.
     */
    inline void printLogHasAnsiAndReset(TestContext& ctx)
    {
        const auto file = makeTempFile("kas_file_logger_printlog");
        TempFileGuard guard { file };

        FileLogger log { file };
        log.printLog("hello");
        log.flush();

        const std::string raw { readFile(file) };

        KAS_TEST_CHECK_IMPL(ctx, hasAnsi(raw));
        KAS_TEST_CHECK_IMPL(ctx, raw.size() >= 5);
        KAS_TEST_CHECK_IMPL(ctx, raw.compare(raw.size() - 5, 5, "\033[0m\n") == 0);

        const std::string plain { stripAnsi(raw) };
        KAS_TEST_CHECK_IMPL(ctx, plain.find("(Log)") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, plain.find("hello") != std::string::npos);
    }

    // --- 5. Every simple level emits its own header -----------------
    /**
     * @brief Verifies that Trace, Debug, Log, Info and Warning each
     *        produce their own header in the file.
     */
    inline void allLevelsHaveHeaders(TestContext& ctx)
    {
        const auto file = makeTempFile("kas_file_logger_levels");
        TempFileGuard guard { file };

        FileLogger log { file };
        log.printTrace("t");
        log.printDebug("d");
        log.printLog("l");
        log.printInfo("i");
        log.printWarn("w");
        log.flush();

        const std::string plain { stripAnsi(readFile(file)) };

        KAS_TEST_CHECK_IMPL(ctx, plain.find("(Trace)") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, plain.find("(Debug)") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, plain.find("(Log)") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, plain.find("(Info)") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, plain.find("(Warning)") != std::string::npos);
    }

    // --- 6. printError carries file:line -> function ----------------
    /**
     * @brief Verifies that printError() output includes the "(Error)"
     *        header, the message, a "(file:line)" fragment with digits,
     *        and the "->" function separator.
     */
    inline void errorHasLocationAndFunction(TestContext& ctx)
    {
        const auto file = makeTempFile("kas_file_logger_error");
        TempFileGuard guard { file };

        FileLogger log { file };
        log.printError("boom");
        log.flush();

        const std::string plain { stripAnsi(readFile(file)) };

        KAS_TEST_CHECK_IMPL(ctx, plain.find("(Error)") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, plain.find("boom") != std::string::npos);

        static const std::regex loc_re { R"(\(\s*[^()]*:\s*(\d+)\s*\))" };
        std::smatch m;
        KAS_TEST_REQUIRE_IMPL(ctx, std::regex_search(plain, m, loc_re));
        KAS_TEST_CHECK_IMPL(ctx, allDigits(m[1]));

        KAS_TEST_CHECK_IMPL(ctx, plain.find("->") != std::string::npos);
    }

    // --- 7. Level filtering -----------------------------------------
    /**
     * @brief Verifies that setLogLevel<Log, Error>() emits only Log
     *        and Error records.
     */
    inline void levelsFiltering(TestContext& ctx)
    {
        using namespace kas::stream::logger::v1::tags;

        const auto file = makeTempFile("kas_file_logger_filter");
        TempFileGuard guard { file };

        FileLogger log { file };
        log.setLogLevel<Log, Error>();

        log.printTrace("MSG_TRACE");
        log.printDebug("MSG_DEBUG");
        log.printLog("MSG_LOG");
        log.printInfo("MSG_INFO");
        log.printWarn("MSG_WARN");
        log.printError("MSG_ERROR");
        log.printFatal("MSG_FATAL");
        log.printException("MSG_EXC");
        log.flush();

        const std::string plain { stripAnsi(readFile(file)) };

        KAS_TEST_CHECK_IMPL(ctx, plain.find("MSG_TRACE") == std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, plain.find("MSG_DEBUG") == std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, plain.find("MSG_LOG") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, plain.find("MSG_INFO") == std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, plain.find("MSG_WARN") == std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, plain.find("MSG_ERROR") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, plain.find("MSG_FATAL") == std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, plain.find("MSG_EXC") == std::string::npos);
    }

    // --- 8. setDisabled / setEnabled --------------------------------
    /**
     * @brief Verifies that setDisabled() suppresses level-based output
     *        but not plain print().
     */
    inline void disableEnable(TestContext& ctx)
    {
        const auto file = makeTempFile("kas_file_logger_disable");
        TempFileGuard guard { file };

        FileLogger log { file };

        log.setDisabled();
        log.printLog("MSG_HIDDEN");
        log.printError("MSG_HIDDEN_2");
        log.flush();

        KAS_TEST_CHECK_IMPL(ctx, readFile(file).empty());

        log.print("MSG_RAW"); // print ignores setDisabled - by design
        log.flush();
        KAS_TEST_CHECK_IMPL(ctx, readFile(file).find("MSG_RAW") != std::string::npos);

        log.setEnabled();
        log.printLog("MSG_VISIBLE");
        log.flush();
        KAS_TEST_CHECK_IMPL(ctx, readFile(file).find("MSG_VISIBLE") != std::string::npos);
    }

    // --- 9. get/set log level ---------------------------------------
    /**
     * @brief Verifies getLogLevel()/setLogLevel(raw) round-trip.
     */
    inline void getSetLogLevel(TestContext& ctx)
    {
        const auto file = makeTempFile("kas_file_logger_setlevel");
        TempFileGuard guard { file };

        FileLogger log { file };

        KAS_TEST_CHECK_IMPL(ctx, log.getLogLevel() == 0);

        log.setLogLevel(static_cast<std::size_t>(
            kas::stream::logger::v1::tags::Warning::code));
        KAS_TEST_CHECK_IMPL(ctx, log.getLogLevel() == 16);
    }

    // --- 10. get/set enable -----------------------------------------
    /**
     * @brief Verifies getEnable()/setEnable() round-trip.
     */
    inline void getSetEnable(TestContext& ctx)
    {
        const auto file = makeTempFile("kas_file_logger_setenable");
        TempFileGuard guard { file };

        FileLogger log { file };

        KAS_TEST_CHECK_IMPL(ctx, log.getEnable() == true);
        log.setEnable(false);
        KAS_TEST_CHECK_IMPL(ctx, log.getEnable() == false);
        log.setEnable(true);
        KAS_TEST_CHECK_IMPL(ctx, log.getEnable() == true);
    }

    // --- 11. getMaxFilesize / setMaxFilesize / resetMaxFilesize -----
    /**
     * @brief Verifies the max-filesize accessors and reset.
     */
    inline void getSetMaxFilesize(TestContext& ctx)
    {
        const auto file = makeTempFile("kas_file_logger_maxsize");
        TempFileGuard guard { file };

        FileLogger log { file };

        KAS_TEST_CHECK_IMPL(ctx, log.getMaxFilesize() == 0);

        log.setMaxFilesize(1024);
        KAS_TEST_CHECK_IMPL(ctx, log.getMaxFilesize() == 1024);

        log.resetMaxFilesize();
        KAS_TEST_CHECK_IMPL(ctx, log.getMaxFilesize() == 0);
    }

    // --- 12. flush actually writes to disk --------------------------
    /**
     * @brief Verifies that flush() makes previously written data
     *        visible to a separate ifstream.
     */
    inline void flushWritesToDisk(TestContext& ctx)
    {
        const auto file = makeTempFile("kas_file_logger_flush");
        TempFileGuard guard { file };

        FileLogger log { file };
        log.print("flushed");
        log.flush();

        // Read the file with a separate ifstream: if flush worked, the
        // data is on disk.
        const std::string content { readFile(file) };
        KAS_TEST_CHECK_IMPL(ctx, content == "flushed\n");
    }

    // --- 13. clearLogFile truncates the file ------------------------
    /**
     * @brief Verifies that clearLogFile() truncates the file and that
     *        logging continues afterwards.
     */
    inline void clearLogFileTruncates(TestContext& ctx)
    {
        const auto file = makeTempFile("kas_file_logger_clear");
        TempFileGuard guard { file };

        FileLogger log { file };
        log.print("before-clear");
        log.flush();

        KAS_TEST_CHECK_IMPL(ctx,
            readFile(file).find("before-clear") != std::string::npos);

        log.clearLogFile();
        log.flush();

        KAS_TEST_CHECK_IMPL(ctx, readFile(file).empty());

        // Logging continues after truncation.
        log.print("after-clear");
        log.flush();
        KAS_TEST_CHECK_IMPL(ctx,
            readFile(file).find("after-clear") != std::string::npos);
    }

    // --- 14. setFilename switches the target file -------------------
    /**
     * @brief Verifies that setFilename() redirects subsequent writes
     *        to the new file and leaves the old file untouched.
     */
    inline void setFilenameSwitchesFile(TestContext& ctx)
    {
        const auto file1 = makeTempFile("kas_file_logger_setname1");
        const auto file2 = makeTempFile("kas_file_logger_setname2");
        TempFileGuard guard1 { file1 };
        TempFileGuard guard2 { file2 };

        FileLogger log { file1 };
        log.print("first");
        log.flush();

        log.setFilename(file2);
        log.print("second");
        log.flush();

        KAS_TEST_CHECK_IMPL(ctx, readFile(file1).find("first") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, readFile(file1).find("second") == std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, readFile(file2).find("second") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, readFile(file2).find("first") == std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, log.getFilename() == file2);
    }

    // --- 15. Rotation when max_filesize is exceeded -----------------
    //        WARNING: checkAndRotateIfNeeded is currently not called,
    //        so this test is expected to FAIL. It is kept red on
    //        purpose, as a signal that max_filesize does not work.
    /**
     * @brief Verifies size-based rotation.
     *
     * @warning This test is expected to FAIL today: size-based
     *          rotation is not wired into the write path. It is kept
     *          as a red test to signal the missing feature.
     */
    inline void rotatesWhenExceedsMaxSize(TestContext& ctx)
    {
        const auto file = makeTempFile("kas_file_logger_rotate");
        TempFileGuard guard { file };

        FileLogger log { file, 128 }; // 128 bytes

        // Write many lines to exceed 128 bytes.
        for (int i = 0; i < 20; ++i) {
            log.print("line ", i, " padding padding padding");
        }
        log.flush();

        const std::string content { readFile(file) };

        // After rotation (trunc), the first line must be gone.
        KAS_TEST_CHECK_IMPL(ctx, content.find("line 0 ") == std::string::npos);

        // The size must stay near max_filesize (a trunc happens on the
        // write that crosses the limit).
        KAS_TEST_CHECK_IMPL(ctx, content.size() < 128 * 4);
    }

    // --- 16. Multi-threading: records are not interleaved ----------
    /**
     * @brief Verifies that concurrent logging through the same
     *        FileLogger does not interleave records.
     *
     * kThreads threads each write kLines records of the form
     * "T<t>L<i>". After joining and flushing, every line in the file
     * must match that shape, and the total count must equal
     * kThreads * kLines.
     */
    inline void multithreadNoInterleave(TestContext& ctx)
    {
        const auto file = makeTempFile("kas_file_logger_mt");
        TempFileGuard guard { file };

        FileLogger log { file };

        constexpr int kThreads = 4;
        constexpr int kLines = 50;

        std::vector<std::thread> threads;
        threads.reserve(kThreads);

        for (int t = 0; t < kThreads; ++t) {
            threads.emplace_back([&log, t] {
                for (int i = 0; i < kLines; ++i) {
                    log.print(std::string { "T" } + std::to_string(t)
                        + "L" + std::to_string(i));
                }
            });
        }
        for (auto& th : threads)
            th.join();
        log.flush();

        static const std::regex re { R"(^T(\d+)L(\d+)$)" };
        std::smatch m;

        std::size_t count = 0;
        bool all_match = true;

        for (const auto& line : splitLines(stripAnsi(readFile(file)))) {
            if (line.empty())
                continue;
            ++count;

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

} // namespace

// ============================================================
//  Entry point
// ============================================================

/**
 * @brief Runs all FileLogger unit tests.
 *
 * Tests are executed in a fixed order: construction, path access,
 * plain and colored output, headers, location, filtering, enable
 * toggle, max-filesize accessors, flush, truncation, filename switch,
 * rotation, multi-threaded no-interleave. The summary is printed via
 * printSummary() at the end.
 *
 * @note rotatesWhenExceedsMaxSize is expected to fail until size-based
 *       rotation is wired into the write path.
 */
inline bool runTests()
{
    TestContext ctx;

    ctorCreatesFileAndDirs(ctx);
    getFilenameReturnsPath(ctx);
    printPlainText(ctx);
    printLogHasAnsiAndReset(ctx);
    allLevelsHaveHeaders(ctx);
    errorHasLocationAndFunction(ctx);
    levelsFiltering(ctx);
    disableEnable(ctx);
    getSetLogLevel(ctx);
    getSetEnable(ctx);
    getSetMaxFilesize(ctx);
    flushWritesToDisk(ctx);
    clearLogFileTruncates(ctx);
    setFilenameSwitchesFile(ctx);
    rotatesWhenExceedsMaxSize(ctx);
    multithreadNoInterleave(ctx);

    return printSummary("kas::stream::logger::v1::tests::stream_file_logger", ctx);
}

}

#endif