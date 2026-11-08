/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_FUNCTIONAL_MOVE_ONLY_FUNCTION_H
#define KASLIBCXX_FUNCTIONAL_MOVE_ONLY_FUNCTION_H
#include <functional>
#include <memory>
#include <utility>

/**
 * @brief Function wrappers and related utilities.
 */
namespace kas::functional {
/**
 * @brief Forward declaration of the move-only callable wrapper.
 *
 * @tparam Signature Callable signature, e.g. `int(double)`.
 */
template <typename Signature>
class MoveOnlyFunction;

/**
 * @brief Type-erased wrapper for move-only callables.
 *
 * `MoveOnlyFunction<TRet(Args...)>` stores any callable that is invocable
 * with `Args...` and returns a value convertible to `TRet`. Unlike
 * `std::function`, the stored callable does not need to be copyable, which
 * makes this type suitable for holding move-only lambdas, `std::unique_ptr`
 * captures, etc.
 *
 * The wrapper itself is move-only. Calling an empty wrapper throws
 * `std::bad_function_call`, matching the behavior of `std::function`.
 *
 * Implementation notes:
 *  - the callable is stored behind a `std::unique_ptr` to a virtual base,
 *    so a heap allocation occurs on construction (unless the wrapper is
 *    default-constructed or null);
 *  - `operator()` is `const` but still invokes the stored callable as a
 *    non-const lvalue reference (the callable is not logically const);
 *  - there is no small-buffer optimization.
 *
 * @tparam TRet Return type of the callable.
 * @tparam Args Argument types accepted by the callable.
 *
 * Example:
 * @code
 * MoveOnlyFunction<int(int)> f { [p = std::make_unique<int>(42)](int x) {
 *     return x + *p;
 * } };
 * int r = f(1); // r == 43
 * @endcode
 */
template <typename TRet, typename... Args>
class MoveOnlyFunction<TRet(Args...)> final {
private:
    /**
     * @brief Type-erased base for the stored callable.
     */
    struct base_t {
        virtual ~base_t() = default;
        virtual TRet invoke(Args... args) = 0;
    };

    /**
     * @brief Concrete holder for a specific callable type.
     *
     * @tparam TFunc Stored callable type (after decay).
     */
    template <typename TFunc>
    class derive_t final : public base_t {
    public:
        /**
         * @brief Constructs the holder from a callable.
         *
         * @param func Callable to store; moved into the holder.
         */
        explicit derive_t(TFunc&& func)
            : m_func { std::move(func) }
        {
        }
        ~derive_t() = default;
        TRet invoke(Args... args) override
        {
            return std::invoke(m_func, std::forward<Args>(args)...);
        }

    private:
        TFunc m_func;
    };

public:
    using result_type = TRet;

    /**
     * @brief Constructs a wrapper holding the given callable.
     *
     * @tparam F Callable type; must be invocable with Args... and return
     *           a value convertible to TRet.
     * @param f Callable to store; taken by forwarding reference and moved
     *          into the holder.
     */
    template <typename F>
        requires std::is_invocable_r_v<TRet, F&, Args...>
    MoveOnlyFunction(F&& f)
        : m_pimpl { std::make_unique<derive_t<std::decay_t<F>>>(std::move(f)) }
    {
    }

    /**
     * @brief Constructs an empty wrapper.
     */
    MoveOnlyFunction() noexcept = default;

    /**
     * @brief Constructs an empty wrapper from nullptr.
     */
    MoveOnlyFunction(std::nullptr_t) noexcept
        : m_pimpl { nullptr }
    {
    }

    ~MoveOnlyFunction() = default;
    MoveOnlyFunction(const MoveOnlyFunction&) = delete;
    MoveOnlyFunction& operator=(const MoveOnlyFunction&) = delete;
    MoveOnlyFunction(MoveOnlyFunction&&) noexcept = default;

    /**
     * @brief Clears the wrapper by assigning nullptr.
     *
     * @return *this
     */
    MoveOnlyFunction& operator=(std::nullptr_t) noexcept
    {
        m_pimpl.reset();
        return *this;
    }

    MoveOnlyFunction& operator=(MoveOnlyFunction&&) noexcept = default;

    /**
     * @brief Destroys the stored callable, leaving the wrapper empty.
     */
    void reset() noexcept
    {
        m_pimpl.reset();
    }

    /**
     * @brief Checks whether the wrapper holds a callable.
     */
    explicit operator bool() const noexcept
    {
        return !!m_pimpl;
    }

    /**
     * @brief Invokes the stored callable.
     *
     * @param args Arguments forwarded to the callable.
     * @return Result of the callable converted to TRet.
     *
     * @throws std::bad_function_call if the wrapper is empty.
     */
    TRet operator()(Args... args) const
    {
        if (!m_pimpl) [[unlikely]]
            throw std::bad_function_call { };

        return m_pimpl->invoke(std::forward<Args>(args)...);
    }

private:
    mutable std::unique_ptr<base_t> m_pimpl;
};

} // namespace kas

#endif