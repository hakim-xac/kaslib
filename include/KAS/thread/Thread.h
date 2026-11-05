/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_THREAD_THREAD_H
#define KASLIBCXX_THREAD_THREAD_H

#include <KAS/functional/MoveOnlyFunction.h>

#include <cstdint>
#include <functional>
#include <signal.h>

namespace kas {

/**
 * @brief Minimal owning thread wrapper over pthreads.
 *
 * Thread owns a single native thread and is responsible for its
 * lifetime. Copying is disabled; moving transfers ownership and
 * leaves the source in the empty (non-joinable) state.
 *
 * The callable must be a move-only callable with the signature
 * void(). The thread entry point catches all exceptions escaping the
 * callable: they are swallowed and do not call std::terminate.
 *
 * Lifecycle:
 *  - A default-constructed Thread is empty: joinable() == false and
 *    get_id() == 0.
 *  - A constructed Thread is joinable until join() or detach() is
 *    called.
 *  - The destructor calls std::terminate() if the thread is still
 *    joinable, matching std::thread.
 *
 * @par Thread safety
 * Different Thread objects can be used from different threads without
 * external synchronization. Concurrent operations on the same Thread
 * object must be synchronized by the caller.
 *
 * @par Example
 * @code
 * kas::Thread t { [] { do_work(); } };
 * t.join();
 * @endcode
 */

class Thread final {

public:
    /// Callable type executed by the thread. Must be move-only.
    using function_t = functional::MoveOnlyFunction<void()>;

    /// Minimum stack size accepted by the constructor.
    static constexpr size_t STACK_MIN_SIZE { 32768 };

    /**
     * @brief Constructs an empty, non-joinable Thread.
     * @post joinable() == false, get_id() == 0.
     */
    Thread() noexcept = default;

    /**
     * @brief Destructor.
     *
     * If the thread is still joinable, calls std::terminate(),
     * matching std::thread. Call join() or detach() before destroying
     * a joinable Thread.
     *
     * @warning std::terminate() is called if joinable() == true.
     */
    ~Thread();

    /**
     * @brief Starts a new thread that executes func.
     *
     * @param func       Move-only callable invoked as func().
     * @param stack_size Requested stack size in bytes. Must be at
     *                   least max(PTHREAD_STACK_MIN, STACK_MIN_SIZE).
     *
     * @throws std::invalid_argument if stack_size is below the minimum.
     * @throws std::runtime_error if the thread attributes or the
     *         thread itself cannot be created.
     *
     * @post On success, joinable() == true.
     * @post On failure, joinable() == false; no thread is leaked.
     */
    explicit Thread(function_t func, size_t stack_size = STACK_MIN_SIZE);

    /// Copying is disabled: Thread uniquely owns the native thread.
    Thread(const Thread&) = delete;

    /**
     * @brief Move constructor.
     *
     * Transfers ownership of the native thread to *this.
     *
     * @post other.joinable() == false, other.get_id() == 0.
     */
    Thread(Thread&&) noexcept;

    /// Copy assignment is disabled.
    Thread& operator=(const Thread&) = delete;

    /**
     * @brief Move assignment.
     *
     * If *this is joinable(), calls std::terminate() (matching
     * std::thread), because silently dropping an owned thread would
     * leak it. Otherwise transfers ownership from other.
     *
     * @post other.joinable() == false, other.get_id() == 0.
     * @warning std::terminate() is called if *this was joinable.
     */
    Thread& operator=(Thread&&) noexcept;

    /**
     * @brief Waits for the thread to finish and releases it.
     *
     * Blocks until the thread represented by *this has completed.
     * After a successful call, joinable() == false and get_id() == 0.
     *
     * @pre joinable() == true. Calling join() on a non-joinable
     *      Thread throws std::system_error with
     *      std::errc::invalid_argument.
     *
     * @throws std::system_error if the underlying pthread_join fails.
     *         In that case the Thread remains joinable; the caller may
     *         retry join() or call detach().
     *
     * @post On success, joinable() == false.
     */
    void join();

    /**
     * @brief Detaches the thread and releases ownership.
     *
     * After detach() the thread continues to run independently and
     * cannot be joined. On a non-joinable Thread this is a no-op.
     *
     * Errors from pthread_detach are ignored; the Thread is marked
     * non-joinable regardless.
     *
     * @post joinable() == false, get_id() == 0.
     */
    void detach() noexcept;

    /**
     * @brief Sends a signal to the thread.
     *
     * Forwards to pthread_kill() on the underlying native thread.
     * Signal 0 performs only permission/existence checking and does
     * not deliver a signal.
     *
     * @param sig Signal number (SIGUSR1 by default).
     * @return 0 on success; ESRCH if the Thread is not joinable or the
     *         target thread no longer exists; other POSIX error codes
     *         as returned by pthread_kill().
     *
     * @note A thread may exit between the joinable() check and the
     *       pthread_kill() call; in that race ESRCH is returned.
     */
    int kill(int sig = SIGUSR1) const noexcept;

    /**
     * @brief Returns true if the Thread owns a running native thread.
     *
     * @return joinable() == true until join() or detach() succeeds.
     */
    [[nodiscard]] bool joinable() const noexcept;

    /**
     * @brief Exchanges the owned threads of *this and other.
     * @post joinable() and get_id() are swapped between the two objects.
     */
    void swap(Thread& other) noexcept;

    /**
     * @brief Returns a numeric identifier of the owned thread.
     *
     * The returned value is a numeric representation of the underlying
     * pthread_t, not pthread_t itself; do not pass it to pthread_*
     * functions directly.
     *
     * @return A non-zero thread id if joinable(), otherwise 0.
     */
    [[nodiscard]] std::uintptr_t get_id() const noexcept;

private:
    std::uintptr_t m_thread_id { }; ///< Numeric form of pthread_t; 0 means "no thread".
};

/**
 * @brief ADL swap for Thread.
 * @post Equivalent to a.swap(b).
 */
void swap(Thread& a, Thread& b) noexcept;

} // namespace kas

#endif