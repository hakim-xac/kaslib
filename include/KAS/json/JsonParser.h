/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_JSON_PARSER_H
#define KASLIBCXX_JSON_PARSER_H

#include "JsonCommon.h"
#include "JsonValue.h"

#include <KAS/expected/expected.h>

#include <cctype>
#include <cstdint>
#include <string_view>

namespace kas::json {

template <typename StringT>
class BasicJsonParser {
public:
    using Value = BasicJsonValue<StringT>;
    using Object = BasicJsonObject<StringT>;
    using Array = BasicJsonArray<StringT>;
    using Result = kas::expected_type<Value, JsonParseError>;

    static constexpr std::size_t kMaxDepth = 512;

    static Result parse(std::string_view text);

private:
    explicit BasicJsonParser(std::string_view t);

    Result parseValue(std::size_t depth);

    Result parseObject(std::size_t depth);

    Result parseArray(std::size_t depth);

    kas::expected_type<StringT, JsonParseError> parseString();

    static StringT makeKey(StringT&& raw);

    Result parseNumber();

    Result parseLiteral();

    bool appendEscape(std::string& out);

    kas::expected_type<unsigned, JsonParseError> parseHex4();

    static void appendUtf8(std::string& out, unsigned cp);

    void skipWhitespace();

    char peek() const;

    char advance();

    bool match(char c);

    kas::unexpected<JsonParseError> fail(JsonErrorCode code, std::string msg) const;

    std::string_view text_;
    std::size_t pos_ = 0;
    std::size_t line_ = 1;
    std::size_t col_ = 1;

    std::optional<kas::unexpected<JsonParseError>> currentError_;
};

using JsonParserOwned = BasicJsonParser<std::string>;
using JsonParserView = BasicJsonParser<JsonStringView>;

} // namespace kas::json

namespace kas::json {

//------------------

template <typename StringT>
BasicJsonParser<StringT>::Result
BasicJsonParser<StringT>::parse(std::string_view text)
{
    BasicJsonParser p(text);
    p.skipWhitespace();
    auto v = p.parseValue(0);
    if (!v)
        return v;
    p.skipWhitespace();
    if (p.pos_ != p.text_.size())
        return p.fail(JsonErrorCode::TrailingCharacters,
            "Trailing characters after JSON value");
    return v;
}

//------------------

template <typename StringT>
BasicJsonParser<StringT>::BasicJsonParser(std::string_view t)
    : text_(t)
{
}

//------------------

template <typename StringT>
BasicJsonParser<StringT>::Result
BasicJsonParser<StringT>::parseValue(std::size_t depth)
{
    if (depth > kMaxDepth)
        return fail(JsonErrorCode::DepthExceeded, "Maximum nesting depth exceeded");

    skipWhitespace();
    if (pos_ >= text_.size())
        return fail(JsonErrorCode::UnexpectedEnd, "Unexpected end of input");

    switch (peek()) {
    case '{':
        return parseObject(depth + 1);
    case '[':
        return parseArray(depth + 1);
    case '"': {
        auto s = parseString();
        if (!s)
            return kas::unexpected<JsonParseError> { std::move(s).error() };
        return Value(std::move(*s));
    }
    case 't':
    case 'f':
    case 'n':
        return parseLiteral();
    default:
        if (peek() == '-' || std::isdigit(static_cast<unsigned char>(peek())))
            return parseNumber();
        return fail(JsonErrorCode::UnexpectedChar,
            std::string("Unexpected character '") + peek() + "'");
    }
}

//------------------

template <typename StringT>
BasicJsonParser<StringT>::Result
BasicJsonParser<StringT>::parseObject(std::size_t depth)
{
    advance(); // '{'
    Object obj;
    skipWhitespace();
    if (peek() == '}') {
        advance();
        return Value(std::move(obj));
    }

    while (true) {
        skipWhitespace();
        if (peek() != '"')
            return fail(JsonErrorCode::ExpectedKey, "Expected string key in object");

        auto key = parseString();
        if (!key)
            return kas::unexpected<JsonParseError> { std::move(key).error() };

        skipWhitespace();
        if (!match(':'))
            return fail(JsonErrorCode::ExpectedColon, "Expected ':'");

        auto val = parseValue(depth);
        if (!val)
            return val;

        obj.emplace(makeKey(std::move(*key)), std::move(*val));

        skipWhitespace();
        if (match(','))
            continue;
        if (match('}'))
            break;
        return fail(JsonErrorCode::ExpectedCommaOrBrace, "Expected ',' or '}'");
    }
    return Value(std::move(obj));
}

//------------------

template <typename StringT>
BasicJsonParser<StringT>::Result
BasicJsonParser<StringT>::parseArray(std::size_t depth)
{
    advance(); // '['
    Array arr;
    skipWhitespace();
    if (peek() == ']') {
        advance();
        return Value(std::move(arr));
    }

    while (true) {
        auto val = parseValue(depth);
        if (!val)
            return val;
        arr.push_back(std::move(*val));

        skipWhitespace();
        if (match(','))
            continue;
        if (match(']'))
            break;
        return fail(JsonErrorCode::ExpectedCommaOrBracket, "Expected ',' or ']'");
    }
    return Value(std::move(arr));
}

//------------------

template <typename StringT>
kas::expected_type<StringT, JsonParseError>
BasicJsonParser<StringT>::parseString()
{
    advance(); // '"'
    std::size_t start = pos_;

    if constexpr (std::is_same_v<StringT, std::string>) {
        std::string out;
        while (true) {
            if (pos_ >= text_.size())
                return fail(JsonErrorCode::UnexpectedEnd, "Unterminated string");

            char c = advance();
            if (c == '"')
                return out;
            if (c != '\\') {
                if (static_cast<unsigned char>(c) < 0x20)
                    return fail(JsonErrorCode::InvalidString,
                        "Unescaped control character in string");
                out += c;
                continue;
            }
            if (!appendEscape(out))
                return std::move(*currentError_);
        }
    } else {
        while (pos_ < text_.size()) {
            char c = text_[pos_];
            if (c == '"') {
                std::string_view sv = text_.substr(start, pos_ - start);
                advance();
                return JsonStringView { sv };
            }
            if (c == '\\')
                break;
            if (static_cast<unsigned char>(c) < 0x20)
                return fail(JsonErrorCode::InvalidString,
                    "Unescaped control character in string");
            advance();
        }

        std::string out(text_.substr(start, pos_ - start));
        while (true) {
            if (pos_ >= text_.size())
                return fail(JsonErrorCode::UnexpectedEnd, "Unterminated string");

            char c = advance();
            if (c == '"')
                return JsonStringView { std::move(out) };
            if (c != '\\') {
                out += c;
                continue;
            }

            if (!appendEscape(out))
                return std::move(*currentError_);
        }
    }
}

//------------------

template <typename StringT>
StringT
BasicJsonParser<StringT>::makeKey(StringT&& raw)
{
    if constexpr (std::is_same_v<StringT, std::string>) {
        return std::move(raw);
    } else {
        std::string owned;
        std::visit([&](const auto& s) { owned.assign(s.data(), s.size()); }, raw);
        return JsonStringView { std::move(owned) };
    }
}

//------------------

template <typename StringT>
BasicJsonParser<StringT>::Result
BasicJsonParser<StringT>::parseNumber()
{
    std::size_t start = pos_;
    const bool negative = (peek() == '-');
    if (negative)
        advance();

    if (pos_ >= text_.size() || !std::isdigit(static_cast<unsigned char>(peek())))
        return fail(JsonErrorCode::InvalidNumber, "Invalid number");

    // Ведущий ноль: "0" допустим; "01", "00" — нет.
    if (peek() == '0') {
        advance();
        if (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(peek())))
            return fail(JsonErrorCode::InvalidNumber, "Leading zeros are not allowed");
    } else {
        while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(peek())))
            advance();
    }

    const bool has_fraction = (pos_ < text_.size() && peek() == '.');
    if (has_fraction) {
        advance();
        if (pos_ >= text_.size() || !std::isdigit(static_cast<unsigned char>(peek())))
            return fail(JsonErrorCode::InvalidNumber, "Expected digit after '.'");
        while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(peek())))
            advance();
    }
    const bool has_exponent = (pos_ < text_.size() && (peek() == 'e' || peek() == 'E'));
    if (has_exponent) {
        advance();
        if (pos_ < text_.size() && (peek() == '+' || peek() == '-'))
            advance();
        if (pos_ >= text_.size() || !std::isdigit(static_cast<unsigned char>(peek())))
            return fail(JsonErrorCode::InvalidNumber, "Expected digit in exponent");
        while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(peek())))
            advance();
    }

    const std::string_view sv = text_.substr(start, pos_ - start);

    // No fraction and no exponent: an integer.
    if (!has_fraction && !has_exponent) {
        if (negative) {
            std::int64_t i = 0;
            auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), i);
            if (ec == std::errc { } && ptr == sv.data() + sv.size())
                return Value(i); // -> JsonTypes::Signed
        } else {
            std::uint64_t u = 0;
            auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), u);
            if (ec == std::errc { } && ptr == sv.data() + sv.size())
                return Value(u); // -> JsonTypes::Unsigned
        }
    }

    // Fallback: floating-point number.
    double value = 0.0;
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);
    if (ec != std::errc { } || ptr != sv.data() + sv.size())
        return fail(JsonErrorCode::InvalidNumber, "Number out of range or malformed");
    return Value(value);
}

//------------------

template <typename StringT>
BasicJsonParser<StringT>::Result
BasicJsonParser<StringT>::parseLiteral()
{
    if (text_.substr(pos_, 4) == "true") {
        pos_ += 4;
        col_ += 4;
        return Value(true);
    }
    if (text_.substr(pos_, 5) == "false") {
        pos_ += 5;
        col_ += 5;
        return Value(false);
    }
    if (text_.substr(pos_, 4) == "null") {
        pos_ += 4;
        col_ += 4;
        return Value(nullptr);
    }
    return fail(JsonErrorCode::InvalidLiteral, "Invalid literal");
}

//------------------

template <typename StringT>
bool BasicJsonParser<StringT>::appendEscape(std::string& out)
{
    currentError_.reset();

    if (pos_ >= text_.size()) {
        currentError_ = fail(JsonErrorCode::UnexpectedEnd, "Unterminated escape");
        return false;
    }
    char esc = advance();
    switch (esc) {
    case '"':
        out += '"';
        return true;
    case '\\':
        out += '\\';
        return true;
    case '/':
        out += '/';
        return true;
    case 'b':
        out += '\b';
        return true;
    case 'f':
        out += '\f';
        return true;
    case 'n':
        out += '\n';
        return true;
    case 'r':
        out += '\r';
        return true;
    case 't':
        out += '\t';
        return true;
    case 'u': {
        auto cp = parseHex4();
        if (!cp) {
            currentError_ = kas::unexpected<JsonParseError> { std::move(cp).error() };
            return false;
        }
        unsigned code = *cp;
        if (code >= 0xD800 && code <= 0xDBFF) {
            if (pos_ + 1 >= text_.size()
                || text_[pos_] != '\\' || text_[pos_ + 1] != 'u') {
                currentError_ = fail(JsonErrorCode::InvalidUnicode,
                    "Expected low surrogate");
                return false;
            }
            advance();
            advance();
            auto low = parseHex4();
            if (!low) {
                currentError_ = kas::unexpected<JsonParseError> { std::move(low).error() };
                return false;
            }
            if (*low < 0xDC00 || *low > 0xDFFF) {
                currentError_ = fail(JsonErrorCode::InvalidUnicode,
                    "Invalid low surrogate");
                return false;
            }
            code = 0x10000 + ((code - 0xD800) << 10) + (*low - 0xDC00);
        }
        appendUtf8(out, code);
        return true;
    }
    default:
        currentError_ = fail(JsonErrorCode::InvalidEscape,
            std::string("Invalid escape '\\") + esc + "'");
        return false;
    }
}

//------------------

template <typename StringT>
kas::expected_type<unsigned, JsonParseError>
BasicJsonParser<StringT>::parseHex4()
{
    unsigned v = 0;
    for (int i = 0; i < 4; ++i) {
        if (pos_ >= text_.size())
            return fail(JsonErrorCode::UnexpectedEnd, "Unexpected end of \\u escape");
        char h = advance();
        v <<= 4;
        if (h >= '0' && h <= '9')
            v |= (h - '0');
        else if (h >= 'a' && h <= 'f')
            v |= (h - 'a' + 10);
        else if (h >= 'A' && h <= 'F')
            v |= (h - 'A' + 10);
        else
            return fail(JsonErrorCode::InvalidUnicode, "Invalid hex digit in \\u escape");
    }
    return v;
}

//------------------

template <typename StringT>
void BasicJsonParser<StringT>::appendUtf8(std::string& out, unsigned cp)
{
    if (cp <= 0x7F) {
        out += static_cast<char>(cp);
    } else if (cp <= 0x7FF) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp <= 0xFFFF) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

//------------------

template <typename StringT>
void BasicJsonParser<StringT>::skipWhitespace()
{
    while (pos_ < text_.size()) {
        char c = text_[pos_];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
            advance();
        else
            break;
    }
}

//------------------

template <typename StringT>
char BasicJsonParser<StringT>::peek() const
{
    return pos_ < text_.size() ? text_[pos_] : '\0';
}

//------------------

template <typename StringT>
char BasicJsonParser<StringT>::advance()
{
    char c = text_[pos_++];
    if (c == '\n') {
        ++line_;
        col_ = 1;
    } else {
        ++col_;
    }
    return c;
}

//------------------

template <typename StringT>
bool BasicJsonParser<StringT>::match(char c)
{
    if (pos_ < text_.size() && text_[pos_] == c) {
        advance();
        return true;
    }
    return false;
}

//------------------

template <typename StringT>
kas::unexpected<JsonParseError>
BasicJsonParser<StringT>::fail(JsonErrorCode code, std::string msg) const
{
    return kas::unexpected<JsonParseError> { JsonParseError { code, line_, col_, std::move(msg) } };
}

//------------------

}

#endif