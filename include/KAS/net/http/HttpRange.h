/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_NET_HTTP_HTTP_RANGE_H
#define KASLIBCXX_NET_HTTP_HTTP_RANGE_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kas::net::http {

/**
 * @brief A single byte range as found in a Range header.
 *
 * Represents one of the forms "start-end", "start-", or "-suffix":
 * - empty @c start -> suffix range ("-N" means the last N bytes);
 * - empty @c end   -> open-ended range ("N-" means from byte N to the end).
 *
 * Both fields may be set, or exactly one of them for the open-ended and
 * suffix forms.
 */
struct ByteRange {
    std::optional<std::size_t> start; ///< First byte of the range, or empty for a suffix range.
    std::optional<std::size_t> end; ///< Last byte of the range (inclusive), or empty for open-ended ranges.
};

/**
 * @brief A parsed Range header.
 *
 * Holds the range unit and the list of parsed byte ranges.
 *
 * @note If @c valid is false, the header was present but syntactically
 *       invalid; the server should respond with 400 Bad Request or ignore
 *       the header.
 */
struct RangeSpec {
    std::string unit; ///< Range unit, currently only "bytes".
    std::vector<ByteRange> ranges; ///< Parsed ranges in the order they appear.
    bool valid = false; ///< Whether the header parsed successfully.
};

/**
 * @brief Parses a Range request header value.
 *
 * Accepts the byte-range form described by RFC 9110, section 14.1.2,
 * for example:
 *     bytes=0-99,200-,300-400
 *
 * Only the "bytes" unit is supported. Each range may be:
 *  - "start-end"  (closed range, start <= end);
 *  - "start-"     (open-ended range, up to the end of the resource);
 *  - "-suffix"    (last N bytes of the resource).
 *
 * Optional whitespace (space, tab) around each range is allowed and is
 * ignored. Empty ranges, missing '-', a range with neither start nor
 * end, and a reversed range (start > end) make the whole header
 * invalid; in that case a RangeSpec with valid == false is returned so
 * the caller can respond with 400 Bad Request (or ignore the header).
 *
 * @param value Raw Range header value.
 * @return A RangeSpec on success, std::nullopt if the value does not
 *         start with a supported unit (for example, not "bytes="), or a
 *         RangeSpec with valid == false on a syntax error inside the
 *         range list.
 */
[[nodiscard]] std::optional<RangeSpec> parseRangeHeader(std::string_view value) noexcept;

/**
 * @brief Resolves a single byte range against the total resource size.
 *
 * Converts a ByteRange (as produced by parseRangeHeader) into a
 * concrete (start, length) pair for a resource of @p total bytes.
 *
 * Supported forms:
 *  - "start-end": end is clamped to total - 1;
 *  - "start-"   : runs from start to the end of the resource;
 *  - "-suffix"  : the last @c suffix bytes of the resource.
 *
 * A range is unsatisfiable and yields std::nullopt when:
 *  - @p total is 0;
 *  - a suffix range has length 0;
 *  - start >= total;
 *  - end < start.
 *
 * @param range Byte range to resolve.
 * @param total Total size of the resource in bytes.
 * @return The resolved (start, length) pair, or std::nullopt if the
 *         range is not satisfiable.
 */
[[nodiscard]] std::optional<std::pair<std::size_t, std::size_t>>
resolveByteRange(const ByteRange& range, std::size_t total) noexcept;

/**
 * @brief Resolves all byte ranges from a parsed RangeSpec.
 *
 * Iterates over @p spec.ranges and resolves each one against @p total
 * via resolveByteRange(). Unsatisfiable ranges are dropped; the
 * remaining ones are returned in the original order.
 *
 * Returns std::nullopt when:
 *  - @p spec is invalid (valid == false);
 *  - @p spec has no ranges;
 *  - @p total is 0;
 *  - after filtering, no satisfiable ranges remain. In this case the
 *    server should respond with 416 Range Not Satisfiable.
 *
 * @param spec  Parsed Range header.
 * @param total Total size of the resource in bytes.
 * @return A vector of (start, length) pairs on success, or std::nullopt
 *         as described above.
 */
[[nodiscard]] std::optional<std::vector<std::pair<std::size_t, std::size_t>>>
resolveByteRanges(const RangeSpec& spec, std::size_t total) noexcept;

/**
 * @brief Builds a multipart/byteranges body.
 *
 * Serializes each (start, length) range from @p ranges as a separate
 * part of a multipart/byteranges body, using @p boundary as the
 * boundary string. Each part contains:
 *  - "Content-Type: <content_type>" (omitted if @p content_type is
 *    empty);
 *  - "Content-Range: bytes <start>-<end>/<total>";
 *  - the corresponding slice of @p body;
 * and is delimited by "--<boundary>", with the final closing delimiter
 * "--<boundary>--".
 *
 * @p ranges must have been produced by resolveByteRanges() and must be
 * satisfiable for @p body: for each (start, length) pair,
 * start + length <= body.size().
 *
 * @param boundary     Boundary string (without quotes).
 * @param body         Original full body.
 * @param ranges       Ranges produced by resolveByteRanges().
 * @param content_type Content-Type of the original body (repeated in
 *                     each part); may be empty to omit the header.
 * @param total        Total size of the original body in bytes.
 * @return The complete multipart/byteranges body.
 */
[[nodiscard]] std::string
buildMultipartByteranges(
    std::string_view boundary,
    std::string_view body,
    const std::vector<std::pair<std::size_t, std::size_t>>& ranges,
    std::string_view content_type,
    std::size_t total);

/**
 * @brief Generates a random boundary string for multipart/byteranges.
 *
 * Produces 16 random bytes encoded as 32 lowercase hex characters. The
 * result is suitable for use as the boundary value in a
 * "Content-Type: multipart/byteranges; boundary=..." header.
 *
 * The boundary is chosen so that it is very unlikely to occur inside
 * the payload; RFC 9110, section 14.6 requires the boundary to not
 * appear in the encapsulated parts.
 *
 * @return A 32-character lowercase hex boundary string.
 */
[[nodiscard]] std::string makeBoundary();

} // namespace kas::net::http

#endif // KASLIBCXX_NET_HTTP_HTTP_RANGE_H