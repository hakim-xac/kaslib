/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_THREAD_THREAD_POOL_TESTS_H
#define KASLIBCXX_THREAD_THREAD_POOL_TESTS_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include "KAS/thread/ThreadPool.h"
#include "tests/tests.h"

/**
 * @file ThreadPoolTests.h
 * @brief Unit tests for kas::ThreadPool.
 *
 * The tests cover:
 *  - start() / stop() / wait() lifecycle and their interaction;
 *  - push_task() return value in running and non-running states;
 *  - actual task execution (single, many, parallel);
 *  - exception isolation: a throwing task must not kill the pool;
 *  - submit() and std::future: result, arguments, not-running error,
 *    many concurrent futures;
 *  - a light stress test with multiple workers;
 *  - static type properties (non-copyable, non-movable).
 *
 * Test style:
 *  - each test is a free function taking TestContext&;
 *  - KAS_TEST_CHECK_IMPL records a failure but does not abort the test;
 *  - shared state between the test body and workers is always
 *    std::atomic to avoid data races;
 *  - waiting for a worker is done by polling an atomic flag with a
 *    bounded loop (200 x 1 ms, or larger for stress tests) to keep
 *    tests deterministic enough on slow CI machines.
 *
 * @note The tests rely on the following ThreadPool contracts:
 *  - push_task() returns false when the pool is not running;
 *  - submit() throws std::runtime_error when the pool is not running;
 *  - stop() does not discard tasks that are already in the queue:
 *    they are still executed, and wait() waits for them;
 *  - a second start() while running is a no-op;
 *  - exceptions thrown by a task do not terminate workers; they are
 *    logged/reported by the implementation (see exceptionInTask).
 */

namespace kas::thread::test_thread_pool::tests {

namespace {
    using namespace kas::tests;
    using kas::ThreadPool;

    // ==================================================================
    //  start / stop / wait
    // ==================================================================

    // --- 1. empty pool - push_task returns false -------------------
    /**
     * @brief Verifies that a pool with zero workers rejects tasks.
     *
     * A pool with count_thread == 0 has no workers to run tasks.
     * start() must succeed (nothing to start), and push_task() must
     * return false because the pool cannot accept work.
     */
    inline void emptyPoolPushFails(TestContext& ctx)
    {
        ThreadPool pool { 0 };
        pool.start();

        KAS_TEST_CHECK_IMPL(ctx, !pool.push_task([] { }));
    }

    // --- 2. push_task before start - false -------------------------
    /**
     * @brief Verifies that push_task() fails before start().
     *
     * The pool is constructed but not started; push_task() must
     * return false because the pool is not running.
     */
    inline void pushBeforeStartFails(TestContext& ctx)
    {
        ThreadPool pool { 2 };
        KAS_TEST_CHECK_IMPL(ctx, !pool.push_task([] { }));
    }

    // --- 3. push_task after stop - false ---------------------------
    /**
     * @brief Verifies that push_task() fails after stop().
     *
     * After stop() the pool is not running, so push_task() must return
     * false. wait() is called afterwards to ensure the pool is drained
     * before destruction.
     */
    inline void pushAfterStopFails(TestContext& ctx)
    {
        ThreadPool pool { 2 };
        pool.start();
        pool.stop();

        KAS_TEST_CHECK_IMPL(ctx, !pool.push_task([] { }));
        pool.wait();
    }

    // ==================================================================
    //  Task execution
    // ==================================================================

    // --- 4. one task is executed -----------------------------------
    /**
     * @brief Verifies that a single pushed task is executed.
     *
     * After push_task() returns true, the test polls the counter until
     * the task has run (bounded wait), then checks the counter and
     * drains the pool with wait().
     */
    inline void oneTask(TestContext& ctx)
    {
        ThreadPool pool { 2 };
        pool.start();

        std::atomic<int> calls { 0 };
        KAS_TEST_CHECK_IMPL(ctx, pool.push_task([&calls] { ++calls; }));

        // wait for completion
        for (int i = 0; i < 200 && calls.load() == 0; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

        KAS_TEST_CHECK_IMPL(ctx, calls.load() == 1);
        pool.wait();
    }

    // --- 5. all N tasks are executed -------------------------------
    /**
     * @brief Verifies that all pushed tasks are eventually executed.
     *
     * Pushes kTasks trivial increments and waits (bounded) until the
     * counter reaches kTasks. Uses relaxed memory order because the
     * counter is only used as a completion signal, not for ordering.
     */
    inline void manyTasks(TestContext& ctx)
    {
        ThreadPool pool { 4 };
        pool.start();

        std::atomic<int> calls { 0 };
        constexpr int kTasks = 100;

        for (int i = 0; i < kTasks; ++i)
            KAS_TEST_CHECK_IMPL(ctx, pool.push_task([&calls] {
                calls.fetch_add(1, std::memory_order_relaxed);
            }));

        for (int i = 0; i < 1000 && calls.load() < kTasks; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

        KAS_TEST_CHECK_IMPL(ctx, calls.load() == kTasks);
        pool.wait();
    }

    // --- 6. tasks run in parallel ----------------------------------
    /**
     * @brief Verifies that more than one worker executes tasks
     *        concurrently.
     *
     * Four tasks each increment a shared "running" counter, record the
     * observed maximum via a CAS loop, sleep, and decrement. If at
     * least two tasks overlapped, max_running >= 2.
     *
     * @note On a single-core machine or with a heavily loaded CI this
     *       test can be flaky; the threshold is deliberately low.
     */
    inline void tasksParallel(TestContext& ctx)
    {
        ThreadPool pool { 4 };
        pool.start();

        std::atomic<int> running { 0 };
        std::atomic<int> max_running { 0 };

        for (int i = 0; i < 4; ++i)
            pool.push_task([&running, &max_running] {
                const int now = running.fetch_add(1) + 1;
                int expected = max_running.load();
                while (now > expected
                    && !max_running.compare_exchange_weak(expected, now)) {
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                running.fetch_sub(1);
            });

        pool.wait();

        KAS_TEST_CHECK_IMPL(ctx, max_running.load() >= 2); // at least 2 in parallel
    }

    // --- 7. exception in a task - pool keeps working ---------------
    /**
     * @brief Verifies that a throwing task does not kill the pool.
     *
     * Pushes a task that throws and a normal task right after it.
     * The normal task must still run, and the pool must remain usable.
     * The exception is expected to be logged by the implementation;
     * CerrMuteThreaded captures it together with the worker thread id.
     */
    inline void exceptionInTask(TestContext& ctx)
    {
        CerrMuteThreaded mute;

        ThreadPool pool { 2 };
        pool.start();

        std::atomic<int> calls { 0 };

        pool.push_task([] { throw std::runtime_error("boom"); });
        pool.push_task([&calls] { ++calls; });

        for (int i = 0; i < 200 && calls.load() == 0; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

        KAS_TEST_CHECK_IMPL(ctx, calls.load() == 1);
        pool.wait();

        // The pool logs the exception; CerrMuteThreaded must have captured it.
        const auto text = mute.str();
        KAS_TEST_CHECK_IMPL(ctx, text.find("boom") != std::string::npos);

        // The capture is attributed to a worker thread, not the test thread.
        const auto entries = mute.entries();
        KAS_TEST_CHECK_IMPL(ctx, !entries.empty());
        for (const auto& e : entries) {
            KAS_TEST_CHECK_IMPL(ctx, e.tid != std::thread::id { });
            KAS_TEST_CHECK_IMPL(ctx, e.tid != std::this_thread::get_id());
        }
    }

    // ==================================================================
    //  stop / wait
    // ==================================================================

    // --- 8. wait drains all queued tasks ---------------------------
    /**
     * @brief Verifies that wait() blocks until all queued tasks finish.
     *
     * Pushes 50 increments and calls wait() without any polling. After
     * wait() returns, all 50 tasks must have completed.
     */
    inline void waitDrainsQueue(TestContext& ctx)
    {
        ThreadPool pool { 2 };
        pool.start();

        std::atomic<int> calls { 0 };
        for (int i = 0; i < 50; ++i)
            pool.push_task([&calls] { ++calls; });

        pool.wait();
        KAS_TEST_CHECK_IMPL(ctx, calls.load() == 50);
    }

    // --- 9. second start - no-op -----------------------------------
    /**
     * @brief Verifies that start() while already running is a no-op.
     *
     * The pool must not spawn duplicate workers or reset state. A task
     * pushed after the second start() must run exactly once.
     */
    inline void startTwice(TestContext& ctx)
    {
        ThreadPool pool { 2 };
        pool.start();
        pool.start(); // second call is a no-op

        std::atomic<int> calls { 0 };
        pool.push_task([&calls] { ++calls; });

        for (int i = 0; i < 200 && calls.load() == 0; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

        KAS_TEST_CHECK_IMPL(ctx, calls.load() == 1);
        pool.wait();
    }

    // --- 10. stop without wait - queued tasks are still executed --
    /**
     * @brief Verifies that stop() does not discard already queued tasks.
     *
     * The test pushes 100 tasks that each sleep briefly, then calls
     * stop() followed by wait(). Per the documented contract, tasks
     * already in the queue are still executed; stop() only prevents
     * new push_task() calls.
     *
     * @note This test pins the "stop drains the queue" contract. If
     *       the implementation instead discards pending tasks, this
     *       test must be updated together with the contract.
     */
    inline void stopKeepsQueuedTasks(TestContext& ctx)
    {
        ThreadPool pool { 2 };
        pool.start();

        std::atomic<int> calls { 0 };

        // tasks that are guaranteed to start before stop
        for (int i = 0; i < 100; ++i)
            pool.push_task([&calls] {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                ++calls;
            });

        pool.stop();
        pool.wait();

        // all 100 tasks must complete (wait waits for them; stop only
        // forbids new push_task calls)
        KAS_TEST_CHECK_IMPL(ctx, calls.load() == 100);
    }

    // ==================================================================
    //  submit / future
    // ==================================================================

    // --- 11. submit returns a future with the result ---------------
    /**
     * @brief Verifies that submit() returns a usable std::future.
     *
     * The future must become ready with the value returned by the
     * callable.
     */
    inline void submitBasic(TestContext& ctx)
    {
        ThreadPool pool { 2 };
        pool.start();

        auto fut = pool.submit([] { return 42; });
        KAS_TEST_CHECK_IMPL(ctx, fut.get() == 42);

        pool.wait();
    }

    // --- 12. submit with arguments ---------------------------------
    /**
     * @brief Verifies argument forwarding in submit().
     *
     * The callable receives the forwarded arguments and its return
     * value is delivered through the future.
     */
    inline void submitWithArgs(TestContext& ctx)
    {
        ThreadPool pool { 2 };
        pool.start();

        auto fut = pool.submit([](int a, int b) { return a + b; }, 2, 3);
        KAS_TEST_CHECK_IMPL(ctx, fut.get() == 5);

        pool.wait();
    }

    // --- 13. submit on a non-running pool throws -------------------
    /**
     * @brief Verifies that submit() throws on a non-running pool.
     *
     * The pool is constructed but not started; submit() must throw
     * std::runtime_error (see ThreadPool::submit contract).
     */
    inline void submitNotRunningThrows(TestContext& ctx)
    {
        ThreadPool pool { 2 };
        // not started

        bool caught = false;
        try {
            auto fut = pool.submit([] { return 1; });
            (void)fut;
        } catch (const std::runtime_error&) {
            caught = true;
        }
        KAS_TEST_CHECK_IMPL(ctx, caught);
    }

    // --- 14. many submits - all futures resolve --------------------
    /**
     * @brief Verifies that many concurrent futures all resolve with
     *        the expected values.
     *
     * Pushes 20 independent tasks, then collects results in order and
     * checks each value.
     */
    inline void submitMany(TestContext& ctx)
    {
        ThreadPool pool { 4 };
        pool.start();

        std::vector<std::future<int>> futs;
        for (int i = 0; i < 20; ++i)
            futs.push_back(pool.submit([i] { return i * 2; }));

        for (int i = 0; i < 20; ++i)
            KAS_TEST_CHECK_IMPL(ctx, futs[i].get() == i * 2);

        pool.wait();
    }

    // ==================================================================
    //  Concurrency / load
    // ==================================================================

    // --- 15. many tasks, several workers ---------------------------
    /**
     * @brief Light stress test with 8 workers and 500 tasks.
     *
     * The test only checks the total count; it is intended to catch
     * gross races in the queue/worker logic rather than to measure
     * performance.
     */
    inline void stressManyTasks(TestContext& ctx)
    {
        ThreadPool pool { 8 };
        pool.start();

        std::atomic<int> total { 0 };
        constexpr int kTasks = 500;

        for (int i = 0; i < kTasks; ++i)
            pool.push_task([&total] {
                total.fetch_add(1, std::memory_order_relaxed);
            });

        pool.wait();
        KAS_TEST_CHECK_IMPL(ctx, total.load() == kTasks);
    }

    inline void submitExceptionPropagates(TestContext& ctx)
    {
        ThreadPool pool { 2 };
        pool.start();
        auto fut = pool.submit([] { throw std::runtime_error("boom"); });
        bool caught = false;
        try {
            fut.get();
        } catch (const std::runtime_error&) {
            caught = true;
        }
        KAS_TEST_CHECK_IMPL(ctx, caught);
        pool.wait();
    }

    inline void dtorStopsWorkers(TestContext& ctx)
    {
        std::atomic<int> calls { 0 };
        {
            ThreadPool pool { 2 };
            pool.start();
            pool.push_task([&calls] { ++calls; });
        } // destructor must stop and join
        KAS_TEST_CHECK_IMPL(ctx, calls.load() == 1);
    }

    // ==================================================================
    //  Multi-threaded submitters (start/stop from a single thread)
    // ==================================================================

    // --- 16. many threads push tasks concurrently -------------------
    /**
     * @brief Verifies that push_task() is safe to call from many
     *        threads while start()/stop() are controlled by one thread.
     *
     * Contract: start()/stop()/wait() are called from a single thread
     * (the test body); push_task() may be called concurrently from
     * worker threads created by the test itself.
     *
     * Each of kProducers threads pushes kTasksPerProducer trivial
     * increments. The pool must execute exactly kProducers *
     * kTasksPerProducer tasks, with no losses and no duplicates.
     */
    inline void concurrentPushFromManyThreads(TestContext& ctx)
    {
        ThreadPool pool { 4 };
        pool.start();

        std::atomic<int> total { 0 };
        constexpr int kProducers = 8;
        constexpr int kTasksPerProducer = 200;

        std::vector<std::thread> producers;
        producers.reserve(kProducers);

        for (int p = 0; p < kProducers; ++p) {
            producers.emplace_back([&pool, &total] {
                for (int i = 0; i < kTasksPerProducer; ++i) {
                    while (!pool.push_task([&total] {
                        total.fetch_add(1, std::memory_order_relaxed);
                    })) {
                        // pool may briefly refuse (not running); retry
                        std::this_thread::yield();
                    }
                }
            });
        }

        for (auto& th : producers)
            th.join();

        pool.wait();

        KAS_TEST_CHECK_IMPL(ctx, total.load() == kProducers * kTasksPerProducer);
        pool.stop();
    }

    // --- 17. many threads submit concurrently -----------------------
    /**
     * @brief Verifies that submit() is safe to call from many threads
     *        while start()/stop() are controlled by one thread.
     *
     * Each producer submits kTasksPerProducer tasks returning a value.
     * All futures must resolve with the expected results.
     */
    inline void concurrentSubmitFromManyThreads(TestContext& ctx)
    {
        ThreadPool pool { 4 };
        pool.start();

        constexpr int kProducers = 8;
        constexpr int kTasksPerProducer = 100;

        std::vector<std::vector<std::future<int>>> futures(kProducers);
        std::vector<std::thread> producers;
        producers.reserve(kProducers);

        for (int p = 0; p < kProducers; ++p) {
            producers.emplace_back([&pool, &futures, p] {
                auto& futs = futures[p];
                futs.reserve(kTasksPerProducer);
                for (int i = 0; i < kTasksPerProducer; ++i) {
                    try {
                        futs.push_back(pool.submit([p, i] { return p * 1000 + i; }));
                    } catch (const std::runtime_error&) {
                        // pool not running; submit again until it succeeds
                        std::this_thread::yield();
                        --i;
                    }
                }
            });
        }

        for (auto& th : producers)
            th.join();

        bool all_ok = true;
        for (int p = 0; p < kProducers; ++p) {
            for (int i = 0; i < kTasksPerProducer; ++i) {
                if (futures[p][i].get() != p * 1000 + i)
                    all_ok = false;
            }
        }
        KAS_TEST_CHECK_IMPL(ctx, all_ok);

        pool.wait();
        pool.stop();
    }

    // --- 18. push_task returns false once stop() was called ---------
    /**
     * @brief Verifies that after a single-threaded stop() no producer
     *        can enqueue new tasks.
     *
     * Start the pool, then stop() from the test thread, then have
     * several producer threads hammer push_task(). All pushes must
     * return false; the total executed count must not grow.
     */
    inline void pushAfterStopFromManyThreads(TestContext& ctx)
    {
        ThreadPool pool { 2 };
        pool.start();
        pool.stop();

        std::atomic<int> accepted { 0 };
        constexpr int kProducers = 4;
        constexpr int kAttempts = 500;

        std::vector<std::thread> producers;
        producers.reserve(kProducers);

        for (int p = 0; p < kProducers; ++p) {
            producers.emplace_back([&pool, &accepted] {
                for (int i = 0; i < kAttempts; ++i) {
                    if (pool.push_task([] { }))
                        accepted.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }

        for (auto& th : producers)
            th.join();

        KAS_TEST_CHECK_IMPL(ctx, accepted.load() == 0);
        pool.wait();
    }

    // --- 19. wait() from a single thread drains tasks from many -----
    /**
     * @brief Verifies that a single wait() call drains tasks pushed
     *        concurrently by many producer threads.
     *
     * Producers are joined first, then wait() is called once from the
     * test thread; after it returns, every pushed task must have run.
     */
    inline void waitDrainsConcurrentPushes(TestContext& ctx)
    {
        ThreadPool pool { 4 };
        pool.start();

        std::atomic<int> done { 0 };
        constexpr int kProducers = 6;
        constexpr int kTasksPerProducer = 150;

        std::vector<std::thread> producers;
        producers.reserve(kProducers);

        for (int p = 0; p < kProducers; ++p) {
            producers.emplace_back([&pool, &done] {
                for (int i = 0; i < kTasksPerProducer; ++i) {
                    while (!pool.push_task([&done] {
                        done.fetch_add(1, std::memory_order_relaxed);
                    })) {
                        std::this_thread::yield();
                    }
                }
            });
        }

        for (auto& th : producers)
            th.join();

        pool.wait();
        KAS_TEST_CHECK_IMPL(ctx, done.load() == kProducers * kTasksPerProducer);
        pool.stop();
    }

    // --- 20. start()/stop() serialized, push_task concurrent -------
    /**
     * @brief Verifies the documented model: a single thread drives
     *        start()/stop(), many threads only push work.
     *
     * Runs several start/push/wait/stop cycles. Producers push during
     * each cycle; between cycles the test thread stops and restarts
     * the pool. This pins the contract that start()/stop() are not
     * called concurrently with each other, only sequentially from the
     * test thread.
     */
    inline void serializedStartStopWithConcurrentPush(TestContext& ctx)
    {
        ThreadPool pool { 3 };

        constexpr int kCycles = 5;
        constexpr int kProducersPerCycle = 3;
        constexpr int kTasksPerProducer = 50;

        std::atomic<int> total { 0 };

        for (int cycle = 0; cycle < kCycles; ++cycle) {
            pool.start();

            std::vector<std::thread> producers;
            producers.reserve(kProducersPerCycle);
            for (int p = 0; p < kProducersPerCycle; ++p) {
                producers.emplace_back([&pool, &total] {
                    for (int i = 0; i < kTasksPerProducer; ++i) {
                        while (!pool.push_task([&total] {
                            total.fetch_add(1, std::memory_order_relaxed);
                        })) {
                            std::this_thread::yield();
                        }
                    }
                });
            }
            for (auto& th : producers)
                th.join();

            pool.wait();
            pool.stop();
        }

        KAS_TEST_CHECK_IMPL(
            ctx,
            total.load() == kCycles * kProducersPerCycle * kTasksPerProducer);
    }

    // --- 21. exceptions in concurrent tasks do not break the pool --
    /**
     * @brief Verifies that concurrently pushed throwing tasks do not
     *        stop the pool from processing other tasks.
     *
     * Half of the tasks throw; the other half increment a counter.
     * All non-throwing tasks must run, and the pool must remain usable
     * afterwards (a follow-up task must still execute).
     *
     * CerrMuteThreaded is used here (it is thread-safe and records the
     * producing thread id per entry), so the test can also verify that
     * every captured "boom" line is attributed to a worker thread and
     * not to the test thread.
     *
     * @note wait() in ThreadPool is stop() + join(), so the pool must
     *       not be waited on before the follow-up check: after wait()
     *       push_task() returns false and the follow-up would never run.
     *       Instead, poll the completion counter, then push the
     *       follow-up task, poll it, and only then call wait().
     */
    inline void concurrentThrowingTasks(TestContext& ctx)
    {
        CerrMuteThreaded mute;

        ThreadPool pool { 4 };
        pool.start();

        std::atomic<int> ok { 0 };
        constexpr int kProducers = 4;
        constexpr int kTasksPerProducer = 100;
        constexpr int kNonThrowing = (kProducers * kTasksPerProducer) / 2;

        std::vector<std::thread> producers;
        producers.reserve(kProducers);

        for (int p = 0; p < kProducers; ++p) {
            producers.emplace_back([&pool, &ok, p] {
                for (int i = 0; i < kTasksPerProducer; ++i) {
                    const bool throwing = ((p + i) % 2) == 0;
                    while (!pool.push_task([&ok, throwing] {
                        if (throwing)
                            throw std::runtime_error("boom");
                        ok.fetch_add(1, std::memory_order_relaxed);
                    })) {
                        std::this_thread::yield();
                    }
                }
            });
        }

        for (auto& th : producers)
            th.join();

        // Wait for all non-throwing tasks without stopping the pool.
        for (int i = 0; i < 2000 && ok.load() < kNonThrowing; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

        KAS_TEST_CHECK_IMPL(ctx, ok.load() == kNonThrowing);

        // Pool is still usable after exceptions: push and wait for a follow-up.
        std::atomic<bool> followup { false };
        KAS_TEST_CHECK_IMPL(ctx, pool.push_task([&followup] { followup.store(true); }));
        for (int i = 0; i < 200 && !followup.load(); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        KAS_TEST_CHECK_IMPL(ctx, followup.load());

        pool.wait();
        pool.stop();

        // Every logged exception must be attributed to a worker thread,
        // not to the test thread.
        const auto entries = mute.entries();
        KAS_TEST_CHECK_IMPL(ctx, !entries.empty());
        for (const auto& e : entries) {
            KAS_TEST_CHECK_IMPL(ctx, e.text.find("boom") != std::string::npos);
            KAS_TEST_CHECK_IMPL(ctx, e.tid != std::thread::id { });
            KAS_TEST_CHECK_IMPL(ctx, e.tid != std::this_thread::get_id());
        }
    }

    // ==================================================================
    //  Type properties
    // ==================================================================

    /**
     * @brief Static type-level contracts of ThreadPool.
     *
     * ThreadPool owns threads and synchronization state, so it must
     * not be copyable or movable.
     */
    static_assert(!std::is_copy_constructible_v<ThreadPool>);
    static_assert(!std::is_copy_assignable_v<ThreadPool>);
    static_assert(!std::is_move_constructible_v<ThreadPool>);
    static_assert(!std::is_move_assignable_v<ThreadPool>);
    static_assert(std::is_nothrow_destructible_v<ThreadPool>);

} // namespace

// ============================================================
//  Entry point
// ============================================================
/**
 * @brief Runs all ThreadPool unit tests.
 *
 * Tests are executed in a fixed order: lifecycle, task execution,
 * stop/wait, submit/future, stress. The summary is printed via
 * printSummary() at the end.
 */
inline bool runTests()
{
    TestContext ctx;

    emptyPoolPushFails(ctx);
    pushBeforeStartFails(ctx);
    pushAfterStopFails(ctx);

    oneTask(ctx);
    manyTasks(ctx);
    tasksParallel(ctx);
    exceptionInTask(ctx);

    waitDrainsQueue(ctx);
    startTwice(ctx);
    stopKeepsQueuedTasks(ctx);

    submitBasic(ctx);
    submitWithArgs(ctx);
    submitNotRunningThrows(ctx);
    submitMany(ctx);

    stressManyTasks(ctx);

    submitExceptionPropagates(ctx);
    dtorStopsWorkers(ctx);

    concurrentPushFromManyThreads(ctx);
    concurrentSubmitFromManyThreads(ctx);
    pushAfterStopFromManyThreads(ctx);
    waitDrainsConcurrentPushes(ctx);
    serializedStartStopWithConcurrentPush(ctx);
    concurrentThrowingTasks(ctx);

    return printSummary("kas::thread::test_thread_pool::tests", ctx);
}

} // namespace kas::thread::test_thread_pool::tests

#endif