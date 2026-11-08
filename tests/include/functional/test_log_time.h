/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_FUNCTIONAL_LOG_TIME_TESTS_H
#define KASLIBCXX_FUNCTIONAL_LOG_TIME_TESTS_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

#include "KAS/functional/LogTime.h"
#include "tests/tests.h"

/**
 * @brief Tests for kas::functional::LogTime.
 */
namespace kas::functional::log_time::tests {

namespace {
    using namespace kas::tests;
    using kas::functional::LogTime;

    // ==================================================================
    // Construction / callback on destruction
    // ==================================================================

    // --- 1. callback is invoked on destruction -----------------------
    inline void callbackOnDestruct(TestContext& ctx)
    {
        int64_t observed = -1;
        {
            LogTime log { [&observed](int64_t diff) { observed = diff; } };
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } // destructor -> callback
        KAS_TEST_CHECK_IMPL(ctx, observed >= 0);
    }

    // --- 2. explicit printDifferenceTime -----------------------------
    inline void explicitPrint(TestContext& ctx)
    {
        int calls = 0;
        LogTime log { [&calls](int64_t) { ++calls; } };
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        log.printDifferenceTime();
        KAS_TEST_CHECK_IMPL(ctx, calls == 1);
    }

    // --- 3. printDifferenceTime returns *this ------------------------
    inline void printReturnsSelf(TestContext& ctx)
    {
        LogTime log { [](int64_t) { } };
        auto& ref = log.printDifferenceTime();
        KAS_TEST_CHECK_IMPL(ctx, &ref == &log);
    }

    // --- 4. printDifferenceTime can be called multiple times ---------
    inline void printMultipleTimes(TestContext& ctx)
    {
        int calls = 0;
        LogTime log { [&calls](int64_t) { ++calls; } };
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        log.printDifferenceTime();
        log.printDifferenceTime();
        log.printDifferenceTime();
        KAS_TEST_CHECK_IMPL(ctx, calls == 3);
    }

    // ==================================================================
    // resetTimePoint
    // ==================================================================

    // --- 5. after resetTimePoint the callback is not invoked ---------
    inline void resetSkipsCallback(TestContext& ctx)
    {
        int calls = 0;
        LogTime log { [&calls](int64_t) { ++calls; } };
        log.resetTimePoint();
        log.printDifferenceTime();
        KAS_TEST_CHECK_IMPL(ctx, calls == 0);
    }

    // --- 6. resetTimePoint returns *this -----------------------------
    inline void resetReturnsSelf(TestContext& ctx)
    {
        LogTime log { [](int64_t) { } };
        auto& ref = log.resetTimePoint();
        KAS_TEST_CHECK_IMPL(ctx, &ref == &log);
    }

    // ==================================================================
    // setNewTimePoint
    // ==================================================================

    // --- 7. setNewTimePoint with an explicit time point --------------
    inline void setExplicitTimePoint(TestContext& ctx)
    {
        int64_t observed = -1;
        LogTime<std::chrono::milliseconds> log {
            [&observed](int64_t diff) { observed = diff; }
        };

        const auto past = std::chrono::steady_clock::now() - std::chrono::seconds(1);
        log.setNewTimePoint(past);
        log.printDifferenceTime();

        KAS_TEST_CHECK_IMPL(ctx, observed >= 1000);
    }

    // --- 8. setNewTimePoint after resetTimePoint ---------------------
    inline void setAfterReset(TestContext& ctx)
    {
        int64_t observed = -1;
        LogTime<std::chrono::milliseconds> log {
            [&observed](int64_t diff) { observed = diff; }
        };
        log.resetTimePoint();
        log.setNewTimePoint();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        log.printDifferenceTime();
        KAS_TEST_CHECK_IMPL(ctx, observed >= 0);
    }

    // --- 9. setNewTimePoint returns *this ----------------------------
    inline void setReturnsSelf(TestContext& ctx)
    {
        LogTime log { [](int64_t) { } };
        auto& ref = log.setNewTimePoint();
        KAS_TEST_CHECK_IMPL(ctx, &ref == &log);
    }

    // ==================================================================
    // Different time units (TTime)
    // ==================================================================

    // --- 10. seconds - difference of about 2 seconds -----------------
    inline void secondsUnit(TestContext& ctx)
    {
        int64_t observed = -1;
        LogTime<std::chrono::seconds> log {
            [&observed](int64_t diff) { observed = diff; }
        };
        const auto past = std::chrono::steady_clock::now() - std::chrono::seconds(2);
        log.setNewTimePoint(past);
        log.printDifferenceTime();
        KAS_TEST_CHECK_IMPL(ctx, observed >= 2);
    }

    // --- 11. microseconds - difference > 5000 ------------------------
    inline void microsecondsUnit(TestContext& ctx)
    {
        int64_t observed = -1;
        LogTime<std::chrono::microseconds> log {
            [&observed](int64_t diff) { observed = diff; }
        };
        const auto past = std::chrono::steady_clock::now() - std::chrono::milliseconds(5);
        log.setNewTimePoint(past);
        log.printDifferenceTime();
        KAS_TEST_CHECK_IMPL(ctx, observed >= 5000);
    }

    // ==================================================================
    // Callback type and move-only
    // ==================================================================

    // --- 12. callback takes int64_t ----------------------------------
    inline void callbackType(TestContext& ctx)
    {
        LogTime log { [](int64_t) { } };
        KAS_TEST_CHECK_IMPL(ctx, true); // just compiles
    }

    // --- 13. move-only lambda (unique_ptr) ---------------------------
    inline void moveOnlyCallback(TestContext& ctx)
    {
        auto ptr = std::make_unique<int>(0);
        {
            LogTime log { [p = std::move(ptr)](int64_t) { ++(*p); } };
            log.printDifferenceTime();
        }
        KAS_TEST_CHECK_IMPL(ctx, true);
    }

    // ==================================================================
    // Type properties
    // ==================================================================
    static_assert(std::is_move_constructible_v<LogTime<>>);
    static_assert(!std::is_copy_constructible_v<LogTime<>>);
    static_assert(!std::is_copy_assignable_v<LogTime<>>);

    // ==================================================================
    // Throwing callback - swallowed
    // ==================================================================

    // --- 14. callback throws std::exception - swallowed --------------
    inline void callbackThrowsStdException(TestContext& ctx)
    {
        CerrMute mute;
        int calls = 0;
        {
            LogTime log { [&calls](int64_t) { ++calls; throw std::runtime_error("boom"); } };
            log.printDifferenceTime();
        }
        KAS_TEST_CHECK_IMPL(ctx, calls == 2);
        KAS_TEST_CHECK_IMPL(ctx, mute.str().find("boom") != std::string::npos);
    }

    // --- 15. callback throws non-std::exception - swallowed ----------
    inline void callbackThrowsUnknown(TestContext& ctx)
    {
        CerrMute mute;
        int calls = 0;
        {
            LogTime log { [&calls](int64_t) {
                ++calls;
                throw 42; // not a std::exception
            } };
            log.printDifferenceTime();
        }
        KAS_TEST_CHECK_IMPL(ctx, calls == 2);
        KAS_TEST_CHECK_IMPL(ctx, mute.str().find("unknown exception in LogTime callback") != std::string::npos);
    }

} // namespace

// ============================================================
// Entry point
// ============================================================
/**
 * @brief Runs all LogTime tests and prints a summary.
 */
inline bool runTests()
{
    TestContext ctx;

    callbackOnDestruct(ctx);
    explicitPrint(ctx);
    printReturnsSelf(ctx);
    printMultipleTimes(ctx);

    resetSkipsCallback(ctx);
    resetReturnsSelf(ctx);

    setExplicitTimePoint(ctx);
    setAfterReset(ctx);
    setReturnsSelf(ctx);

    secondsUnit(ctx);
    microsecondsUnit(ctx);

    callbackType(ctx);
    moveOnlyCallback(ctx);

    callbackThrowsStdException(ctx);
    callbackThrowsUnknown(ctx);

    return printSummary("kas::functional::log_time::tests", ctx);
}

} // namespace kas::functional::log_time::tests

#endif