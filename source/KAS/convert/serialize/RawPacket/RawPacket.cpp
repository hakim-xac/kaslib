// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include "KAS/convert/serialize/RawPacket/RawPacket.h"

namespace kas::convert::serialize {

namespace {

    /**
     * @brief Appends @p bytes big-endian bytes of @p value to @p out.
     *
     * The most significant byte is written first. If @p bytes is greater than
     * 8, the extra leading bytes are zero.
     *
     * @param out   Output vector.
     * @param value Value to encode.
     * @param bytes Number of bytes to write.
     */
    void appendBe(std::vector<std::uint8_t>& out, std::uint64_t value, std::size_t bytes)
    {
        if (bytes == 0) [[unlikely]]
            return;

        const auto old_size = out.size();
        out.resize(old_size + bytes);
        auto* p = out.data() + old_size;

        for (std::size_t i = 0; i < bytes; ++i) {
            const std::size_t shift = 8 * (bytes - 1 - i);
            p[i] = (shift < 64) ? static_cast<std::uint8_t>(value >> shift) : std::uint8_t { 0 };
        }
    }

    /**
     * @brief Appends @p bytes little-endian bytes of @p value to @p out.
     *
     * The least significant byte is written first. If @p bytes is greater than
     * 8, the extra trailing bytes are zero.
     *
     * @param out   Output vector.
     * @param value Value to encode.
     * @param bytes Number of bytes to write.
     */
    void appendLe(std::vector<std::uint8_t>& out, std::uint64_t value, std::size_t bytes)
    {
        if (bytes == 0) [[unlikely]]
            return;

        const auto old_size = out.size();
        out.resize(old_size + bytes);
        auto* p = out.data() + old_size;

        for (std::size_t i = 0; i < bytes; ++i) {
            const std::size_t shift = 8 * i;
            p[i] = (shift < 64) ? static_cast<std::uint8_t>(value >> shift) : std::uint8_t { 0 };
        }
    }

    /**
     * @brief Checks whether the range [offset, offset + bytes) fits in [0, size).
     *
     * Guards against overflow: offset + bytes is never computed directly, so
     * wrapping around is impossible. The check is ordered so that each
     * subtraction is performed only when the minuend is not smaller than the
     * subtrahend (offset <= size, then bytes <= size - offset).
     *
     * @param size   Total size of the buffer.
     * @param offset Start of the range.
     * @param bytes  Length of the range.
     * @return true if offset <= size and bytes <= size - offset, false otherwise.
     */
    [[nodiscard]] constexpr bool fits(std::size_t size, std::size_t offset, std::size_t bytes) noexcept
    {
        // Guards against overflow: offset + bytes could wrap around.
        return offset <= size && bytes <= size - offset;
    }

} // namespace

// ----------------------------------------------------------------------------
//  reserve / resize
// ----------------------------------------------------------------------------

RawPacket& RawPacket::reserve(std::size_t max_count_bytes) &
{
    m_data.reserve(max_count_bytes);
    return *this;
}

RawPacket&& RawPacket::reserve(std::size_t max_count_bytes) &&
{
    reserve(max_count_bytes);
    return std::move(*this);
}

RawPacket& RawPacket::resize(std::size_t max_count_bytes) &
{
    m_data.resize(max_count_bytes);
    return *this;
}

RawPacket&& RawPacket::resize(std::size_t max_count_bytes) &&
{
    resize(max_count_bytes);
    return std::move(*this);
}

// ----------------------------------------------------------------------------
//  put8
// ----------------------------------------------------------------------------

RawPacket& RawPacket::put8(std::uint8_t value) &
{
    m_data.push_back(value);
    return *this;
}

RawPacket&& RawPacket::put8(std::uint8_t value) &&
{
    put8(value);
    return std::move(*this);
}

// ----------------------------------------------------------------------------
//  put16
// ----------------------------------------------------------------------------

RawPacket& RawPacket::put16_be(std::uint16_t value) &
{
    appendBe(m_data, value, 2);
    return *this;
}

RawPacket&& RawPacket::put16_be(std::uint16_t value) &&
{
    put16_be(value);
    return std::move(*this);
}

RawPacket& RawPacket::put16_le(std::uint16_t value) &
{
    appendLe(m_data, value, 2);
    return *this;
}

RawPacket&& RawPacket::put16_le(std::uint16_t value) &&
{
    put16_le(value);
    return std::move(*this);
}

// ----------------------------------------------------------------------------
//  put32
// ----------------------------------------------------------------------------

RawPacket& RawPacket::put32_be(std::uint32_t value) &
{
    appendBe(m_data, value, 4);
    return *this;
}

RawPacket&& RawPacket::put32_be(std::uint32_t value) &&
{
    put32_be(value);
    return std::move(*this);
}

RawPacket& RawPacket::put32_le(std::uint32_t value) &
{
    appendLe(m_data, value, 4);
    return *this;
}

RawPacket&& RawPacket::put32_le(std::uint32_t value) &&
{
    put32_le(value);
    return std::move(*this);
}

// ----------------------------------------------------------------------------
//  put64
// ----------------------------------------------------------------------------

RawPacket& RawPacket::put64_be(std::uint64_t value) &
{
    appendBe(m_data, value, 8);
    return *this;
}

RawPacket&& RawPacket::put64_be(std::uint64_t value) &&
{
    put64_be(value);
    return std::move(*this);
}

RawPacket& RawPacket::put64_le(std::uint64_t value) &
{
    appendLe(m_data, value, 8);
    return *this;
}

RawPacket&& RawPacket::put64_le(std::uint64_t value) &&
{
    put64_le(value);
    return std::move(*this);
}

// ----------------------------------------------------------------------------
//  putArr / putVec / putRawPacket
// ----------------------------------------------------------------------------

RawPacket& RawPacket::putArr(std::span<const std::uint8_t> data_sp) &
{
    m_data.insert(m_data.end(), data_sp.begin(), data_sp.end());
    return *this;
}

RawPacket&& RawPacket::putArr(std::span<const std::uint8_t> data_sp) &&
{
    putArr(data_sp);
    return std::move(*this);
}

RawPacket& RawPacket::putVec(std::vector<std::uint8_t> data) &
{
    if (m_data.empty()) {
        m_data = std::move(data);
    } else {
        m_data.insert(m_data.end(), data.begin(), data.end());
    }
    return *this;
}

RawPacket&& RawPacket::putVec(std::vector<std::uint8_t> data) &&
{
    putVec(std::move(data));
    return std::move(*this);
}

RawPacket& RawPacket::putRawPacket(const RawPacket& data) &
{
    if (std::addressof(data) != this) {
        m_data.insert(m_data.end(), data.m_data.begin(), data.m_data.end());
    }

    return *this;
}

RawPacket&& RawPacket::putRawPacket(const RawPacket& data) &&
{
    putRawPacket(data);
    return std::move(*this);
}

RawPacket& RawPacket::putRawPacket(RawPacket&& data) &
{
    if (std::addressof(data) != this) {
        if (m_data.empty()) {
            m_data = std::move(data.m_data);
        } else {
            m_data.insert(m_data.end(), data.m_data.begin(), data.m_data.end());
        }
    }

    return *this;
}

RawPacket&& RawPacket::putRawPacket(RawPacket&& data) &&
{
    putRawPacket(std::move(data));
    return std::move(*this);
}

// ----------------------------------------------------------------------------
//  update8
// ----------------------------------------------------------------------------

bool RawPacket::update8(std::uint8_t value, offset_t offset) noexcept
{
    if (offset >= m_data.size()) [[unlikely]]
        return false;

    m_data[offset] = value;
    return true;
}

// ----------------------------------------------------------------------------
//  update16
// ----------------------------------------------------------------------------

bool RawPacket::update16_be(std::uint16_t value, offset_t offset) noexcept
{
    if (!fits(m_data.size(), offset, 2)) [[unlikely]]
        return false;

    m_data[offset] = static_cast<std::uint8_t>(value >> 8);
    m_data[offset + 1] = static_cast<std::uint8_t>(value >> 0);
    return true;
}

bool RawPacket::update16_le(std::uint16_t value, offset_t offset) noexcept
{
    if (!fits(m_data.size(), offset, 2)) [[unlikely]]
        return false;

    m_data[offset] = static_cast<std::uint8_t>(value >> 0);
    m_data[offset + 1] = static_cast<std::uint8_t>(value >> 8);
    return true;
}

// ----------------------------------------------------------------------------
//  update32
// ----------------------------------------------------------------------------

bool RawPacket::update32_be(std::uint32_t value, offset_t offset) noexcept
{
    if (!fits(m_data.size(), offset, 4)) [[unlikely]]
        return false;

    m_data[offset] = static_cast<std::uint8_t>(value >> 24);
    m_data[offset + 1] = static_cast<std::uint8_t>(value >> 16);
    m_data[offset + 2] = static_cast<std::uint8_t>(value >> 8);
    m_data[offset + 3] = static_cast<std::uint8_t>(value >> 0);
    return true;
}

bool RawPacket::update32_le(std::uint32_t value, offset_t offset) noexcept
{
    if (!fits(m_data.size(), offset, 4)) [[unlikely]]
        return false;

    m_data[offset] = static_cast<std::uint8_t>(value >> 0);
    m_data[offset + 1] = static_cast<std::uint8_t>(value >> 8);
    m_data[offset + 2] = static_cast<std::uint8_t>(value >> 16);
    m_data[offset + 3] = static_cast<std::uint8_t>(value >> 24);
    return true;
}

// ----------------------------------------------------------------------------
//  update64
// ----------------------------------------------------------------------------

bool RawPacket::update64_be(std::uint64_t value, offset_t offset) noexcept
{
    if (!fits(m_data.size(), offset, 8)) [[unlikely]]
        return false;

    for (std::size_t i = 0; i < 8; ++i)
        m_data[offset + i] = static_cast<std::uint8_t>(value >> (8 * (7 - i)));
    return true;
}

bool RawPacket::update64_le(std::uint64_t value, offset_t offset) noexcept
{
    if (!fits(m_data.size(), offset, 8)) [[unlikely]]
        return false;

    for (std::size_t i = 0; i < 8; ++i)
        m_data[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
    return true;
}

// ----------------------------------------------------------------------------
//  updateRange
// ----------------------------------------------------------------------------

bool RawPacket::updateRange(std::span<const std::uint8_t> data_sp,
    offset_t offset) noexcept
{
    if (!fits(m_data.size(), offset, data_sp.size())) [[unlikely]]
        return false;

    std::ranges::copy(data_sp, m_data.begin() + offset);
    return true;
}

// ----------------------------------------------------------------------------
//  Геттеры
// ----------------------------------------------------------------------------

bool RawPacket::hasData() const noexcept
{
    return !m_data.empty();
}

std::size_t RawPacket::size() const noexcept
{
    return m_data.size();
}

const std::uint8_t* RawPacket::data() const& noexcept
{
    return m_data.data();
}

const std::vector<std::uint8_t>&
RawPacket::toRawBytes() const& noexcept
{
    return m_data;
}

std::vector<std::uint8_t>
RawPacket::toRawBytes() && noexcept
{
    return std::move(m_data);
}
} // namespace kas::convert::serialize::v1
