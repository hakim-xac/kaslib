/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_CONVERT_SERIALIZE_SERIALIZE_TYPE_H
#define KASLIBCXX_CONVERT_SERIALIZE_SERIALIZE_TYPE_H

#include <KAS/convert/serialize/concepts.h>

namespace kas::convert::serialize {
/**
 * @brief Прокси класс, для класса 'Serializer'
 *
 * @tparam T
 */
template <concepts::type_info_c T>
struct SerializeType {
    using info_type = T;
    using value_type = typename T::value_type;

    template <typename V>
        requires std::is_trivial_v<V> && std::same_as<V, value_type>
    inline explicit SerializeType(V&& v)
        : value { std::forward<V>(v) }
    {
    }

    template <typename V>
        requires(!std::is_trivial_v<V>) && std::is_convertible_v<V, value_type>
    inline explicit SerializeType(V&& v)
        : value { std::forward<V>(v) }
    {
    }

    value_type value;
};
} // namespace kas::convert

#endif