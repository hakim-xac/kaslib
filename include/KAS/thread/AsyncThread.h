/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_THREAD_ASYNC_THREAD_H
#define KASLIBCXX_THREAD_ASYNC_THREAD_H

/*
 * kaslibcxx/AsyncThread.h - run a callable asynchronously with an
 * optional std::stop_token and retrieve its result via std::future.
 *
 * AsyncThread<Func, Args...> starts Func(Args...) (or
 * Func(std::stop_token, Args...) when supported) on a dedicated thread
 * managed by JThread, and exposes the result through a std::future.
 *
 * Design points:
 *   - The callable may accept a leading std::stop_token. The choice is
 *     made at compile time: if Func is invocable with
 *     (std::stop_token, Args...), that form is used; otherwise the
 *     plain (Args...) form is used.
 *   - result_type is deduced from the selected invocation form, so the
 *     future's value type matches what the callable actually returns.
 *   - A stack_size_t may be supplied to control the thread's stack size.
 *   - Stopping is cooperative: request_stop() forwards to the underlying
 *     JThread; the running callable observes it via the stop_token.
 *
 * Exceptions:
 *   - The callable is executed through a std::promise/std::future pair.
 *     Exceptions are stored twice: once in the promise (so future.get()
 *     rethrows them), and once in a shared state that owns an
 *     independent std::exception_ptr. get() rethrows the independent
 *     copy, so the exception object outlives the promise/future shared
 *     state that the worker thread destroys.
 *   - This avoids the use-after-free that occurs when the caller reads
 *     e.what() from an exception_ptr owned by the future's shared
 *     state, while the worker thread destroys that shared state
 *     concurrently.
 *
 * The object is move-only and owns both the future and the thread.
 * get() may be called once; a second call throws std::future_error
 * with std::future_errc::no_state.
 */

#include "JThread.h"

#include <chrono>
#include <cstddef>
#include <exception>
#include <future>
#include <memory>
#include <stop_token>
#include <type_traits>
#include <utility>

namespace kas {

/**
 * Tag type carrying a requested stack size (in bytes) for a thread
 * created by AsyncThread.
 */
struct stack_size_t {
    size_t value;
};

/**
 * Creates a stack_size_t from a size in bytes.
 *
 * No validation is performed here; the value is passed through to
 * JThread, which is responsible for accepting or rejecting it.
 */
[[nodiscard]] inline constexpr stack_size_t stack_size(size_t v) noexcept
{
    return { v };
}

namespace detail {
    /*
     * Prefer the invocation form that takes a std::stop_token first.
     * The overload taking int is a better match than the one taking
     * long, so when Func is invocable both with and without a
     * stop_token, the stop_token form wins.
     */
    template <typename F, typename... A>
    auto invoke_result_with_stop(int) -> std::invoke_result_t<F, std::stop_token, A...>;

    template <typename F, typename... A>
    auto invoke_result_with_stop(long) -> std::invoke_result_t<F, A...>;

} // namespace detail

/**
 * Runs a callable on a dedicated thread and exposes its result as a
 * std::future.
 *
 * The callable may be written as either:
 *     R f(Args...)
 *     R f(std::stop_token, Args...)
 * and the matching form is selected at compile time.
 */
template <typename Func, typename... Args>
class AsyncThread final {
public:
    /*
     * Result type of the invocation form that will actually be used.
     * If Func is invocable with a stop_token, that form defines the
     * result type; otherwise the plain form does.
     */
    using result_type = decltype(detail::invoke_result_with_stop<Func&, Args...>(0));

private:
    /*
     * Shared state between the worker thread and the caller.
     *
     * exception holds an independent copy of the thrown exception,
     * created via std::current_exception() inside the worker thread
     * before the exception is forwarded to the promise. Unlike the
     * exception_ptr stored in the promise/future shared state, this
     * copy owns the exception object and keeps it alive as long as
     * this SharedState exists, independently of when the promise and
     * future release their shared state.
     *
     * The write to exception happens-before promise.set_exception(),
     * which in turn happens-before future.get() (or future.wait()) in
     * the caller. This is the happens-before chain that makes the
     * value visible in get() without an extra mutex.
     */
    struct SharedState {
        std::exception_ptr exception;
    };

public:
    /**
     * Starts Func(Args...) on a new thread using the default stack size.
     */
    explicit AsyncThread(Func func, Args... args)
        : AsyncThread { stack_size(JThread::STACK_MIN_SIZE),
            std::move(func), std::move(args)... }
    {
    }

    /**
     * Starts Func(Args...) on a new thread using the given stack size.
     *
     * The callable, its arguments, the promise/future pair and the
     * shared state are all moved into the thread; after construction
     * this object owns the running thread and the pending result.
     */
    AsyncThread(stack_size_t ss, Func func, Args... args)
        : m_state { std::make_shared<SharedState>() }
    {
        static_assert(
            std::invocable<Func&, std::stop_token, Args...>
                || std::invocable<Func&, Args...>,
            "Func must be invocable with Args..., optionally preceded by std::stop_token");

        auto promise = std::make_shared<std::promise<result_type>>();
        m_future = promise->get_future();

        /*
         * Run the callable in the worker, then deliver either the
         * value or the exception to the promise. The independent copy
         * of the exception is stored in m_state before the exception
         * is forwarded to the promise, so that it is visible to get()
         * via the happens-before chain through the future.
         */
        auto state = m_state;

        m_thread = JThread {
            [func = std::move(func),
                ... args = std::move(args),
                promise = std::move(promise),
                state](std::stop_token token) mutable {
                try {
                    if constexpr (std::invocable<Func&, std::stop_token, Args...>) {
                        if constexpr (std::is_void_v<result_type>) {
                            std::invoke(std::move(func), token, std::move(args)...);
                            promise->set_value();
                        } else {
                            promise->set_value(
                                std::invoke(std::move(func), token, std::move(args)...));
                        }
                    } else {
                        if constexpr (std::is_void_v<result_type>) {
                            std::invoke(std::move(func), std::move(args)...);
                            promise->set_value();
                        } else {
                            promise->set_value(
                                std::invoke(std::move(func), std::move(args)...));
                        }
                    }
                } catch (...) {
                    // Independent copy of the exception; keeps the
                    // exception object alive independently of the
                    // promise/future shared state.
                    state->exception = std::current_exception();
                    promise->set_exception(state->exception);
                }
            },
            ss.value
        };
    }

    AsyncThread(const AsyncThread&) = delete;
    AsyncThread(AsyncThread&&) noexcept = default;
    AsyncThread& operator=(const AsyncThread&) = delete;
    AsyncThread& operator=(AsyncThread&&) noexcept = default;
    ~AsyncThread() = default;

    /**
     * Waits for the callable to finish and returns its result.
     *
     * For a void result the call returns after the task completes.
     * Exceptions thrown by the callable are rethrown here.
     *
     * The exception is rethrown from an independent exception_ptr
     * stored in the shared state, so the exception object remains
     * valid even if the worker thread has already destroyed the
     * promise/future shared state.
     *
     * Throws std::future_error with std::future_errc::no_state if the
     * future is no longer valid (for example, get() was already called).
     */
    result_type get()
    {
        if (!m_future.valid())
            throw std::future_error(std::future_errc::no_state);

        // Ensure the worker has finished and the future is ready before
        // reading m_state->exception. future.wait() synchronizes with
        // promise.set_value()/set_exception(), which happens-after the
        // write to m_state->exception in the worker.
        m_future.wait();

        if (m_state->exception) {
            // Rethrow the independent copy. This keeps the exception
            // object alive for the duration of the catch block in the
            // caller, regardless of when the worker destroys the
            // promise/future shared state.
            std::rethrow_exception(m_state->exception);
        }

        if constexpr (std::is_void_v<result_type>) {
            m_future.get();
        } else {
            return m_future.get();
        }
    }

    /**
     * Non-blocking check whether the callable has finished.
     *
     * Returns false if the future is not valid.
     */
    [[nodiscard]] bool ready() const
    {
        return m_future.valid()
            && m_future.wait_for(std::chrono::seconds(0))
            == std::future_status::ready;
    }

    /**
     * Requests cooperative cancellation of the running callable.
     *
     * The callable observes the request through the std::stop_token it
     * received (if it accepted one). This does not forcibly terminate
     * the thread.
     */
    void request_stop() noexcept { m_thread.request_stop(); }

    /** Returns the stop_token associated with the underlying thread. */
    [[nodiscard]] std::stop_token get_stop_token() const noexcept
    {
        return m_thread.get_stop_token();
    }

    /**
     * Joins the underlying thread.
     *
     * Like std::thread::join, this must be called at most once; a
     * second call (or a call on a non-joinable thread) throws
     * std::system_error.
     */
    void join() { m_thread.join(); }

    /** True if the underlying thread is joinable. */
    [[nodiscard]] bool joinable() const noexcept
    {
        return m_thread.joinable();
    }

private:
    std::future<result_type> m_future;
    std::shared_ptr<SharedState> m_state;
    JThread m_thread;
};

} // namespace kas

#endif