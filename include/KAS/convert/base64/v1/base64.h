/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_CONVERT_BASE64_V1_H
#define KASLIBCXX_CONVERT_BASE64_V1_H

#include <KAS/expected/expected.h>

#include <cstdint>
#include <span>
#include <string>
#include <vector>

/**
 * @brief Base64 encoding and decoding (version 1 implementation).
 */
namespace kas::convert::base64::v1 {

/**
 * @brief Encodes a byte buffer into a Base64 string.
 *
 * @param data Input bytes to encode.
 * @return Base64-encoded string representation of the input data.
 */
[[nodiscard]] std::string encode(const std::span<const std::uint8_t> data);

/**
 * @brief Decodes a Base64 string into a byte buffer.
 *
 * @param encoded Base64-encoded input string.
 * @return On success, a vector containing the decoded bytes; on failure,
 *         an error value (e.g. for invalid characters or malformed input).
 */
[[nodiscard]] kas::expected<std::vector<std::uint8_t>> decode(const std::string& encoded);
} // namespace kas::convert::base64

#endif