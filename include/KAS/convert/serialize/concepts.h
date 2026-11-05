/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_CONVERT_SERIALIZE_CONCEPTS_H
#define KASLIBCXX_CONVERT_SERIALIZE_CONCEPTS_H

#include <concepts>
#include <cstdint>
#include <type_traits>

namespace kas::convert::serialize::concepts {
template <typename T>
concept as_type_info = requires {
    typename T::code_type;
    typename T::value_type;
    T::code;
};

template <typename T>
concept as_serialize_type = requires {
    typename T::info_type;
    typename T::value_type;
    T::value;
};
} // namespace kas::convert::concepts

#endif