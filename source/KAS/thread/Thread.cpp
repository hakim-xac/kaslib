// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include "KAS/thread/Thread.h"

#include "KAS/utils/utils.h"

#include <iostream>
#include <memory>
#include <pthread.h>
#include <stop_token>
#include <string>
#include <utility>

namespace kas {

namespace {

    //------------------

    [[nodiscard]] std::string
    make_error_string(const char* what, int err_code)
    {
        return std::string { what } + " failed: "
            + std::to_string(err_code) + " ("
            + std::system_category().message(err_code) + ')';
    }

    //------------------

    class pthread_attr_guard final {
    public:
        explicit pthread_attr_guard(pthread_attr_t* attr) noexcept
            : m_attr { attr }
        {
        }

        ~pthread_attr_guard()
        {
            if (m_attr != nullptr)
                ::pthread_attr_destroy(m_attr);
        }

        pthread_attr_guard(const pthread_attr_guard&) = delete;
        pthread_attr_guard& operator=(const pthread_attr_guard&) = delete;
        pthread_attr_guard(pthread_attr_guard&&) = delete;
        pthread_attr_guard& operator=(pthread_attr_guard&&) = delete;

    private:
        pthread_attr_t* m_attr;
    };

    //------------------

    void* thread_entry(void* arg) noexcept
    {
        const std::unique_ptr<kas::Thread::function_t> func { static_cast<kas::Thread::function_t*>(arg) };
        try {
            if (func != nullptr)
                (*func)();
        } catch (const std::exception& ex) {
            std::cerr << "ex: " << ex.what() << "\n";
        } catch (...) {
            std::cerr << "unknown exception!\n";
        }
        return nullptr;
    }

    //------------------

} // namespace

//------------------

void swap(kas::Thread& a, kas::Thread& b) noexcept
{
    a.swap(b);
}

//------------------

Thread::Thread(Thread::function_t func, std::size_t stack_size)
    : m_thread_id { }
{
    const auto stack_min_size {
        static_cast<size_t>(PTHREAD_STACK_MIN) > STACK_MIN_SIZE
            ? static_cast<size_t>(PTHREAD_STACK_MIN)
            : STACK_MIN_SIZE
    };

    if (stack_size < stack_min_size) [[unlikely]] {

        throw std::invalid_argument {
            utils::stringify(
                "kas::Thread: stack_size (",
                utils::intToString(stack_size).value_or("-"),
                ") less than stack_min_size ("
                ", stack_size: ",
                utils::intToString(stack_min_size).value_or("-"),
                ")")
        };
    }

    pthread_attr_t attr;

    int rc = ::pthread_attr_init(&attr);
    if (rc != 0) [[unlikely]]
        throw std::runtime_error { make_error_string("pthread_attr_init", rc) };

    pthread_attr_guard attr_guard { &attr };

    rc = ::pthread_attr_setstacksize(&attr, stack_size);
    if (rc != 0) [[unlikely]] {
        throw std::runtime_error {
            utils::stringify(
                make_error_string("pthread_attr_setstacksize", rc),
                ", stack_size: ", utils::intToString(stack_size).value_or("-"))
        };
    }

    auto func_holder { std::make_unique<Thread::function_t>(std::move(func)) };

    rc = ::pthread_create(
        &m_thread_id,
        &attr,
        &thread_entry,
        func_holder.get());

    if (rc != 0) [[unlikely]] {
        m_thread_id = 0;
        throw std::runtime_error {
            utils::stringify(
                make_error_string("pthread_create", rc),
                ", stack_size: ", utils::intToString(stack_size).value_or("-"))
        };
    }

    // Ownership transferred to the new thread; thread_entry deletes it.
    func_holder.release();
}

//------------------

Thread::~Thread()
{
    if (joinable()) {
        std::terminate();
    }
}

//------------------

Thread::Thread(Thread&& other) noexcept
    : m_thread_id { std::exchange(other.m_thread_id, 0) }
{
}

//------------------

Thread& Thread::operator=(Thread&& other) noexcept
{
    if (this != std::addressof(other)) {
        if (joinable()) {
            std::terminate();
        }
        m_thread_id = std::exchange(other.m_thread_id, 0);
    }

    return *this;
}

//------------------

std::uintptr_t
Thread::get_id() const noexcept
{
    return joinable() ? m_thread_id : std::uintptr_t { };
}

//------------------

bool Thread::joinable() const noexcept
{
    return m_thread_id != 0;
}

//------------------

void Thread::join()
{
    if (!joinable()) [[unlikely]] {
        throw std::system_error(
            std::make_error_code(std::errc::invalid_argument),
            "kas::Thread::join: the thread is not joinable");
    }

    int rc = ::pthread_join(m_thread_id, nullptr);
    if (rc != 0) [[unlikely]]
        throw std::system_error(rc, std::generic_category(), "pthread_join");

    m_thread_id = 0;
}

//------------------

void Thread::detach() noexcept
{
    if (joinable()) {
        (void)::pthread_detach(m_thread_id);
        m_thread_id = 0;
    }
}

//------------------

int Thread::kill(int sig) const noexcept
{
    if (!joinable())
        return ESRCH;

    return ::pthread_kill(m_thread_id, sig);
}

//------------------

void Thread::swap(Thread& other) noexcept
{
    using std::swap;
    swap(m_thread_id, other.m_thread_id);
}

//------------------

} // namespace kas