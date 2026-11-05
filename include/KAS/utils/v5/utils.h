/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_UTILS_V5_H
#define KASLIBCXX_UTILS_V5_H

#include <KAS/concepts/concepts.h>
#include <KAS/expected/expected.h>

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <chrono>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/**
 * @brief General-purpose utility functions: strings, numbers, time, files.
 *
 * This header is a grab-bag of small, mostly standalone helpers. Unless
 * stated otherwise:
 *  - functions that return std::string_view return views into their input
 *    arguments, so the caller must keep the input alive;
 *  - string_views accepted as input are treated as raw bytes (no encoding
 *    assumptions beyond ASCII where explicitly mentioned);
 *  - functions marked noexcept do not throw (except for std::bad_alloc,
 *    which is treated as fatal).
 */
namespace kas::utils::v5 {

// ============================================================================
// splitString
// ============================================================================

/**
 * @brief Splits a string into tokens separated by @p delimiter.
 *
 * Empty tokens are skipped. If @p delimiter is empty, returns an empty
 * vector (no infinite loop).
 *
 * The returned views point into @p str; @p str must outlive the vector.
 *
 * @param str       Input string.
 * @param delimiter Separator substring (must be non-empty to produce any
 *                  tokens).
 * @return Vector of views into @p str (no allocation for the token bytes
 *         themselves; only the vector storage is allocated).
 */
[[nodiscard]] std::vector<std::string_view>
splitString(std::string_view str, std::string_view delimiter);

/**
 * @brief Returns the token at @p index among the non-empty tokens.
 *
 * If @p delimiter is empty, returns an error. If @p index is out of range,
 * returns an error.
 *
 * The returned view points into @p str; @p str must outlive the view.
 *
 * @param str       Input string.
 * @param delimiter Separator substring.
 * @param index     Zero-based index of the token to return.
 * @return On success, a view into @p str; on failure (empty delimiter or
 *         index out of range), an error value.
 */
[[nodiscard]] kas::expected<std::string_view>
splitString(std::string_view str, std::string_view delimiter, std::size_t index) noexcept;

/**
 * @brief Invokes @p callable for each non-empty token.
 *
 * If @p delimiter is empty, this is a no-op (no infinite loop).
 *
 * @tparam Callable Callable type; must be invocable with std::string_view.
 * @param str      Input string.
 * @param delimiter Separator substring.
 * @param callable Callback invoked for each non-empty token.
 */
template <std::invocable<std::string_view> Callable>
void splitString(std::string_view str, std::string_view delimiter,
    Callable&& callable) noexcept(std::is_nothrow_invocable_v<Callable, std::string_view>);

/**
 * @brief Splits by a set of delimiter characters returned by a callable.
 *
 * Each character in the returned set is treated as a separate delimiter
 * (uses find_first_of). If the callable returns an empty set, this is a
 * no-op.
 *
 * @tparam SeparatorCallable Callable returning a string_view of delimiters.
 * @tparam FuncCallable      Callable invoked for each non-empty token.
 * @param str                Input string.
 * @param delimiter_callable Callable producing the delimiter set.
 * @param func_callable      Callback invoked for each non-empty token.
 */
template <std::invocable SeparatorCallable,
    std::invocable<std::string_view> FuncCallable>
void splitString(std::string_view str, SeparatorCallable&& delimiter_callable, FuncCallable&& func_callable);

/**
 * @brief Checks whether @p token appears as a comma-separated item in @p header.
 *
 * Matching is exact (case-insensitive ASCII via iequals) and trims optional
 * whitespace (space, tab) around each item; the caller does not need to
 * pre-trim.
 */
bool containsToken(std::string_view header, std::string_view token) noexcept;

/**
 * @brief Case-insensitive ASCII comparison of two strings.
 *
 * Only ASCII letters are folded; non-ASCII bytes are compared as-is.
 */
bool iequals(std::string_view a, std::string_view b) noexcept;

// ============================================================================
// stringify / joinStr / joinString
// ============================================================================

/**
 * @brief Concatenates all arguments into a single string (no separator).
 *
 * Performs one size pass over the arguments, reserves the exact total
 * length, and appends each argument as a string_view (no temporary
 * std::string per argument).
 *
 * Arguments are taken by const reference, so temporaries such as
 * std::to_string(...) stay alive for the duration of the call and are
 * safe to view.
 *
 * @tparam Ts Argument types; each must be convertible to std::string_view.
 * @param vals Values to concatenate.
 * @return Concatenated string.
 */
template <std::convertible_to<std::string_view>... Ts>
[[nodiscard]] std::string stringify(const Ts&... vals);

/**
 * @brief Joins arguments with @p separator (fold expression, no recursion).
 *
 * Requires at least one argument; a separator alone is not a valid call.
 *
 * @tparam Arg  First argument type (convertible to std::string_view).
 * @tparam Args Remaining argument types (convertible to std::string_view).
 * @param separator String inserted between arguments.
 * @param arg       First value.
 * @param args      Remaining values.
 * @return Joined string.
 */
template <std::convertible_to<std::string_view> Arg,
    std::convertible_to<std::string_view>... Args>
[[nodiscard]] std::string joinStr(std::string_view separator, Arg&& arg, Args&&... args);

/**
 * @brief Joins non-empty elements of a range with @p delimiter.
 *
 * Elements must be convertible to std::string_view (and support empty()).
 * Empty elements are skipped entirely (no doubled delimiters).
 *
 * @tparam Iter Iterator type over elements convertible to std::string_view.
 * @param begin     Range begin.
 * @param end       Range end.
 * @param delimiter String inserted between non-empty elements.
 * @return Joined string.
 */
template <kas::concepts::input_or_output_iterator_concept Iter>
[[nodiscard]] std::string joinString(Iter begin, Iter end, std::string delimiter);

// ============================================================================
// isRange / clamp
// ============================================================================

/**
 * @brief Checks whether @p value lies within [left, right].
 *
 * @return true if left <= value <= right, false otherwise.
 */
template <kas::concepts::is_integral T>
[[nodiscard]] constexpr bool isRange(T value, T left, T right) noexcept;

/**
 * @brief Deprecated alias for isRange. Returns bool, NOT a clamp.
 *
 * Kept for backward compatibility; use isRange instead.
 */
template <kas::concepts::is_integral T>
[[nodiscard, deprecated("use isRange")]]
constexpr bool clamp(T value, T left, T right) noexcept;

// ============================================================================
// Time
// ============================================================================

/**
 * @brief Formats a system_clock time point using strftime-style format.
 *
 * @param time_point Time point to format.
 * @param format     strftime format string.
 * @return On success, the formatted string; on failure (invalid format or
 *         conversion error), std::nullopt.
 */
[[nodiscard]] std::optional<std::string>
convertTimePointToString(
    const std::chrono::system_clock::time_point& time_point,
    const std::string& format = "%Y-%m-%d %H:%M:%S");

/**
 * @brief Returns the current (or given) system time as a formatted string.
 *
 * Thin wrapper over convertTimePointToString: returns an empty string when
 * conversion fails.
 *
 * @param time_point Time point to format; defaults to now.
 * @param format     strftime format string.
 * @return Formatted string, or an empty string on failure.
 */
[[nodiscard]] std::string systemTimeAsString(
    std::chrono::system_clock::time_point time_point = std::chrono::system_clock::now(),
    const std::string& format = "%Y-%m-%d %H:%M:%S");

/**
 * @brief Converts a filesystem file time to a human-readable string.
 *
 * Returns the time as a decimal number of seconds since the Unix epoch,
 * converted through system_clock.
 */
[[nodiscard]] std::string fileTimeToString(const std::filesystem::file_time_type& ftime);

/**
 * @brief Parses a date/time string into a system_clock time point.
 *
 * @param date_time_str Input string.
 * @param zone_offset   Offset added to the parsed time to get UTC.
 * @param format        strftime format string used for parsing.
 * @return On success, the parsed time point; on failure (parse error or
 *         out-of-range value), std::nullopt.
 */
[[nodiscard]] std::optional<std::chrono::system_clock::time_point>
stringToSystemTimePoint(
    const std::string& date_time_str,
    std::chrono::minutes zone_offset = std::chrono::minutes { },
    std::string format = "%Y-%m-%d %H:%M:%S");

// ============================================================================
// Files (bytes)
// ============================================================================

/**
 * @brief Writes a string to a file, overwriting it.
 *
 * Opens the file in text mode (no std::ios::binary), so on Windows the
 * usual newline translation applies. This matches fileGetContents, which
 * also opens in text mode, making the pair symmetric.
 *
 * @param filename Path to the file.
 * @param str      String to write.
 * @return true on success; false on failure.
 */
[[nodiscard]] kas::expected<bool>
stringToFile(std::string_view filename, std::string_view str);

/**
 * @brief Reads a whole file into a byte vector.
 *
 * @param filename Path to the file.
 * @return On success, the file contents; on failure (open/read error),
 *         an error string (string_view over a static literal or over the
 *         provided filename).
 */
[[nodiscard]] kas::expected_string<std::vector<std::uint8_t>>
fileToBytesArray(std::string_view filename);

/**
 * @brief Reads a whole file into a byte vector (C-string path overload).
 */
[[nodiscard]] kas::expected_string<std::vector<std::uint8_t>>
fileToBytesArray(const char* filename);

/**
 * @brief Writes a byte span to a file, overwriting it.
 *
 * @param filename Path to the file.
 * @param data     Bytes to write.
 * @return true on success; false on failure.
 */
[[nodiscard]] kas::expected<bool>
byteArrayToFile(std::string_view filename, std::span<const std::uint8_t> data);

/**
 * @brief Copies the bytes of a string into a byte vector.
 *
 * No encoding conversion is performed; bytes are copied as-is.
 */
[[nodiscard]] std::vector<std::uint8_t>
stringToBytes(std::string_view data);

/**
 * @brief Builds a string from a byte span.
 *
 * No encoding conversion is performed; bytes are copied as-is (including
 * any embedded NULs).
 */
[[nodiscard]] std::string
bytesAsString(std::span<const std::uint8_t> data);

/**
 * @brief Prints a byte span as hex, with a delimiter every
 *        @p count_on_delimiter bytes.
 *
 * @param out                Output stream.
 * @param data_sp            Bytes to print.
 * @param count_on_delimiter Number of bytes between delimiters; 0 disables
 *                           delimiters.
 */
void printRawData(std::ostream& out, std::span<const std::uint8_t> data_sp,
    std::size_t count_on_delimiter = 8);

// ============================================================================
// Files (string)
// ============================================================================

/**
 * @brief Reads a whole text file into a string.
 *
 * Canonical overload; the other overloads delegate to this one.
 *
 * @param filename Path to the file.
 * @return On success, the file contents; on failure, an error string.
 */
[[nodiscard]] kas::expected_string<std::string>
fileGetContents(const std::filesystem::path& filename);

/**
 * @brief Reads a whole text file into a string (C-string path overload).
 */
[[nodiscard]] kas::expected_string<std::string>
fileGetContents(const char* filename);

/**
 * @brief Reads a whole text file into a string (std::string path overload).
 */
[[nodiscard]] kas::expected_string<std::string>
fileGetContents(const std::string& filename);

// ============================================================================
// Processes
// ============================================================================

/**
 * @brief Runs a shell command and captures its standard output.
 *
 * The command is executed via popen(cmd, "r"). On success, the captured
 * stdout is returned with a single trailing '\n' removed (if present).
 * A non-zero exit status, termination by signal, popen/pclose failure, or
 * read error is reported as an error.
 *
 * @param cmd Command line to execute (must be NUL-terminated internally;
 *            the string is passed to popen as-is).
 * @return On success, the captured output; on failure, an error string.
 */
[[nodiscard]] kas::expected_string<std::string>
exec(const std::string& cmd);

// ============================================================================
// Strings / numbers
// ============================================================================

/**
 * @brief Parses a hexadecimal string (with optional 0x/0X prefix) into an
 *        integer.
 *
 * The whole input must be consumed; trailing characters make the parse fail.
 * Overflow makes the parse fail.
 *
 * @tparam T Integral result type.
 * @param hex Input string.
 * @return Parsed value, or std::nullopt if the input is empty or invalid.
 */
template <kas::concepts::is_integral T>
[[nodiscard]] std::optional<T> hexToInteger(std::string_view hex);

/**
 * @brief Parses an integer from a string in the given base.
 *
 * The whole input must be consumed; trailing characters make the parse fail.
 *
 * @tparam To Integral result type.
 * @param number_as_str Input string.
 * @param base          Numeric base (2..36).
 * @return On success, the parsed value; on failure (invalid input or
 *         out-of-range), an error value.
 */
template <kas::concepts::is_integral To>
[[nodiscard]] kas::expected<To> stringToInteger(std::string_view number_as_str,
    int base = 10);

/**
 * @brief Converts an integral value to its decimal string representation.
 *
 * Uses std::to_chars into a stack buffer, so no heap allocation happens
 * during conversion; the only allocation is the final std::string built
 * on success. On failure, an unexpected holding an error message is
 * returned; the error type does not allocate beyond the message literal.
 *
 * The function accepts any type satisfying std::is_integral_v<T>. Enums
 * are not supported directly; cast to the underlying integral type at
 * the call site if needed.
 *
 * @tparam T Integral type (std::is_integral_v<T> must hold).
 * @param value Value to convert.
 * @return Decimal string on success, or an error message on failure.
 *         std::to_chars reports std::errc::value_too_large when the
 *         buffer is too small, which cannot happen with a 32-byte buffer
 *         for any 64-bit (or narrower) integral type; the check is kept
 *         for robustness and for potential future wider types.
 */
template <kas::concepts::is_integral T>
[[nodiscard]]
kas::expected<std::string>
intToString(T value);

/**
 * @brief Checks whether every element in a range parses as an integer of
 *        type To.
 *
 * @tparam To   Integral target type.
 * @tparam Iter Iterator type over std::string_view elements.
 * @return true if all elements parse successfully.
 *
 * @note This function is noexcept: stringToInteger<T> uses
 *       std::from_chars and returns expected<T, std::string_view>,
 *       so neither parsing nor error construction can throw.
 */
template <kas::concepts::is_integral To,
    kas::concepts::input_or_output_iterator_concept Iter>
[[nodiscard]] bool checkRangeElemAsInteger(Iter begin, Iter end) noexcept;

/**
 * @brief Converts an unsigned integer to a hex string with optional 0x
 *        prefix, width and case.
 *
 * @p max_setw sets the minimum number of hex digits; the value is zero-padded
 * on the left to that width. The "0x" prefix (if requested) is added before
 * the digits and does not count towards the width. If the number needs more
 * than @p max_setw digits, no truncation occurs.
 *
 * The parameter type is std::uintmax_t (the widest addressable unsigned
 * integer), so the function accepts values of any unsigned integral type
 * without narrowing.
 *
 * Parameters after @p num are ordered by expected frequency of use:
 * case first (most call sites care about it), then prefix, then width.
 *
 * @param num           Value to convert.
 * @param is_upper_case If true, use uppercase hex digits.
 * @param is_prefix     If true, prepend "0x".
 * @param max_setw      Minimum digit width (zero-padded); clamped to 1024.
 * @return Formatted hexadecimal string.
 */
[[nodiscard]] std::string integerToHEX(std::uintmax_t num,
    bool is_upper_case = false,
    bool is_prefix = false,
    std::size_t max_setw = 4);

/**
 * @brief Checks whether a string consists only of ASCII digits.
 *
 * An empty string returns true (vacuously).
 */
[[nodiscard]] constexpr bool isStrAsDigit(std::string_view str) noexcept;

/**
 * @brief Returns a lowercase copy of the input string (ASCII only).
 *
 * Non-ASCII bytes are copied as-is.
 */
[[nodiscard]] std::string toLower(std::string data);

/**
 * @brief Returns an uppercase copy of the input string (ASCII only).
 *
 * Non-ASCII bytes are copied as-is.
 */
[[nodiscard]] std::string toUpper(std::string data);

/**
 * @brief Generates a random ASCII string of the given length.
 *
 * The exact alphabet and distribution are implementation-defined; the
 * result is suitable for non-cryptographic uses unless documented otherwise.
 */
[[nodiscard]] std::string randomString(std::size_t length);

/**
 * @brief Trims leading whitespace (or @p delimiter characters).
 *
 * With the default delimiter ' ', whitespace is any of space, \t, \n, \v,
 * \f, \r. With a non-space delimiter, only that character is trimmed.
 *
 * The returned view points into @p str; @p str must outlive the view.
 */
template <char delimiter = ' '>
[[nodiscard]] std::string_view ltrim(std::string_view str) noexcept;

/**
 * @brief Trims trailing whitespace (or @p delimiter characters).
 *
 * Same delimiter semantics as ltrim.
 */
template <char delimiter = ' '>
[[nodiscard]] std::string_view rtrim(std::string_view str) noexcept;

/**
 * @brief Trims both leading and trailing whitespace (or @p delimiter
 *        characters).
 *
 * Equivalent to ltrim<delimiter>(rtrim<delimiter>(str)).
 */
template <char delimiter = ' '>
[[nodiscard]] std::string_view trim(std::string_view str) noexcept;

} // namespace kas::utils::v5

// #################################
//           DEFINITION
// #################################

namespace kas::utils::v5 {

// ============================================================================
// joinString
// ============================================================================

template <kas::concepts::input_or_output_iterator_concept Iter>
[[nodiscard]] std::string
joinString(Iter begin, Iter end, std::string delimiter)
{
    std::string result;
    bool first = true;

    for (auto it = begin; it != end; ++it) {
        if (it->empty())
            continue;
        if (!first)
            result += delimiter;
        result += *it;
        first = false;
    }

    return result;
}

// ============================================================================
// stringify / joinStr
// ============================================================================

template <std::convertible_to<std::string_view>... Ts>
[[nodiscard]] std::string stringify(const Ts&... vals)
{
    const std::array<std::string_view, sizeof...(Ts)> views { std::string_view { vals }... };

    std::size_t len = 0;
    for (const auto& v : views)
        len += v.size();

    std::string result;
    result.reserve(len);

    for (const auto& v : views)
        result += v;

    return result;
}

// Implementation note: fold expression over the remaining arguments, so
// the code expands to a straight-line sequence of appends (no recursion).
template <std::convertible_to<std::string_view> Arg,
    std::convertible_to<std::string_view>... Args>
[[nodiscard]] std::string
joinStr([[maybe_unused]] std::string_view separator, Arg&& arg, Args&&... args)
{
    std::string result { arg };
    ((result += separator, result += args), ...);
    return result;
}

// ============================================================================
// isRange / clamp
// ============================================================================

template <kas::concepts::is_integral T>
[[nodiscard]] constexpr bool
isRange(T value, T left, T right) noexcept
{
    return value >= left && value <= right;
}

template <kas::concepts::is_integral T>
[[nodiscard, deprecated("use isRange")]]
constexpr bool
clamp(T value, T left, T right) noexcept
{
    return isRange(value, left, right);
}

// ============================================================================
// splitString (callable)
// ============================================================================

template <std::invocable<std::string_view> Callable>
void splitString(std::string_view str, std::string_view delimiter,
    Callable&& callable) noexcept(std::is_nothrow_invocable_v<Callable, std::string_view>)
{
    // Implementation note: early return on empty delimiter prevents the
    // infinite loop that would otherwise occur when find() always succeeds
    // on an empty needle.
    if (delimiter.empty()) [[unlikely]]
        return; // no hang

    std::size_t prev_pos = 0, pos = 0;

    while ((pos = str.find(delimiter, pos)) != std::string_view::npos) {
        const std::string_view current_elem { str.substr(prev_pos, pos - prev_pos) };
        if (!current_elem.empty())
            std::invoke(callable, current_elem);

        pos += delimiter.size();
        prev_pos = pos;
    }

    const std::string_view tail { str.substr(prev_pos) };
    if (!tail.empty())
        std::invoke(callable, tail);
}

template <std::invocable SeparatorCallable,
    std::invocable<std::string_view> FuncCallable>
void splitString(std::string_view str,
    SeparatorCallable&& delimiter_callable,
    FuncCallable&& func_callable)
{
    if (str.empty()) [[unlikely]]
        return;

    const std::string_view delimiter {
        std::invoke(std::forward<SeparatorCallable>(delimiter_callable))
    };

    if (delimiter.empty()) [[unlikely]]
        return; // no hang

    std::size_t pos_prev = 0;
    while (pos_prev != std::string_view::npos) {
        const std::size_t pos = str.find_first_of(delimiter, pos_prev);
        if (pos == std::string_view::npos) {
            const std::string_view tail { str.substr(pos_prev) };
            if (!tail.empty())
                std::invoke(func_callable, tail);
            break;
        }

        const std::string_view current_elem { str.substr(pos_prev, pos - pos_prev) };
        if (!current_elem.empty())
            std::invoke(func_callable, current_elem);

        pos_prev = pos + 1;
    }
}

// ============================================================================
// hexToInteger - via from_chars, with 0x prefix and ptr check
// ============================================================================

template <kas::concepts::is_integral T>
[[nodiscard]] std::optional<T>
hexToInteger(std::string_view hex)
{
    if (hex.size() >= 2 && hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X'))
        hex.remove_prefix(2);

    if (hex.empty())
        return std::nullopt;

    T result { };
    const auto [ptr, ec] = std::from_chars(hex.data(), hex.data() + hex.size(),
        result, 16);
    // Implementation note: ptr must reach the end, otherwise the input has
    // trailing characters and the parse is rejected.
    if (ec != std::errc() || ptr != hex.data() + hex.size())
        return std::nullopt;

    return result;
}

// ============================================================================
// stringToInteger - with ptr check
// ============================================================================

template <kas::concepts::is_integral To>
[[nodiscard]] kas::expected<To>
stringToInteger(std::string_view number_as_str, int base)
{
    To result { };
    const auto [ptr, ec] = std::from_chars(
        number_as_str.data(),
        number_as_str.data() + number_as_str.size(),
        result, base);

    // Implementation note: full consumption is required; std::from_chars
    // reports success on a prefix otherwise.
    if (ec == std::errc() && ptr == number_as_str.data() + number_as_str.size())
        [[likely]]
        return result;

    return kas::unexpected { "Failed to convert to Integer type!" };
}

template <kas::concepts::is_integral T>
[[nodiscard]]
kas::expected<std::string>
intToString(T value)
{
    // 20 digits for uint64_t max + sign + a safety margin.
    char buf[32];

    const auto result = std::to_chars(buf, buf + sizeof(buf), value);
    if (result.ec != std::errc { })
        return kas::unexpected { "intToString: value too large" };

    return std::string { buf, result.ptr };
}

// ============================================================================
// checkRangeElemAsInteger (renamed from checkRangeElemAsInterger)
// ============================================================================

template <kas::concepts::is_integral To,
    kas::concepts::input_or_output_iterator_concept Iter>
[[nodiscard]] bool
checkRangeElemAsInteger(Iter begin, Iter end) noexcept
{
    return std::all_of(begin, end, [](std::string_view elem) {
        return stringToInteger<To>(elem).has_value();
    });
}

// ============================================================================
// isStrAsDigit - constexpr without std::isdigit
// ============================================================================

[[nodiscard]] constexpr bool
isStrAsDigit(std::string_view str) noexcept
{
    for (char c : str) {
        if (c < '0' || c > '9')
            return false;
    }
    return true;
}

// ============================================================================
// ltrim / rtrim / trim
// ============================================================================

template <char delimiter>
[[nodiscard]] std::string_view
ltrim(std::string_view str) noexcept
{
    // Implementation note: when delimiter == ' ', the whitespace set is
    // exactly {' ', '\t', '\n', '\v', '\f', '\r'}, matching the classic
    // "isspace" in the C locale but without <cctype>.
    const auto first = std::find_if_not(
        str.begin(), str.end(),
        [](unsigned char ch) {
            if constexpr (delimiter == ' ')
                return ch == 0x20 || (ch >= 0x09 && ch <= 0x0D);
            else
                return ch == static_cast<unsigned char>(delimiter);
        });
    return std::string_view(first, str.end());
}

//------------------

template <char delimiter>
[[nodiscard]] std::string_view
rtrim(std::string_view str) noexcept
{
    const auto last = std::find_if_not(
        str.rbegin(), str.rend(),
        [](unsigned char ch) {
            if constexpr (delimiter == ' ')
                return ch == 0x20 || (ch >= 0x09 && ch <= 0x0D);
            else
                return ch == static_cast<unsigned char>(delimiter);
        })
                          .base();
    return std::string_view(str.begin(), last);
}

//------------------

template <char delimiter>
[[nodiscard]] std::string_view
trim(std::string_view str) noexcept
{
    return ltrim<delimiter>(rtrim<delimiter>(str));
}

} // namespace kas::utils::v5

#endif // KASLIBCXX_UTILS_V5_H