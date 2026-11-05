/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_NET_HTTP_RESPONSE_TYPES_H
#define KASLIBCXX_NET_HTTP_RESPONSE_TYPES_H

#include <concepts>
#include <cstdint>
#include <string_view>

namespace kas::net::http::concepts {

/**
 * @brief Concept for types that describe an HTTP response status.
 *
 * A conforming type must provide:
 * - a nested type alias @c response_type_t;
 * - a static member @c code convertible to std::uint16_t (the status code);
 * - a static member @c text convertible to std::string_view (the full
 *   status line, for example, "404 Not Found").
 */
template <typename T>
concept as_response_type = requires {
    typename T::response_type_t;
    { T::code } -> std::convertible_to<std::uint16_t>;
    { T::text } -> std::convertible_to<std::string_view>;
};
} // namespace kas::net::http::concepts

namespace kas::net::http {
/**
 * @brief Well-known HTTP response statuses as tag types.
 *
 * Each tag type exposes:
 * - @c response_type_t: a marker type used to detect response tags;
 * - @c code: the numeric status code (static, constexpr);
 * - @c text: the full status line (static, constexpr), for example,
 *   "200 OK".
 *
 * Tags satisfy @ref concepts::as_response_type and are intended to be used
 * as compile-time descriptors of HTTP responses, for example, with
 * @c Response::setResponseCode<T>().
 */
namespace response_types {
    /// @brief 101 Switching Protocols.
    struct SwitchingProtocols {
        using response_type_t = void;
        static constexpr std::uint16_t code { 101 };
        static constexpr std::string_view text { "101 Switching Protocols" };
    };
    /// @brief 200 OK.
    struct OK {
        using response_type_t = void;
        static constexpr std::uint16_t code { 200 };
        static constexpr std::string_view text { "200 OK" };
    };
    /// @brief 201 Created.
    struct Created {
        using response_type_t = void;
        static constexpr std::uint16_t code { 201 };
        static constexpr std::string_view text { "201 Created" };
    };
    /// @brief 202 Accepted.
    struct Accepted {
        using response_type_t = void;
        static constexpr std::uint16_t code { 202 };
        static constexpr std::string_view text { "202 Accepted" };
    };
    /// @brief 204 No Content.
    struct NoContent {
        using response_type_t = void;
        static constexpr std::uint16_t code { 204 };
        static constexpr std::string_view text { "204 No Content" };
    };
    /// @brief 206 Partial Content.
    struct PartialContent {
        using response_type_t = void;
        static constexpr std::uint16_t code { 206 };
        static constexpr std::string_view text { "206 Partial Content" };
    };
    /// @brief 301 Moved Permanently.
    struct MovedPermanently {
        using response_type_t = void;
        static constexpr std::uint16_t code { 301 };
        static constexpr std::string_view text { "301 Moved Permanently" };
    };
    /// @brief 302 Found.
    struct Found {
        using response_type_t = void;
        static constexpr std::uint16_t code { 302 };
        static constexpr std::string_view text { "302 Found" };
    };
    /// @brief 304 Not Modified.
    struct NotModified {
        using response_type_t = void;
        static constexpr std::uint16_t code { 304 };
        static constexpr std::string_view text { "304 Not Modified" };
    };
    /// @brief 400 Bad Request.
    struct BadRequest {
        using response_type_t = void;
        static constexpr std::uint16_t code { 400 };
        static constexpr std::string_view text { "400 Bad Request" };
    };
    /// @brief 401 Unauthorized.
    struct Unauthorized {
        using response_type_t = void;
        static constexpr std::uint16_t code { 401 };
        static constexpr std::string_view text { "401 Unauthorized" };
    };
    /// @brief 403 Forbidden.
    struct Forbidden {
        using response_type_t = void;
        static constexpr std::uint16_t code { 403 };
        static constexpr std::string_view text { "403 Forbidden" };
    };
    /// @brief 404 Not Found.
    struct NotFound {
        using response_type_t = void;
        static constexpr std::uint16_t code { 404 };
        static constexpr std::string_view text { "404 Not Found" };
    };
    /// @brief 405 Method Not Allowed.
    struct MethodNotAllowed {
        using response_type_t = void;
        static constexpr std::uint16_t code { 405 };
        static constexpr std::string_view text { "405 Method Not Allowed" };
    };
    /// @brief 409 Conflict.
    struct Conflict {
        using response_type_t = void;
        static constexpr std::uint16_t code { 409 };
        static constexpr std::string_view text { "409 Conflict" };
    };
    /// @brief 413 Payload Too Large.
    struct PayloadTooLarge {
        using response_type_t = void;
        static constexpr std::uint16_t code { 413 };
        static constexpr std::string_view text { "413 Payload Too Large" };
    };
    /// @brief 415 Unsupported Media Type.
    struct UnsupportedMediaType {
        using response_type_t = void;
        static constexpr std::uint16_t code { 415 };
        static constexpr std::string_view text { "415 Unsupported Media Type" };
    };
    /// @brief 416 Range Not Satisfiable.
    struct RangeNotSatisfiable {
        using response_type_t = void;
        static constexpr std::uint16_t code { 416 };
        static constexpr std::string_view text { "416 Range Not Satisfiable" };
    };
    /// @brief 429 Too Many Requests.
    struct TooManyRequests {
        using response_type_t = void;
        static constexpr std::uint16_t code { 429 };
        static constexpr std::string_view text { "429 Too Many Requests" };
    };
    /// @brief 500 Internal Server Error.
    struct InternalServerError {
        using response_type_t = void;
        static constexpr std::uint16_t code { 500 };
        static constexpr std::string_view text { "500 Internal Server Error" };
    };
    /// @brief 501 Not Implemented.
    struct NotImplemented {
        using response_type_t = void;
        static constexpr std::uint16_t code { 501 };
        static constexpr std::string_view text { "501 Not Implemented" };
    };
    /// @brief 502 Bad Gateway.
    struct BadGateway {
        using response_type_t = void;
        static constexpr std::uint16_t code { 502 };
        static constexpr std::string_view text { "502 Bad Gateway" };
    };
    /// @brief 503 Service Unavailable.
    struct ServiceUnavailable {
        using response_type_t = void;
        static constexpr std::uint16_t code { 503 };
        static constexpr std::string_view text { "503 Service Unavailable" };
    };
    /// @brief 504 Gateway Timeout.
    struct GatewayTimeout {
        using response_type_t = void;
        static constexpr std::uint16_t code { 504 };
        static constexpr std::string_view text { "504 Gateway Timeout" };
    };
} // namespace response_types

/**
 * @brief Returns the numeric status code of a response type tag.
 *
 * @tparam T Response type tag satisfying @ref concepts::as_response_type.
 * @return The static @c T::code value.
 */
template <concepts::as_response_type T>
[[nodiscard]] inline std::uint16_t getStatusCode() noexcept
{
    return T::code;
}

/**
 * @brief Returns the full status line of a response type tag.
 *
 * @tparam T Response type tag satisfying @ref concepts::as_response_type.
 * @return The static @c T::text value, for example, "404 Not Found".
 */
template <concepts::as_response_type T>
[[nodiscard]] inline std::string_view getStatusText() noexcept
{
    return T::text;
}

} // namespace kas::net::http

#endif