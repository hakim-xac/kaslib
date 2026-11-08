/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_CONVERT_SHA256_V2_H
#define KASLIBCXX_CONVERT_SHA256_V2_H

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>

namespace kas::convert::sha256::v2 {

[[nodiscard]] std::array<std::uint8_t, 32> digest(std::span<const std::uint8_t> data);

[[nodiscard]] std::array<std::uint8_t, 32> digest(const std::string& data);

[[nodiscard]] std::string hexDigest(std::span<const std::uint8_t> data);

[[nodiscard]] std::string hexDigest(const std::string& data);

[[nodiscard]] std::string hash(const std::string& data);

[[nodiscard]] std::optional<std::string> hashFile(const std::filesystem::path& filename);

[[nodiscard]] std::optional<std::string> hashFile(const std::filesystem::path& filename, std::size_t start_bytes, std::size_t end_bytes);

} // namespace kas::convert::sha256::v2
#endif