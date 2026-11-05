// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include "KAS/convert/sha256/sha256.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <optional>
#include <span>

namespace kas::convert::sha256::v2 {

namespace {

    // --- Constants SHA-256 ---
    constexpr std::array<std::uint32_t, 64> K = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
        0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
        0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
        0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
        0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
        0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
        0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };

    constexpr std::array<std::uint32_t, 8> H0 = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    constexpr std::uint32_t rotr(std::uint32_t x, std::uint32_t n) noexcept
    {
        return (x >> n) | (x << (32 - n));
    }
    constexpr std::uint32_t shr(std::uint32_t x, std::uint32_t n) noexcept
    {
        return x >> n;
    }
    constexpr std::uint32_t ch(std::uint32_t x, std::uint32_t y, std::uint32_t z) noexcept
    {
        return (x & y) ^ (~x & z);
    }
    constexpr std::uint32_t maj(std::uint32_t x, std::uint32_t y, std::uint32_t z) noexcept
    {
        return (x & y) ^ (x & z) ^ (y & z);
    }
    constexpr std::uint32_t sigma0(std::uint32_t x) noexcept
    {
        return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
    }
    constexpr std::uint32_t sigma1(std::uint32_t x) noexcept
    {
        return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
    }
    constexpr std::uint32_t gamma0(std::uint32_t x) noexcept
    {
        return rotr(x, 7) ^ rotr(x, 18) ^ shr(x, 3);
    }
    constexpr std::uint32_t gamma1(std::uint32_t x) noexcept
    {
        return rotr(x, 17) ^ rotr(x, 19) ^ shr(x, 10);
    }

    void transform(std::span<const std::uint8_t, 64> block,
        std::array<std::uint32_t, 8>& state) noexcept
    {
        std::array<std::uint32_t, 64> w { };
        for (int i = 0; i < 16; ++i) {
            w[i] = (std::uint32_t(block[i * 4]) << 24)
                | (std::uint32_t(block[i * 4 + 1]) << 16)
                | (std::uint32_t(block[i * 4 + 2]) << 8)
                | std::uint32_t(block[i * 4 + 3]);
        }
        for (int i = 16; i < 64; ++i)
            w[i] = gamma1(w[i - 2]) + w[i - 7] + gamma0(w[i - 15]) + w[i - 16];

        auto [a, b, c, d, e, f, g, h] = state;
        for (int i = 0; i < 64; ++i) {
            const std::uint32_t t1 = h + sigma1(e) + ch(e, f, g) + K[i] + w[i];
            const std::uint32_t t2 = sigma0(a) + maj(a, b, c);
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }
        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        state[5] += f;
        state[6] += g;
        state[7] += h;
    }

    class Hasher final {
    public:
        void update(std::span<const std::uint8_t> data) noexcept
        {
            m_bit_count += std::uint64_t(data.size()) * 8;

            std::size_t i = 0;
            while (i < data.size()) {
                if (m_buffer_pos == 64) {
                    transform(m_buffer, m_state);
                    m_buffer_pos = 0;
                }
                const std::size_t to_copy = std::min(data.size() - i, 64 - m_buffer_pos);
                std::copy_n(data.begin() + i, to_copy, m_buffer.begin() + m_buffer_pos);
                m_buffer_pos += to_copy;
                i += to_copy;
            }
        }

        [[nodiscard]] std::array<std::uint8_t, 32> finish() &&
        {
            if (m_buffer_pos == 64) {
                transform(m_buffer, m_state);
                m_buffer_pos = 0;
            }

            m_buffer[m_buffer_pos++] = 0x80;

            if (m_buffer_pos > 56) {
                while (m_buffer_pos < 64)
                    m_buffer[m_buffer_pos++] = 0;
                transform(m_buffer, m_state);
                m_buffer_pos = 0;
            }
            while (m_buffer_pos < 56)
                m_buffer[m_buffer_pos++] = 0;

            for (int i = 7; i >= 0; --i)
                m_buffer[m_buffer_pos++] = std::uint8_t(m_bit_count >> (i * 8));

            transform(m_buffer, m_state);

            std::array<std::uint8_t, 32> out { };
            for (int k = 0; k < 8; ++k) {
                out[k * 4] = std::uint8_t(m_state[k] >> 24);
                out[k * 4 + 1] = std::uint8_t(m_state[k] >> 16);
                out[k * 4 + 2] = std::uint8_t(m_state[k] >> 8);
                out[k * 4 + 3] = std::uint8_t(m_state[k]);
            }
            return out;
        }

    private:
        std::array<std::uint32_t, 8> m_state = H0;
        std::uint64_t m_bit_count = 0;
        std::array<std::uint8_t, 64> m_buffer { };
        std::size_t m_buffer_pos = 0;
    };
}

//------------------

std::array<std::uint8_t, 32>
digest(std::span<const std::uint8_t> data)
{
    Hasher h;
    h.update(data);
    return std::move(h).finish();
}

//------------------

std::array<std::uint8_t, 32>
digest(const std::string& data)
{
    return digest(std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(data.data()), data.size()));
}

//------------------

std::string
hexDigest(std::span<const std::uint8_t> data)
{
    static constexpr char HEX[] = "0123456789abcdef";
    const auto bytes = digest(data);
    std::string out(64, '\0');
    for (std::size_t k = 0; k < 32; ++k) {
        out[k * 2] = HEX[bytes[k] >> 4];
        out[k * 2 + 1] = HEX[bytes[k] & 0x0F];
    }
    return out;
}

//------------------

std::string
hexDigest(const std::string& data)
{
    return hexDigest(std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(data.data()), data.size()));
}

//------------------

std::string
hash(const std::string& data)
{
    return hexDigest(data);
}

//------------------

std::optional<std::string>
hashFile(const std::filesystem::path& filename)
{
    std::ifstream fs { filename, std::ios::binary };
    if (!fs.is_open()) [[unlikely]]
        return std::nullopt;

    Hasher h;
    std::array<char, 4096> buffer { };
    while (fs) {
        fs.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize bytes_read = fs.gcount();
        if (bytes_read > 0) {
            h.update(std::span<const std::uint8_t>(
                reinterpret_cast<const std::uint8_t*>(buffer.data()),
                static_cast<std::size_t>(bytes_read)));
        }
    }

    const auto bytes = std::move(h).finish();
    static constexpr char HEX[] = "0123456789abcdef";
    std::string out(64, '\0');
    for (std::size_t k = 0; k < 32; ++k) {
        out[k * 2] = HEX[bytes[k] >> 4];
        out[k * 2 + 1] = HEX[bytes[k] & 0x0F];
    }
    return out;
}

//------------------

std::optional<std::string>
hashFile(const std::filesystem::path& filename,
    std::size_t start_bytes,
    std::size_t end_bytes)
{
    if (start_bytes > end_bytes) [[unlikely]]
        return std::nullopt;

    std::ifstream fs { filename, std::ios::binary };
    if (!fs.is_open()) [[unlikely]]
        return std::nullopt;

    fs.seekg(0, std::ios::end);
    const auto file_length = static_cast<std::streamoff>(fs.tellg());
    if (file_length < 0
        || static_cast<std::uintmax_t>(end_bytes) > static_cast<std::uintmax_t>(file_length))
        [[unlikely]]
        return std::nullopt;

    fs.seekg(static_cast<std::streamoff>(start_bytes), std::ios::beg);

    Hasher h;
    std::size_t remaining = end_bytes - start_bytes;
    std::array<char, 4096> buffer { };

    while (remaining > 0) {
        const std::size_t to_read = std::min(remaining, buffer.size());
        fs.read(buffer.data(), static_cast<std::streamsize>(to_read));
        const std::streamsize bytes_read = fs.gcount();
        if (bytes_read <= 0) [[unlikely]]
            break;

        h.update(std::span<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t*>(buffer.data()),
            static_cast<std::size_t>(bytes_read)));
        remaining -= static_cast<std::size_t>(bytes_read);
    }

    const auto bytes = std::move(h).finish();
    static constexpr char HEX[] = "0123456789abcdef";
    std::string out(64, '\0');
    for (std::size_t k = 0; k < 32; ++k) {
        out[k * 2] = HEX[bytes[k] >> 4];
        out[k * 2 + 1] = HEX[bytes[k] & 0x0F];
    }
    return out;
}

//------------------

} // namespace kas::convert::sha256::v2