// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include "KAS/convert/base64/v1/base64.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace kas::convert::base64::v1 {

namespace {
    static constexpr std::string_view G_BASE64_CHARS {
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/"
    };

} // namespace

//------------------

[[nodiscard]]
std::string
encode(const std::span<const std::uint8_t> data)
{
    if (data.empty())
        return { };

    std::string res;
    const size_t data_size { data.size() };
    res.reserve(((data_size + 2) / 3) * 4);

    std::array<std::uint8_t, 3> tmp { };
    std::array<std::uint8_t, 4> tmp_buff { };

    for (size_t i { }; i + 3 <= data_size; i += 3) {

        auto iter { std::next(std::cbegin(data), i) };
        for (size_t j { }; j < 3; ++j)
            tmp[j] = static_cast<std::uint8_t>(*iter++);

        tmp_buff[0] = ((tmp[0] & 0xfc) >> 2);
        tmp_buff[1] = (((tmp[0] & 0x03) << 4) + ((tmp[1] & 0xf0) >> 4));
        tmp_buff[2] = (((tmp[1] & 0x0f) << 2) + ((tmp[2] & 0xc0) >> 6));
        tmp_buff[3] = (tmp[2] & 0x3f);

        for (const auto& e : tmp_buff)
            res += G_BASE64_CHARS[e];
    }

    const size_t rest_num { data_size % 3 };
    if (rest_num > 0) {
        std::array<std::uint8_t, 3> tmp { };
        auto iter { std::prev(std::cend(data), rest_num) };
        for (size_t j { }; j < rest_num; ++j)
            tmp[j] = static_cast<std::uint8_t>(*iter++);

        std::array<std::uint8_t, 4> tmp_buff { };
        tmp_buff[0] = ((tmp[0] & 0xfc) >> 2);
        tmp_buff[1] = (((tmp[0] & 0x03) << 4u) + ((tmp[1] & 0xf0) >> 4u));
        tmp_buff[2] = (((tmp[1] & 0x0f) << 2u) + ((tmp[2] & 0xc0) >> 6u));
        tmp_buff[3] = (tmp[2] & 0x3f);
        for (size_t i { }, ie { rest_num }; i <= ie; ++i)
            res += G_BASE64_CHARS[tmp_buff[i]];

        res.append(3 - rest_num, '=');
    }
    return res;
}

//------------------

[[nodiscard]]
kas::expected<std::vector<std::uint8_t>>
decode(const std::string& encoded)
{
    if (encoded.empty())
        return std::vector<std::uint8_t> { };

    const size_t encoded_size { encoded.size() };

    if (encoded_size % 4 != 0) [[unlikely]]
        return unexpected { "Invalid Base64 string: length must be a multiple of 4" };

    static const std::array<int32_t, 256> decode_table {
        [] {
            std::array<int32_t, 256> table { };
            std::ranges::fill(table, -1);
            for (size_t i { }, ie { G_BASE64_CHARS.size() }; i < ie; ++i)
                table[G_BASE64_CHARS[i]] = static_cast<int32_t>(i);
            return table;
        }()
    };

    size_t padding_count { };
    for (char c : encoded) {
        if (c == '=') {
            ++padding_count;
            if (padding_count > 2) [[unlikely]]
                return unexpected { "Invalid Base64 string: too many padding characters" };
            continue;
        }

        if (decode_table[static_cast<uint8_t>(c)] == -1) [[unlikely]]
            return unexpected { "Invalid Base64 string: invalid character" };
    }
    const size_t padding { padding_count };
    const size_t decoded_size { (encoded_size / 4) * 3 - padding };

    std::vector<std::uint8_t> result;
    result.reserve(decoded_size);

    for (size_t i { }; i < encoded_size; i += 4) {
        std::array<uint8_t, 4> char_indices { };

        for (size_t j { }; j < 4; ++j) {
            if (encoded[i + j] != '=') [[likely]] {
                const int32_t value { decode_table[static_cast<uint8_t>(encoded[i + j])] };
                char_indices[j] = static_cast<uint8_t>(value);
            } else {
                if (i + j < encoded_size - padding) [[unlikely]]
                    return unexpected { "Invalid Base64 string: indentation in the middle of the line" };
            }
        }

        std::array<uint8_t, 3> decoded_bytes { };
        decoded_bytes[0] = (char_indices[0] << 2) | ((char_indices[1] & 0x30) >> 4);
        decoded_bytes[1] = ((char_indices[1] & 0x0F) << 4) | ((char_indices[2] & 0x3C) >> 2);
        decoded_bytes[2] = ((char_indices[2] & 0x03) << 6) | char_indices[3];

        size_t bytes_to_output { 3 };

        if (i + 4 == encoded_size && padding > 0) {
            bytes_to_output = 3 - padding;

            if (padding == 1) {
                if ((char_indices[3] & 0x3F) != 0) [[unlikely]]
                    return unexpected { "Invalid Base64 string: invalid padding bits" };
            } else if (padding == 2) {
                if ((char_indices[2] & 0x3C) != 0) [[unlikely]]
                    return unexpected { "Invalid Base64 string: invalid padding bits" };
            }
        }

        for (size_t j { }; j < bytes_to_output; ++j)
            result.push_back(decoded_bytes[j]);
    }

    return result;
}

//------------------
} // namespace kas::convert::base64::v1
