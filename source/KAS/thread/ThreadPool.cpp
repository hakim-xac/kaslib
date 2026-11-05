// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include "KAS/thread/ThreadPool.h"
#include "KAS/utils/utils.h"

#include <algorithm>
#include <iostream>
#include <optional>

namespace kas {

//------------------

ThreadPool::ThreadPool(count_thread_t count_thread)
    : m_threads { count_thread }
    , m_mx_queue { }
    , m_queue_task { }
    , m_cv_queue { }
    , m_is_running { false }
{
}

//------------------

ThreadPool::~ThreadPool()
{
    wait();
}

//------------------

void ThreadPool::start()
{
    if (m_threads.empty())
        return;

    {
        std::lock_guard lk { m_mx_queue };

        // Already running: no-op.
        if (m_is_running)
            return;

        // All workers are stopped/joined or freshly constructed (non-joinable).
        // If any is still joinable, the pool is in an inconsistent state:
        // either start() was called without a prior join, or a worker is
        // still running. Treat it as a no-op to keep the old semantics.
        if (std::ranges::any_of(m_threads, [](const JThread& t) { return t.joinable(); }))
            return;

        m_is_running = true;
    }
    run();
}

//------------------

void ThreadPool::stop()
{
    {
        std::lock_guard lk { m_mx_queue };
        m_is_running = false;
    }
    m_cv_queue.notify_all();
}

//------------------

void ThreadPool::run()
{
    for (auto& t : m_threads) {
        t = JThread {
            function_t {
                [this] {
                    while (true) {
                        function_t func { };
                        {
                            std::unique_lock lk { m_mx_queue };

                            m_cv_queue.wait(
                                lk,
                                [this] {
                                    return !m_is_running
                                        || !m_queue_task.empty();
                                });

                            if (!m_is_running
                                && m_queue_task.empty())
                                return;

                            func = std::move(m_queue_task.front());
                            m_queue_task.pop();
                        }

                        try {
                            std::invoke(func);
                        } catch (const std::exception& ex) {
                            const auto err { utils::stringify("exception: ", ex.what(), "\n") };
                            std::cerr << err;
                        } catch (...) {
                            std::cerr << "unknown exception!\n";
                        }
                    }
                } }
        };
    }
}

//------------------

bool ThreadPool::push_task(function_t func)
{
    {
        std::lock_guard lk { m_mx_queue };
        if (!m_is_running)
            return false;
        m_queue_task.emplace(std::move(func));
    }
    m_cv_queue.notify_one();
    return true;
}

//------------------

void ThreadPool::wait()
{
    stop();
    for (auto& t : m_threads)
        if (t.joinable())
            t.join();
}

//------------------

} // namespace kas