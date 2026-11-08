/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_NET_SUPPORTED_HTTP_METHODS_H
#define KASLIBCXX_NET_SUPPORTED_HTTP_METHODS_H

#include <string_view>
#include <vector>

namespace kas::net::http {

/**
 * @brief Supported HTTP methods as a compile-time tag enum.
 *
 * Values correspond to standard HTTP methods (RFC 9110, section 9), plus
 * the special @ref Methods::ALL tag that expands to the full list of
 * concrete methods.
 */
enum class Methods {
    GET, ///< GET method.
    HEAD, ///< HEAD method.
    POST, ///< POST method.
    PUT, ///< PUT method.
    PATCH, ///< PATCH method.
    DELETE, ///< DELETE method.
    OPTIONS, ///< OPTIONS method.
    TRACE, ///< TRACE method.
    CONNECT, ///< CONNECT method.
    ALL ///< Pseudo-tag covering all concrete methods above.
};

/**
 * @brief Compile-time mapping from a @ref Methods tag to its string form.
 *
 * Primary template is intentionally left undefined so that only explicitly
 * supported methods can be used (a missing specialization produces a
 * compile-time error).
 *
 * Each specialization provides:
 * - @c method: the HTTP method name as a @c constexpr std::string_view
 *   (for concrete method tags);
 * - @c method_t: a marker type used by @c HttpRoute::setMethod to detect
 *   single-method specializations;
 * - @c all_method_t and @c allowed_methods: a marker type and the list of
 *   all supported method names (for @ref Methods::ALL only).
 *
 * @tparam Tag Method tag to map.
 */
template <Methods>
struct Method;

/// @brief Specialization for @ref Methods::GET.
template <>
struct Method<Methods::GET> {
    using method_t = void;
    constexpr static std::string_view method { "GET" };
};
/// @brief Specialization for @ref Methods::HEAD.
template <>
struct Method<Methods::HEAD> {
    using method_t = void;
    constexpr static std::string_view method { "HEAD" };
};
/// @brief Specialization for @ref Methods::POST.
template <>
struct Method<Methods::POST> {
    using method_t = void;
    constexpr static std::string_view method { "POST" };
};
/// @brief Specialization for @ref Methods::PUT.
template <>
struct Method<Methods::PUT> {
    using method_t = void;
    constexpr static std::string_view method { "PUT" };
};
/// @brief Specialization for @ref Methods::PATCH.
template <>
struct Method<Methods::PATCH> {
    using method_t = void;
    constexpr static std::string_view method { "PATCH" };
};
/// @brief Specialization for @ref Methods::DELETE.
template <>
struct Method<Methods::DELETE> {
    using method_t = void;
    constexpr static std::string_view method { "DELETE" };
};
/// @brief Specialization for @ref Methods::OPTIONS.
template <>
struct Method<Methods::OPTIONS> {
    using method_t = void;
    constexpr static std::string_view method { "OPTIONS" };
};
/// @brief Specialization for @ref Methods::TRACE.
template <>
struct Method<Methods::TRACE> {
    using method_t = void;
    constexpr static std::string_view method { "TRACE" };
};
/// @brief Specialization for @ref Methods::CONNECT.
template <>
struct Method<Methods::CONNECT> {
    using method_t = void;
    constexpr static std::string_view method { "CONNECT" };
};
/**
 * @brief Specialization for @ref Methods::ALL.
 *
 * Unlike concrete-method specializations, this one exposes
 * @c all_method_t and @c allowed_methods, which list every supported
 * concrete HTTP method.
 */
template <>
struct Method<Methods::ALL> {
    using all_method_t = void;
    inline static const std::vector<std::string_view> allowed_methods {
        Method<Methods::GET>::method,
        Method<Methods::HEAD>::method,
        Method<Methods::POST>::method,
        Method<Methods::PUT>::method,
        Method<Methods::PATCH>::method,
        Method<Methods::DELETE>::method,
        Method<Methods::OPTIONS>::method,
        Method<Methods::TRACE>::method,
        Method<Methods::CONNECT>::method
    };
};
} // namespace kas::net::http

#endif