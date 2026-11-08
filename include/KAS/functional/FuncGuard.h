/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_FUNCTIONAL_FUNC_GUARD_H
#define KASLIBCXX_FUNCTIONAL_FUNC_GUARD_H

#include <cstdio>
#include <exception>
#include <functional>
#include <type_traits>
#include <utility>

/**
 * @brief Scope-guard utilities.
 */
namespace kas::functional {

/**
 * @brief Runs one callable on construction and another on destruction.
 *
 * 'FuncGuard' is a non-copyable, non-movable RAII helper. The constructor
 * invokes 'func_in_ctor' immediately, and the destructor invokes
 * 'func_in_dtor'. The destructor is 'noexcept' by contract: any exception
 * thrown by the destructor callable is swallowed.
 *
 * Typical use: run a setup action on construction and the matching cleanup
 * action on scope exit (success or exception).
 *
 * @tparam FOut Callable type invoked on destruction. Must be invocable with
 *              no arguments.
 *
 * Example:
 * @code
 * {
 *     FuncGuard cleanup {
 *         [] { openResource(); },
 *         [] { closeResource(); }
 *     };
 *     // ... work with the resource ...
 * } // closeResource() is invoked here, even if an exception is thrown
 * @endcode
 *
 * @warning The destructor swallows all exceptions from 'func_in_dtor'.
 *          Do not rely on it for critical cleanup that must be observable;
 *          log or handle errors inside the callable instead.
 */
template <typename FOut>
    requires std::is_invocable_v<FOut>
class FuncGuard final {
public:
    /**
     * @brief Constructs the guard and immediately invokes 'func_in_ctor'.
     *
     * @tparam FIn Callable type invoked on construction. Must be invocable
     *              with no arguments.
     * @param func_in_ctor Callable invoked on construction.
     * @param func_in_dtor Callable invoked on destruction.
     *
     * @note If 'func_in_ctor' throws, the guard is not constructed and
     *       'func_in_dtor' is not invoked.
     */
    template <typename FIn>
        requires std::is_invocable_v<FIn>
    explicit FuncGuard(FIn&& func_in_ctor, FOut func_in_dtor)
        : m_func_in_dtor { std::move(func_in_dtor) }
    {
        std::invoke(std::forward<FIn>(func_in_ctor));
    }

    FuncGuard(const FuncGuard&) = delete;
    FuncGuard& operator=(const FuncGuard&) = delete;
    FuncGuard(FuncGuard&&) = delete;
    FuncGuard& operator=(FuncGuard&&) = delete;

    /**
     * @brief Invokes the destructor callable.
     *
     * Any exception thrown by the callable is swallowed.
     */
    ~FuncGuard()
    {
        try {
            m_func_in_dtor();
        } catch (const std::exception& ex) {
            std::fprintf(stderr, "FuncGuard deleter threw: %s\n", ex.what());
            std::terminate();
        } catch (...) {
            std::fputs("FuncGuard deleter threw unknown exception\n", stderr);
            std::terminate();
        }
    }

private:
    FOut m_func_in_dtor;
};

} // namespace kas::functional

#endif