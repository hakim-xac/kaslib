/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_FUNCTIONAL_TYPE_GUARD_H
#define KASLIBCXX_FUNCTIONAL_TYPE_GUARD_H

#include "MoveOnlyFunction.h"

#include <cstdio>
#include <exception>
#include <type_traits>
#include <utility>

namespace kas::functional {

template <typename T>
class TypeGuard final {
public:
    using deleter_t = MoveOnlyFunction<void(T&)>;

    template <typename... Args>
    TypeGuard(deleter_t deleter, Args&&... args)
        : m_deleter { std::move(deleter) }
        , m_value { std::forward<Args>(args)... }
    {
    }

    TypeGuard(const TypeGuard&) = delete;
    TypeGuard(TypeGuard&&) noexcept = default;
    TypeGuard& operator=(const TypeGuard&) = delete;
    TypeGuard& operator=(TypeGuard&&) noexcept = delete;

    ~TypeGuard()
    {
        if (!m_deleter)
            return;
        try {
            m_deleter(m_value);
        } catch (const std::exception& ex) {
            std::fprintf(stderr, "TypeGuard deleter threw: %s\n", ex.what());
            std::terminate();
        } catch (...) {
            std::fputs("TypeGuard deleter threw unknown exception\n", stderr);
            std::terminate();
        }
    }

    [[nodiscard]] const T& get() const& noexcept
    {
        return m_value;
    }

    [[nodiscard]] T& get() & noexcept
    {
        return m_value;
    }

    [[nodiscard]] const T& operator*() const& noexcept
    {
        return get();
    }

    [[nodiscard]] T& operator*() & noexcept
    {
        return get();
    }

    [[nodiscard]] const T* operator->() const& noexcept
    {
        return std::addressof(m_value);
    }

    [[nodiscard]] T* operator->() & noexcept
    {
        return std::addressof(m_value);
    }

    [[nodiscard]] T release() noexcept(std::is_nothrow_move_constructible_v<T>)
    {
        m_deleter.reset();
        return std::move(m_value);
    }

    void dismiss() & noexcept
    {
        m_deleter.reset();
    }

private:
    deleter_t m_deleter;
    T m_value;
};
}

#endif