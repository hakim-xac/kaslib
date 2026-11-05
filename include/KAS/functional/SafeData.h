/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_FUNCTIONAL_SAFE_DATA_H
#define KASLIBCXX_FUNCTIONAL_SAFE_DATA_H
#include <mutex>
#include <type_traits>
#include <utility>

/**
 * @brief Function wrappers and related utilities.
 */
namespace kas::functional {
/**
 * @brief Thread-safe wrapper around a value of type TData.
 *
 * `SafeData<TData>` protects an internal value with a `std::mutex` and
 * exposes the value only through locked operations:
 *  - `update()` replaces the value under the lock;
 *  - `get()` returns a copy of the value under the lock;
 *  - `read()` invokes a callable with a const reference to the value;
 *  - `modify()` invokes a callable with a mutable reference to the value.
 *
 * The wrapper is non-copyable and non-movable, so a single instance owns
 * the value and the mutex. Copying would silently duplicate the mutex, and
 * moving would invalidate references held by concurrent readers.
 *
 * All accessors take the lock for the entire duration of the operation,
 * so callables passed to `read()`/`modify()` must not block for long and
 * must not call back into the same `SafeData` instance (that would
 * deadlock on a non-recursive mutex).
 *
 * @tparam TData Type of the protected value. Must be movable, because
 *               `update()` moves a new value into the internal storage.
 *
 * Example:
 * @code
 * SafeData<int> counter { 0 };
 * counter.modify([](int& v) { ++v; });
 * int current = counter.get();
 * counter.read([](const int& v) { std::cout << v << '\n'; });
 * @endcode
 */
template <typename TData>
class SafeData {
public:
    /**
     * @brief Constructs the protected value in place from the given
     *        arguments.
     *
     * @tparam Args Constructor argument types for TData.
     * @param args Arguments forwarded to the TData constructor.
     */
    template <typename... Args>
    explicit SafeData(Args&&... args)
        : m_data { std::forward<Args>(args)... }
        , m_mx { }
    {
    }

    SafeData(const SafeData&) = delete;
    SafeData(SafeData&&) = delete;
    SafeData& operator=(const SafeData&) = delete;
    SafeData& operator=(SafeData&&) = delete;
    ~SafeData() = default;

    /**
     * @brief Replaces the protected value under the lock.
     *
     * @param data New value; moved into the internal storage.
     * @return *this
     */
    SafeData& update(TData data) &
    {
        {
            std::lock_guard lk { m_mx };
            m_data = std::move(data);
        }
        return *this;
    }

    /**
     * @brief Returns a copy of the protected value under the lock.
     *
     * @return Copy of the stored value.
     *
     * @note The returned copy is detached from the internal state; later
     *       updates to the wrapper do not affect it.
     */
    [[nodiscard]] TData get() const
    {
        std::lock_guard lk { m_mx };
        return m_data;
    }

    /**
     * @brief Invokes a callable with a const reference to the value under
     *        the lock.
     *
     * The callable runs while the mutex is held, so it must be short and
     * must not re-enter the same SafeData instance.
     *
     * @tparam Func Callable type; must be invocable with `const TData&`.
     * @param func Callable to invoke.
     */
    template <std::invocable<const TData&> Func>
    void read(Func&& func) const
    {
        std::lock_guard lk { m_mx };
        std::forward<Func>(func)(std::as_const(m_data));
    }

    /**
     * @brief Invokes a callable with a mutable reference to the value under
     *        the lock.
     *
     * The callable runs while the mutex is held, so it must be short and
     * must not re-enter the same SafeData instance.
     *
     * @tparam Func Callable type; must be invocable with `TData&`.
     * @param func Callable to invoke.
     */
    template <std::invocable<TData&> Func>
    void modify(Func&& func)
    {
        std::lock_guard lk { m_mx };
        std::forward<Func>(func)(m_data);
    }

private:
    TData m_data;
    mutable std::mutex m_mx;
};

} // namespace kas::functional
#endif