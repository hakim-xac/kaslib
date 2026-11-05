/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_CONVERT_CRC_CCITT_FALSE_H
#define KASLIBCXX_CONVERT_CRC_CCITT_FALSE_H

#include <cstdint>
#include <span>

/**
 * @brief CRC-16/CCITT-FALSE (also known as CRC-16/AUG-CCITT or CRC-16/SPI-FUJITSU).
 *
 * Parameters:
 *   - Width:    16
 *   - Polynomial (normal): 0x1021
 *   - Initial value:       0xFFFF
 *   - Input reflected:     no
 *   - Output reflected:    no
 *   - Final XOR:           0x0000
 */
namespace kas::convert::crc::ccitt_false {

/**
 * @brief Computes the CRC-16/CCITT-FALSE checksum of a byte buffer.
 *
 * @param data Input bytes to process.
 * @return 16-bit CRC value.
 */
[[nodiscard]] std::uint16_t crc16(std::span<const std::uint8_t> data);

} // namespace kas::convert::crc::ccitt_false

#endif