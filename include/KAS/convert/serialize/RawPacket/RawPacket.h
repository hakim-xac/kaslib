/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_CONVERT_SERIALIZE_RAW_PACKET_H
#define KASLIBCXX_CONVERT_SERIALIZE_RAW_PACKET_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace kas::convert::serialize {

/**
 * @brief Класс для формирования сырого буфера с явным порядком байт.
 *
 * Все многобайтовые put/update имеют явный суффикс:
 *   _be - big-endian (сетевой порядок, старший байт первым)
 *   _le - little-endian (младший байт первым)
 *
 * put8 / update8 не имеют суффикса: у одного байта нет порядка.
 *
 * update*_be / update*_le атомарны: при нехватке места возвращают false
 * и НЕ изменяют буфер.
 */
class RawPacket final {
public:
    using offset_t = std::size_t;

public:
    RawPacket() = default;

    // ------------------------------------------------------------------
    // reserve / resize
    // ------------------------------------------------------------------

    RawPacket& reserve(std::size_t max_count_bytes) &;
    RawPacket&& reserve(std::size_t max_count_bytes) &&;

    RawPacket& resize(std::size_t max_count_bytes) &;
    RawPacket&& resize(std::size_t max_count_bytes) &&;

    // ------------------------------------------------------------------
    // put8
    // ------------------------------------------------------------------

    RawPacket& put8(std::uint8_t value) &;
    RawPacket&& put8(std::uint8_t value) &&;

    // ------------------------------------------------------------------
    // put16 - big-endian
    // ------------------------------------------------------------------

    RawPacket& put16_be(std::uint16_t value) &;
    RawPacket&& put16_be(std::uint16_t value) &&;

    // ------------------------------------------------------------------
    // put16 - little-endian
    // ------------------------------------------------------------------

    RawPacket& put16_le(std::uint16_t value) &;
    RawPacket&& put16_le(std::uint16_t value) &&;

    // ------------------------------------------------------------------
    // put32 - big-endian / little-endian
    // ------------------------------------------------------------------

    RawPacket& put32_be(std::uint32_t value) &;
    RawPacket&& put32_be(std::uint32_t value) &&;

    RawPacket& put32_le(std::uint32_t value) &;
    RawPacket&& put32_le(std::uint32_t value) &&;

    // ------------------------------------------------------------------
    // put64 - big-endian / little-endian
    // ------------------------------------------------------------------

    RawPacket& put64_be(std::uint64_t value) &;
    RawPacket&& put64_be(std::uint64_t value) &&;

    RawPacket& put64_le(std::uint64_t value) &;
    RawPacket&& put64_le(std::uint64_t value) &&;

    // ------------------------------------------------------------------
    // putArr / putVec / putRawPacket
    // ------------------------------------------------------------------

    RawPacket& putArr(std::span<const std::uint8_t> data_sp) &;
    RawPacket&& putArr(std::span<const std::uint8_t> data_sp) &&;

    RawPacket& putVec(std::vector<std::uint8_t> data) &;
    RawPacket&& putVec(std::vector<std::uint8_t> data) &&;

    RawPacket& putRawPacket(const RawPacket& data) &;
    RawPacket&& putRawPacket(const RawPacket& data) &&;

    RawPacket& putRawPacket(RawPacket&& data) &;
    RawPacket&& putRawPacket(RawPacket&& data) &&;

    // ------------------------------------------------------------------
    // update8
    // ------------------------------------------------------------------

    bool update8(std::uint8_t value, offset_t offset) noexcept;

    // ------------------------------------------------------------------
    // update16 - атомарные
    // ------------------------------------------------------------------

    bool update16_be(std::uint16_t value, offset_t offset) noexcept;
    bool update16_le(std::uint16_t value, offset_t offset) noexcept;

    // ------------------------------------------------------------------
    // update32 - атомарные
    // ------------------------------------------------------------------

    bool update32_be(std::uint32_t value, offset_t offset) noexcept;
    bool update32_le(std::uint32_t value, offset_t offset) noexcept;

    // ------------------------------------------------------------------
    // update64 - атомарные
    // ------------------------------------------------------------------

    bool update64_be(std::uint64_t value, offset_t offset) noexcept;
    bool update64_le(std::uint64_t value, offset_t offset) noexcept;

    // ------------------------------------------------------------------
    // updateRange
    // ------------------------------------------------------------------

    bool updateRange(std::span<const std::uint8_t> data_sp, offset_t offset) noexcept;

    // ------------------------------------------------------------------
    // Геттеры
    // ------------------------------------------------------------------

    [[nodiscard]] bool hasData() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] const std::uint8_t* data() const& noexcept;

    [[nodiscard]] const std::vector<std::uint8_t>& toRawBytes() const& noexcept;
    [[nodiscard]] std::vector<std::uint8_t> toRawBytes() && noexcept;

private:
    std::vector<std::uint8_t> m_data;
};

} // namespace kas::convert

#endif