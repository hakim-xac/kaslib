/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_THREAD_JTHREAD_TESTS_H
#define KASLIBCXX_THREAD_JTHREAD_TESTS_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <stop_token>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "KAS/thread/JThread.h"
#include "tests/tests.h"

/**
 * @file JThreadTests.h
 * @brief Unit tests for kas::JThread.
 *
 * The tests cover:
 *  - construction from function_t, function_stop_t and the template ctor;
 *  - get_stop_token() / request_stop() and cooperative cancellation;
 *  - destructor behavior (request_stop + join, RAII);
 *  - move construction and move assignment (including stop_token transfer);
 *  - kill(), joinable(), get_id();
 *  - static type properties (nothrow move, non-copyable).
 *
 * Test style:
 *  - each test is a free function taking TestContext&;
 *  - KAS_TEST_CHECK_IMPL records a failure but does not abort the test;
 *  - shared state between the test body and the worker thread is always
 *    std::atomic to avoid data races;
 *  - waiting for a worker to start is done by polling an atomic flag
 *    with a bounded loop (200 x 1 ms) to keep tests deterministic enough
 *    on slow CI machines.
 *
 * @note The tests assume the following contracts of kas::Thread:
 *  - Thread::kill() returns ESRCH when there is no running thread;
 *  - Thread::join() throws std::system_error when the thread is not
 *    joinable (same as std::thread).
 */

namespace kas::thread::test_jthread::tests {

namespace {
    using namespace kas::tests;
    using kas::JThread;

    // ==================================================================
    //  Construction
    // ==================================================================

    // --- 1. function_t (no stop token) ------------------------------
    /**
     * @brief Constructs JThread from a no-argument callable.
     *
     * Verifies that the thread is joinable right after construction,
     * becomes non-joinable after join(), and that the callable runs
     * exactly once.
     */
    inline void ctorFunctionT(TestContext& ctx)
    {
        std::atomic<int> calls { 0 };
        JThread t { [&calls] { ++calls; } };
        KAS_TEST_CHECK_IMPL(ctx, t.joinable());

        t.join();
        KAS_TEST_CHECK_IMPL(ctx, !t.joinable());
        KAS_TEST_CHECK_IMPL(ctx, calls.load() == 1);
    }

    // --- 2. function_stop_t (with stop token) -----------------------
    /**
     * @brief Constructs JThread from a callable taking std::stop_token.
     *
     * Verifies that the callable receives a token and runs exactly once.
     *
     * @note Because the lambda is also invocable with std::stop_token,
     *       overload resolution may select the template constructor
     *       instead of the function_stop_t overload. To test the
     *       function_stop_t overload explicitly, wrap the lambda in
     *       JThread::function_stop_t first.
     */
    inline void ctorFunctionStopT(TestContext& ctx)
    {
        std::atomic<int> calls { 0 };
        JThread t { [&calls](std::stop_token) { ++calls; } };
        KAS_TEST_CHECK_IMPL(ctx, t.joinable());

        t.join();
        KAS_TEST_CHECK_IMPL(ctx, calls.load() == 1);
    }

    // --- 3. template ctor (lambda with stop_token) ------------------
    /**
     * @brief Constructs JThread via the template constructor.
     *
     * The lambda accepts std::stop_token, so the template ctor with
     * std::invocable<F, std::stop_token> is selected. Verifies that
     * the callable runs exactly once.
     */
    inline void ctorTemplateWithStopToken(TestContext& ctx)
    {
        std::atomic<int> calls { 0 };
        JThread t { [&calls](std::stop_token) { ++calls; } };
        t.join();
        KAS_TEST_CHECK_IMPL(ctx, calls.load() == 1);
    }

    // ==================================================================
    //  get_stop_token / request_stop
    // ==================================================================

    // --- 4. get_stop_token + request_stop --------------------------
    /**
     * @brief Verifies that get_stop_token() returns a token that
     *        observes request_stop().
     *
     * The worker only sleeps; the test checks that the token obtained
     * before request_stop() reflects the stop request afterwards.
     */
    inline void getStopToken(TestContext& ctx)
    {
        JThread t { [](std::stop_token) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        } };

        const auto token = t.get_stop_token();
        KAS_TEST_CHECK_IMPL(ctx, !token.stop_requested());

        t.request_stop();
        KAS_TEST_CHECK_IMPL(ctx, token.stop_requested());

        t.join();
    }

    // --- 5. request_stop from outside -> worker exits ---------------
    /**
     * @brief Verifies cooperative cancellation from another thread.
     *
     * The worker spins until stop_requested() becomes true, then sets
     * a flag. The test waits until the worker has started, calls
     * request_stop(), joins, and checks both flags.
     */
    inline void requestStopFromOutside(TestContext& ctx)
    {
        std::atomic<bool> started { false };
        std::atomic<bool> stopped { false };

        JThread t { [&started, &stopped](std::stop_token token) {
            started.store(true);
            while (!token.stop_requested())
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            stopped.store(true);
        } };

        for (int i = 0; i < 200 && !started.load(); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

        t.request_stop();
        t.join();

        KAS_TEST_CHECK_IMPL(ctx, started.load());
        KAS_TEST_CHECK_IMPL(ctx, stopped.load());
    }

    // ==================================================================
    //  ~JThread - request_stop + join
    // ==================================================================

    // --- 6. destructor stops and joins -----------------------------
    /**
     * @brief Verifies RAII behavior of ~JThread.
     *
     * The JThread lives in an inner scope; leaving the scope must call
     * request_stop() and join() so that the worker observes the stop
     * request and sets its flag before the outer check runs.
     *
     * @warning If the destructor does not stop the worker, this test
     *          will hang (the worker loops forever). That is intentional:
     *          a hang is a clear failure signal for RAII.
     */
    inline void dtorStopsAndJoins(TestContext& ctx)
    {
        std::atomic<bool> stopped { false };
        {
            JThread t { [&stopped](std::stop_token token) {
                while (!token.stop_requested())
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                stopped.store(true);
            } };
        }
        KAS_TEST_CHECK_IMPL(ctx, stopped.load());
    }

    // ==================================================================
    //  Move semantics
    // ==================================================================

    // --- 7. move ctor function_t ----------------------------------
    /**
     * @brief Verifies move construction for a no-token callable.
     *
     * After the move:
     *  - the source is not joinable;
     *  - the target is joinable;
     *  - the target keeps the same thread id;
     *  - the callable still runs exactly once.
     */
    inline void moveConstructFunctionT(TestContext& ctx)
    {
        std::atomic<int> calls { 0 };
        JThread a { [&calls] { ++calls; } };
        const auto id_a = a.get_id();

        JThread b { std::move(a) };
        KAS_TEST_CHECK_IMPL(ctx, !a.joinable());
        KAS_TEST_CHECK_IMPL(ctx, b.joinable());
        KAS_TEST_CHECK_IMPL(ctx, b.get_id() == id_a);

        b.join();
        KAS_TEST_CHECK_IMPL(ctx, calls.load() == 1);
    }

    // --- 8. move ctor function_stop_t - token is transferred ------
    /**
     * @brief Verifies move construction for a stop-token callable.
     *
     * The worker is started via the source object, then the JThread is
     * moved. The target must own the thread and its stop_source, so
     * request_stop() on the target must reach the worker.
     *
     * @note As in ctorFunctionStopT, the lambda may select the template
     *       constructor rather than the function_stop_t overload.
     */
    inline void moveConstructFunctionStopT(TestContext& ctx)
    {
        std::atomic<bool> started { false };
        std::atomic<bool> stopped { false };

        JThread a { [&started, &stopped](std::stop_token token) {
            started.store(true);
            while (!token.stop_requested())
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            stopped.store(true);
        } };

        for (int i = 0; i < 200 && !started.load(); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

        JThread b { std::move(a) };
        KAS_TEST_CHECK_IMPL(ctx, !a.joinable());
        KAS_TEST_CHECK_IMPL(ctx, b.joinable());

        b.request_stop();
        b.join();
        KAS_TEST_CHECK_IMPL(ctx, stopped.load());
    }

    // --- 9. move assign function_t -------------------------------
    /**
     * @brief Verifies move assignment for a no-token callable.
     *
     * The target is first joined so it is not joinable at the moment of
     * assignment. After the move, the source is not joinable, the target
     * is joinable, and the callable runs exactly once.
     */
    inline void moveAssignFunctionT(TestContext& ctx)
    {
        std::atomic<int> calls { 0 };
        JThread a { [&calls] { ++calls; } };
        JThread b { [] { } };
        b.join();

        b = std::move(a);
        KAS_TEST_CHECK_IMPL(ctx, !a.joinable());
        KAS_TEST_CHECK_IMPL(ctx, b.joinable());

        b.join();
        KAS_TEST_CHECK_IMPL(ctx, calls.load() == 1);
    }

    // --- 10. move assign function_stop_t ------------------------
    /**
     * @brief Verifies move assignment for a stop-token callable.
     *
     * Same shape as moveAssignFunctionT, but the worker is cancellable
     * and must observe request_stop() issued through the target object
     * after the move.
     */
    inline void moveAssignFunctionStopT(TestContext& ctx)
    {
        std::atomic<bool> stopped { false };

        JThread a { [&stopped](std::stop_token token) {
            while (!token.stop_requested())
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            stopped.store(true);
        } };

        JThread b { [] { } };
        b.join();

        b = std::move(a);
        KAS_TEST_CHECK_IMPL(ctx, !a.joinable());
        KAS_TEST_CHECK_IMPL(ctx, b.joinable());

        b.request_stop();
        b.join();
        KAS_TEST_CHECK_IMPL(ctx, stopped.load());
    }

    // ==================================================================
    //  kill / joinable / get_id
    // ==================================================================

    // --- 11. kill(0) on a joinable thread -> 0 -------------------
    /**
     * @brief Verifies that kill(0) on a running thread succeeds.
     *
     * Signal 0 performs only permission/existence checking and does not
     * deliver a signal. The test waits until the worker has started,
     * then expects kill(0) == 0.
     *
     * @note This relies on the kas::Thread::kill() contract: the return
     *       value follows kill(2) semantics (0 on success, errno-like
     *       value on failure).
     */
    inline void killJoinable(TestContext& ctx)
    {
        std::atomic<bool> started { false };
        JThread t { [&started](std::stop_token) {
            started.store(true);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        } };

        for (int i = 0; i < 200 && !started.load(); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

        KAS_TEST_CHECK_IMPL(ctx, t.kill(0) == 0);
        t.request_stop();
        t.join();
    }

    // --- 12. kill on a joined thread -> ESRCH --------------------
    /**
     * @brief Verifies that kill(0) on a non-running thread reports ESRCH.
     *
     * After join() the JThread has no running thread, so kill() must
     * return ESRCH (no such process/thread).
     *
     * @note This is a contract of kas::Thread::kill(), not of POSIX in
     *       general: the implementation is expected to map "no thread"
     *       to ESRCH.
     */
    inline void killEmpty(TestContext& ctx)
    {
        JThread t { [] { } };
        t.join();
        KAS_TEST_CHECK_IMPL(ctx, t.kill(0) == ESRCH);
    }

    // --- 13. join on a non-joinable thread -> system_error -------
    /**
     * @brief Verifies that join() on a non-joinable JThread throws.
     *
     * The first join() consumes the thread; the second join() must
     * throw std::system_error, matching std::thread behavior.
     *
     * @note This fixes the exception type contract of kas::Thread::join().
     */
    inline void joinEmptyThrows(TestContext& ctx)
    {
        JThread t { [] { } };
        t.join();

        bool caught = false;
        try {
            t.join();
        } catch (const std::system_error&) {
            caught = true;
        }
        KAS_TEST_CHECK_IMPL(ctx, caught);
    }

    // ==================================================================
    //  Type properties
    // ==================================================================

    /**
     * @brief Static type-level contracts of JThread.
     *
     * JThread must be movable without throwing, and must not be copyable.
     */
    static_assert(std::is_nothrow_move_constructible_v<JThread>);
    static_assert(std::is_nothrow_move_assignable_v<JThread>);
    static_assert(!std::is_copy_constructible_v<JThread>);
    static_assert(!std::is_copy_assignable_v<JThread>);

} // namespace

// ============================================================
//  Entry point
// ============================================================
/**
 * @brief Runs all JThread unit tests.
 *
 * Tests are executed in a fixed order: construction, stop-token
 * handling, destructor, move semantics, kill/joinable/get_id.
 * The summary is printed via printSummary() at the end.
 */
inline bool runTests()
{
    TestContext ctx;

    ctorFunctionT(ctx);
    ctorFunctionStopT(ctx);
    ctorTemplateWithStopToken(ctx);

    getStopToken(ctx);
    requestStopFromOutside(ctx);

    dtorStopsAndJoins(ctx);

    moveConstructFunctionT(ctx);
    moveConstructFunctionStopT(ctx);
    moveAssignFunctionT(ctx);
    moveAssignFunctionStopT(ctx);

    killJoinable(ctx);
    killEmpty(ctx);
    joinEmptyThrows(ctx);

    return printSummary("kas::thread::test_jthread::tests", ctx);
}

} // namespace kas::thread::test_jthread::tests

#endif