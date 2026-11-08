/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_JSON_COMMON_H
#define KASLIBCXX_JSON_COMMON_H

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>

#include "KAS/utils/utils.h"

namespace kas::json {

enum class JsonTypes {
    Null,
    Boolean,
    Integer, ///< Signed integer that fits into std::int64_t.
    Unsigned, ///< Unsigned integer that fits into std::uint64_t.
    Signed, ///< Signed integer that fits into std::int64_t.
    Number, ///< Floating-point number (double).
    String,
    Array,
    Object
};

enum class JsonErrorCode {
    UnexpectedEnd,
    UnexpectedChar,
    InvalidLiteral,
    InvalidNumber,
    InvalidString,
    InvalidEscape,
    InvalidUnicode,
    TrailingCharacters,
    DepthExceeded,
    ExpectedKey,
    ExpectedColon,
    ExpectedCommaOrBrace,
    ExpectedCommaOrBracket,
};

struct JsonParseError {
    JsonErrorCode code;
    std::size_t line;
    std::size_t col;
    std::string message;

    std::string what() const;
};

using JsonStringView = std::variant<std::string_view, std::string>;

struct JsonStringViewLess {
    using is_transparent = void;

    static std::string_view get(const JsonStringView& v);

    static std::string_view get(std::string_view s);

    bool operator()(const JsonStringView& a, const JsonStringView& b) const;

    bool operator()(const JsonStringView& a, std::string_view b) const;

    bool operator()(std::string_view a, const JsonStringView& b) const;
};

template <JsonTypes T>
struct JsonType;

template <>
struct JsonType<JsonTypes::Null> {
    using type = std::nullptr_t;
};
template <>
struct JsonType<JsonTypes::Boolean> {
    using type = bool;
};
template <>
struct JsonType<JsonTypes::Integer> {
    using type = std::int64_t;
};
template <>
struct JsonType<JsonTypes::Unsigned> {
    using type = std::uint64_t;
};
template <>
struct JsonType<JsonTypes::Signed> {
    using type = std::int64_t;
};
template <>
struct JsonType<JsonTypes::Number> {
    using type = double;
};

template <JsonTypes T>
inline constexpr bool is_container_v = (T == JsonTypes::Array || T == JsonTypes::Object);

template <JsonTypes T>
inline constexpr bool is_scalar_v = !is_container_v<T>;

template <JsonTypes T>
inline constexpr bool is_integral_json_v = (T == JsonTypes::Integer || T == JsonTypes::Unsigned || T == JsonTypes::Signed);

} // namespace kas::json

namespace kas::json {

//------------------

inline bool JsonStringViewLess::operator()(const JsonStringView& a, const JsonStringView& b) const
{
    return get(a) < get(b);
}

//------------------

inline bool JsonStringViewLess::operator()(const JsonStringView& a, std::string_view b) const
{
    return get(a) < b;
}

//------------------

inline bool JsonStringViewLess::operator()(std::string_view a, const JsonStringView& b) const
{
    return a < get(b);
}

//------------------

inline std::string
JsonParseError::what() const
{
    return utils::stringify(
        "JSON error at line ",
        std::to_string(line),
        ", col ",
        std::to_string(col),
        ": ", message);
}

//------------------

inline std::string_view
JsonStringViewLess::get(const JsonStringView& v)
{
    return std::visit([](const auto& s) -> std::string_view {
        return std::string_view(s);
    },
        v);
}

//------------------

inline std::string_view
JsonStringViewLess::get(std::string_view s)
{
    return s;
}

//------------------
} // namespace kas::json

#endif