// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include "KAS/net/http/HttpRange.h"

#include <array>
#include <charconv>
#include <cstdint>
#include <random>

namespace kas::net::http {

namespace {

    /**
     * @brief Parses a decimal unsigned integer from @p s.
     *
     * Uses std::from_chars, which does not allocate and does not throw.
     * The entire input must be consumed: trailing characters, an empty
     * string, or a value that does not fit into std::size_t make the
     * function return std::nullopt.
     *
     * @param s Input string.
     * @return The parsed value, or std::nullopt on empty/invalid input or
     *         overflow.
     */
    std::optional<std::size_t> parseSize(std::string_view s) noexcept
    {
        if (s.empty())
            return std::nullopt;
        std::size_t v = 0;
        const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
        if (ec != std::errc { } || ptr != s.data() + s.size())
            return std::nullopt;
        return v;
    }

} // namespace

//------------------

std::optional<RangeSpec>
parseRangeHeader(std::string_view value) noexcept
{
    // Example: "bytes=0-99,200-,300-400"
    const auto eq = value.find('=');
    if (eq == std::string_view::npos)
        return std::nullopt;

    const auto unit = value.substr(0, eq);
    if (unit != "bytes")
        return std::nullopt; // only "bytes" is supported for now

    RangeSpec spec;
    spec.unit = std::string { unit };
    spec.valid = true;

    std::string_view rest = value.substr(eq + 1);
    // Empty range list ("bytes=") - syntax error.
    if (rest.empty()) {
        spec.valid = false;
        return spec;
    }

    while (!rest.empty()) {
        const auto comma = rest.find(',');
        std::string_view part = (comma == std::string_view::npos)
            ? rest
            : rest.substr(0, comma);

        // Trim optional whitespace (RFC 9110 allows OWS around ranges).
        while (!part.empty() && (part.front() == ' ' || part.front() == '\t'))
            part.remove_prefix(1);
        while (!part.empty() && (part.back() == ' ' || part.back() == '\t'))
            part.remove_suffix(1);

        if (part.empty()) {
            spec.valid = false;
            return spec;
        }

        const auto dash = part.find('-');
        if (dash == std::string_view::npos) {
            spec.valid = false;
            return spec;
        }

        const auto start_str = part.substr(0, dash);
        const auto end_str = part.substr(dash + 1);

        ByteRange r;
        if (!start_str.empty()) {
            auto s = parseSize(start_str);
            if (!s) {
                spec.valid = false;
                return spec;
            }
            r.start = *s;
        }
        if (!end_str.empty()) {
            auto e = parseSize(end_str);
            if (!e) {
                spec.valid = false;
                return spec;
            }
            r.end = *e;
        }

        // "bytes=-" - neither start nor end: invalid.
        if (!r.start && !r.end) {
            spec.valid = false;
            return spec;
        }

        // "bytes=5-3" - start > end: invalid.
        if (r.start && r.end && *r.start > *r.end) {
            spec.valid = false;
            return spec;
        }

        spec.ranges.push_back(r);

        if (comma == std::string_view::npos)
            break;
        rest = rest.substr(comma + 1);

        // "bytes=0-99," - trailing comma without a range -> invalid.
        if (rest.empty()) {
            spec.valid = false;
            return spec;
        }
    }

    return spec;
}

//------------------

std::optional<std::pair<std::size_t, std::size_t>>
resolveByteRange(const ByteRange& range, std::size_t total) noexcept
{
    if (total == 0)
        return std::nullopt;

    // "bytes=-N": the last N bytes.
    if (!range.start) {
        // range.end is guaranteed to be set (parseRangeHeader checks it).
        if (!range.end || *range.end == 0)
            return std::nullopt;
        const std::size_t suffix = *range.end;
        const std::size_t start = (suffix >= total) ? 0 : (total - suffix);
        return std::pair { start, total - start };
    }

    // range.start is set.
    const std::size_t start = *range.start;
    if (start >= total)
        return std::nullopt;

    std::size_t end = total - 1;
    if (range.end && *range.end < total)
        end = *range.end;

    if (end < start)
        return std::nullopt;

    return std::pair { start, end - start + 1 };
}

//------------------

std::optional<std::vector<std::pair<std::size_t, std::size_t>>>
resolveByteRanges(const RangeSpec& spec, std::size_t total) noexcept
{
    if (!spec.valid || spec.ranges.empty() || total == 0)
        return std::nullopt;

    std::vector<std::pair<std::size_t, std::size_t>> result;
    result.reserve(spec.ranges.size());

    for (const auto& r : spec.ranges) {
        if (auto resolved = resolveByteRange(r, total))
            result.push_back(*resolved);
    }

    if (result.empty())
        return std::nullopt; // all ranges unsatisfiable -> 416

    return result;
}

//------------------

std::string makeBoundary()
{
    // 16 random bytes encoded as hex; unlikely to collide with content.
    std::array<std::uint8_t, 16> bytes { };
    std::random_device rd;
    for (auto& b : bytes)
        b = static_cast<std::uint8_t>(rd());

    static constexpr char hex[] = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (auto b : bytes) {
        out.push_back(hex[b >> 4]);
        out.push_back(hex[b & 0x0F]);
    }
    return out;
}

//------------------

std::string buildMultipartByteranges(
    std::string_view boundary,
    std::string_view body,
    const std::vector<std::pair<std::size_t, std::size_t>>& ranges,
    std::string_view content_type,
    std::size_t total)
{
    std::string out;
    // Rough estimate: ~200 bytes of overhead per part + the payload.
    std::size_t est = 64;
    for (const auto& [s, l] : ranges)
        est += l + 200;
    out.reserve(est);

    const std::string total_str = std::to_string(total);

    for (const auto& [start, len] : ranges) {
        out += "--";
        out += boundary;
        out += "\r\n";
        if (!content_type.empty()) {
            out += "Content-Type: ";
            out += content_type;
            out += "\r\n";
        }
        out += "Content-Range: bytes ";
        out += std::to_string(start);
        out += "-";
        out += std::to_string(start + len - 1);
        out += "/";
        out += total_str;
        out += "\r\n\r\n";
        out.append(body.data() + start, len);
        out += "\r\n";
    }

    out += "--";
    out += boundary;
    out += "--\r\n";

    return out;
}

//------------------

} // namespace kas::net::http