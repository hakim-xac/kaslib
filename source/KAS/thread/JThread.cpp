// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include "KAS/thread/JThread.h"

#include <functional>
#include <iostream>
#include <memory>
#include <thread>
#include <utility>

namespace kas {

//------------------

JThread::JThread(function_t func, size_t stack_size)
    : m_thread { std::move(func), stack_size }
{
}

//------------------

JThread::JThread(function_stop_t func, size_t stack_size)
    : m_stop_source { }
    , m_thread { [f = std::move(func), token = m_stop_source.get_token()] {
                    f(token);
                },
        stack_size }
{
}

//------------------

JThread& JThread::operator=(JThread&& other) noexcept
{
    if (this != std::addressof(other)) {
        if (m_thread.joinable()) {
            m_stop_source.request_stop();
            try {
                m_thread.join();
            } catch (...) {
                std::terminate();
            }
        }
        m_thread = std::move(other.m_thread);
        m_stop_source = std::move(other.m_stop_source);
    }
    return *this;
}

//------------------

JThread::~JThread()
{
    if (m_thread.joinable()) {
        m_stop_source.request_stop();
        try {
            m_thread.join();
        } catch (const std::exception& e) {
            std::cerr << e.what() << '\n';
        } catch (...) {
            std::terminate();
        }
    }
}

// ----------------

std::uintptr_t
JThread::get_id() const noexcept
{
    return m_thread.get_id();
}

// ----------------

void JThread::join()
{
    m_thread.join();
}

// ----------------
int JThread::kill(int sig) const noexcept
{
    return m_thread.kill(sig);
}

// ----------------

bool JThread::joinable() const noexcept
{
    return m_thread.joinable();
}

// ----------------
} // namespace kas