/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_THREAD_JTHREAD_H
#define KASLIBCXX_THREAD_JTHREAD_H

#include "Thread.h"

#include <KAS/functional/MoveOnlyFunction.h>

#include <memory>
#include <signal.h>
#include <stop_token>

namespace kas {

/**
 * @brief Thread with cooperative cancellation (similar to std::jthread).
 *
 * JThread is a wrapper around kas::Thread that adds:
 *  - std::stop_source / std::stop_token for cooperative task cancellation;
 *  - automatic request_stop() and join() in the destructor (RAII);
 *  - the ability to send a signal to the thread via kill().
 *
 * The class owns the thread: copying is disabled, moving is allowed.
 * After a move, the source object becomes non-joinable but remains
 * in a valid state and can be destroyed safely.
 *
 * @par Usage model
 * @code
 * kas::JThread t([](std::stop_token st) {
 *     while (!st.stop_requested()) {
 *         // ... useful work ...
 *     }
 * });
 * // ...
 * t.request_stop();   // cooperative stop
 * t.join();           // wait for completion
 * @endcode
 *
 * @par Thread safety
 * request_stop(), get_stop_token(), joinable(), get_id() and kill()
 * may be called from other threads. join() must be called from a single
 * thread and must not run concurrently with another join() or the destructor.
 *
 * @warning The destructor calls request_stop() and join(). If the task
 *          ignores the stop_token and never finishes, the destructor will
 *          block. Make sure the task reacts to stop_requested().
 *
 * @warning kill() sends a signal (SIGUSR1 by default) and is an
 *          asynchronous mechanism. Use it only as a last resort, not as
 *          the normal way to stop a task.
 */
class JThread final {
public:
    /// Minimum stack size, forwarded to kas::Thread.
    static constexpr size_t STACK_MIN_SIZE { kas::Thread::STACK_MIN_SIZE };

    /// Function type without stop_token (compatible with kas::Thread::function_t).
    using function_t = Thread::function_t;

    /// Function type that accepts std::stop_token for cooperative cancellation.
    using function_stop_t = functional::MoveOnlyFunction<void(std::stop_token)>;

    /**
     * @brief Creates a JThread with no associated thread.
     * @post joinable() == false.
     */
    JThread() = default;

    /**
     * @brief Starts a thread that runs func without stop_token support.
     * @param func       Callable object with no arguments.
     * @param stack_size Stack size for the new thread (>= STACK_MIN_SIZE).
     * @throws May throw if thread creation fails.
     * @post joinable() == true on success.
     */
    JThread(function_t func, size_t stack_size = STACK_MIN_SIZE);

    /**
     * @brief Starts a thread that runs func with a std::stop_token.
     * @param func       Callable object that accepts std::stop_token.
     * @param stack_size Stack size for the new thread (>= STACK_MIN_SIZE).
     * @throws May throw if thread creation fails.
     * @post joinable() == true on success.
     */
    JThread(function_stop_t func, size_t stack_size = STACK_MIN_SIZE);

    /**
     * @brief Template constructor for arbitrary callables that accept
     *        std::stop_token.
     *
     * Captures func by value (move/copy) and the token of the current
     * stop_source. Inside the thread it calls std::invoke(func, token).
     *
     * @tparam F Callable type; requires std::invocable<F, std::stop_token>.
     * @param func       Callable object.
     * @param stack_size Stack size for the new thread.
     * @throws May throw if thread creation fails.
     */
    template <typename F>
        requires std::invocable<F, std::stop_token>
    JThread(F&& func, size_t stack_size = STACK_MIN_SIZE)
    {
        auto token = m_stop_source.get_token();
        m_thread = Thread {
            [f = std::forward<F>(func), token]() mutable {
                std::invoke(f, token);
            },
            stack_size
        };
    }

    /// Copying is disabled: JThread uniquely owns the thread.
    JThread(const JThread&) = delete;

    /**
     * @brief Move constructor.
     * @post The source object remains valid but non-joinable.
     */
    JThread(JThread&&) noexcept = default;

    /// Copy assignment is disabled.
    JThread& operator=(const JThread&) = delete;

    /**
     * @brief Move assignment.
     *
     * If the current object is joinable(), it first calls request_stop()
     * and join(), then takes the thread and stop_source from the source.
     *
     * @post The source object remains valid but non-joinable.
     */
    JThread& operator=(JThread&&) noexcept;

    /**
     * @brief Destructor.
     *
     * If the thread is joinable(), calls request_stop() and then join().
     * JThread therefore follows the RAII model of std::jthread.
     *
     * @warning If the task does not react to stop_token, the destructor
     *          may block indefinitely.
     */
    ~JThread();

    /**
     * @brief Returns the thread identifier.
     * @return Thread id, or a value meaning "no thread" if JThread
     *         is not joinable().
     */
    [[nodiscard]] std::uintptr_t get_id() const noexcept;

    /**
     * @brief Waits for the thread to finish.
     * @pre joinable() == true.
     * @post joinable() == false.
     * @throws May throw depending on the kas::Thread implementation.
     */
    void join();

    /**
     * @brief Sends a signal to the thread.
     * @param sig Signal number (SIGUSR1 by default).
     * @return 0 on success, non-zero on error (kill(2) semantics).
     * @note This is an asynchronous mechanism; for normal cancellation
     *       use request_stop() and stop_token.
     */
    int kill(int sig = SIGUSR1) const noexcept;

    /**
     * @brief Checks whether the object is associated with a running thread.
     * @return true if the thread is running and has not been joined/moved.
     */
    [[nodiscard]] bool joinable() const noexcept;

    /**
     * @brief Requests cooperative cancellation of the task.
     *
     * Sets the flag in stop_source; the task can check it via
     * stop_token::stop_requested(). Does not block the caller.
     */
    void request_stop() noexcept { m_stop_source.request_stop(); }

    /**
     * @brief Returns the stop_token associated with this JThread.
     * @return Token for checking the stop request.
     */
    [[nodiscard]] std::stop_token get_stop_token() const noexcept
    {
        return m_stop_source.get_token();
    }

private:
    std::stop_source m_stop_source; ///< Source of the stop request.
    kas::Thread m_thread; ///< Owned thread.
};
} // namespace kas

#endif