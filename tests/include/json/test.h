#ifndef KASLIBCXX_JSON_TESTS_H
#define KASLIBCXX_JSON_TESTS_H

#include "tests/tests.h"

#include "KAS/json/json.h"

#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace kas::json::tests {

namespace {

    using namespace kas::tests;
    using namespace kas::json;

    //--------------------------------------------------------------------
    //  Shared helpers
    //--------------------------------------------------------------------

    /// Checks that parse() returned a value (not an error) and returns it.
    template <typename Parser, typename Text>
    typename Parser::Value parseOk(TestContext& ctx, Text text)
    {
        auto r = Parser::parse(text);
        if (!r) {
            std::cerr << "    parse() failed for input: [" << text << "]\n";
            KAS_TEST_CHECK_IMPL(ctx, false && "parse() failed");
            return { };
        }
        return std::move(*r);
    }

    /// Checks that parse() returned an error with the given code.
    template <typename Parser, typename Text>
    void expectError(TestContext& ctx, Text text, JsonErrorCode code,
        const char* what = "")
    {
        auto r = Parser::parse(text);
        if (!r) {
            if (r.error().code != code) {
                std::cerr << "    expected error code "
                          << static_cast<int>(code) << ", got "
                          << static_cast<int>(r.error().code)
                          << " for input: [ " << text << " ]\n";
            }
            KAS_TEST_CHECK_IMPL(ctx, r.error().code == code);
            return;
        }
        std::cerr << "    expected error code " << static_cast<int>(code)
                  << ", got SUCCESS for input: [ " << text << " ]\n";
        (void)what;
        KAS_TEST_CHECK_IMPL(ctx, false && "expected parse error, got value");
    }

    /// Checks a round-trip: parse(text) -> toString == canon.
    template <typename Parser, typename Serializer>
    void expectRoundTrip(TestContext& ctx, const char* text, const char* canon)
    {
        auto v = parseOk<Parser>(ctx, std::string_view(text));
        KAS_TEST_CHECK_IMPL(ctx, Serializer::toString(v) == canon);
    }

    //--------------------------------------------------------------------
    //  JsonValue: basic types, type(), is*, as*, try_as
    //--------------------------------------------------------------------

    void testValueBasics(TestContext& ctx)
    {
        // Null
        {
            JsonValue v;
            KAS_TEST_CHECK_IMPL(ctx, v.type() == JsonTypes::Null);
            KAS_TEST_CHECK_IMPL(ctx, v.isNull());
            KAS_TEST_CHECK_IMPL(ctx,
                !v.isBool() && !v.isInteger() && !v.isUnsigned()
                    && !v.isSigned() && !v.isNumber() && !v.isString()
                    && !v.isArray() && !v.isObject());
            KAS_TEST_CHECK_IMPL(ctx, v.has<JsonTypes::Null>());
        }

        // Bool
        {
            JsonValue v(true);
            KAS_TEST_CHECK_IMPL(ctx, v.type() == JsonTypes::Boolean);
            KAS_TEST_CHECK_IMPL(ctx, v.isBool());
            KAS_TEST_CHECK_IMPL(ctx, v.asBool() == true);

            JsonValue f(false);
            KAS_TEST_CHECK_IMPL(ctx, f.asBool() == false);
        }

        // Number (double)
        {
            JsonValue v(3.5);
            KAS_TEST_CHECK_IMPL(ctx, v.type() == JsonTypes::Number);
            KAS_TEST_CHECK_IMPL(ctx, v.isNumber());
            KAS_TEST_CHECK_IMPL(ctx, v.asNumber() == 3.5);
        }

        // Signed integer via the template ctor
        {
            JsonValue v(42);
            KAS_TEST_CHECK_IMPL(ctx, v.type() == JsonTypes::Signed);
            KAS_TEST_CHECK_IMPL(ctx, v.isSigned());
            KAS_TEST_CHECK_IMPL(ctx, v.isInteger());
            KAS_TEST_CHECK_IMPL(ctx, v.asSigned() == 42);
            KAS_TEST_CHECK_IMPL(ctx, v.asInteger() == 42);
            KAS_TEST_CHECK_IMPL(ctx, v.asNumber() == 42.0);

            JsonValue n(-7);
            KAS_TEST_CHECK_IMPL(ctx, n.isSigned());
            KAS_TEST_CHECK_IMPL(ctx, n.asSigned() == -7);
        }

        // Unsigned integer via the template ctor
        {
            JsonValue u(std::size_t { 7 });
            KAS_TEST_CHECK_IMPL(ctx, u.type() == JsonTypes::Unsigned);
            KAS_TEST_CHECK_IMPL(ctx, u.isUnsigned());
            KAS_TEST_CHECK_IMPL(ctx, u.isInteger());
            KAS_TEST_CHECK_IMPL(ctx, u.asUnsigned() == 7u);
            KAS_TEST_CHECK_IMPL(ctx, u.asInteger() == 7);
            KAS_TEST_CHECK_IMPL(ctx, u.asNumber() == 7.0);
        }

        // Explicit std::int64_t / std::uint64_t constructors
        {
            JsonValue s(std::int64_t { -123 });
            KAS_TEST_CHECK_IMPL(ctx, s.isSigned());
            KAS_TEST_CHECK_IMPL(ctx, s.asSigned() == -123);

            JsonValue u(std::uint64_t { 123 });
            KAS_TEST_CHECK_IMPL(ctx, u.isUnsigned());
            KAS_TEST_CHECK_IMPL(ctx, u.asUnsigned() == 123u);
        }

        // String
        {
            JsonValue v("hello");
            KAS_TEST_CHECK_IMPL(ctx, v.type() == JsonTypes::String);
            KAS_TEST_CHECK_IMPL(ctx, v.asString() == "hello");
        }

        // Array
        {
            JsonValue v(JsonArray { JsonValue(1), JsonValue(2) });
            KAS_TEST_CHECK_IMPL(ctx, v.type() == JsonTypes::Array);
            KAS_TEST_CHECK_IMPL(ctx, v.asArray().size() == 2);
            KAS_TEST_CHECK_IMPL(ctx, v.asArray()[0].asNumber() == 1.0);
        }

        // Object
        {
            JsonObject o;
            o.emplace("k", JsonValue(7.0));
            JsonValue v(o);
            KAS_TEST_CHECK_IMPL(ctx, v.type() == JsonTypes::Object);
            KAS_TEST_CHECK_IMPL(ctx, v.asObject().size() == 1);
            KAS_TEST_CHECK_IMPL(ctx, v.asObject().at("k").asNumber() == 7.0);
        }
    }

    void testValueAsAndTryAs(TestContext& ctx)
    {
        JsonValue num(2.0);
        JsonValue str("abc");

        KAS_TEST_CHECK_IMPL(ctx, num.try_as<JsonTypes::Number>().has_value());
        KAS_TEST_CHECK_IMPL(ctx, *num.try_as<JsonTypes::Number>() == 2.0);
        KAS_TEST_CHECK_IMPL(ctx, !num.try_as<JsonTypes::String>().has_value());

        auto sv = str.try_as<JsonTypes::String>();
        KAS_TEST_CHECK_IMPL(ctx, sv.has_value());
        KAS_TEST_CHECK_IMPL(ctx, sv->get() == "abc");

        bool threw = false;
        try {
            (void)num.as<JsonTypes::String>();
        } catch (const std::bad_variant_access&) {
            threw = true;
        }
        KAS_TEST_CHECK_IMPL(ctx, threw);

        const JsonValue cnum(4.0);
        KAS_TEST_CHECK_IMPL(ctx, cnum.try_as<JsonTypes::Number>().has_value());
        KAS_TEST_CHECK_IMPL(ctx, cnum.has<JsonTypes::Number>());
        KAS_TEST_CHECK_IMPL(ctx, !cnum.has<JsonTypes::String>());
    }

    //--------------------------------------------------------------------
    //  JsonValue: integer types
    //--------------------------------------------------------------------

    void testValueIntegerAccessors(TestContext& ctx)
    {
        // Signed: as<Signed>, as<Integer>, asNumber, try_as
        {
            JsonValue v(std::int64_t { -42 });
            KAS_TEST_CHECK_IMPL(ctx, v.isSigned());
            KAS_TEST_CHECK_IMPL(ctx, v.isInteger());
            KAS_TEST_CHECK_IMPL(ctx, !v.isUnsigned());
            KAS_TEST_CHECK_IMPL(ctx, !v.isNumber());

            KAS_TEST_CHECK_IMPL(ctx, v.as<JsonTypes::Signed>() == -42);
            KAS_TEST_CHECK_IMPL(ctx, v.as<JsonTypes::Integer>() == -42);
            KAS_TEST_CHECK_IMPL(ctx, v.asNumber() == -42.0);

            auto t = v.try_as<JsonTypes::Signed>();
            KAS_TEST_CHECK_IMPL(ctx, t.has_value());
            KAS_TEST_CHECK_IMPL(ctx, *t == -42);
        }

        // Unsigned: as<Unsigned>, as<Integer>, asNumber, try_as
        {
            JsonValue v(std::uint64_t { 42 });
            KAS_TEST_CHECK_IMPL(ctx, v.isUnsigned());
            KAS_TEST_CHECK_IMPL(ctx, v.isInteger());
            KAS_TEST_CHECK_IMPL(ctx, !v.isSigned());
            KAS_TEST_CHECK_IMPL(ctx, !v.isNumber());

            KAS_TEST_CHECK_IMPL(ctx, v.as<JsonTypes::Unsigned>() == 42u);
            KAS_TEST_CHECK_IMPL(ctx, v.as<JsonTypes::Integer>() == 42);
            KAS_TEST_CHECK_IMPL(ctx, v.asNumber() == 42.0);

            auto t = v.try_as<JsonTypes::Unsigned>();
            KAS_TEST_CHECK_IMPL(ctx, t.has_value());
            KAS_TEST_CHECK_IMPL(ctx, *t == 42u);
        }

        // Cross-kind accessors must fail / return nullopt
        {
            JsonValue s(std::int64_t { -1 });
            KAS_TEST_CHECK_IMPL(ctx, !s.try_as<JsonTypes::Unsigned>().has_value());

            bool threw = false;
            try {
                (void)s.as<JsonTypes::Unsigned>();
            } catch (const std::bad_variant_access&) {
                threw = true;
            }
            KAS_TEST_CHECK_IMPL(ctx, threw);

            JsonValue u(std::uint64_t { 1 });
            KAS_TEST_CHECK_IMPL(ctx, !u.try_as<JsonTypes::Signed>().has_value());
        }

        // Number is not an integer
        {
            JsonValue d(1.0);
            KAS_TEST_CHECK_IMPL(ctx, !d.isInteger());
            KAS_TEST_CHECK_IMPL(ctx, !d.try_as<JsonTypes::Integer>().has_value());
        }
    }

    //--------------------------------------------------------------------
    //  Parser: scalar values
    //--------------------------------------------------------------------

    void testParseScalars(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, parseOk<JsonParserOwned>(ctx, "null").isNull());
        KAS_TEST_CHECK_IMPL(ctx, parseOk<JsonParserOwned>(ctx, "true").asBool() == true);
        KAS_TEST_CHECK_IMPL(ctx, parseOk<JsonParserOwned>(ctx, "false").asBool() == false);

        // Integer literals
        KAS_TEST_CHECK_IMPL(ctx, parseOk<JsonParserOwned>(ctx, "0").asNumber() == 0.0);
        KAS_TEST_CHECK_IMPL(ctx, parseOk<JsonParserOwned>(ctx, "-1").asNumber() == -1.0);
        KAS_TEST_CHECK_IMPL(ctx, parseOk<JsonParserOwned>(ctx, "3.25").asNumber() == 3.25);
        KAS_TEST_CHECK_IMPL(ctx, parseOk<JsonParserOwned>(ctx, "1e3").asNumber() == 1000.0);
        KAS_TEST_CHECK_IMPL(ctx, parseOk<JsonParserOwned>(ctx, "1E-2").asNumber() == 0.01);
        KAS_TEST_CHECK_IMPL(ctx, parseOk<JsonParserOwned>(ctx, "-0.0").asNumber() == 0.0);

        // Strings
        KAS_TEST_CHECK_IMPL(ctx,
            parseOk<JsonParserOwned>(ctx, "\"abc\"").asString() == "abc");
        KAS_TEST_CHECK_IMPL(ctx,
            parseOk<JsonParserOwned>(ctx, "\"\"").asString().empty());

        // Whitespace around the value
        KAS_TEST_CHECK_IMPL(ctx,
            parseOk<JsonParserOwned>(ctx, "  \t\r\n 123 \n").asNumber() == 123.0);
    }

    //--------------------------------------------------------------------
    //  Parser: integer / unsigned / signed classification
    //--------------------------------------------------------------------

    void testParseIntegerKinds(TestContext& ctx)
    {
        // Non-negative integer without a fraction -> Unsigned
        {
            auto v = parseOk<JsonParserOwned>(ctx, "0");
            KAS_TEST_CHECK_IMPL(ctx, v.isUnsigned());
            KAS_TEST_CHECK_IMPL(ctx, v.asUnsigned() == 0u);
        }
        {
            auto v = parseOk<JsonParserOwned>(ctx, "42");
            KAS_TEST_CHECK_IMPL(ctx, v.isUnsigned());
            KAS_TEST_CHECK_IMPL(ctx, v.asUnsigned() == 42u);
            KAS_TEST_CHECK_IMPL(ctx, v.asInteger() == 42);
            KAS_TEST_CHECK_IMPL(ctx, v.asNumber() == 42.0);
        }

        // Negative integer without a fraction -> Signed
        {
            auto v = parseOk<JsonParserOwned>(ctx, "-7");
            KAS_TEST_CHECK_IMPL(ctx, v.isSigned());
            KAS_TEST_CHECK_IMPL(ctx, v.asSigned() == -7);
            KAS_TEST_CHECK_IMPL(ctx, v.asInteger() == -7);
            KAS_TEST_CHECK_IMPL(ctx, v.asNumber() == -7.0);
        }

        // Any fraction or exponent -> Number (double)
        {
            auto v = parseOk<JsonParserOwned>(ctx, "1.0");
            KAS_TEST_CHECK_IMPL(ctx, v.isNumber());
            KAS_TEST_CHECK_IMPL(ctx, !v.isInteger());
        }
        {
            auto v = parseOk<JsonParserOwned>(ctx, "1e3");
            KAS_TEST_CHECK_IMPL(ctx, v.isNumber());
            KAS_TEST_CHECK_IMPL(ctx, !v.isInteger());
        }
        {
            auto v = parseOk<JsonParserOwned>(ctx, "-0.0");
            KAS_TEST_CHECK_IMPL(ctx, v.isNumber());
        }

        // Large unsigned value that exceeds int64_t
        {
            auto v = parseOk<JsonParserOwned>(ctx, "18446744073709551615"); // UINT64_MAX
            KAS_TEST_CHECK_IMPL(ctx, v.isUnsigned());
            KAS_TEST_CHECK_IMPL(ctx, v.asUnsigned() == std::numeric_limits<std::uint64_t>::max());
        }

        // Smallest signed int64_t
        {
            auto v = parseOk<JsonParserOwned>(ctx, "-9223372036854775808");
            KAS_TEST_CHECK_IMPL(ctx, v.isSigned());
            KAS_TEST_CHECK_IMPL(ctx, v.asSigned() == std::numeric_limits<std::int64_t>::min());
        }
    }

    //--------------------------------------------------------------------
    //  Parser: string escapes
    //--------------------------------------------------------------------

    void testParseStringEscapes(TestContext& ctx)
    {
        auto s1 = parseOk<JsonParserOwned>(ctx, R"("a\"b\\c")").asString();
        KAS_TEST_CHECK_IMPL(ctx, s1 == "a\"b\\c");

        auto s2 = parseOk<JsonParserOwned>(ctx, R"("\b\f\n\r\t")").asString();
        KAS_TEST_CHECK_IMPL(ctx, s2 == std::string("\x08\x0c\x0a\x0d\x09"));

        auto s3 = parseOk<JsonParserOwned>(ctx, R"("\u0041")").asString();
        KAS_TEST_CHECK_IMPL(ctx, s3 == "A");

        auto s4 = parseOk<JsonParserOwned>(ctx, R"("\u0416")").asString();
        KAS_TEST_CHECK_IMPL(ctx, s4 == "\xD0\x96"); // Cyrillic Zhe

        auto s5 = parseOk<JsonParserOwned>(ctx, R"("\uD83D\uDE00")").asString();
        KAS_TEST_CHECK_IMPL(ctx, s5 == "\xF0\x9F\x98\x80");
    }

    //--------------------------------------------------------------------
    //  Parser: arrays and objects
    //--------------------------------------------------------------------

    void testParseContainers(TestContext& ctx)
    {
        auto a0 = parseOk<JsonParserOwned>(ctx, "[]");
        KAS_TEST_CHECK_IMPL(ctx, a0.isArray());
        KAS_TEST_CHECK_IMPL(ctx, a0.asArray().empty());

        auto a1 = parseOk<JsonParserOwned>(ctx, "[1, true, null, \"x\"]");
        KAS_TEST_CHECK_IMPL(ctx, a1.isArray());
        KAS_TEST_REQUIRE_IMPL(ctx, a1.asArray().size() == 4);
        KAS_TEST_CHECK_IMPL(ctx, a1.asArray()[0].asNumber() == 1.0);
        KAS_TEST_CHECK_IMPL(ctx, a1.asArray()[0].isUnsigned());
        KAS_TEST_CHECK_IMPL(ctx, a1.asArray()[1].asBool() == true);
        KAS_TEST_CHECK_IMPL(ctx, a1.asArray()[2].isNull());
        KAS_TEST_CHECK_IMPL(ctx, a1.asArray()[3].asString() == "x");

        auto o0 = parseOk<JsonParserOwned>(ctx, "{}");
        KAS_TEST_CHECK_IMPL(ctx, o0.isObject());
        KAS_TEST_CHECK_IMPL(ctx, o0.asObject().empty());

        auto o1 = parseOk<JsonParserOwned>(ctx, R"({"a": 1, "b": [2, 3]})");
        KAS_TEST_REQUIRE_IMPL(ctx, o1.isObject());
        const auto& obj = o1.asObject();
        KAS_TEST_REQUIRE_IMPL(ctx, obj.count("a") == 1);
        KAS_TEST_REQUIRE_IMPL(ctx, obj.count("b") == 1);
        KAS_TEST_CHECK_IMPL(ctx, obj.at("a").asNumber() == 1.0);
        KAS_TEST_CHECK_IMPL(ctx, obj.at("b").isArray());
        KAS_TEST_CHECK_IMPL(ctx, obj.at("b").asArray().size() == 2);

        auto deep = parseOk<JsonParserOwned>(
            ctx, R"({"x": {"y": [{"z": null}]}})");
        KAS_TEST_CHECK_IMPL(
            ctx,
            deep.asObject().at("x").asObject().at("y").asArray()[0].asObject().at("z").isNull());
    }

    //--------------------------------------------------------------------
    //  Parser: errors
    //--------------------------------------------------------------------

    void testParseErrors(TestContext& ctx)
    {
        expectError<JsonParserOwned>(ctx, "", JsonErrorCode::UnexpectedEnd);
        expectError<JsonParserOwned>(ctx, "   ", JsonErrorCode::UnexpectedEnd);
        expectError<JsonParserOwned>(ctx, "nul", JsonErrorCode::InvalidLiteral);
        expectError<JsonParserOwned>(ctx, "tru", JsonErrorCode::InvalidLiteral);
        expectError<JsonParserOwned>(ctx, "truex", JsonErrorCode::TrailingCharacters);
        expectError<JsonParserOwned>(ctx, "{}x", JsonErrorCode::TrailingCharacters);
        // "01" has a leading zero; parseNumber rejects it as InvalidNumber.
        expectError<JsonParserOwned>(ctx, "01", JsonErrorCode::InvalidNumber);
        expectError<JsonParserOwned>(ctx, "-", JsonErrorCode::InvalidNumber);
        expectError<JsonParserOwned>(ctx, "1.", JsonErrorCode::InvalidNumber);
        expectError<JsonParserOwned>(ctx, "1e", JsonErrorCode::InvalidNumber);
        expectError<JsonParserOwned>(ctx, "+1", JsonErrorCode::UnexpectedChar);

        expectError<JsonParserOwned>(ctx, "\"abc", JsonErrorCode::UnexpectedEnd);
        expectError<JsonParserOwned>(ctx, "\"a\\q\"", JsonErrorCode::InvalidEscape);
        // The string ended before 4 hex digits -> UnexpectedEnd.
        expectError<JsonParserOwned>(ctx, "\"a\\u12", JsonErrorCode::UnexpectedEnd);
        // The fourth position is '"', not a hex digit -> InvalidUnicode.
        expectError<JsonParserOwned>(ctx, "\"a\\u12\"", JsonErrorCode::InvalidUnicode);
        expectError<JsonParserOwned>(ctx, "\"a\\u12G4\"", JsonErrorCode::InvalidUnicode);

        std::string ctrl = "\"a";
        ctrl += '\x01';
        ctrl += "\"";
        expectError<JsonParserOwned>(ctx, ctrl, JsonErrorCode::InvalidString);

        expectError<JsonParserOwned>(ctx, R"("\uD83D")", JsonErrorCode::InvalidUnicode);
        expectError<JsonParserOwned>(ctx, R"({"a" 1})", JsonErrorCode::ExpectedColon);
        expectError<JsonParserOwned>(ctx, R"({"a": 1)", JsonErrorCode::ExpectedCommaOrBrace);
        expectError<JsonParserOwned>(ctx, R"({"a": 1,})", JsonErrorCode::ExpectedKey);

        expectError<JsonParserOwned>(ctx, "[", JsonErrorCode::UnexpectedEnd);
        expectError<JsonParserOwned>(ctx, "[1", JsonErrorCode::ExpectedCommaOrBracket);
        expectError<JsonParserOwned>(ctx, "[1,", JsonErrorCode::UnexpectedEnd);
        expectError<JsonParserOwned>(ctx, "[1 2]", JsonErrorCode::ExpectedCommaOrBracket);
        expectError<JsonParserOwned>(ctx, "[{]", JsonErrorCode::ExpectedKey);
        expectError<JsonParserOwned>(ctx, "{1: 2}", JsonErrorCode::ExpectedKey);
        expectError<JsonParserOwned>(ctx, "{", JsonErrorCode::ExpectedKey);
    }

    void testParseErrorPosition(TestContext& ctx)
    {
        // Line 3 has ']' where ',' or '}' was expected.
        // Error position: line = 3, col = 1 (after skipWhitespace on '\n').
        auto r = JsonParserOwned::parse("{\n  \"a\": 1\n]");
        KAS_TEST_REQUIRE_IMPL(ctx, !r.has_value());
        KAS_TEST_CHECK_IMPL(ctx, r.error().line == 3);
        KAS_TEST_CHECK_IMPL(ctx, r.error().col == 1);
        KAS_TEST_CHECK_IMPL(ctx, !r.error().message.empty());
        KAS_TEST_CHECK_IMPL(ctx, !r.error().what().empty());
    }

    void testParseDepthLimit(TestContext& ctx)
    {
        std::string deep(JsonParserOwned::kMaxDepth + 2, '[');
        deep += "0";
        deep.append(JsonParserOwned::kMaxDepth + 2, ']');

        auto r = JsonParserOwned::parse(deep);
        KAS_TEST_REQUIRE_IMPL(ctx, !r.has_value());
        KAS_TEST_CHECK_IMPL(ctx, r.error().code == JsonErrorCode::DepthExceeded);
    }

    //--------------------------------------------------------------------
    //  Parser: view mode and owning fallback on escapes
    //--------------------------------------------------------------------

    void testParserView(TestContext& ctx)
    {
        const std::string src = R"("plain")";
        auto r = JsonParserView::parse(src);
        KAS_TEST_REQUIRE_IMPL(ctx, r.has_value());
        KAS_TEST_REQUIRE_IMPL(ctx, r->isString());

        const JsonStringView& sv = r->asString();
        KAS_TEST_REQUIRE_IMPL(ctx, std::holds_alternative<std::string_view>(sv));
        auto view = std::get<std::string_view>(sv);
        KAS_TEST_CHECK_IMPL(ctx, view == "plain");

        const auto* srcBegin = src.data();
        const auto* srcEnd = src.data() + src.size();
        const auto* vBegin = view.data();
        const auto* vEnd = view.data() + view.size();
        KAS_TEST_CHECK_IMPL(ctx,
            !std::less<const char*> { }(vBegin, srcBegin)
                && !std::less<const char*> { }(srcEnd, vEnd));

        const std::string esc = R"("a\nb")";
        auto r2 = JsonParserView::parse(esc);
        KAS_TEST_REQUIRE_IMPL(ctx, r2.has_value());
        const JsonStringView& sv2 = r2->asString();
        KAS_TEST_REQUIRE_IMPL(ctx, std::holds_alternative<std::string>(sv2));
        KAS_TEST_CHECK_IMPL(ctx, std::get<std::string>(sv2) == "a\nb");
    }

    //--------------------------------------------------------------------
    //  Serializer: scalars and strings
    //--------------------------------------------------------------------

    void testSerializeScalars(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx, JsonSerializer::toString(JsonValue(nullptr)) == "null");
        KAS_TEST_CHECK_IMPL(ctx, JsonSerializer::toString(JsonValue(true)) == "true");
        KAS_TEST_CHECK_IMPL(ctx, JsonSerializer::toString(JsonValue(false)) == "false");
        KAS_TEST_CHECK_IMPL(ctx, JsonSerializer::toString(JsonValue(0.0)) == "0");
        KAS_TEST_CHECK_IMPL(ctx, JsonSerializer::toString(JsonValue(-5.0)) == "-5");
        KAS_TEST_CHECK_IMPL(ctx, JsonSerializer::toString(JsonValue(2.5)) == "2.5");
        KAS_TEST_CHECK_IMPL(ctx, JsonSerializer::toString(JsonValue("abc")) == "\"abc\"");

        KAS_TEST_CHECK_IMPL(ctx, JsonSerializer::toString(JsonValue(-0.0)) == "-0");
    }

    //--------------------------------------------------------------------
    //  Serializer: integer types
    //--------------------------------------------------------------------

    void testSerializeIntegers(TestContext& ctx)
    {
        // Signed
        KAS_TEST_CHECK_IMPL(ctx,
            JsonSerializer::toString(JsonValue(std::int64_t { 0 })) == "0");
        KAS_TEST_CHECK_IMPL(ctx,
            JsonSerializer::toString(JsonValue(std::int64_t { 42 })) == "42");
        KAS_TEST_CHECK_IMPL(ctx,
            JsonSerializer::toString(JsonValue(std::int64_t { -42 })) == "-42");
        KAS_TEST_CHECK_IMPL(ctx,
            JsonSerializer::toString(
                JsonValue(std::numeric_limits<std::int64_t>::min()))
                == std::to_string(std::numeric_limits<std::int64_t>::min()));

        // Unsigned
        KAS_TEST_CHECK_IMPL(ctx,
            JsonSerializer::toString(JsonValue(std::uint64_t { 0 })) == "0");
        KAS_TEST_CHECK_IMPL(ctx,
            JsonSerializer::toString(JsonValue(std::uint64_t { 42 })) == "42");
        KAS_TEST_CHECK_IMPL(ctx,
            JsonSerializer::toString(
                JsonValue(std::numeric_limits<std::uint64_t>::max()))
                == std::to_string(std::numeric_limits<std::uint64_t>::max()));

        // serializedSize stays in sync with toString for integers
        KAS_TEST_CHECK_IMPL(ctx,
            JsonValue(std::int64_t { -42 }).serializedSize()
                == std::string("-42").size());
        KAS_TEST_CHECK_IMPL(ctx,
            JsonValue(std::uint64_t { 42 }).serializedSize()
                == std::string("42").size());
    }

    void testSerializeStringEscapes(TestContext& ctx)
    {
        auto s = JsonSerializer::toString(
            JsonValue(std::string("a\"b\\c") + "\x08\x0c\x0a\x0d\x09"));
        KAS_TEST_CHECK_IMPL(ctx, s == "\"a\\\"b\\\\c\\b\\f\\n\\r\\t\"");

        std::string raw;
        raw += '\x01';
        raw += '\x1f';
        auto s2 = JsonSerializer::toString(JsonValue(raw));
        KAS_TEST_CHECK_IMPL(ctx, s2 == "\"\\u0001\\u001f\"");

        auto s3 = JsonSerializer::toString(JsonValue("\xD0\x96"));
        KAS_TEST_CHECK_IMPL(ctx, s3 == "\"\xD0\x96\"");
    }

    void testSerializeContainers(TestContext& ctx)
    {
        KAS_TEST_CHECK_IMPL(ctx,
            JsonSerializer::toString(JsonValue(JsonArray { })) == "[]");

        JsonArray arr {
            JsonValue(1), JsonValue(true), JsonValue(nullptr), JsonValue("x")
        };
        KAS_TEST_CHECK_IMPL(ctx,
            JsonSerializer::toString(JsonValue(arr)) == "[1,true,null,\"x\"]");

        JsonObject obj;
        obj.emplace("b", JsonValue(2));
        obj.emplace("a", JsonValue(1));
        KAS_TEST_CHECK_IMPL(ctx,
            JsonSerializer::toString(JsonValue(obj)) == "{\"a\":1,\"b\":2}");

        KAS_TEST_CHECK_IMPL(ctx,
            JsonSerializer::toString(JsonValue(JsonObject { })) == "{}");
    }

    void testSerializeIndented(TestContext& ctx)
    {
        JsonArray arr { JsonValue(1), JsonValue(2) };
        auto pretty = JsonSerializer::toString(JsonValue(arr), 2);
        auto expected = std::string("[\n  1,\n  2\n]");
        KAS_TEST_CHECK_IMPL(ctx, pretty == expected);

        JsonObject obj;
        obj.emplace("k", JsonValue(1));
        auto prettyObj = JsonSerializer::toString(JsonValue(obj), 2);
        auto expectedObj = std::string("{\n  \"k\": 1\n}");
        KAS_TEST_CHECK_IMPL(ctx, prettyObj == expectedObj);
    }

    //--------------------------------------------------------------------
    //  serializedSize is consistent with the actual output
    //--------------------------------------------------------------------

    void testSerializedSizeMatches(TestContext& ctx)
    {
        auto check = [&](const JsonValue& v) {
            KAS_TEST_CHECK_IMPL(ctx,
                JsonSerializer::serializedSize(v, 0)
                    == JsonSerializer::toString(v, 0).size());
            KAS_TEST_CHECK_IMPL(ctx,
                JsonSerializer::serializedSize(v, 1)
                    == JsonSerializer::toString(v, 1).size());
            KAS_TEST_CHECK_IMPL(ctx,
                JsonSerializer::serializedSize(v, 2)
                    == JsonSerializer::toString(v, 2).size());
            KAS_TEST_CHECK_IMPL(ctx,
                JsonSerializer::serializedSize(v, 3)
                    == JsonSerializer::toString(v, 3).size());
        };

        check(JsonValue(nullptr));
        check(JsonValue(true));
        check(JsonValue(-12.5));
        check(JsonValue("a\nb\"c"));
        check(JsonValue(std::int64_t { -42 }));
        check(JsonValue(std::int64_t { 42 }));
        check(JsonValue(std::uint64_t { 42 }));
        check(JsonValue(std::numeric_limits<std::int64_t>::min()));
        check(JsonValue(std::numeric_limits<std::uint64_t>::max()));

        JsonArray arr {
            JsonValue(1), JsonValue(2.5), JsonValue("x"), JsonValue(false)
        };
        check(JsonValue(arr));

        JsonObject obj;
        obj.emplace("alpha", JsonValue(1));
        obj.emplace("beta", JsonValue(JsonArray { JsonValue(2) }));
        check(JsonValue(obj));
    }

    //--------------------------------------------------------------------
    //  Round-trip: parse -> serialize -> canonical form
    //--------------------------------------------------------------------

    void testRoundTrip(TestContext& ctx)
    {
        expectRoundTrip<JsonParserOwned, JsonSerializer>(ctx, " 1 ", "1");
        expectRoundTrip<JsonParserOwned, JsonSerializer>(ctx, "-1", "-1");
        expectRoundTrip<JsonParserOwned, JsonSerializer>(ctx, "true", "true");
        expectRoundTrip<JsonParserOwned, JsonSerializer>(ctx, "null", "null");
        expectRoundTrip<JsonParserOwned, JsonSerializer>(ctx, "\"x\"", "\"x\"");
        expectRoundTrip<JsonParserOwned, JsonSerializer>(ctx,
            " [ 1 , 2 , 3 ] ", "[1,2,3]");
        expectRoundTrip<JsonParserOwned, JsonSerializer>(ctx,
            " { \"b\" : 2 , \"a\" : 1 } ", "{\"a\":1,\"b\":2}");
        expectRoundTrip<JsonParserOwned, JsonSerializer>(ctx,
            R"({"s":"a\nb","n":[1,2.5]})",
            R"({"n":[1,2.5],"s":"a\nb"})");
    }

    //--------------------------------------------------------------------
    //  Entry point
    //--------------------------------------------------------------------

    inline bool runTests()
    {
        TestContext ctx;

        testValueBasics(ctx);
        testValueAsAndTryAs(ctx);
        testValueIntegerAccessors(ctx);

        testParseScalars(ctx);
        testParseIntegerKinds(ctx);
        testParseStringEscapes(ctx);
        testParseContainers(ctx);
        testParseErrors(ctx);
        testParseErrorPosition(ctx);
        testParseDepthLimit(ctx);
        testParserView(ctx);

        testSerializeScalars(ctx);
        testSerializeIntegers(ctx);
        testSerializeStringEscapes(ctx);
        testSerializeContainers(ctx);
        testSerializeIndented(ctx);
        testSerializedSizeMatches(ctx);

        testRoundTrip(ctx);

        return printSummary("kas::json::tests", ctx);
    }

} // namespace

} // namespace kas::json::tests

#endif // KASLIBCXX_JSON_TESTS_H