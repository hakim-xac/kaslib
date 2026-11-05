/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_CONVERT_SHA1_V1_H
#define KASLIBCXX_CONVERT_SHA1_V1_H

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace kas::convert::sha1::v1 {

[[nodiscard]] std::string hash(std::string data);

[[nodiscard]] std::array<std::uint8_t, 20> hashBinary(std::string data);

[[nodiscard]] std::optional<std::string> hashFile(const std::filesystem::path& filename);

[[nodiscard]] std::optional<std::string> hashFile(const std::filesystem::path& filename, std::size_t start_bytes, std::size_t end_bytes);

} // namespace kas::convert::sha1::v1

#endif