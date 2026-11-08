/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_THREAD_THREAD_TESTS_H
#define KASLIBCXX_THREAD_THREAD_TESTS_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "KAS/thread/Thread.h"
#include "tests/tests.h"

/**
 * @file ThreadTests.h
 * @brief Unit tests for kas::Thread.
 *
 * The tests cover:
 *  - construction, join, get_id, joinable;
 *  - stack size validation (below / at / above the minimum);
 *  - detach() and its interaction with the destructor;
 *  - join() on a non-joinable thread throws std::system_error;
 *  - move construction and move assignment;
 *  - member swap() and ADL swap();
 *  - kill() on a joinable and a non-joinable thread;
 *  - exception isolation: an exception escaping the callable must
 *    not terminate the process;
 *  - callable forms: function pointer and move-only lambda;
 *  - many threads running concurrently;
 *  - static type properties.
 *
 * Test style:
 *  - each test is a free function taking TestContext&;
 *  - KAS_TEST_CHECK_IMPL records a failure but does not abort the test;
 *  - shared state between the test body and a worker is always
 *    std::atomic to avoid data races;
 *  - waiting for a worker to start is done by polling an atomic flag
 *    with a bounded loop to keep tests deterministic enough on slow
 *    CI machines.
 *
 * @note The tests assume the following Thread contracts:
 *  - get_id() returns 0 for a non-joinable thread;
 *  - join() on a non-joinable thread throws std::system_error;
 *  - kill() follows kill(2) semantics and returns ESRCH when there is
 *    no running thread;
 *  - detach() on a non-joinable thread is a no-op;
 *  - an exception escaping the callable is caught by the thread entry
 *    point and does not call std::terminate.
 */

namespace kas::thread::test_thread::tests {

namespace {
    using namespace kas::tests;
    using kas::Thread;

    // ==================================================================
    //  Construction / join / get_id / joinable
    // ==================================================================

    // --- 1. successful construction + join --------------------------
    /**
     * @brief Verifies basic construction and join().
     *
     * A freshly constructed Thread is joinable and has a non-zero id.
     * After join() it is not joinable, its id becomes 0, and the
     * callable has run exactly once.
     */
    inline void constructAndJoin(TestContext& ctx)
    {
        std::atomic<int> calls { 0 };
        Thread t { [&calls] { ++calls; } };

        KAS_TEST_CHECK_IMPL(ctx, t.joinable());
        KAS_TEST_CHECK_IMPL(ctx, t.get_id() != 0u);

        t.join();
        KAS_TEST_CHECK_IMPL(ctx, !t.joinable());
        KAS_TEST_CHECK_IMPL(ctx, t.get_id() == 0u);
        KAS_TEST_CHECK_IMPL(ctx, calls.load() == 1);
    }

    // --- 2. stack_size < min -> invalid_argument --------------------
    /**
     * @brief Verifies that a stack size below STACK_MIN_SIZE is
     *        rejected with std::invalid_argument.
     */
    inline void stackTooSmall(TestContext& ctx)
    {
        bool caught = false;
        try {
            Thread t { [] { }, 1024 }; // less than STACK_MIN_SIZE
        } catch (const std::invalid_argument&) {
            caught = true;
        }
        KAS_TEST_CHECK_IMPL(ctx, caught);
    }

    // --- 3. stack size == minimum is accepted -----------------------
    /**
     * @brief Verifies that STACK_MIN_SIZE itself is accepted.
     */
    inline void stackAtMin(TestContext& ctx)
    {
        std::atomic<int> calls { 0 };
        Thread t { [&calls] { ++calls; }, Thread::STACK_MIN_SIZE };
        t.join();
        KAS_TEST_CHECK_IMPL(ctx, calls.load() == 1);
    }

    // --- 4. stack size > minimum is accepted ------------------------
    /**
     * @brief Verifies that a stack size above the minimum is accepted.
     */
    inline void stackAboveMin(TestContext& ctx)
    {
        std::atomic<int> calls { 0 };
        Thread t { [&calls] { ++calls; }, Thread::STACK_MIN_SIZE * 4 };
        t.join();
        KAS_TEST_CHECK_IMPL(ctx, calls.load() == 1);
    }

    // --- 5. default ctor produces a non-joinable thread -------------
    /**
     * @brief Verifies that a default-constructed Thread is empty.
     *
     * It must not be joinable and get_id() must return 0.
     */
    inline void defaultConstruct(TestContext& ctx)
    {
        const Thread t;
        KAS_TEST_CHECK_IMPL(ctx, !t.joinable());
        KAS_TEST_CHECK_IMPL(ctx, t.get_id() == 0u);
    }

    // ==================================================================
    //  detach
    // ==================================================================

    // --- 6. detach + the worker still runs --------------------------
    /**
     * @brief Verifies detach() semantics.
     *
     * After detach() the Thread is no longer joinable, and leaving the
     * scope must not call std::terminate. The detached worker still
     * executes the callable; the test waits for it with a bounded poll.
     */
    inline void detachBasic(TestContext& ctx)
    {
        std::atomic<int> calls { 0 };
        {
            Thread t { [&calls] { ++calls; } };
            t.detach();
            KAS_TEST_CHECK_IMPL(ctx, !t.joinable());
        } // destructor must not terminate (detached)

        // wait for the detached thread
        for (int i = 0; i < 100 && calls.load() == 0; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));

        KAS_TEST_CHECK_IMPL(ctx, calls.load() == 1);
    }

    // --- 7. detach on an empty Thread is a no-op --------------------
    /**
     * @brief Verifies that detach() on a non-joinable Thread is a
     *        no-op and does not throw.
     */
    inline void detachEmpty(TestContext& ctx)
    {
        Thread t { [] { } };
        t.join();
        t.detach();
        KAS_TEST_CHECK_IMPL(ctx, !t.joinable());
    }

    // ==================================================================
    //  join on a non-joinable thread throws
    // ==================================================================

    // --- 8. join on an empty Thread -> system_error -----------------
    /**
     * @brief Verifies that a second join() throws std::system_error.
     *
     * The first join() consumes the thread; the second one must throw,
     * matching std::thread behavior.
     */
    inline void joinEmptyThrows(TestContext& ctx)
    {
        Thread t { [] { } };
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
    //  Move semantics
    // ==================================================================

    // --- 9. move ctor leaves the source empty ----------------------
    /**
     * @brief Verifies move construction.
     *
     * After the move the source is not joinable, the target is
     * joinable, the target keeps the same id, and the callable still
     * runs exactly once.
     */
    inline void moveConstruct(TestContext& ctx)
    {
        std::atomic<int> calls { 0 };
        Thread a { [&calls] { ++calls; } };
        const auto id_a = a.get_id();

        Thread b { std::move(a) };
        KAS_TEST_CHECK_IMPL(ctx, !a.joinable());
        KAS_TEST_CHECK_IMPL(ctx, b.joinable());
        KAS_TEST_CHECK_IMPL(ctx, b.get_id() == id_a);

        b.join();
        KAS_TEST_CHECK_IMPL(ctx, calls.load() == 1);
    }

    // --- 10. move assignment ----------------------------------------
    /**
     * @brief Verifies move assignment.
     *
     * The target is first joined so it is not joinable at the moment
     * of assignment. After the move, the source is not joinable, the
     * target is joinable, and the callable runs exactly once.
     */
    inline void moveAssign(TestContext& ctx)
    {
        std::atomic<int> calls { 0 };
        Thread a { [&calls] { ++calls; } };
        Thread b { [] { } };
        b.join();

        b = std::move(a);
        KAS_TEST_CHECK_IMPL(ctx, !a.joinable());
        KAS_TEST_CHECK_IMPL(ctx, b.joinable());

        b.join();
        KAS_TEST_CHECK_IMPL(ctx, calls.load() == 1);
    }

    // ==================================================================
    //  swap
    // ==================================================================

    // --- 11. swap two threads ---------------------------------------
    /**
     * @brief Verifies member swap().
     *
     * After swap() the ids are exchanged, and both callables still run
     * exactly once.
     */
    inline void swapBasic(TestContext& ctx)
    {
        std::atomic<int> calls_a { 0 };
        std::atomic<int> calls_b { 0 };

        Thread a { [&calls_a] { ++calls_a; } };
        Thread b { [&calls_b] { ++calls_b; } };

        const auto id_a = a.get_id();
        const auto id_b = b.get_id();

        a.swap(b);

        KAS_TEST_CHECK_IMPL(ctx, a.get_id() == id_b);
        KAS_TEST_CHECK_IMPL(ctx, b.get_id() == id_a);

        a.join();
        b.join();
        KAS_TEST_CHECK_IMPL(ctx, calls_a.load() == 1);
        KAS_TEST_CHECK_IMPL(ctx, calls_b.load() == 1);
    }

    // --- 12. ADL swap -----------------------------------------------
    /**
     * @brief Verifies that swap() is found via ADL (kas::swap).
     *
     * Unqualified swap(a, b) must exchange the ids as expected.
     */
    inline void adlSwap(TestContext& ctx)
    {
        Thread a { [] { } };
        Thread b { [] { } };
        const auto id_a = a.get_id();
        const auto id_b = b.get_id();

        swap(a, b); // via ADL (kas::swap)

        KAS_TEST_CHECK_IMPL(ctx, a.get_id() == id_b);
        KAS_TEST_CHECK_IMPL(ctx, b.get_id() == id_a);

        a.join();
        b.join();
    }

    // ==================================================================
    //  kill
    // ==================================================================

    // --- 13. kill on a non-joinable thread -> ESRCH -----------------
    /**
     * @brief Verifies that kill(0) on a non-running thread returns
     *        ESRCH (no such process/thread).
     *
     * @note This is a contract of kas::Thread::kill(), not of POSIX in
     *       general: the implementation is expected to map "no thread"
     *       to ESRCH.
     */
    inline void killEmpty(TestContext& ctx)
    {
        Thread t { [] { } };
        t.join();
        const int rc = t.kill(0);
        KAS_TEST_CHECK_IMPL(ctx, rc == ESRCH);
    }

    // --- 14. kill(0) on a joinable thread -> 0 ----------------------
    /**
     * @brief Verifies that kill(0) on a running thread succeeds.
     *
     * Signal 0 performs only permission/existence checking and does
     * not deliver a signal. The test waits until the worker has
     * started, then expects kill(0) == 0.
     */
    inline void killJoinable(TestContext& ctx)
    {
        std::atomic<bool> started { false };
        Thread t { [&started] {
            started.store(true);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        } };

        for (int i = 0; i < 200 && !started.load(); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

        const int rc = t.kill(0);
        KAS_TEST_CHECK_IMPL(ctx, rc == 0);

        t.join();
    }

    // ==================================================================
    //  Exception in the callable - must not terminate
    // ==================================================================

    // --- 15. exception in a thread is swallowed ---------------------
    /**
     * @brief Verifies exception isolation.
     *
     * The callable throws; the thread entry point must catch the
     * exception and not call std::terminate. The test checks that the
     * callable ran (the "ran" flag is set before the throw), and that
     * the exception was logged by the implementation and attributed to
     * the worker thread (not the test thread).
     */
    inline void exceptionInThread(TestContext& ctx)
    {
        CerrMuteThreaded mute;

        std::atomic<bool> ran { false };
        {
            Thread t { [&ran] {
                ran.store(true);
                throw std::runtime_error("thread boom");
            } };
            t.join();
        }

        KAS_TEST_CHECK_IMPL(ctx, ran.load());

        // The thread logs the exception; CerrMuteThreaded must have captured it.
        const auto text = mute.str();
        KAS_TEST_CHECK_IMPL(ctx, text.find("thread boom") != std::string::npos);

        // The capture is attributed to the worker thread, not the test thread.
        const auto entries = mute.entries();
        KAS_TEST_CHECK_IMPL(ctx, !entries.empty());
        for (const auto& e : entries) {
            KAS_TEST_CHECK_IMPL(ctx, e.tid != std::thread::id { });
            KAS_TEST_CHECK_IMPL(ctx, e.tid != std::this_thread::get_id());
        }
    }

    // ==================================================================
    //  Callable forms: function pointer / move-only
    // ==================================================================

    /**
     * @brief Global counter for the free-function callable test.
     *
     * A free function cannot capture state, so the counter is a
     * namespace-scope atomic.
     */
    std::atomic<int> g_free_calls { 0 };
    void freeFunc() { ++g_free_calls; }

    // --- 16. function pointer ---------------------------------------
    /**
     * @brief Verifies that a plain function pointer is accepted as a
     *        callable.
     */
    inline void functionPointer(TestContext& ctx)
    {
        g_free_calls.store(0);
        Thread t { &freeFunc };
        t.join();
        KAS_TEST_CHECK_IMPL(ctx, g_free_calls.load() == 1);
    }

    // --- 17. move-only lambda (unique_ptr) --------------------------
    /**
     * @brief Verifies that a move-only callable is accepted.
     *
     * The lambda captures std::unique_ptr by move, so it is not
     * copyable. Thread must still accept it.
     */
    inline void moveOnlyLambda(TestContext& ctx)
    {
        std::atomic<int> calls { 0 };
        auto ptr = std::make_unique<int>(0);

        Thread t { [&calls, p = std::move(ptr)] {
            ++(*p);
            calls.fetch_add(1, std::memory_order_relaxed);
        } };
        t.join();

        KAS_TEST_CHECK_IMPL(ctx, calls.load() == 1);
    }

    // ==================================================================
    //  Many threads
    // ==================================================================

    // --- 18. N threads all run --------------------------------------
    /**
     * @brief Light concurrency test with 8 threads.
     *
     * Each thread increments a shared counter; after joining all of
     * them, the counter must equal the number of threads.
     */
    inline void manyThreads(TestContext& ctx)
    {
        std::atomic<int> total { 0 };
        constexpr int kThreads = 8;

        std::vector<Thread> threads;
        threads.reserve(kThreads);
        for (int i = 0; i < kThreads; ++i) {
            threads.emplace_back([&total] {
                total.fetch_add(1, std::memory_order_relaxed);
            });
        }
        for (auto& t : threads)
            t.join();

        KAS_TEST_CHECK_IMPL(ctx, total.load() == kThreads);
    }

    // ==================================================================
    //  Type properties
    // ==================================================================

    /**
     * @brief Static type-level contracts of Thread.
     *
     * Thread must be nothrow default-constructible and nothrow
     * movable, and must not be copyable.
     */
    static_assert(std::is_nothrow_default_constructible_v<Thread>);
    static_assert(std::is_nothrow_move_constructible_v<Thread>);
    static_assert(std::is_nothrow_move_assignable_v<Thread>);
    static_assert(!std::is_copy_constructible_v<Thread>);
    static_assert(!std::is_copy_assignable_v<Thread>);

} // namespace

// ============================================================
//  Entry point
// ============================================================
/**
 * @brief Runs all Thread unit tests.
 *
 * Tests are executed in a fixed order: construction, detach, join,
 * move semantics, swap, kill, exception isolation, callable forms,
 * many threads. The summary is printed via printSummary() at the end.
 */
inline bool runTests()
{
    TestContext ctx;

    constructAndJoin(ctx);
    stackTooSmall(ctx);
    stackAtMin(ctx);
    stackAboveMin(ctx);
    defaultConstruct(ctx);

    detachBasic(ctx);
    detachEmpty(ctx);

    joinEmptyThrows(ctx);

    moveConstruct(ctx);
    moveAssign(ctx);

    swapBasic(ctx);
    adlSwap(ctx);

    killEmpty(ctx);
    killJoinable(ctx);

    exceptionInThread(ctx);

    functionPointer(ctx);
    moveOnlyLambda(ctx);

    manyThreads(ctx);

    return printSummary("kas::thread::test_thread::tests", ctx);
}

} // namespace kas::thread::test_thread::tests

#endif