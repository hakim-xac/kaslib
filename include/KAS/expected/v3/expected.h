/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_EXPECTED_V3_H
#define KASLIBCXX_EXPECTED_V3_H

#include <exception>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

/**
 * @brief A lightweight std::expected-like type, version 3.
 *
 * `expected<T, E>` holds either a value of type T or an error of type E.
 * It is implemented on top of std::variant and provides both value/error
 * accessors and std::optional-like observers.
 *
 * The void specialization `expected<void, E>` represents an operation that
 * either succeeds (no value) or fails with an error.
 *
 * @note The implementation differs from C++23 std::expected:
 *       - the error type is wrapped in `unexpected<E>`;
 *       - there is no `value_or`, `and_then`, `transform`, etc.;
 *       - accessors throw std::runtime_error instead of std::bad_expected_access;
 *       - a default-constructed `expected<T, E>` holds a default-constructed T.
 */
namespace kas::v3 {

/**
 * @brief Wrapper marking a value as an error for expected construction.
 *
 * @tparam E Error type.
 */
template <typename E>
struct unexpected {
    E error;
};

/**
 * @brief Deduction guide for unexpected, allowing `unexpected{value}`.
 */
template <typename E>
unexpected(E) -> unexpected<E>;

/**
 * @brief Holds either a value of type T or an error of type E.
 *
 * @tparam T Value type.
 * @tparam E Error type.
 */
template <typename T, typename E>
class expected final {
    template <typename Err>
    using err_type_t = unexpected<Err>;

    static constexpr std::size_t kMonostate = 0;
    static constexpr std::size_t kValue = 1;
    static constexpr std::size_t kError = 2;

public:
    /**
     * @brief Constructs an expected holding a default-constructed T.
     *
     * Available only if T is default-constructible.
     */
    constexpr expected()
        requires std::default_initializable<T>
        : m_var { std::in_place_index<kValue> }
    {
    }

    /**
     * @brief Constructs an expected holding a value.
     *
     * @param u Value to store; must be convertible to T and must not be
     *          an expected or unexpected.
     */
    template <typename U>
        requires std::constructible_from<T, U>
        && (!std::same_as<std::remove_cvref_t<U>, expected>)
        && (!std::same_as<std::remove_cvref_t<U>, err_type_t<E>>)
    constexpr expected(U&& u)
        : m_var { std::in_place_index<kValue>, std::forward<U>(u) }
    {
    }

    /**
     * @brief Constructs an expected holding an error.
     *
     * @param e Unexpected wrapper around the error value.
     */
    template <typename U>
        requires std::constructible_from<E, U&&>
    constexpr expected(unexpected<U> e)
        : m_var { std::in_place_index<kError>,
            err_type_t<E> { E { std::move(e).error } } }
    {
    }

    expected(const expected&) = default;
    expected(expected&&) = default;
    expected& operator=(const expected&) = default;
    expected& operator=(expected&&) = default;
    ~expected() = default;

    /**
     * @brief Replaces the contents with a value.
     *
     * @param u Value to store.
     * @return *this
     */
    template <typename U>
        requires std::constructible_from<T, U>
    expected& emplace(U&& u) &
    {
        m_var.template emplace<kValue>(std::forward<U>(u));
        return *this;
    }

    /**
     * @brief Replaces the contents with an error.
     *
     * @param u Unexpected wrapper around the error value.
     * @return *this
     */
    template <typename U>
        requires std::same_as<std::remove_cvref_t<U>, err_type_t<E>>
    expected& emplace(U&& u) &
    {
        m_var.template emplace<kError>(std::forward<U>(u));
        return *this;
    }

    /**
     * @brief Resets the object to the valueless monostate state.
     *
     * After reset(), both has_value() and has_error() return false.
     *
     * @return *this
     */
    expected& reset() &
    {
        m_var.template emplace<kMonostate>();
        return *this;
    }

    /**
     * @brief Returns the stored value.
     *
     * @throws std::runtime_error if the object does not hold a value.
     */
    [[nodiscard]] constexpr T& value() &
    {
        if (m_var.index() != kValue) [[unlikely]]
            throw std::runtime_error { "Expected object has no value!" };
        return std::get<kValue>(m_var);
    }

    /**
     * @brief Returns the stored value (const).
     *
     * @throws std::runtime_error if the object does not hold a value.
     */
    [[nodiscard]] constexpr const T& value() const&
    {
        if (m_var.index() != kValue) [[unlikely]]
            throw std::runtime_error { "Expected object has no value!" };
        return std::get<kValue>(m_var);
    }

    /**
     * @brief Returns the stored value (rvalue).
     *
     * @throws std::runtime_error if the object does not hold a value.
     */
    [[nodiscard]] constexpr T&& value() &&
    {
        if (m_var.index() != kValue) [[unlikely]]
            throw std::runtime_error { "Expected object has no value!" };
        return std::move(std::get<kValue>(m_var));
    } /**
       * @brief Returns the stored value, or @p default_value if none is present.
       *
       * On value: returns a copy of value() (lvalue overload) or moves it
       * (rvalue overload). On error or in the monostate state: returns
       * @p default_value (copied in the lvalue overload, moved in the rvalue
       * overload).
       *
       * @param default_value Fallback value.
       * @return The stored value or the fallback.
       */
    template <typename U>
        requires std::copy_constructible<T> && std::convertible_to<U&&, T>
    [[nodiscard]] constexpr T value_or(U&& default_value) const&
    {
        if (has_value())
            return value();
        return static_cast<T>(std::forward<U>(default_value));
    }

    /**
     * @brief Returns the stored value, or @p default_value if none is present.
     *
     * @param default_value Fallback value.
     * @return The moved value or the moved fallback.
     */
    template <typename U>
        requires std::move_constructible<T> && std::convertible_to<U&&, T>
    [[nodiscard]] constexpr T value_or(U&& default_value) &&
    {
        if (has_value())
            return std::move(value());
        return static_cast<T>(std::forward<U>(default_value));
    }

    /**
     * @brief Returns the stored error.
     *
     * @throws std::runtime_error if the object does not hold an error.
     */
    [[nodiscard]] constexpr E& error() &
    {
        if (m_var.index() != kError) [[unlikely]]
            throw std::runtime_error { "Expected object has no error!" };
        return std::get<kError>(m_var).error;
    }

    /**
     * @brief Returns the stored error (const).
     *
     * @throws std::runtime_error if the object does not hold an error.
     */
    [[nodiscard]] constexpr const E& error() const&
    {
        if (m_var.index() != kError) [[unlikely]]
            throw std::runtime_error { "Expected object has no error!" };
        return std::get<kError>(m_var).error;
    }

    /**
     * @brief Returns the stored error (rvalue).
     *
     * @throws std::runtime_error if the object does not hold an error.
     */
    [[nodiscard]] constexpr E&& error() &&
    {
        if (m_var.index() != kError) [[unlikely]]
            throw std::runtime_error { "Expected object has no error!" };
        return std::move(std::get<kError>(m_var)).error;
    }

    /**
     * @brief Checks whether the object holds a value.
     */
    [[nodiscard]] constexpr bool has_value() const noexcept
    {
        return m_var.index() == kValue;
    }

    /**
     * @brief Checks whether the object holds an error.
     */
    [[nodiscard]] constexpr bool has_error() const noexcept
    {
        return m_var.index() == kError;
    }

    /**
     * @brief Contextual conversion to bool; true if a value is held.
     */
    constexpr explicit operator bool() const noexcept { return has_value(); }

    /**
     * @brief Dereferences the stored value.
     *
     * @throws std::runtime_error if the object does not hold a value.
     */
    [[nodiscard]] constexpr T& operator*() &
    {
        if (m_var.index() != kValue) [[unlikely]]
            throw std::runtime_error { "Expected object has no value!" };
        return std::get<kValue>(m_var);
    }

    /**
     * @brief Dereferences the stored value (const).
     *
     * @throws std::runtime_error if the object does not hold a value.
     */
    [[nodiscard]] constexpr const T& operator*() const&
    {
        if (m_var.index() != kValue) [[unlikely]]
            throw std::runtime_error { "Expected object has no value!" };
        return std::get<kValue>(m_var);
    }

    /**
     * @brief Dereferences the stored value (rvalue).
     *
     * @throws std::runtime_error if the object does not hold a value.
     */
    [[nodiscard]] constexpr T&& operator*() &&
    {
        if (m_var.index() != kValue) [[unlikely]]
            throw std::runtime_error { "Expected object has no value!" };
        return std::move(std::get<kValue>(m_var));
    }

    /**
     * @brief Member access to the stored value.
     *
     * @throws std::runtime_error if the object does not hold a value.
     */
    constexpr T* operator->()
    {
        if (m_var.index() != kValue) [[unlikely]]
            throw std::runtime_error { "Expected object has no value!" };
        return std::addressof(std::get<kValue>(m_var));
    }

    /**
     * @brief Member access to the stored value (const).
     *
     * @throws std::runtime_error if the object does not hold a value.
     */
    constexpr const T* operator->() const
    {
        if (m_var.index() != kValue) [[unlikely]]
            throw std::runtime_error { "Expected object has no value!" };
        return std::addressof(std::get<kValue>(m_var));
    }

private:
    std::variant<std::monostate, T, err_type_t<E>> m_var;
};

// Specialization for void value type: represents success without a value.

/**
 * @brief Specialization of expected for operations that return no value.
 *
 * Holds either "success" (no value) or an error of type E.
 *
 * @tparam E Error type.
 */
template <typename E>
class expected<void, E> final {
    template <typename Err>
    using err_type_t = unexpected<Err>;

    static constexpr std::size_t kValue = 0; // monostate == success
    static constexpr std::size_t kError = 1;

public:
    /**
     * @brief Constructs a successful expected (no value).
     */
    constexpr expected() noexcept
        : m_var { std::in_place_index<kValue> }
    {
    }

    /**
     * @brief Constructs an expected holding an error.
     *
     * @param e Unexpected wrapper around the error value.
     */
    template <typename U>
        requires std::constructible_from<E, U&&>
    constexpr expected(unexpected<U> e)
        : m_var { std::in_place_index<kError>,
            err_type_t<E> { E { std::move(e).error } } }
    {
    }

    expected(const expected&) = default;
    expected(expected&&) = default;
    expected& operator=(const expected&) = default;
    expected& operator=(expected&&) = default;
    ~expected() = default;

    /**
     * @brief Checks that the object represents success.
     *
     * @throws std::runtime_error if the object holds an error.
     */
    void value() const
    {
        if (m_var.index() != kValue) [[unlikely]]
            throw std::runtime_error { "Expected object has no value!" };
    }

    /**
     * @brief Returns the stored error.
     *
     * @throws std::runtime_error if the object does not hold an error.
     */
    [[nodiscard]] constexpr E& error() &
    {
        if (m_var.index() != kError) [[unlikely]]
            throw std::runtime_error { "Expected object has no error!" };
        return std::get<kError>(m_var).error;
    }

    /**
     * @brief Returns the stored error (const).
     *
     * @throws std::runtime_error if the object does not hold an error.
     */
    [[nodiscard]] constexpr const E& error() const&
    {
        if (m_var.index() != kError) [[unlikely]]
            throw std::runtime_error { "Expected object has no error!" };
        return std::get<kError>(m_var).error;
    }

    /**
     * @brief Returns the stored error (rvalue).
     *
     * @throws std::runtime_error if the object does not hold an error.
     */
    [[nodiscard]] constexpr E&& error() &&
    {
        if (m_var.index() != kError) [[unlikely]]
            throw std::runtime_error { "Expected object has no error!" };
        return std::move(std::get<kError>(m_var)).error;
    }

    /**
     * @brief Checks whether the object represents success.
     */
    [[nodiscard]] constexpr bool has_value() const noexcept
    {
        return m_var.index() == kValue;
    }

    /**
     * @brief Checks whether the object holds an error.
     */
    [[nodiscard]] constexpr bool has_error() const noexcept
    {
        return m_var.index() == kError;
    }

    /**
     * @brief Contextual conversion to bool; true on success.
     */
    constexpr explicit operator bool() const noexcept { return has_value(); }

    /**
     * @brief Resets the object to the success state.
     *
     * @return *this
     */
    expected& reset() &
    {
        m_var.template emplace<kValue>();
        return *this;
    }

private:
    std::variant<std::monostate, err_type_t<E>> m_var;
};

// A deduction guide for unexpected<void> is not needed - T is explicit.
} // namespace kas::v3

#endif