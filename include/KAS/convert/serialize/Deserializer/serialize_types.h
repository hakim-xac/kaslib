/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_CONVERT_SERIALIZE_SERIALIZE_TYPES_H
#define KASLIBCXX_CONVERT_SERIALIZE_SERIALIZE_TYPES_H

#include <cstdint>
#include <string>
#include <vector>

/**
 * @brief Поддерживаемые типы в модуле 'convert'
 *
 */
namespace kas::convert::serialize::types {

using general_size_type_t = std::uint16_t;

struct u8 {
    using code_type = general_size_type_t;
    using value_type = std::uint8_t;
    constexpr static code_type code { 1 };
};
struct i8 {
    using code_type = general_size_type_t;
    using value_type = std::int8_t;
    constexpr static code_type code { 2 };
};
struct u16 {
    using code_type = general_size_type_t;
    using value_type = std::uint16_t;
    constexpr static code_type code { 3 };
};
struct i16 {
    using code_type = general_size_type_t;
    using value_type = std::int16_t;
    constexpr static code_type code { 4 };
};
struct u32 {
    using code_type = general_size_type_t;
    using value_type = std::uint32_t;
    constexpr static code_type code { 5 };
};
struct i32 {
    using code_type = general_size_type_t;
    using value_type = std::int32_t;
    constexpr static code_type code { 6 };
};
struct u64 {
    using code_type = general_size_type_t;
    using value_type = std::uint64_t;
    constexpr static code_type code { 7 };
};
struct i64 {
    using code_type = general_size_type_t;
    using value_type = std::int64_t;
    constexpr static code_type code { 8 };
};
struct string {
    using code_type = general_size_type_t;
    using value_type = std::string;
    constexpr static code_type code { 9 };
};
struct raw_data {
    using code_type = general_size_type_t;
    using value_type = std::vector<std::uint8_t>;
    constexpr static code_type code { 10 };
};
} // namespace kas::convert::types

#endif