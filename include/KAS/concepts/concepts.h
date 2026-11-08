/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_CONCEPTS_H
#define KASLIBCXX_CONCEPTS_H

#include <iterator>
#include <type_traits>

namespace kas::concepts {
//------------------

namespace {
    /**
     * @brief Helper alias that forms an lvalue reference to T.
     */
    template <class T>
    using with_ref_t = T&;
}
/**
 * @brief Checks that a reference to T can be formed.
 *
 * True if T is a type for which an lvalue reference can be created
 * (i.e. T is not void and not an abominable function type).
 */
template <class T>
concept can_reference_concept = requires { typename with_ref_t<T>; };

//------------------

/**
 * @brief Minimal iterator concept: dereferenceable and incrementable.
 *
 * Requires that dereferencing an object of type I yields a reference
 * (satisfies can_reference_concept) and that I is weakly incrementable.
 */
template <class I>
concept input_or_output_iterator_concept = requires(I i) {
    { *i } -> can_reference_concept;
} && std::weakly_incrementable<I>;

//------------------

/**
 * @brief Checks that a value of T can be streamed to std::ostream.
 *
 * True if `std::ostream << value` is a valid expression for an rvalue
 * of type T.
 */
template <typename T>
concept to_ostream = requires(T&& t) {
    std::declval<std::ostream>() << std::forward<T>(t);
};

//------------------

/**
 * @brief Checks that T is an integral type.
 *
 * Mirrors std::is_integral_v<T> as a concept.
 */
template <class T>
concept is_integral = std::is_integral_v<T>;

//------------------

/**
 * @brief Checks that values of T support ordered comparison.
 *
 * Requires both `a >= b` and `a <= b` to be valid and to return bool.
 */
template <typename T>
concept is_compared_concept = requires(T a, T b) {
    { a >= b } -> std::same_as<bool>;
    { a <= b } -> std::same_as<bool>;
};

//------------------

/**
 * @brief Checks that T is a trivial type.
 *
 * Note: this concept only verifies that the expression
 * std::is_trivial_v<T> is well-formed; it does not constrain on its
 * value. To require triviality, use the requirement form
 * `requires { requires std::is_trivial_v<T>; }` or
 * `std::is_trivial_v<T>` directly as a constraint.
 */
template <typename T>
concept is_trivial_concept = requires {
    std::is_trivial_v<T>;
};

//------------------

} // namespace kas::concepts

#endif