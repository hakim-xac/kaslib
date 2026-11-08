// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include "KAS/convert/md5/v3/md5.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>

namespace kas::convert::md5::v3 {

namespace {
    // ========================================================================
    // MD5 implementation details (hidden in anonymous namespace)
    // ========================================================================

    struct MD5Context {
        std::uint64_t size = 0; // total input size in bytes
        std::uint32_t buffer[4] { }; // current hash state
        std::uint8_t input[64] { }; // pending 64-byte block
        std::uint8_t digest[16] { }; // result bytes
    };

    constexpr std::uint32_t
    rotateLeft(std::uint32_t x, std::uint32_t n) noexcept
    {
        return (x << n) | (x >> (32 - n));
    }

    void md5Init(MD5Context& ctx) noexcept
    {
        ctx.size = 0;
        ctx.buffer[0] = 0x67452301u;
        ctx.buffer[1] = 0xefcdab89u;
        ctx.buffer[2] = 0x98badcfeu;
        ctx.buffer[3] = 0x10325476u;
    }

    void md5Step(std::uint32_t* buffer, const std::uint32_t* input) noexcept
    {
        static constexpr std::uint32_t S[] = {
            7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
            5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
            4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
            6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
        };

        static constexpr std::uint32_t K[] = {
            0xd76aa478u, 0xe8c7b756u, 0x242070dbu, 0xc1bdceeeu, 0xf57c0fafu, 0x4787c62au, 0xa8304613u, 0xfd469501u,
            0x698098d8u, 0x8b44f7afu, 0xffff5bb1u, 0x895cd7beu, 0x6b901122u, 0xfd987193u, 0xa679438eu, 0x49b40821u,
            0xf61e2562u, 0xc040b340u, 0x265e5a51u, 0xe9b6c7aau, 0xd62f105du, 0x02441453u, 0xd8a1e681u, 0xe7d3fbc8u,
            0x21e1cde6u, 0xc33707d6u, 0xf4d50d87u, 0x455a14edu, 0xa9e3e905u, 0xfcefa3f8u, 0x676f02d9u, 0x8d2a4c8au,
            0xfffa3942u, 0x8771f681u, 0x6d9d6122u, 0xfde5380cu, 0xa4beea44u, 0x4bdecfa9u, 0xf6bb4b60u, 0xbebfbc70u,
            0x289b7ec6u, 0xeaa127fau, 0xd4ef3085u, 0x04881d05u, 0xd9d4d039u, 0xe6db99e5u, 0x1fa27cf8u, 0xc4ac5665u,
            0xf4292244u, 0x432aff97u, 0xab9423a7u, 0xfc93a039u, 0x655b59c3u, 0x8f0ccc92u, 0xffeff47du, 0x85845dd1u,
            0x6fa87e4fu, 0xfe2ce6e0u, 0xa3014314u, 0x4e0811a1u, 0xf7537e82u, 0xbd3af235u, 0x2ad7d2bbu, 0xeb86d391u
        };

        std::uint32_t AA = buffer[0];
        std::uint32_t BB = buffer[1];
        std::uint32_t CC = buffer[2];
        std::uint32_t DD = buffer[3];

        for (std::uint32_t i = 0; i < 64; ++i) {
            std::uint32_t E;
            std::uint32_t j;

            switch (i / 16) {
            case 0:
                E = (BB & CC) | (~BB & DD);
                j = i;
                break;
            case 1:
                E = (BB & DD) | (CC & ~DD);
                j = (i * 5 + 1) % 16;
                break;
            case 2:
                E = BB ^ CC ^ DD;
                j = (i * 3 + 5) % 16;
                break;
            default:
                E = CC ^ (BB | ~DD);
                j = (i * 7) % 16;
                break;
            }

            const std::uint32_t temp = AA;
            AA = DD;
            DD = CC;
            CC = BB;
            BB = BB + rotateLeft(temp + E + K[i] + input[j], S[i]);
        }

        buffer[0] += AA;
        buffer[1] += BB;
        buffer[2] += CC;
        buffer[3] += DD;
    }

    void md5Update(MD5Context& ctx, const std::uint8_t* input, std::size_t input_len) noexcept
    {
        std::uint32_t block[16] { };
        std::uint32_t offset = static_cast<std::uint32_t>(ctx.size % 64);
        ctx.size += static_cast<std::uint64_t>(input_len);

        for (std::size_t i = 0; i < input_len; ++i) {
            ctx.input[offset++] = input[i];

            if (offset == 64) {
                for (std::uint32_t j = 0; j < 16; ++j) {
                    block[j] = static_cast<std::uint32_t>(ctx.input[j * 4 + 3]) << 24
                        | static_cast<std::uint32_t>(ctx.input[j * 4 + 2]) << 16
                        | static_cast<std::uint32_t>(ctx.input[j * 4 + 1]) << 8
                        | static_cast<std::uint32_t>(ctx.input[j * 4]);
                }

                md5Step(ctx.buffer, block);
                offset = 0;
            }
        }
    }

    void md5Finalize(MD5Context& ctx) noexcept
    {
        static constexpr std::uint8_t PADDING[64] = {
            0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        };

        std::uint32_t block[16] { };
        const std::uint32_t offset = static_cast<std::uint32_t>(ctx.size % 64);
        const std::uint32_t padding_length = (offset < 56) ? (56 - offset) : (120 - offset);

        const std::uint64_t original_size = ctx.size;

        md5Update(ctx, PADDING, padding_length);

        // Restore original size (md5Update added padding_length to ctx.size).
        // Instead of mutating back, compute length bits directly.
        const std::uint64_t bit_len = original_size * 8;

        for (std::uint32_t j = 0; j < 14; ++j) {
            block[j] = static_cast<std::uint32_t>(ctx.input[j * 4 + 3]) << 24
                | static_cast<std::uint32_t>(ctx.input[j * 4 + 2]) << 16
                | static_cast<std::uint32_t>(ctx.input[j * 4 + 1]) << 8
                | static_cast<std::uint32_t>(ctx.input[j * 4]);
        }

        block[14] = static_cast<std::uint32_t>(bit_len);
        block[15] = static_cast<std::uint32_t>(bit_len >> 32);

        md5Step(ctx.buffer, block);

        for (std::uint32_t i = 0; i < 4; ++i) {
            ctx.digest[i * 4 + 0] = static_cast<std::uint8_t>(ctx.buffer[i] & 0x000000FFu);
            ctx.digest[i * 4 + 1] = static_cast<std::uint8_t>((ctx.buffer[i] & 0x0000FF00u) >> 8);
            ctx.digest[i * 4 + 2] = static_cast<std::uint8_t>((ctx.buffer[i] & 0x00FF0000u) >> 16);
            ctx.digest[i * 4 + 3] = static_cast<std::uint8_t>((ctx.buffer[i] & 0xFF000000u) >> 24);
        }
    }

    std::string
    dataToString(const std::uint8_t* digest, std::size_t size)
    {
        static constexpr char HEX[] = "0123456789abcdef";
        std::string out;
        out.reserve(size * 2);
        for (std::size_t i = 0; i < size; ++i) {
            out.push_back(HEX[digest[i] >> 4]);
            out.push_back(HEX[digest[i] & 0x0F]);
        }
        return out;
    }

    std::string
    hashMemory(const std::uint8_t* data, std::size_t size)
    {
        MD5Context ctx { };
        md5Init(ctx);
        md5Update(ctx, data, size);
        md5Finalize(ctx);
        return dataToString(ctx.digest, sizeof(ctx.digest));
    }
} // namespace

//------------------

std::string
hashString(const std::string_view str)
{
    return hashMemory(reinterpret_cast<const std::uint8_t*>(str.data()), str.size());
}

//------------------

std::string
hashBytes(const std::uint8_t* data, const std::size_t size)
{
    return hashMemory(data, size);
}

//------------------

std::string
hashBytes(const std::span<const std::uint8_t> data)
{
    return hashMemory(data.data(), data.size());
}

//------------------

std::optional<std::string>
hashFile(const std::filesystem::path& filename)
{
    std::ifstream fs { filename, std::ios::binary };
    if (!fs.is_open()) [[unlikely]]
        return std::nullopt;

    MD5Context ctx { };
    md5Init(ctx);

    std::array<char, 256> buffer { };
    while (fs) {
        fs.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize bytes_read = fs.gcount();
        if (bytes_read > 0) {
            md5Update(ctx,
                reinterpret_cast<const std::uint8_t*>(buffer.data()),
                static_cast<std::size_t>(bytes_read));
        }
    }

    md5Finalize(ctx);
    return dataToString(ctx.digest, sizeof(ctx.digest));
}

//------------------

std::optional<std::string>
hashFile(const std::filesystem::path& filename,
    const std::size_t start_bytes,
    const std::size_t end_bytes)
{
    if (start_bytes > end_bytes) [[unlikely]]
        return std::nullopt;

    std::ifstream fs { filename, std::ios::binary };
    if (!fs.is_open()) [[unlikely]]
        return std::nullopt;

    fs.seekg(0, std::ios::end);
    const auto file_length = static_cast<std::streamoff>(fs.tellg());
    if (file_length < 0 || static_cast<std::uintmax_t>(end_bytes) > static_cast<std::uintmax_t>(file_length))
        [[unlikely]]
        return std::nullopt;

    fs.seekg(static_cast<std::streamoff>(start_bytes), std::ios::beg);

    MD5Context ctx { };
    md5Init(ctx);

    std::size_t remaining = end_bytes - start_bytes;
    std::array<char, 256> buffer { };

    while (remaining > 0) {
        const std::size_t to_read = std::min(remaining, buffer.size());
        fs.read(buffer.data(), static_cast<std::streamsize>(to_read));
        const std::streamsize bytes_read = fs.gcount();
        if (bytes_read <= 0) [[unlikely]]
            break; // unexpected EOF - return what we have (or could return nullopt)

        md5Update(ctx,
            reinterpret_cast<const std::uint8_t*>(buffer.data()),
            static_cast<std::size_t>(bytes_read));
        remaining -= static_cast<std::size_t>(bytes_read);
    }

    md5Finalize(ctx);
    return dataToString(ctx.digest, sizeof(ctx.digest));
}

//------------------

} // namespace kas::md5::v3
