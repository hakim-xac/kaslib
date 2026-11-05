/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_CONVERT_UTILS_H
#define KASLIBCXX_CONVERT_UTILS_H

namespace kas::concepts {
template <typename T>
concept swap_supported = std::integral<T>
    && (std::same_as<T, std::uint8_t>
        || std::same_as<T, std::uint16_t>
        || std::same_as<T, std::uint32_t>
        || std::same_as<T, std::uint64_t>);
}

#include <cstdint>

namespace kas::convert::utils {
/**
 * @brief Переварачивает байты, из big endian в little endian и наоборот
 * @note можно использовать std::byteswap начиная с c++23
 * @tparam T
 * @param value
 * @return T
 */
template <concepts::swap_supported T>
[[nodiscard]] constexpr T swap_binary(T value) noexcept
{
    if constexpr (std::same_as<T, std::uint8_t>)
        return value;
    else if constexpr (std::same_as<T, std::uint16_t>)
        return static_cast<T>(((value & 0xffu) << 8) | ((value >> 8) & 0xffu));
    else if constexpr (std::same_as<T, std::uint32_t>)
        return static_cast<T>(
            static_cast<std::uint32_t>(swap_binary(static_cast<std::uint16_t>(value & 0xffffu))) << 16
            | static_cast<std::uint32_t>(swap_binary(static_cast<std::uint16_t>((value >> 16) & 0xffffu))));
    else if constexpr (std::same_as<T, std::uint64_t>)
        return static_cast<T>(
            static_cast<std::uint64_t>(swap_binary(static_cast<std::uint32_t>(value & 0xffffffffu))) << 32
            | static_cast<std::uint64_t>(swap_binary(static_cast<std::uint32_t>((value >> 32) & 0xffffffffu))));

    return T { };
}

template <concepts::swap_supported T>
[[nodiscard]] constexpr T be_to_le(T value) noexcept
{
    if constexpr (std::endian::native == std::endian::little)
        return swap_binary(value);
    else
        return value;
}

template <concepts::swap_supported T>
[[nodiscard]] constexpr T le_to_be(T value) noexcept
{
    return be_to_le(value);
}

} // namespace kas::convert::utils

#endif