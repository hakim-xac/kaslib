/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_JSON_VALUE_H
#define KASLIBCXX_JSON_VALUE_H

#include "JsonCommon.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>

namespace kas::json {

template <typename StringT>
class BasicJsonValue;

template <typename Value>
class BasicJsonSerializer;

template <typename StringT>
using BasicJsonArray = std::vector<BasicJsonValue<StringT>>;

template <typename StringT>
using BasicJsonObject = std::map<
    StringT,
    BasicJsonValue<StringT>,
    std::conditional_t<
        std::is_same_v<StringT, JsonStringView>,
        JsonStringViewLess,
        std::less<StringT>>>;

template <typename StringT, JsonTypes T>
struct BasicJsonType : JsonType<T> { };

template <typename StringT>
struct BasicJsonType<StringT, JsonTypes::String> {
    using type = StringT;
};

template <typename StringT>
struct BasicJsonType<StringT, JsonTypes::Array> {
    using type = BasicJsonArray<StringT>;
};

template <typename StringT>
struct BasicJsonType<StringT, JsonTypes::Object> {
    using type = BasicJsonObject<StringT>;
};

template <typename StringT, JsonTypes T>
using JsonTypeOf = typename BasicJsonType<StringT, T>::type;

template <typename StringT, JsonTypes T>
struct BasicJsonAsResult;

template <typename StringT, JsonTypes T>
struct BasicJsonAsResultConst;

template <typename StringT>
struct BasicJsonAsResult<StringT, JsonTypes::Array> {
    using type = BasicJsonArray<StringT>&;
};
template <typename StringT>
struct BasicJsonAsResult<StringT, JsonTypes::Object> {
    using type = BasicJsonObject<StringT>&;
};
template <typename StringT>
struct BasicJsonAsResult<StringT, JsonTypes::String> {
    using type = const StringT&;
};
template <typename StringT>
struct BasicJsonAsResult<StringT, JsonTypes::Boolean> {
    using type = bool;
};
template <typename StringT>
struct BasicJsonAsResult<StringT, JsonTypes::Integer> {
    using type = std::int64_t;
};
template <typename StringT>
struct BasicJsonAsResult<StringT, JsonTypes::Unsigned> {
    using type = std::uint64_t;
};
template <typename StringT>
struct BasicJsonAsResult<StringT, JsonTypes::Signed> {
    using type = std::int64_t;
};
template <typename StringT>
struct BasicJsonAsResult<StringT, JsonTypes::Number> {
    using type = double;
};

template <typename StringT>
struct BasicJsonAsResultConst<StringT, JsonTypes::Array> {
    using type = const BasicJsonArray<StringT>&;
};
template <typename StringT>
struct BasicJsonAsResultConst<StringT, JsonTypes::Object> {
    using type = const BasicJsonObject<StringT>&;
};
template <typename StringT>
struct BasicJsonAsResultConst<StringT, JsonTypes::String> {
    using type = const StringT&;
};
template <typename StringT>
struct BasicJsonAsResultConst<StringT, JsonTypes::Boolean> {
    using type = bool;
};
template <typename StringT>
struct BasicJsonAsResultConst<StringT, JsonTypes::Integer> {
    using type = std::int64_t;
};
template <typename StringT>
struct BasicJsonAsResultConst<StringT, JsonTypes::Unsigned> {
    using type = std::uint64_t;
};
template <typename StringT>
struct BasicJsonAsResultConst<StringT, JsonTypes::Signed> {
    using type = std::int64_t;
};
template <typename StringT>
struct BasicJsonAsResultConst<StringT, JsonTypes::Number> {
    using type = double;
};

template <typename StringT, JsonTypes T>
using JsonAsResult = typename BasicJsonAsResult<StringT, T>::type;

template <typename StringT, JsonTypes T>
using JsonAsResultConst = typename BasicJsonAsResultConst<StringT, T>::type;

template <typename StringT, JsonTypes T>
struct BasicJsonTryAsResult;

template <typename StringT, JsonTypes T>
struct BasicJsonTryAsResultConst;

template <typename StringT>
struct BasicJsonTryAsResult<StringT, JsonTypes::Array> {
    using type = std::optional<std::reference_wrapper<BasicJsonArray<StringT>>>;
};
template <typename StringT>
struct BasicJsonTryAsResult<StringT, JsonTypes::Object> {
    using type = std::optional<std::reference_wrapper<BasicJsonObject<StringT>>>;
};
template <typename StringT>
struct BasicJsonTryAsResult<StringT, JsonTypes::String> {
    using type = std::optional<std::reference_wrapper<const StringT>>;
};
template <typename StringT>
struct BasicJsonTryAsResult<StringT, JsonTypes::Boolean> {
    using type = std::optional<bool>;
};
template <typename StringT>
struct BasicJsonTryAsResult<StringT, JsonTypes::Integer> {
    using type = std::optional<std::int64_t>;
};
template <typename StringT>
struct BasicJsonTryAsResult<StringT, JsonTypes::Unsigned> {
    using type = std::optional<std::uint64_t>;
};
template <typename StringT>
struct BasicJsonTryAsResult<StringT, JsonTypes::Signed> {
    using type = std::optional<std::int64_t>;
};
template <typename StringT>
struct BasicJsonTryAsResult<StringT, JsonTypes::Number> {
    using type = std::optional<double>;
};

template <typename StringT>
struct BasicJsonTryAsResultConst<StringT, JsonTypes::Array> {
    using type = std::optional<std::reference_wrapper<const BasicJsonArray<StringT>>>;
};
template <typename StringT>
struct BasicJsonTryAsResultConst<StringT, JsonTypes::Object> {
    using type = std::optional<std::reference_wrapper<const BasicJsonObject<StringT>>>;
};
template <typename StringT>
struct BasicJsonTryAsResultConst<StringT, JsonTypes::String> {
    using type = std::optional<std::reference_wrapper<const StringT>>;
};
template <typename StringT>
struct BasicJsonTryAsResultConst<StringT, JsonTypes::Boolean> {
    using type = std::optional<bool>;
};
template <typename StringT>
struct BasicJsonTryAsResultConst<StringT, JsonTypes::Integer> {
    using type = std::optional<std::int64_t>;
};
template <typename StringT>
struct BasicJsonTryAsResultConst<StringT, JsonTypes::Unsigned> {
    using type = std::optional<std::uint64_t>;
};
template <typename StringT>
struct BasicJsonTryAsResultConst<StringT, JsonTypes::Signed> {
    using type = std::optional<std::int64_t>;
};
template <typename StringT>
struct BasicJsonTryAsResultConst<StringT, JsonTypes::Number> {
    using type = std::optional<double>;
};

template <typename StringT, JsonTypes T>
using JsonTryAsResult = typename BasicJsonTryAsResult<StringT, T>::type;

template <typename StringT, JsonTypes T>
using JsonTryAsResultConst = typename BasicJsonTryAsResultConst<StringT, T>::type;

template <typename StringT>
class BasicJsonValue final {
public:
    using String = StringT;
    using Array = BasicJsonArray<StringT>;
    using Object = BasicJsonObject<StringT>;
    using Storage = std::variant<
        std::nullptr_t, // Null
        bool, // Boolean
        std::int64_t, // Integer / Signed
        std::uint64_t, // Unsigned
        double, // Number
        StringT, // String
        std::shared_ptr<Array>, // Array
        std::shared_ptr<Object>>; // Object

    BasicJsonValue();

    BasicJsonValue(std::nullptr_t);

    BasicJsonValue(bool b);

    /// Signed integer: stores as JsonTypes::Signed.
    BasicJsonValue(std::int64_t i);

    /// Unsigned integer: stores as JsonTypes::Unsigned.
    BasicJsonValue(std::uint64_t u);

    /// Floating-point number: stores as JsonTypes::Number.
    BasicJsonValue(double d);

    template <typename T>
        requires(std::is_integral_v<T>
            && !std::is_same_v<T, bool>
            && !std::is_same_v<T, char>)
    BasicJsonValue(T i);

    BasicJsonValue(StringT s);

    BasicJsonValue(const char* s);

    BasicJsonValue(std::string_view s);

    BasicJsonValue(Array a);

    BasicJsonValue(Object o);

    JsonTypes type() const noexcept;

    bool isNull() const noexcept;
    bool isBool() const noexcept;
    bool isInteger() const noexcept; ///< Signed or unsigned integer.
    bool isUnsigned() const noexcept;
    bool isSigned() const noexcept;
    bool isNumber() const noexcept;
    bool isString() const noexcept;
    bool isArray() const noexcept;
    bool isObject() const noexcept;

    bool asBool() const;
    std::int64_t asInteger() const; ///< Works for Integer and Signed.
    std::uint64_t asUnsigned() const; ///< Works for Unsigned only.
    std::int64_t asSigned() const; ///< Works for Signed only.
    double asNumber() const; ///< Works for Number; for integers returns the exact double value.
    const StringT& asString() const;

    Array& asArray();
    const Array& asArray() const;
    Object& asObject();
    const Object& asObject() const;

    template <JsonTypes T>
    bool has() const noexcept;

    template <JsonTypes T>
    JsonAsResult<StringT, T> as();
    template <JsonTypes T>
    JsonAsResultConst<StringT, T> as() const;

    template <JsonTypes T>
    JsonTryAsResult<StringT, T> try_as() noexcept;

    template <JsonTypes T>
    JsonTryAsResultConst<StringT, T> try_as() const noexcept;

    auto begin();
    auto end();
    auto begin() const;
    auto end() const;

    auto beginObj();
    auto endObj();
    auto beginObj() const;
    auto endObj() const;

    struct ArrayRange {
        Array& a;
        auto begin();
        auto end();
    };
    struct ConstArrayRange {
        const Array& a;
        auto begin() const;
        auto end() const;
    };
    struct ObjectRange {
        Object& o;
        auto begin();
        auto end();
    };
    struct ConstObjectRange {
        const Object& o;
        auto begin() const;
        auto end() const;
    };

    ArrayRange asRange();
    ConstArrayRange asRange() const;
    ObjectRange asObjRange();
    ConstObjectRange asObjRange() const;

    std::string toString(int indent = 0) const;
    std::size_t serializedSize(int indent = 0) const;

private:
    static StringT makeString(std::string_view s);

    Storage data_;
};

template <typename StringT>
std::ostream& operator<<(std::ostream& os, const BasicJsonValue<StringT>& v)
{
    os << v.toString();
    return os;
}

//=======================================

using JsonValue = BasicJsonValue<std::string>;
using JsonArray = BasicJsonArray<std::string>;
using JsonObject = BasicJsonObject<std::string>;

using JsonValueView = BasicJsonValue<JsonStringView>;
using JsonArrayView = BasicJsonArray<JsonStringView>;
using JsonObjectView = BasicJsonObject<JsonStringView>;

template <JsonTypes T>
using JsonTypeOwned = JsonTypeOf<std::string, T>;
template <JsonTypes T>
using JsonTypeView = JsonTypeOf<JsonStringView, T>;

} // namespace kas::json

//=======================================

namespace kas::json {

//------------------
template <typename StringT>
BasicJsonValue<StringT>::BasicJsonValue()
    : data_(nullptr)
{
}

//------------------
template <typename StringT>
BasicJsonValue<StringT>::BasicJsonValue(std::nullptr_t)
    : data_(nullptr)
{
}

//------------------
template <typename StringT>
BasicJsonValue<StringT>::BasicJsonValue(bool b)
    : data_(b)
{
}

//------------------
template <typename StringT>
BasicJsonValue<StringT>::BasicJsonValue(std::int64_t i)
    : data_(i)
{
}

//------------------
template <typename StringT>
BasicJsonValue<StringT>::BasicJsonValue(std::uint64_t u)
    : data_(u)
{
}

//------------------
template <typename StringT>
BasicJsonValue<StringT>::BasicJsonValue(double d)
    : data_(d)
{
}

//------------------
template <typename StringT>
template <typename T>
    requires(std::is_integral_v<T>
        && !std::is_same_v<T, bool>
        && !std::is_same_v<T, char>)
BasicJsonValue<StringT>::BasicJsonValue(T i)
{
    if constexpr (std::is_signed_v<T>)
        data_ = static_cast<std::int64_t>(i);
    else
        data_ = static_cast<std::uint64_t>(i);
}

//------------------
template <typename StringT>
BasicJsonValue<StringT>::BasicJsonValue(StringT s)
    : data_(std::move(s))
{
}

//------------------
template <typename StringT>
BasicJsonValue<StringT>::BasicJsonValue(const char* s)
    : data_(makeString(s))
{
}

//------------------
template <typename StringT>
BasicJsonValue<StringT>::BasicJsonValue(std::string_view s)
    : data_(makeString(s))
{
}

//------------------
template <typename StringT>
BasicJsonValue<StringT>::BasicJsonValue(Array a)
    : data_(std::make_shared<Array>(std::move(a)))
{
}

//------------------
template <typename StringT>
BasicJsonValue<StringT>::BasicJsonValue(Object o)
    : data_(std::make_shared<Object>(std::move(o)))
{
}

//------------------

template <typename StringT>
JsonTypes
BasicJsonValue<StringT>::type() const noexcept
{
    switch (data_.index()) {
    case 0:
        return JsonTypes::Null;
    case 1:
        return JsonTypes::Boolean;
    case 2:
        return JsonTypes::Signed;
    case 3:
        return JsonTypes::Unsigned;
    case 4:
        return JsonTypes::Number;
    case 5:
        return JsonTypes::String;
    case 6:
        return JsonTypes::Array;
    case 7:
        return JsonTypes::Object;
    }
    return JsonTypes::Null;
}

//------------------

template <typename StringT>
bool BasicJsonValue<StringT>::isNull() const noexcept
{
    return std::holds_alternative<std::nullptr_t>(data_);
}

//------------------

template <typename StringT>
bool BasicJsonValue<StringT>::isBool() const noexcept
{
    return std::holds_alternative<bool>(data_);
}

//------------------

template <typename StringT>
bool BasicJsonValue<StringT>::isInteger() const noexcept
{
    return std::holds_alternative<std::int64_t>(data_)
        || std::holds_alternative<std::uint64_t>(data_);
}

//------------------

template <typename StringT>
bool BasicJsonValue<StringT>::isUnsigned() const noexcept
{
    return std::holds_alternative<std::uint64_t>(data_);
}

//------------------

template <typename StringT>
bool BasicJsonValue<StringT>::isSigned() const noexcept
{
    return std::holds_alternative<std::int64_t>(data_);
}

//------------------

template <typename StringT>
bool BasicJsonValue<StringT>::isNumber() const noexcept
{
    return std::holds_alternative<double>(data_);
}

//------------------

template <typename StringT>
bool BasicJsonValue<StringT>::isString() const noexcept
{
    return std::holds_alternative<StringT>(data_);
}

//------------------

template <typename StringT>
bool BasicJsonValue<StringT>::isArray() const noexcept
{
    return std::holds_alternative<std::shared_ptr<Array>>(data_);
}

//------------------

template <typename StringT>
bool BasicJsonValue<StringT>::isObject() const noexcept
{
    return std::holds_alternative<std::shared_ptr<Object>>(data_);
}

//------------------

template <typename StringT>
bool BasicJsonValue<StringT>::asBool() const
{
    return std::get<bool>(data_);
}

//------------------

template <typename StringT>
std::int64_t BasicJsonValue<StringT>::asInteger() const
{
    if (std::holds_alternative<std::int64_t>(data_))
        return std::get<std::int64_t>(data_);
    if (std::holds_alternative<std::uint64_t>(data_))
        return static_cast<std::int64_t>(std::get<std::uint64_t>(data_));
    throw std::bad_variant_access { };
}

//------------------

template <typename StringT>
std::uint64_t BasicJsonValue<StringT>::asUnsigned() const
{
    return std::get<std::uint64_t>(data_);
}

//------------------

template <typename StringT>
std::int64_t BasicJsonValue<StringT>::asSigned() const
{
    return std::get<std::int64_t>(data_);
}

//------------------

template <typename StringT>
double BasicJsonValue<StringT>::asNumber() const
{
    if (std::holds_alternative<double>(data_))
        return std::get<double>(data_);
    if (std::holds_alternative<std::int64_t>(data_))
        return static_cast<double>(std::get<std::int64_t>(data_));
    if (std::holds_alternative<std::uint64_t>(data_))
        return static_cast<double>(std::get<std::uint64_t>(data_));
    throw std::bad_variant_access { };
}

//------------------

template <typename StringT>
const StringT& BasicJsonValue<StringT>::asString() const
{
    return std::get<StringT>(data_);
}

//------------------

template <typename StringT>
BasicJsonValue<StringT>::Array&
BasicJsonValue<StringT>::asArray()
{
    return *std::get<std::shared_ptr<Array>>(data_);
}

//------------------

template <typename StringT>
const BasicJsonValue<StringT>::Array&
BasicJsonValue<StringT>::asArray() const
{
    return *std::get<std::shared_ptr<Array>>(data_);
}

//------------------

template <typename StringT>
BasicJsonValue<StringT>::Object&
BasicJsonValue<StringT>::asObject()
{
    return *std::get<std::shared_ptr<Object>>(data_);
}

//------------------

template <typename StringT>
const BasicJsonValue<StringT>::Object&
BasicJsonValue<StringT>::asObject() const
{
    return *std::get<std::shared_ptr<Object>>(data_);
}

//------------------

template <typename StringT>
template <JsonTypes T>
bool BasicJsonValue<StringT>::has() const noexcept
{
    if constexpr (T == JsonTypes::Null)
        return isNull();
    else if constexpr (T == JsonTypes::Boolean)
        return isBool();
    else if constexpr (T == JsonTypes::Integer)
        return isInteger();
    else if constexpr (T == JsonTypes::Unsigned)
        return isUnsigned();
    else if constexpr (T == JsonTypes::Signed)
        return isSigned();
    else if constexpr (T == JsonTypes::Number)
        return isNumber();
    else if constexpr (T == JsonTypes::String)
        return isString();
    else if constexpr (T == JsonTypes::Array)
        return isArray();
    else if constexpr (T == JsonTypes::Object)
        return isObject();
}

//------------------

template <typename StringT>
std::string
BasicJsonValue<StringT>::toString(int indent) const
{
    return BasicJsonSerializer<BasicJsonValue>::toString(*this, indent);
}

//------------------

template <typename StringT>
std::size_t
BasicJsonValue<StringT>::serializedSize(int indent) const
{
    return BasicJsonSerializer<BasicJsonValue>::serializedSize(*this, indent);
}

//------------------

template <typename StringT>
template <JsonTypes T>
JsonAsResult<StringT, T>
BasicJsonValue<StringT>::as()
{
    if constexpr (T == JsonTypes::Integer) {
        if (isInteger())
            return asInteger();
        throw std::bad_variant_access { };
    } else if constexpr (T == JsonTypes::Unsigned) {
        if (isUnsigned())
            return asUnsigned();
        throw std::bad_variant_access { };
    } else if constexpr (T == JsonTypes::Signed) {
        if (isSigned())
            return asSigned();
        throw std::bad_variant_access { };
    } else {
        if (type() != T)
            throw std::bad_variant_access { };

        if constexpr (T == JsonTypes::Array) {
            return asArray();
        } else if constexpr (T == JsonTypes::Object) {
            return asObject();
        } else if constexpr (T == JsonTypes::String) {
            return asString();
        } else if constexpr (T == JsonTypes::Boolean) {
            return asBool();
        } else if constexpr (T == JsonTypes::Number) {
            return asNumber();
        } else {
            static_assert(T != JsonTypes::Null, "JsonTypes::Null has no value");
        }
    }
}

//------------------

template <typename StringT>
template <JsonTypes T>
JsonAsResultConst<StringT, T>
BasicJsonValue<StringT>::as() const
{
    if constexpr (T == JsonTypes::Integer) {
        if (isInteger())
            return asInteger();
        throw std::bad_variant_access { };
    } else if constexpr (T == JsonTypes::Unsigned) {
        if (isUnsigned())
            return asUnsigned();
        throw std::bad_variant_access { };
    } else if constexpr (T == JsonTypes::Signed) {
        if (isSigned())
            return asSigned();
        throw std::bad_variant_access { };
    } else {
        if (type() != T)
            throw std::bad_variant_access { };

        if constexpr (T == JsonTypes::Array) {
            return asArray();
        } else if constexpr (T == JsonTypes::Object) {
            return asObject();
        } else if constexpr (T == JsonTypes::String) {
            return asString();
        } else if constexpr (T == JsonTypes::Boolean) {
            return asBool();
        } else if constexpr (T == JsonTypes::Number) {
            return asNumber();
        } else {
            static_assert(T != JsonTypes::Null, "JsonTypes::Null has no value");
        }
    }
}

//------------------

template <typename StringT>
template <JsonTypes T>
JsonTryAsResult<StringT, T>
BasicJsonValue<StringT>::try_as() noexcept
{
    if constexpr (T == JsonTypes::Integer) {
        if (!isInteger())
            return std::nullopt;
        return asInteger();
    } else if constexpr (T == JsonTypes::Unsigned) {
        if (!isUnsigned())
            return std::nullopt;
        return asUnsigned();
    } else if constexpr (T == JsonTypes::Signed) {
        if (!isSigned())
            return std::nullopt;
        return asSigned();
    } else {
        if (type() != T)
            return std::nullopt;

        if constexpr (T == JsonTypes::Array) {
            return std::ref(asArray());
        } else if constexpr (T == JsonTypes::Object) {
            return std::ref(asObject());
        } else if constexpr (T == JsonTypes::String) {
            return std::cref(asString());
        } else if constexpr (T == JsonTypes::Boolean) {
            return asBool();
        } else if constexpr (T == JsonTypes::Number) {
            return asNumber();
        } else {
            static_assert(T != JsonTypes::Null, "JsonTypes::Null has no value");
        }
    }
}

//------------------

template <typename StringT>
template <JsonTypes T>
JsonTryAsResultConst<StringT, T>
BasicJsonValue<StringT>::try_as() const noexcept
{
    if constexpr (T == JsonTypes::Integer) {
        if (!isInteger())
            return std::nullopt;
        return asInteger();
    } else if constexpr (T == JsonTypes::Unsigned) {
        if (!isUnsigned())
            return std::nullopt;
        return asUnsigned();
    } else if constexpr (T == JsonTypes::Signed) {
        if (!isSigned())
            return std::nullopt;
        return asSigned();
    } else {
        if (type() != T)
            return std::nullopt;

        if constexpr (T == JsonTypes::Array) {
            return std::cref(asArray());
        } else if constexpr (T == JsonTypes::Object) {
            return std::cref(asObject());
        } else if constexpr (T == JsonTypes::String) {
            return std::cref(asString());
        } else if constexpr (T == JsonTypes::Boolean) {
            return asBool();
        } else if constexpr (T == JsonTypes::Number) {
            return asNumber();
        } else {
            static_assert(T != JsonTypes::Null, "JsonTypes::Null has no value");
        }
    }
}

//------------------

template <typename StringT>
auto BasicJsonValue<StringT>::begin()
{
    return asArray().begin();
}

//------------------

template <typename StringT>
auto BasicJsonValue<StringT>::end()
{
    return asArray().end();
}

//------------------

template <typename StringT>
auto BasicJsonValue<StringT>::begin() const
{
    return asArray().begin();
}

//------------------

template <typename StringT>
auto BasicJsonValue<StringT>::end() const
{
    return asArray().end();
}

//------------------

template <typename StringT>
auto BasicJsonValue<StringT>::beginObj()
{
    return asObject().begin();
}

//------------------

template <typename StringT>
auto BasicJsonValue<StringT>::endObj()
{
    return asObject().end();
}

//------------------

template <typename StringT>
auto BasicJsonValue<StringT>::beginObj() const
{
    return asObject().begin();
}

//------------------

template <typename StringT>
auto BasicJsonValue<StringT>::endObj() const
{
    return asObject().end();
}

//------------------

template <typename StringT>
auto BasicJsonValue<StringT>::ArrayRange::begin()
{
    return a.begin();
}

//------------------

template <typename StringT>
auto BasicJsonValue<StringT>::ArrayRange::end()
{
    return a.end();
}

//------------------

template <typename StringT>
auto BasicJsonValue<StringT>::ConstArrayRange::begin() const
{
    return a.begin();
}

//------------------

template <typename StringT>
auto BasicJsonValue<StringT>::ConstArrayRange::end() const
{
    return a.end();
}

//------------------

template <typename StringT>
auto BasicJsonValue<StringT>::ObjectRange::begin()
{
    return o.begin();
}

//------------------

template <typename StringT>
auto BasicJsonValue<StringT>::ObjectRange::end()
{
    return o.end();
}

//------------------

template <typename StringT>
auto BasicJsonValue<StringT>::ConstObjectRange::begin() const
{
    return o.begin();
}

//------------------

template <typename StringT>
auto BasicJsonValue<StringT>::ConstObjectRange::end() const
{
    return o.end();
}

//------------------

template <typename StringT>
BasicJsonValue<StringT>::ArrayRange
BasicJsonValue<StringT>::asRange()
{
    return { asArray() };
}

//------------------

template <typename StringT>
BasicJsonValue<StringT>::ConstArrayRange
BasicJsonValue<StringT>::asRange() const
{
    return { asArray() };
}

//------------------

template <typename StringT>
BasicJsonValue<StringT>::ObjectRange
BasicJsonValue<StringT>::asObjRange()
{
    return { asObject() };
}

//------------------

template <typename StringT>
BasicJsonValue<StringT>::ConstObjectRange
BasicJsonValue<StringT>::asObjRange() const
{
    return { asObject() };
}

//------------------

template <typename StringT>
StringT
BasicJsonValue<StringT>::makeString(std::string_view s)
{
    if constexpr (std::is_same_v<StringT, std::string>) {
        return std::string(s);
    } else {
        return JsonStringView { std::string(s) };
    }
}

//------------------
}

#endif