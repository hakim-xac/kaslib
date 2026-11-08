/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_ASYNC_THREAD_TESTS_H
#define KASLIBCXX_ASYNC_THREAD_TESTS_H

#include "KAS/thread/AsyncThread.h"
#include "tests/tests.h"

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <type_traits>
#include <utility>

namespace kas::thread::test_async_thread::tests {

namespace {
    using namespace kas::tests;
    using kas::AsyncThread;
    using kas::stack_size;

    /*
     * Polls ready() until the task reports completion or the timeout
     * elapses. Used instead of a fixed sleep so that a regression does
     * not turn into a hang.
     */
    template <typename T>
    bool waitReady(T& t, std::chrono::milliseconds timeout = std::chrono::milliseconds(2000))
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (t.ready())
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return t.ready();
    }

    // --------------------------------------------------------
    //  Callable without stop_token, non-void result
    // --------------------------------------------------------
    void test_basic_nonvoid(TestContext& ctx)
    {
        AsyncThread t { [] { return 42; } };

        KAS_TEST_REQUIRE_IMPL(ctx, t.joinable());
        KAS_TEST_CHECK_IMPL(ctx, t.get() == 42);
        KAS_TEST_CHECK_IMPL(ctx, !t.ready()); // future already consumed
        t.join();
        KAS_TEST_CHECK_IMPL(ctx, !t.joinable());
    }

    // --------------------------------------------------------
    //  Callable without stop_token, void result
    // --------------------------------------------------------
    void test_basic_void(TestContext& ctx)
    {
        std::atomic<bool> ran { false };
        AsyncThread t { [&ran] { ran.store(true, std::memory_order_release); } };

        t.get(); // blocks until completion
        KAS_TEST_CHECK_IMPL(ctx, ran.load(std::memory_order_acquire));
        t.join();
    }

    // --------------------------------------------------------
    //  Callable with leading std::stop_token, non-void result
    // --------------------------------------------------------
    void test_stop_token_nonvoid(TestContext& ctx)
    {
        AsyncThread t { [](std::stop_token tok) {
            return tok.stop_requested() ? 1 : 0;
        } };

        KAS_TEST_CHECK_IMPL(ctx, t.get() == 0);
        t.join();
    }

    // --------------------------------------------------------
    //  Callable with leading std::stop_token, void result
    // --------------------------------------------------------
    void test_stop_token_void(TestContext& ctx)
    {
        std::atomic<bool> sawToken { false };
        AsyncThread t { [&sawToken](std::stop_token tok) {
            sawToken.store(!tok.stop_requested(), std::memory_order_release);
        } };

        t.get();
        KAS_TEST_CHECK_IMPL(ctx, sawToken.load(std::memory_order_acquire));
        t.join();
    }

    // --------------------------------------------------------
    //  Arguments are forwarded; move-only argument
    // --------------------------------------------------------
    void test_move_only_arg(TestContext& ctx)
    {
        auto p = std::make_unique<int>(7);
        AsyncThread t { [](std::unique_ptr<int> q) { return *q; }, std::move(p) };

        KAS_TEST_CHECK_IMPL(ctx, t.get() == 7);
        t.join();
    }

    // --------------------------------------------------------
    //  Arguments are forwarded together with stop_token
    // --------------------------------------------------------
    void test_stop_token_with_args(TestContext& ctx)
    {
        AsyncThread t {
            [](std::stop_token, int a, int b) { return a + b; }, 20, 22
        };

        KAS_TEST_CHECK_IMPL(ctx, t.get() == 42);
        t.join();
    }

    // --------------------------------------------------------
    //  result_type matches the selected invocation form
    // --------------------------------------------------------
    void test_result_type(TestContext& ctx)
    {
        using R1 = decltype(AsyncThread { [] { return 1; } })::result_type;
        using R2 = decltype(AsyncThread { [](std::stop_token) { return 1; } })::result_type;
        using R3 = decltype(AsyncThread { [] { } })::result_type;
        using R4 = decltype(AsyncThread { [](std::stop_token) { } })::result_type;
        using R5 = decltype(AsyncThread { [] { return std::string { "x" }; } })::result_type;

        KAS_TEST_CHECK_IMPL(ctx, (std::is_same_v<R1, int>));
        KAS_TEST_CHECK_IMPL(ctx, (std::is_same_v<R2, int>));
        KAS_TEST_CHECK_IMPL(ctx, (std::is_same_v<R3, void>));
        KAS_TEST_CHECK_IMPL(ctx, (std::is_same_v<R4, void>));
        KAS_TEST_CHECK_IMPL(ctx, (std::is_same_v<R5, std::string>));
    }

    // --------------------------------------------------------
    //  ready() before and after completion
    // --------------------------------------------------------
    void test_ready(TestContext& ctx)
    {
        std::atomic<bool> release { false };
        AsyncThread t { [&release] {
            while (!release.load(std::memory_order_acquire))
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            return 1;
        } };

        KAS_TEST_CHECK_IMPL(ctx, !t.ready());
        release.store(true, std::memory_order_release);
        KAS_TEST_REQUIRE_IMPL(ctx, waitReady(t));
        KAS_TEST_CHECK_IMPL(ctx, t.ready());
        KAS_TEST_CHECK_IMPL(ctx, t.get() == 1);
        t.join();
    }

    // --------------------------------------------------------
    //  get() blocks until the result is available
    // --------------------------------------------------------
    void test_get_blocks(TestContext& ctx)
    {
        AsyncThread t { [] {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            return 5;
        } };

        KAS_TEST_CHECK_IMPL(ctx, t.get() == 5);
        t.join();
    }

    // --------------------------------------------------------
    //  Second get() throws std::future_error(no_state)
    // --------------------------------------------------------
    void test_get_twice(TestContext& ctx)
    {
        AsyncThread t { [] { return 1; } };
        KAS_TEST_CHECK_IMPL(ctx, t.get() == 1);

        bool threw = false;
        std::error_code code { };
        try {
            (void)t.get();
        } catch (const std::future_error& e) {
            threw = true;
            code = e.code();
        }
        KAS_TEST_CHECK_IMPL(ctx, threw);
        KAS_TEST_CHECK_IMPL(ctx, code == std::make_error_code(std::future_errc::no_state));
        t.join();
    }

    // --------------------------------------------------------
    //  Exceptions from the callable are rethrown by get()
    //
    //  The exception object is rethrown from an independent
    //  exception_ptr stored in AsyncThread's shared state, so
    //  reading e.what() here is safe even after the worker has
    //  released the promise/future shared state.
    // --------------------------------------------------------
    void test_exception_propagation(TestContext& ctx)
    {
        AsyncThread t { []() -> int { throw std::runtime_error("boom"); } };

        bool threw = false;
        std::string what;
        try {
            (void)t.get();
        } catch (const std::runtime_error& e) {
            threw = true;
            what = e.what();
        }
        KAS_TEST_CHECK_IMPL(ctx, threw);
        KAS_TEST_CHECK_IMPL(ctx, what == "boom");
        t.join();
    }

    // --------------------------------------------------------
    //  Custom exception type is rethrown with its original type
    // --------------------------------------------------------
    struct CustomError : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    void test_custom_exception_type(TestContext& ctx)
    {
        AsyncThread t { []() -> int { throw CustomError("custom"); } };

        bool caught_custom = false;
        std::string what;
        try {
            (void)t.get();
        } catch (const CustomError& e) {
            caught_custom = true;
            what = e.what();
        } catch (...) {
            // Wrong type: fail below.
        }
        KAS_TEST_CHECK_IMPL(ctx, caught_custom);
        KAS_TEST_CHECK_IMPL(ctx, what == "custom");
        t.join();
    }

    // --------------------------------------------------------
    //  Exception message remains valid after join()
    // --------------------------------------------------------
    void test_exception_survives_join(TestContext& ctx)
    {
        std::string what;
        bool threw = false;
        {
            AsyncThread t { []() -> int { throw std::runtime_error("survives"); } };
            try {
                (void)t.get();
            } catch (const std::runtime_error& e) {
                threw = true;
                what = e.what();
            }
            t.join();
        }
        KAS_TEST_CHECK_IMPL(ctx, threw);
        KAS_TEST_CHECK_IMPL(ctx, what == "survives");
    }

    // --------------------------------------------------------
    //  request_stop() during execution is visible via stop_token
    // --------------------------------------------------------
    void test_request_stop_during(TestContext& ctx)
    {
        std::atomic<bool> started { false };
        AsyncThread t { [&started](std::stop_token tok) {
            started.store(true, std::memory_order_release);
            while (!tok.stop_requested())
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            return 1;
        } };

        while (!started.load(std::memory_order_acquire))
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

        KAS_TEST_CHECK_IMPL(ctx, !t.get_stop_token().stop_requested());
        t.request_stop();
        KAS_TEST_CHECK_IMPL(ctx, t.get_stop_token().stop_requested());
        KAS_TEST_CHECK_IMPL(ctx, t.get() == 1);
        t.join();
    }

    // --------------------------------------------------------
    //  request_stop() before start: task still runs and sees stop
    // --------------------------------------------------------
    void test_request_stop_before(TestContext& ctx)
    {
        AsyncThread t { [](std::stop_token tok) {
            return tok.stop_requested() ? 1 : 0;
        } };

        t.request_stop();
        const int r = t.get();
        KAS_TEST_CHECK_IMPL(ctx, r == 0 || r == 1); // must not throw or hang
        t.join();
    }

    // --------------------------------------------------------
    //  Custom stack size (smoke test)
    // --------------------------------------------------------
    void test_custom_stack_size(TestContext& ctx)
    {
        AsyncThread t { stack_size(kas::JThread::STACK_MIN_SIZE), [] { return 3; } };
        KAS_TEST_CHECK_IMPL(ctx, t.get() == 3);
        t.join();
    }

    // --------------------------------------------------------
    //  Move construction transfers ownership
    // --------------------------------------------------------
    void test_move_construct(TestContext& ctx)
    {
        AsyncThread a { [] { return 11; } };
        KAS_TEST_CHECK_IMPL(ctx, a.joinable());

        AsyncThread b { std::move(a) };
        KAS_TEST_CHECK_IMPL(ctx, b.joinable());
        KAS_TEST_CHECK_IMPL(ctx, !a.joinable());
        KAS_TEST_CHECK_IMPL(ctx, b.get() == 11);
        b.join();
    }

    // --------------------------------------------------------
    //  Move assignment transfers ownership
    // --------------------------------------------------------
    void test_move_assign(TestContext& ctx)
    {
        auto fn = [] { return 1; };
        using T = AsyncThread<decltype(fn)>;

        T a { fn };
        T b { fn };

        // Make b non-joinable so that overwriting it does not require
        // joining a running thread (JThread::operator= joins it anyway,
        // but the intent of this test is the transfer of ownership).
        b.join();

        b = std::move(a); // b takes over a's future and thread; a is empty

        KAS_TEST_CHECK_IMPL(ctx, b.joinable());
        KAS_TEST_CHECK_IMPL(ctx, !a.joinable());
        KAS_TEST_CHECK_IMPL(ctx, b.get() == 1);
        b.join();
    }

    // --------------------------------------------------------
    //  join() on a non-joinable thread throws std::system_error
    // --------------------------------------------------------
    void test_join_twice(TestContext& ctx)
    {
        AsyncThread t { [] { return 1; } };
        (void)t.get();
        t.join();

        bool threw = false;
        try {
            t.join();
        } catch (const std::system_error&) {
            threw = true;
        }
        KAS_TEST_CHECK_IMPL(ctx, threw);
    }

} // namespace

// ============================================================
//  Entry point
// ============================================================
inline bool runTests()
{
    TestContext ctx;

    test_basic_nonvoid(ctx);
    test_basic_void(ctx);
    test_stop_token_nonvoid(ctx);
    test_stop_token_void(ctx);
    test_move_only_arg(ctx);
    test_stop_token_with_args(ctx);
    test_result_type(ctx);
    test_ready(ctx);
    test_get_blocks(ctx);
    test_get_twice(ctx);
    test_exception_propagation(ctx);
    test_custom_exception_type(ctx);
    test_exception_survives_join(ctx);
    test_request_stop_during(ctx);
    test_request_stop_before(ctx);
    test_custom_stack_size(ctx);
    test_move_construct(ctx);
    test_move_assign(ctx);
    test_join_twice(ctx);

    return printSummary("kas::thread::test_async_thread::tests", ctx);
}

} // namespace kas::thread::test_async_thread::tests

#endif