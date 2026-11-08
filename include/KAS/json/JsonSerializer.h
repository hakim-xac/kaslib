/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_JSON_SERIALIZER_H
#define KASLIBCXX_JSON_SERIALIZER_H

#include "JsonCommon.h"
#include "JsonValue.h"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

namespace kas::json {

template <typename Value>
class BasicJsonSerializer {
public:
    static std::string toString(const Value& v, int indent = 0);

    static void writeTo(const Value& v, std::string& out, int indent = 0);

    static std::size_t serializedSize(const Value& v, int indent = 0);

private:
    static void writeIndent(std::string& out, int indent, int level);

    static std::size_t indentSize(int indent, int level);

    static void writeString(std::string& out, std::string_view s);

    static std::size_t stringSize(std::string_view s);

    static std::string_view formatNumber(double d, char (&buf)[32]);

    static void writeNumber(std::string& out, double d);
    static std::size_t numberSize(double d);

    static std::string_view svOf(const std::string& s);
    static std::string_view svOf(std::string_view s);
    static std::string_view svOf(const JsonStringView& s);

    static void writeValue(std::string& out, const Value& v, int indent, int level);

    static std::size_t calcSize(const Value& v, int indent, int level);

    static void writeInteger(std::string& out, std::int64_t v);
    static void writeUnsigned(std::string& out, std::uint64_t v);
    static std::size_t integerSize(std::int64_t v);
    static std::size_t unsignedSize(std::uint64_t v);
};

using JsonSerializer = BasicJsonSerializer<JsonValue>;
using JsonSerializerView = BasicJsonSerializer<JsonValueView>;

} // namespace kas::json

namespace kas::json {

//------------------

template <typename Value>
std::string
BasicJsonSerializer<Value>::toString(const Value& v, int indent)
{
    std::string out;
    writeValue(out, v, indent, 0);
    return out;
}

//------------------

template <typename Value>
void BasicJsonSerializer<Value>::writeTo(const Value& v, std::string& out, int indent)
{
    writeValue(out, v, indent, 0);
}

//------------------

template <typename Value>
std::size_t
BasicJsonSerializer<Value>::serializedSize(const Value& v, int indent)
{
    return calcSize(v, indent, 0);
}

//------------------

template <typename Value>
void BasicJsonSerializer<Value>::writeIndent(std::string& out, int indent, int level)
{
    if (indent > 0) {
        out += '\n';
        out.append(static_cast<std::size_t>(indent * level), ' ');
    }
}

//------------------

template <typename Value>
std::size_t
BasicJsonSerializer<Value>::indentSize(int indent, int level)
{
    return indent > 0
        ? 1 + static_cast<std::size_t>(indent * level)
        : 0;
}

//------------------

template <typename Value>
void BasicJsonSerializer<Value>::writeString(std::string& out, std::string_view s)
{
    static constexpr char kHex[] = "0123456789abcdef";

    out += '"';
    for (unsigned char c : s) {
        switch (c) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\b':
            out += "\\b";
            break;
        case '\f':
            out += "\\f";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (c < 0x20) {
                out += "\\u00";
                out += kHex[(c >> 4) & 0xF];
                out += kHex[c & 0xF];
            } else {
                out += static_cast<char>(c);
            }
        }
    }
    out += '"';
}

//------------------

template <typename Value>
std::size_t
BasicJsonSerializer<Value>::stringSize(std::string_view s)
{
    std::size_t n = 2;
    for (unsigned char c : s) {
        switch (c) {
        case '"':
        case '\\':
        case '\b':
        case '\f':
        case '\n':
        case '\r':
        case '\t':
            n += 2;
            break;
        default:
            n += (c < 0x20) ? 6 : 1;
        }
    }
    return n;
}

//------------------

template <typename Value>
std::string_view
BasicJsonSerializer<Value>::formatNumber(double d, char (&buf)[32])
{
    std::to_chars_result res;
    if (std::trunc(d) == d && std::abs(d) < 1e15)
        res = std::to_chars(buf, buf + sizeof(buf), d, std::chars_format::fixed, 0);
    else
        res = std::to_chars(buf, buf + sizeof(buf), d, std::chars_format::general);

    if (res.ec != std::errc { })
        return { };
    return std::string_view(buf, static_cast<std::size_t>(res.ptr - buf));
}

//------------------

template <typename Value>
void BasicJsonSerializer<Value>::writeNumber(std::string& out, double d)
{
    char buf[32];
    out += formatNumber(d, buf);
}

//------------------

template <typename Value>
std::size_t
BasicJsonSerializer<Value>::numberSize(double d)
{
    char buf[32];
    return formatNumber(d, buf).size();
}

//------------------

template <typename Value>
std::string_view
BasicJsonSerializer<Value>::svOf(const std::string& s)
{
    return s;
}

//------------------

template <typename Value>
std::string_view
BasicJsonSerializer<Value>::svOf(std::string_view s)
{
    return s;
}

//------------------

template <typename Value>
std::string_view
BasicJsonSerializer<Value>::svOf(const JsonStringView& s)
{
    return std::visit(
        [](const auto& x) -> std::string_view {
            return std::string_view(x);
        },
        s);
}

//------------------

template <typename Value>
void BasicJsonSerializer<Value>::writeValue(std::string& out, const Value& v, int indent, int level)
{
    switch (v.type()) {
    case JsonTypes::Null:
        out += "null";
        break;
    case JsonTypes::Boolean:
        out += v.asBool() ? "true" : "false";
        break;
    case JsonTypes::Number:
        writeNumber(out, v.asNumber());
        break;
    case JsonTypes::String:
        writeString(out, svOf(v.asString()));
        break;
    case JsonTypes::Array: {
        const auto& a = v.asArray();
        out += '[';
        bool first = true;
        for (const auto& e : a) {
            if (!first)
                out += ',';
            first = false;
            writeIndent(out, indent, level + 1);
            writeValue(out, e, indent, level + 1);
        }
        if (!a.empty())
            writeIndent(out, indent, level);
        out += ']';
        break;
    }
    case JsonTypes::Object: {
        const auto& o = v.asObject();
        out += '{';
        bool first = true;
        for (const auto& [k, val] : o) {
            if (!first)
                out += ',';
            first = false;
            writeIndent(out, indent, level + 1);
            writeString(out, svOf(k));
            out += indent > 0 ? ": " : ":";
            writeValue(out, val, indent, level + 1);
        }
        if (!o.empty())
            writeIndent(out, indent, level);
        out += '}';
        break;
    }
    case JsonTypes::Integer:
    case JsonTypes::Signed:
        writeInteger(out, v.asSigned());
        break;
    case JsonTypes::Unsigned:
        writeUnsigned(out, v.asUnsigned());
        break;
        break;
    }
}

//------------------

template <typename Value>
std::size_t
BasicJsonSerializer<Value>::calcSize(const Value& v, int indent, int level)
{
    switch (v.type()) {
    case JsonTypes::Null:
        return 4;
    case JsonTypes::Boolean:
        return v.asBool() ? 4 : 5;
    case JsonTypes::String:
        return stringSize(svOf(v.asString()));
    case JsonTypes::Array: {
        const auto& a = v.asArray();
        std::size_t n = 2;
        bool first = true;
        for (const auto& e : a) {
            if (!first)
                n += 1;
            first = false;
            n += indentSize(indent, level + 1);
            n += calcSize(e, indent, level + 1);
        }
        if (!a.empty())
            n += indentSize(indent, level);
        return n;
    }
    case JsonTypes::Object: {
        const auto& o = v.asObject();
        std::size_t n = 2;
        bool first = true;
        for (const auto& [k, val] : o) {
            if (!first)
                n += 1;
            first = false;
            n += indentSize(indent, level + 1);
            n += stringSize(svOf(k));
            n += indent > 0 ? 2 : 1;
            n += calcSize(val, indent, level + 1);
        }
        if (!o.empty())
            n += indentSize(indent, level);
        return n;
    }
    case JsonTypes::Integer:
    case JsonTypes::Signed:
        return integerSize(v.asSigned());
    case JsonTypes::Unsigned:
        return unsignedSize(v.asUnsigned());
    case JsonTypes::Number:
        return numberSize(v.asNumber());
    }

    return 0;
}

//------------------

template <typename Value>
void BasicJsonSerializer<Value>::writeInteger(std::string& out, std::int64_t v)
{
    char buf[24];
    auto res = std::to_chars(buf, buf + sizeof(buf), v);
    if (res.ec == std::errc { })
        out.append(buf, static_cast<std::size_t>(res.ptr - buf));
}

//------------------

template <typename Value>
void BasicJsonSerializer<Value>::writeUnsigned(std::string& out, std::uint64_t v)
{
    char buf[24];
    auto res = std::to_chars(buf, buf + sizeof(buf), v);
    if (res.ec == std::errc { })
        out.append(buf, static_cast<std::size_t>(res.ptr - buf));
}

//------------------

template <typename Value>
std::size_t BasicJsonSerializer<Value>::integerSize(std::int64_t v)
{
    char buf[24];
    auto res = std::to_chars(buf, buf + sizeof(buf), v);
    return res.ec == std::errc { } ? static_cast<std::size_t>(res.ptr - buf) : 0;
}

//------------------

template <typename Value>
std::size_t BasicJsonSerializer<Value>::unsignedSize(std::uint64_t v)
{
    char buf[24];
    auto res = std::to_chars(buf, buf + sizeof(buf), v);
    return res.ec == std::errc { } ? static_cast<std::size_t>(res.ptr - buf) : 0;
}

//------------------
}

#endif