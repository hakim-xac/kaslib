/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_CONVERT_MD5_V3_H
#define KASLIBCXX_CONVERT_MD5_V3_H

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

/**
 * @brief MD5 hashing utilities, version 3.
 *
 * Provides hashing of strings, raw byte buffers, and files. All functions
 * return the digest as a lowercase hexadecimal string (32 characters).
 *
 * @note MD5 is cryptographically broken and must not be used for security
 *       purposes (signatures, password storage, integrity against attackers).
 *       It is suitable only for non-security tasks such as checksums,
 *       cache keys, or content addressing.
 */
namespace kas::convert::md5::v3 {

/**
 * @brief Computes the MD5 digest of a string.
 *
 * @param str Input string; its bytes are hashed as-is.
 * @return Lowercase hexadecimal MD5 digest (32 characters).
 */
[[nodiscard]] std::string hashString(std::string_view str);

/**
 * @brief Computes the MD5 digest of a raw byte buffer.
 *
 * @param data Pointer to the first byte of the buffer.
 * @param size Number of bytes to hash.
 * @return Lowercase hexadecimal MD5 digest (32 characters).
 *
 * @note If size is 0, data may be nullptr.
 */
[[nodiscard]] std::string hashBytes(const std::uint8_t* data, std::size_t size);

/**
 * @brief Computes the MD5 digest of a byte span.
 *
 * @param data Span of bytes to hash.
 * @return Lowercase hexadecimal MD5 digest (32 characters).
 */
[[nodiscard]] std::string hashBytes(std::span<const std::uint8_t> data);

/**
 * @brief Computes the MD5 digest of an entire file.
 *
 * Reads the file in a streaming fashion; the whole file is not required to
 * fit in memory.
 *
 * @param filename Path to the file to hash.
 * @return On success, the lowercase hexadecimal MD5 digest (32 characters);
 *         on failure (e.g. the file cannot be opened or read), std::nullopt.
 */
[[nodiscard]] std::optional<std::string> hashFile(const std::filesystem::path& filename);

/**
 * @brief Computes the MD5 digest of a byte range within a file.
 *
 * Hashes exactly the bytes in the half-open range [start_bytes, end_bytes).
 * The range is interpreted as byte offsets from the beginning of the file.
 *
 * @param filename    Path to the file to hash.
 * @param start_bytes Offset of the first byte to include.
 * @param end_bytes   Offset one past the last byte to include.
 * @return On success, the lowercase hexadecimal MD5 digest (32 characters);
 *         on failure (e.g. the file cannot be opened, the range is invalid,
 *         or the file is shorter than end_bytes), std::nullopt.
 *
 * @note If start_bytes >= end_bytes, the behavior is implementation-defined
 *       and should be clarified (either an empty-range digest or an error).
 */
[[nodiscard]] std::optional<std::string> hashFile(const std::filesystem::path& filename, std::size_t start_bytes, std::size_t end_bytes);
} // namespace kas::convert::md5::v3

#endif