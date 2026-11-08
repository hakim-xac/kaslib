// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include "KAS/convert/sha1/v1/sha1.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace kas::convert::sha1::v1 {
namespace {

    static constexpr std::uint32_t G_H0 { 0x67452301 };
    static constexpr std::uint32_t G_H1 { 0xEFCDAB89 };
    static constexpr std::uint32_t G_H2 { 0x98BADCFE };
    static constexpr std::uint32_t G_H3 { 0x10325476 };
    static constexpr std::uint32_t G_H4 { 0xC3D2E1F0 };

    static constexpr std::uint32_t G_K1 { 0x5A827999 };
    static constexpr std::uint32_t G_K2 { 0x6ED9EBA1 };
    static constexpr std::uint32_t G_K3 { 0x8F1BBCDC };
    static constexpr std::uint32_t G_K4 { 0xCA62C1D6 };

    //------------------

    [[nodiscard]] constexpr std::uint32_t
    f(int t, std::uint32_t b, std::uint32_t c, std::uint32_t d) noexcept
    {
        if (t <= 19)
            return (b & c) | ((~b) & d);
        if (t <= 39)
            return b ^ c ^ d;
        if (t <= 59)
            return (b & c) | (b & d) | (c & d);
        return b ^ c ^ d;
    }

    [[nodiscard]] constexpr std::uint32_t
    leftRotate(std::uint32_t value, int shift) noexcept
    {
        return (value << shift) | (value >> (32 - shift));
    }

    //------------------
    //  SHA-1 incremental context
    //------------------

    struct SHA1Context {
        std::uint32_t h[5] { G_H0, G_H1, G_H2, G_H3, G_H4 };
        std::uint64_t total_len_bytes { 0 };
        std::uint8_t buffer[64] { };
        std::size_t buffer_len { 0 };
    };

    void sha1Init(SHA1Context& ctx) noexcept
    {
        ctx.h[0] = G_H0;
        ctx.h[1] = G_H1;
        ctx.h[2] = G_H2;
        ctx.h[3] = G_H3;
        ctx.h[4] = G_H4;
        ctx.total_len_bytes = 0;
        ctx.buffer_len = 0;
    }

    void sha1ProcessBlock(SHA1Context& ctx, const std::uint8_t* block) noexcept
    {
        static constexpr std::uint32_t K[] { G_K1, G_K2, G_K3, G_K4 };

        std::uint32_t w[80];
        for (int j = 0; j < 16; ++j) {
            w[j] = (std::uint32_t(block[j * 4]) << 24)
                | (std::uint32_t(block[j * 4 + 1]) << 16)
                | (std::uint32_t(block[j * 4 + 2]) << 8)
                | std::uint32_t(block[j * 4 + 3]);
        }
        for (int j = 16; j < 80; ++j)
            w[j] = leftRotate(w[j - 3] ^ w[j - 8] ^ w[j - 14] ^ w[j - 16], 1);

        std::uint32_t a = ctx.h[0];
        std::uint32_t b = ctx.h[1];
        std::uint32_t c = ctx.h[2];
        std::uint32_t d = ctx.h[3];
        std::uint32_t e = ctx.h[4];

        for (int t = 0; t < 80; ++t) {
            const std::uint32_t temp = leftRotate(a, 5) + f(t, b, c, d) + e + w[t] + K[t / 20];
            e = d;
            d = c;
            c = leftRotate(b, 30);
            b = a;
            a = temp;
        }

        ctx.h[0] += a;
        ctx.h[1] += b;
        ctx.h[2] += c;
        ctx.h[3] += d;
        ctx.h[4] += e;
    }

    void sha1Update(SHA1Context& ctx, const std::uint8_t* data, std::size_t len) noexcept
    {
        ctx.total_len_bytes += len;

        std::size_t i = 0;
        while (i < len) {
            if (ctx.buffer_len == 64) {
                sha1ProcessBlock(ctx, ctx.buffer);
                ctx.buffer_len = 0;
            }
            const std::size_t to_copy = std::min(len - i, 64 - ctx.buffer_len);
            std::copy_n(data + i, to_copy, ctx.buffer + ctx.buffer_len);
            ctx.buffer_len += to_copy;
            i += to_copy;
        }
    }

    void sha1Finalize(SHA1Context& ctx, std::uint8_t out[20]) noexcept
    {
        const std::uint64_t bit_len = ctx.total_len_bytes * 8;

        // 0x80 + zero + 64-bit len (big-endian).
        ctx.buffer[ctx.buffer_len++] = 0x80;
        if (ctx.buffer_len > 56) {
            while (ctx.buffer_len < 64)
                ctx.buffer[ctx.buffer_len++] = 0;
            sha1ProcessBlock(ctx, ctx.buffer);
            ctx.buffer_len = 0;
        }
        while (ctx.buffer_len < 56)
            ctx.buffer[ctx.buffer_len++] = 0;

        for (int i = 7; i >= 0; --i)
            ctx.buffer[ctx.buffer_len++] = std::uint8_t(bit_len >> (i * 8));

        sha1ProcessBlock(ctx, ctx.buffer);

        for (int k = 0; k < 5; ++k) {
            out[k * 4] = std::uint8_t(ctx.h[k] >> 24);
            out[k * 4 + 1] = std::uint8_t(ctx.h[k] >> 16);
            out[k * 4 + 2] = std::uint8_t(ctx.h[k] >> 8);
            out[k * 4 + 3] = std::uint8_t(ctx.h[k]);
        }
    }

    [[nodiscard]] std::string
    bytesToHex(const std::uint8_t* data, std::size_t size)
    {
        static constexpr char HEX[] = "0123456789abcdef";
        std::string out;
        out.reserve(size * 2);
        for (std::size_t i = 0; i < size; ++i) {
            out.push_back(HEX[data[i] >> 4]);
            out.push_back(HEX[data[i] & 0x0F]);
        }
        return out;
    }

} // namespace

//------------------

std::string
hash(std::string data)
{
    SHA1Context ctx;
    sha1Init(ctx);
    sha1Update(ctx, reinterpret_cast<const std::uint8_t*>(data.data()), data.size());

    std::uint8_t digest[20];
    sha1Finalize(ctx, digest);
    return bytesToHex(digest, sizeof(digest));
}

//------------------

std::array<std::uint8_t, 20>
hashBinary(std::string data)
{
    SHA1Context ctx;
    sha1Init(ctx);
    sha1Update(ctx, reinterpret_cast<const std::uint8_t*>(data.data()), data.size());

    std::array<std::uint8_t, 20> out { };
    sha1Finalize(ctx, out.data());
    return out;
}

//------------------

std::optional<std::string>
hashFile(const std::filesystem::path& filename)
{
    std::ifstream fs { filename, std::ios::binary };
    if (!fs.is_open()) [[unlikely]]
        return std::nullopt;

    SHA1Context ctx;
    sha1Init(ctx);

    std::array<char, 4096> buffer { };
    while (fs) {
        fs.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize bytes_read = fs.gcount();
        if (bytes_read > 0) {
            sha1Update(ctx,
                reinterpret_cast<const std::uint8_t*>(buffer.data()),
                static_cast<std::size_t>(bytes_read));
        }
    }

    std::uint8_t digest[20];
    sha1Finalize(ctx, digest);
    return bytesToHex(digest, sizeof(digest));
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

    SHA1Context ctx;
    sha1Init(ctx);

    std::size_t remaining = end_bytes - start_bytes;
    std::array<char, 4096> buffer { };

    while (remaining > 0) {
        const std::size_t to_read = std::min(remaining, buffer.size());
        fs.read(buffer.data(), static_cast<std::streamsize>(to_read));
        const std::streamsize bytes_read = fs.gcount();
        if (bytes_read <= 0) [[unlikely]]
            break;

        sha1Update(ctx,
            reinterpret_cast<const std::uint8_t*>(buffer.data()),
            static_cast<std::size_t>(bytes_read));
        remaining -= static_cast<std::size_t>(bytes_read);
    }

    std::uint8_t digest[20];
    sha1Finalize(ctx, digest);
    return bytesToHex(digest, sizeof(digest));
}

//------------------

} // namespace kas::convert::sha1::v1