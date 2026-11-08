/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_THREAD_THREAD_POOL_H
#define KASLIBCXX_THREAD_THREAD_POOL_H

#include "JThread.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

namespace kas {

/**
 * @brief Fixed-size pool of worker threads with a task queue.
 *
 * ThreadPool owns a vector of kas::JThread workers. Tasks are placed
 * into a FIFO queue protected by a mutex; idle workers wake up on a
 * condition variable. The pool is non-copyable and non-movable.
 *
 * Lifecycle:
 *  - Construction: the constructor validates the requested worker
 *    count and prepares the pool. It does not necessarily start
 *    workers; call start() explicitly (see below).
 *  - start(): starts all worker threads. If the pool is already
 *    running, it must be a no-op.
 *  - stop(): requests cooperative stop on all workers, wakes them up,
 *    and joins them. After stop() the pool is not running and cannot
 *    accept tasks until start() is called again.
 *  - wait(): blocks until the task queue is empty and no worker is
 *    busy. The exact definition of "busy" depends on the
 *    implementation (see wait() below).
 *  - Destructor: must not leave running threads behind. A typical
 *    implementation calls stop() (and therefore joins all workers).
 *
 * @par Task submission
 * Use push_task() for raw callables with no return value, or submit()
 * for callables with arguments and a result. submit() wraps the
 * callable into std::packaged_task, obtains a std::future, and returns
 * it to the caller.
 *
 * @par Thread safety
 * push_task(), submit(), start(), stop() and wait() may be called from
 * multiple threads, except that stop() and start() are expected to be
 * serialized by the caller (calling stop() concurrently with start()
 * is a data race on m_is_running and m_threads).
 *
 * @par Exception safety
 * submit() throws std::runtime_error if the pool is not running.
 * Exceptions thrown by a submitted task are captured by the
 * std::packaged_task and rethrown from the associated std::future, so
 * they do not terminate the worker.
 *
 * @par Example
 * @code
 * kas::ThreadPool pool(4);
 * pool.start();
 * auto fut = pool.submit([](int a, int b) { return a + b; }, 2, 3);
 * int r = fut.get(); // 5
 * pool.stop();
 * @endcode
 */
class ThreadPool final {

public:
    /// Callable type accepted by the task queue (no return value).
    using function_t = JThread::function_t;

    /// Type used for the number of worker threads.
    using count_thread_t = size_t;

    /**
     * @brief Constructs the pool with the given number of workers.
     * @param count_thread Number of worker threads to create.
     *
     * @note Whether workers are started by the constructor or only by
     *       start() is an implementation detail; callers should call
     *       start() explicitly to make the intent clear.
     *
     * @throws May throw if a worker cannot be created (depending on
     *         the implementation, workers may be created in start()
     *         instead).
     */
    explicit ThreadPool(count_thread_t count_thread);

    /// Copying is disabled: the pool owns threads and synchronization state.
    ThreadPool(const ThreadPool&) = delete;

    /// Moving is disabled: worker threads capture a stable `this` pointer.
    ThreadPool(ThreadPool&&) = delete;

    /// Copy assignment is disabled.
    ThreadPool& operator=(const ThreadPool&) = delete;

    /// Move assignment is disabled.
    ThreadPool& operator=(ThreadPool&&) = delete;

    /**
     * @brief Destructor.
     *
     * Must stop and join all worker threads. Pending tasks that have
     * not started yet may be either executed or discarded depending on
     * the implementation; document the chosen behavior.
     *
     * @warning The destructor blocks until all workers have finished.
     */
    ~ThreadPool();

    /**
     * @brief Enqueues a callable for execution by a worker.
     * @param func Callable with no arguments and no return value.
     * @return true if the task was enqueued, false if the pool is not
     *         running.
     *
     * @note The callable is moved into the queue.
     */
    bool push_task(function_t func);

    /**
     * @brief Starts all worker threads.
     *
     * If the pool is already running, this is a no-op.
     *
     * @post m_is_running == true and m_threads contains joinable workers.
     */
    void start();

    /**
     * @brief Requests stop on all workers and joins them.
     *
     * After stop() returns, no worker is running. The pool can be
     * started again with start().
     *
     * @note Tasks still in the queue may be discarded; document the
     *       chosen behavior.
     */
    void stop();

    /**
     * @brief Blocks until the pool has no pending or running tasks.
     *
     * The precise condition is implementation-defined; typically this
     * means "the task queue is empty and no worker is executing a task".
     * wait() does not stop the pool.
     */
    void wait();

    /**
     * @brief Submits a callable with arguments and returns a future.
     *
     * Wraps `f(args...)` into a std::packaged_task, obtains its
     * std::future, and enqueues the task. The callable and its arguments
     * are decay-copied into the task object, so they must be copyable
     * or movable.
     *
     * @tparam F    Callable type.
     * @tparam Args Argument types.
     * @param f     Callable to invoke.
     * @param args  Arguments forwarded to the callable.
     * @return std::future for the callable result.
     *
     * @throws std::runtime_error if the pool is not running.
     *
     * @note Exceptions thrown by the callable are stored in the
     *       packaged_task and rethrown by std::future::get().
     */
    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>
    {
        using R = std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<R()>>(
            [f = std::decay_t<F>(std::forward<F>(f)), ... args = std::forward<Args>(args)]() mutable {
                return std::invoke(f, std::move(args)...);
            });

        std::future<R> fut = task->get_future();

        if (!push_task([task] { (*task)(); }))
            throw std::runtime_error("ThreadPool::submit: pool is not running");

        return fut;
    }

private:
    /**
     * @brief Worker loop executed by each JThread.
     *
     * Waits on the queue condition variable, pops tasks, and executes
     * them. Must exit cooperatively when stop() is requested (for
     * example, by checking a stop_token or a shared stop flag).
     */
    void run();

    std::vector<JThread> m_threads; ///< Worker threads owned by the pool.
    std::mutex m_mx_queue; ///< Protects m_queue_task and m_is_running.
    std::queue<function_t> m_queue_task; ///< Pending tasks (FIFO).
    std::condition_variable m_cv_queue; ///< Wakes idle workers when tasks arrive or on stop.
    bool m_is_running; ///< True while workers are running.
};
} // namespace kas

#endif