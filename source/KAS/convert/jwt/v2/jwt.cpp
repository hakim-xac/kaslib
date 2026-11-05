// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include "KAS/convert/jwt/v2/jwt.h"

#include "KAS/convert/base64/base64.h"
#include "KAS/convert/sha256/sha256.h"
#include "KAS/utils/utils.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <numeric>
#include <ranges>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace kas::convert::jwt::v2 {

namespace {

    //------------------

    /**
     * @brief Computes HMAC-SHA256 of @p data using @p key (RFC 2104).
     *
     * Implements the standard HMAC construction with SHA-256 as the
     * underlying hash:
     *  - the key is normalized to a 64-byte block; keys longer than the block
     *    size are first hashed with SHA-256, shorter keys are zero-padded;
     *  - the key block is XORed with the inner pad (0x36) and the outer pad
     *    (0x5C);
     *  - the result is SHA-256(outer_pad || SHA-256(inner_pad || data)).
     *
     * The returned array is the raw 32-byte MAC (not hex-encoded).
     *
     * @param key  Secret key; may be shorter or longer than the 64-byte block.
     * @param data Message bytes to authenticate.
     * @return 32-byte HMAC-SHA256 tag.
     */
    std::array<std::uint8_t, 32>
    hmacSha256(const std::string& key, std::span<const std::uint8_t> data)
    {
        constexpr std::size_t BLOCK_SIZE = 64;

        std::array<std::uint8_t, BLOCK_SIZE> key_block { };

        if (key.size() <= BLOCK_SIZE) {
            std::copy(key.begin(), key.end(), key_block.begin());
        } else {
            const auto hashed_key = sha256::v2::digest(key);
            std::copy(hashed_key.begin(), hashed_key.end(), key_block.begin());
        }

        std::array<std::uint8_t, BLOCK_SIZE> inner_pad { };
        std::array<std::uint8_t, BLOCK_SIZE> outer_pad { };
        for (std::size_t i = 0; i < BLOCK_SIZE; ++i) {
            inner_pad[i] = key_block[i] ^ 0x36;
            outer_pad[i] = key_block[i] ^ 0x5C;
        }

        std::vector<std::uint8_t> inner_input;
        inner_input.reserve(BLOCK_SIZE + data.size());
        inner_input.insert(inner_input.end(), inner_pad.begin(), inner_pad.end());
        inner_input.insert(inner_input.end(), data.begin(), data.end());
        const auto inner_hash = sha256::v2::digest(inner_input);

        std::array<std::uint8_t, BLOCK_SIZE + 32> outer_input { };
        std::copy(outer_pad.begin(), outer_pad.end(), outer_input.begin());
        std::copy(inner_hash.begin(), inner_hash.end(), outer_input.begin() + BLOCK_SIZE);

        return sha256::v2::digest(outer_input);
    }

    /**
     * @brief Computes HMAC-SHA256 of a string message using a string key.
     *
     * Convenience overload for string inputs: reinterprets the bytes of
     * @p data as a byte span and forwards to the span-based
     * @ref hmacSha256(const std::string&, std::span<const std::uint8_t>).
     *
     * The returned array is the raw 32-byte MAC (not hex-encoded).
     *
     * @param key  Secret key.
     * @param data Message bytes (used as-is, including any embedded NULs).
     * @return 32-byte HMAC-SHA256 tag.
     */
    std::array<std::uint8_t, 32>
    hmacSha256(const std::string& key, const std::string& data)
    {
        const std::span sp { reinterpret_cast<const std::uint8_t*>(data.data()), data.size() };
        return hmacSha256(key, sp);
    }

    /**
     * @brief Appends a "\uXXXX" escape sequence for a 16-bit value.
     *
     * Writes a backslash, the letter 'u', and exactly four lowercase hex
     * digits encoding @p value (0..0xFFFF). Leading zeros are preserved, so
     * small values still produce a full four-digit sequence (for example,
     * 0x1B -> "\\u001b", 0x00 -> "\\u0000").
     *
     * This is the standard JSON/ECMAScript form for characters that have no
     * short escape (that is, control characters 0x00..0x1F other than the ones
     * with dedicated escapes like \b, \f, \n, \r, \t).
     *
     * @param out   Output string to append to.
     * @param value Value to encode; the type guarantees it fits in \uXXXX.
     */
    void appendUnicodeEscape(std::string& out, std::uint16_t value)
    {
        static constexpr char HEX_LOWER[] = "0123456789abcdef";
        char buf[6];
        buf[0] = '\\';
        buf[1] = 'u';
        buf[2] = HEX_LOWER[(value >> 12) & 0xF];
        buf[3] = HEX_LOWER[(value >> 8) & 0xF];
        buf[4] = HEX_LOWER[(value >> 4) & 0xF];
        buf[5] = HEX_LOWER[value & 0xF];
        out.append(buf, sizeof(buf));
    }

    /**
     * @brief Escapes a UTF-8 string for embedding into a JSON string literal.
     *
     * Produces the contents of a JSON string value (without the surrounding
     * double quotes), applying the escapes required by RFC 8259, section 7:
     *  - '"'  -> \"
     *  - '\\' -> \\
     *  - backspace, form feed, newline, carriage return, tab use their short
     *    forms (\b, \f, \n, \r, \t);
     *  - any other control character (bytes below 0x20) is emitted as a
     *    \u00XX escape via @ref appendUnicodeEscape.
     *
     * All other bytes are copied unchanged. Bytes >= 0x80 are passed through
     * as-is, which is correct when @p input is valid UTF-8 (the usual case for
     * JSON); the string is not re-encoded.
     *
     * @param input UTF-8 input to escape.
     * @return Escaped string suitable for use as a JSON string body.
     */
    std::string escapeJson(std::string_view input)
    {
        std::string out;
        out.reserve(input.size() + 8);

        std::size_t i = 0;
        const std::size_t n = input.size();

        while (i < n) {
            const auto uc = static_cast<unsigned char>(input[i]);
            switch (input[i]) {
            case '"':
                out += "\\\"";
                ++i;
                break;
            case '\\':
                out += "\\\\";
                ++i;
                break;
            case '\b':
                out += "\\b";
                ++i;
                break;
            case '\f':
                out += "\\f";
                ++i;
                break;
            case '\n':
                out += "\\n";
                ++i;
                break;
            case '\r':
                out += "\\r";
                ++i;
                break;
            case '\t':
                out += "\\t";
                ++i;
                break;
            default:
                if (uc < 0x20) {
                    appendUnicodeEscape(out, uc);
                    ++i;
                } else {
                    const std::size_t start = i;
                    while (i < n) {
                        const auto b = static_cast<unsigned char>(input[i]);
                        const char ch = input[i];
                        if (b < 0x20 || ch == '"' || ch == '\\')
                            break;
                        ++i;
                    }
                    out.append(input.data() + start, i - start);
                }
                break;
            }
        }
        return out;
    }

    /**
     * @brief Skips ASCII whitespace starting at position @p i.
     *
     * Advances @p i past any characters contained in the whitespace set
     * " \t\r\n" (space, tab, carriage return, line feed) as defined by
     * RFC 8259, section 2. Returns the index of the first non-whitespace
     * character, or @p s.size() if the rest of the input is whitespace.
     *
     * The function never reads past @p s.size(); it is noexcept and does no
     * allocations.
     *
     * @param s Input string view.
     * @param i Starting index (must be <= s.size()).
     * @return Index of the first non-whitespace character, or s.size().
     */
    std::size_t skipWs(std::string_view s, std::size_t i) noexcept
    {
        static constexpr std::string_view WS = " \t\r\n";
        while (i < s.size() && WS.find(s[i]) != std::string_view::npos)
            ++i;
        return i;
    }

    /**
     * @brief Parses a JSON string literal at position @p i.
     *
     * Expects @p s[i] to be the opening '"'. On success, decodes the string
     * body into @p out and advances @p i past the closing '"'.
     *
     * Supported escape sequences (RFC 8259, section 7): \" \\ \/ \b \f \n \r \t.
     * Unicode escapes (\uXXXX) and any other escapes are rejected: the function
     * returns false and the caller must treat the input as malformed. This is a
     * deliberate limitation for this parser (see the comment in the default
     * branch); if full JSON compliance is required, \uXXXX handling must be
     * added here.
     *
     * On failure @p out may contain a partially decoded prefix and @p i is
     * unspecified; on success @p out contains the decoded string (without the
     * surrounding quotes) and @p i points just past the closing '"'.
     *
     * @param s   Input JSON text.
     * @param i   In/out cursor: on entry must point at the opening quote; on
     *            success points just past the closing quote.
     * @param out Output string with the decoded value (cleared on entry).
     * @return true on success, false on a malformed string.
     */
    bool parseString(std::string_view s, std::size_t& i, std::string& out)
    {
        if (i >= s.size() || s[i] != '"')
            return false;
        ++i;
        out.clear();

        while (i < s.size() && s[i] != '"') {
            if (s[i] == '\\') {
                if (i + 1 >= s.size())
                    return false;
                const char esc = s[i + 1];
                switch (esc) {
                case '"':
                    out += '"';
                    break;
                case '\\':
                    out += '\\';
                    break;
                case '/':
                    out += '/';
                    break;
                case 'b':
                    out += '\b';
                    break;
                case 'f':
                    out += '\f';
                    break;
                case 'n':
                    out += '\n';
                    break;
                case 'r':
                    out += '\r';
                    break;
                case 't':
                    out += '\t';
                    break;
                default:
                    return false; // \uXXXX is not supported in the payload
                }
                i += 2;
            } else {
                const std::size_t start = i;
                while (i < s.size() && s[i] != '"' && s[i] != '\\')
                    ++i;
                out.append(s.data() + start, i - start);
            }
        }

        if (i >= s.size())
            return false; // unterminated string
        ++i; // skip closing quote
        return true;
    }

    /**
     * @brief Parses a JSON-style integer at position @p i.
     *
     * Skips leading whitespace, then reads an optional sign and one or more
     * ASCII decimal digits. On success stores the value in @p out and advances
     * @p i past the last consumed digit.
     *
     * A leading '+' is not accepted (JSON-style numbers only allow an optional
     * '-'). Values outside int64_t range are rejected.
     *
     * On failure @p i is left just past the leading whitespace (no rollback);
     * callers that need to retry should save/restore @p i themselves.
     *
     * @param s   Input text.
     * @param i   In/out cursor.
     * @param out Parsed value (only set on success).
     * @return true on success, false on malformed input or overflow.
     */
    bool parseInteger(std::string_view s, std::size_t& i, std::int64_t& out)
    {
        i = skipWs(s, i);
        if (i >= s.size())
            return false;

        const std::size_t start = i;

        // JSON-style: optional '-', then digits. '+' is not accepted.
        if (s[i] == '-')
            ++i;

        const std::size_t digits_begin = i;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9')
            ++i;

        if (i == digits_begin)
            return false; // no digits after optional sign

        const auto parsed = utils::stringToInteger<std::int64_t>(s.substr(start, i - start));
        if (!parsed)
            return false;

        out = *parsed;
        return true;
    }

} // namespace

//------------------

std::string toJson(const JwtPayload& payload)
{
    std::string out = utils::stringify(
        "{",
        "\"sub\":\"", escapeJson(payload.subject), "\",",
        "\"name\":\"", escapeJson(payload.name), "\"");

    // Reserve room for both optional numeric fields so appends below
    // never reallocate.
    out.reserve(out.size() + 2 * (sizeof(",\"iat\":") + 20));

    if (payload.issued_at != 0) {
        if (const auto iat = utils::intToString(payload.issued_at)) {
            out += ",\"iat\":";
            out += *iat;
        }
    }
    if (payload.expires_at != 0) {
        if (const auto exp = utils::intToString(payload.expires_at)) {
            out += ",\"exp\":";
            out += *exp;
        }
    }

    out += '}';
    return out;
}

//------------------

kas::expected<JwtPayload>
fromJson(std::string_view json)
{
    JwtPayload p;
    std::size_t i = skipWs(json, 0);
    if (i >= json.size() || json[i] != '{')
        return kas::unexpected { "payload: expected '{'" };
    ++i;

    while (true) {
        i = skipWs(json, i);
        if (i >= json.size())
            return kas::unexpected { "payload: unexpected end" };
        if (json[i] == '}') {
            ++i;
            break;
        }

        std::string key;
        if (!parseString(json, i, key))
            return kas::unexpected { "payload: invalid key" };

        i = skipWs(json, i);
        if (i >= json.size() || json[i] != ':')
            return kas::unexpected { "payload: expected ':'" };
        ++i;
        i = skipWs(json, i);

        if (key == "sub") {
            if (!parseString(json, i, p.subject))
                return kas::unexpected { "payload: invalid 'sub'" };
        } else if (key == "name") {
            if (!parseString(json, i, p.name))
                return kas::unexpected { "payload: invalid 'name'" };
        } else if (key == "iat") {
            if (!parseInteger(json, i, p.issued_at))
                return kas::unexpected { "payload: invalid 'iat'" };
        } else if (key == "exp") {
            if (!parseInteger(json, i, p.expires_at))
                return kas::unexpected { "payload: invalid 'exp'" };
        } else {
            if (i < json.size() && json[i] == '"') {
                std::string dummy;
                if (!parseString(json, i, dummy))
                    return kas::unexpected { "payload: invalid unknown value" };
            } else {
                std::int64_t dummy;
                if (!parseInteger(json, i, dummy))
                    return kas::unexpected { "payload: invalid unknown value" };
            }
        }

        i = skipWs(json, i);
        if (i < json.size() && json[i] == ',') {
            ++i;
            continue;
        }
        if (i < json.size() && json[i] == '}') {
            ++i;
            break;
        }
        return kas::unexpected { "payload: expected ',' or '}'" };
    }

    return p;
}

//------------------

bool isExpired(const JwtPayload& payload) noexcept
{
    using namespace std::chrono;

    if (payload.expires_at == 0)
        return false;

    const auto now = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
    return now > payload.expires_at;
}

//------------------
//------------------

Jwt::Jwt(std::string secret)
    : m_secret(std::move(secret))
{
}

//------------------

std::string
Jwt::createToken(const JwtPayload& payload) const
{
    static constexpr std::string_view HEADER = R"({"alg":"HS256","typ":"JWT"})";

    const std::span header_sp { reinterpret_cast<const std::uint8_t*>(HEADER.data()), HEADER.size() };

    const auto json = toJson(payload);
    const std::span payload_sp { reinterpret_cast<const std::uint8_t*>(json.data()), json.size() };

    auto data_to_sign = utils::stringify(base64::encode(header_sp), ".", base64::encode(payload_sp));
    const auto signature = hmacSha256(m_secret, data_to_sign);

    return utils::stringify(std::move(data_to_sign), ".", base64::encode(signature));
}

//------------------

std::string
Jwt::createToken(
    std::string_view subject,
    std::string_view name,
    std::chrono::seconds ttl) const
{
    using namespace std::chrono;
    const auto now = duration_cast<std::chrono::seconds>(system_clock::now().time_since_epoch()).count();

    const JwtPayload p {
        .subject = std::string(subject),
        .name = std::string(name),
        .issued_at = now,
        .expires_at = ttl.count() > 0 ? now + ttl.count() : 0
    };
    return createToken(p);
}

//------------------

kas::expected<JwtPayload>
Jwt::verify(std::string_view token) const
{
    const auto first = token.find('.');
    if (first == std::string_view::npos)
        return kas::unexpected { "Invalid token format" };

    const auto second = token.find('.', first + 1);
    if (second == std::string_view::npos)
        return kas::unexpected { "Invalid token format" };

    if (token.find('.', second + 1) != std::string_view::npos)
        return kas::unexpected { "Invalid token format" };

    const std::string_view encoded_header = token.substr(0, first);
    const std::string_view encoded_payload = token.substr(first + 1, second - first - 1);
    const std::string_view encoded_signature = token.substr(second + 1);

    const std::string data_to_sign { utils::stringify(encoded_header, ".", encoded_payload) };
    const auto expected_sig = hmacSha256(m_secret, data_to_sign);
    const std::string recomputed = base64::encode(expected_sig);

    if (recomputed.size() != encoded_signature.size())
        return kas::unexpected { "Invalid signature" };

    unsigned diff = 0;
    for (std::size_t i = 0; i < recomputed.size(); ++i)
        diff |= static_cast<unsigned>(recomputed[i] ^ encoded_signature[i]);

    if (diff != 0)
        return kas::unexpected { "Invalid signature" };

    const auto decoded = base64::decode(std::string(encoded_payload));
    if (!decoded)
        return kas::unexpected { "Failed to decode payload" };

    const std::string json(decoded.value().begin(), decoded.value().end());
    return fromJson(json);
}

//------------------

kas::expected<JwtPayload>
Jwt::verifyAndValidate(std::string_view token) const
{
    auto result = verify(token);
    if (!result)
        return result;
    if (isExpired(*result))
        return kas::unexpected { "Token has expired" };
    return result;
}

//------------------

} // namespace kas::convert::jwt::v2