/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include "KAS/utils/v5/utils.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>
#include <memory>
#include <random>
#include <sstream>

#include <sys/wait.h>

namespace kas::utils::v5 {

//------------------

std::vector<std::string_view>
splitString(const std::string_view str, const std::string_view delimiter)
{
    if (delimiter.empty()) [[unlikely]]
        return { };

    std::vector<std::string_view> data;

    std::string_view::size_type prev_pos = 0, pos = 0;

    while ((pos = str.find(delimiter, pos)) != std::string_view::npos) {
        const std::string_view current_elem { str.substr(prev_pos, pos - prev_pos) };
        if (!current_elem.empty())
            data.emplace_back(current_elem);

        pos += delimiter.size();
        prev_pos = pos;
    }

    const std::string_view tail { str.substr(prev_pos) };
    if (!tail.empty())
        data.emplace_back(tail);

    return data;
}

//------------------

kas::expected<std::string_view>
splitString(const std::string_view str, const std::string_view delimiter, const std::size_t index) noexcept
{
    if (str.empty()) [[unlikely]]
        return kas::unexpected { "Input string is empty!" };

    if (delimiter.empty()) [[unlikely]]
        return kas::unexpected { "Delimiter is empty!" };

    std::string_view::size_type prev_pos = 0, pos = 0;
    size_t iter { };

    while ((pos = str.find(delimiter, pos)) != std::string_view::npos) {
        const std::string_view current_elem { str.substr(prev_pos, pos - prev_pos) };
        if (!current_elem.empty()) {
            if (iter == index)
                return current_elem;
            ++iter;
        }
        pos += delimiter.size();
        prev_pos = pos;
    }

    const std::string_view tail { str.substr(prev_pos) };
    if (!tail.empty() && iter == index)
        return tail;

    return kas::unexpected { "Current index not found!" };
}

//------------------

std::optional<std::string>
convertTimePointToString(const std::chrono::system_clock::time_point& time_point, const std::string& format)
{
    const std::time_t t = std::chrono::system_clock::to_time_t(time_point);

    std::tm tm { };
#if defined(_WIN32)
    if (localtime_s(&tm, &t) != 0)
        return std::nullopt;
#else
    if (localtime_r(&t, &tm) == nullptr)
        return std::nullopt;
#endif

    char buffer[128] { };
    const std::size_t n { std::strftime(buffer, sizeof(buffer), format.c_str(), &tm) };
    if (n == 0)
        return std::nullopt;

    return std::string { buffer, n };
}

//------------------

std::string
systemTimeAsString(std::chrono::system_clock::time_point time_point, const std::string& format)
{
    return convertTimePointToString(time_point, format).value_or(std::string { });
}

//------------------

kas::expected<bool>
byteArrayToFile(std::string_view filename, std::span<const std::uint8_t> data)
{
    std::ofstream fd { filename.data(), std::ios::binary };

    if (!fd.is_open()) [[unlikely]]
        return kas::unexpected { "Can not open file!" };

    fd.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    fd.close();

    if (fd.fail()) [[unlikely]]
        return kas::unexpected { "Can not write to the file!" };

    return true;
}

//------------------

void printRawData(std::ostream& out, std::span<const std::uint8_t> data_sp, size_t count_on_delimiter)
{
    if (count_on_delimiter == 0)
        count_on_delimiter = 1;

    const auto flags = out.flags();
    const auto fill = out.fill();

    const size_t n = data_sp.size();
    for (size_t i = 0; i < n; ++i) {
        out << std::hex << std::setfill('0') << std::setw(2)
            << static_cast<std::uint32_t>(data_sp[i]);

        const bool last_in_line = ((i + 1) % count_on_delimiter == 0);
        const bool is_last_elem = (i + 1 == n);

        if (last_in_line)
            out << '\n';
        else if (!is_last_elem)
            out << ' ';
    }

    if (n == 0 || n % count_on_delimiter != 0)
        out << '\n';

    out.flags(flags);
    out.fill(fill);
}

//------------------

kas::expected_string<std::vector<std::uint8_t>>
fileToBytesArray(std::string_view filename)
{
    return fileToBytesArray(filename.data());
}

//------------------

kas::expected_string<std::vector<std::uint8_t>>
fileToBytesArray(const char* filename)
{
    std::ifstream fs { filename, std::ios::binary };
    if (!fs.is_open()) [[unlikely]]
        return kas::unexpected_string { stringify("Can not open file: ", filename) };

    std::error_code ec;
    const auto filesize = std::filesystem::file_size(filename, ec);
    if (ec) [[unlikely]]
        return kas::unexpected_string { stringify("file_size: ", ec.message()) };

    if (filesize > static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max()))
        [[unlikely]]
        return kas::unexpected_string { "file too large" };

    const auto size = static_cast<std::size_t>(filesize);

    std::vector<std::uint8_t> result(size);
    if (size > 0) {
        if (!fs.read(reinterpret_cast<char*>(result.data()),
                static_cast<std::streamsize>(size))) [[unlikely]] {
            const auto got = static_cast<std::size_t>(fs.gcount());
            return kas::unexpected_string {
                stringify(
                    "Couldn't read all the data! - read: ",
                    std::to_string(got),
                    "; filesize: ",
                    std::to_string(size))
            };
        }
    }

    return result;
}

//------------------

std::vector<std::uint8_t>
stringToBytes(std::string_view data)
{
    return std::vector<std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(data.data()),
        reinterpret_cast<const std::uint8_t*>(data.data()) + data.size());
}

//------------------

std::string
bytesAsString(std::span<const std::uint8_t> data)
{
    return std::string(reinterpret_cast<const char*>(std::data(data)), std::size(data));
}

//------------------

std::string
fileTimeToString(const std::filesystem::file_time_type& ftime)
{
    return std::to_string(std::chrono::system_clock::to_time_t(std::chrono::file_clock::to_sys(ftime)));
}

//------------------

std::optional<std::chrono::system_clock::time_point>
stringToSystemTimePoint(const std::string& date_time_str, std::chrono::minutes zone_offset, std::string format)
{
    std::tm tm { };
    std::stringstream ss { date_time_str };
    ss >> std::get_time(&tm, format.c_str());

    if (ss.fail())
        return std::nullopt;

    auto tp { std::chrono::system_clock::from_time_t(std::mktime(&tm)) };
    tp += zone_offset;
    return tp;
}

//------------------

std::string
randomString(size_t length)
{
    static constexpr std::string_view alphanum {
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
    };
    static std::mt19937 rng { std::random_device { }() };
    std::uniform_int_distribution<> dist { 0, std::size(alphanum) - 1 };

    std::string result;
    result.reserve(length);
    while (length--)
        result += alphanum[dist(rng)];
    return result;
}

//------------------

kas::expected_string<std::string>
fileGetContents(const char* filename)
{
    return fileGetContents(std::filesystem::path { filename });
}

//------------------

kas::expected_string<std::string>
fileGetContents(const std::string& filename)
{
    return fileGetContents(std::filesystem::path { filename });
}

//------------------

kas::expected_string<std::string>
fileGetContents(const std::filesystem::path& filename)
{
    std::ifstream fs { filename, std::ios::binary };
    if (!fs.is_open()) [[unlikely]]
        return kas::unexpected_string { stringify("Can not open file: ", filename.c_str()) };

    std::stringstream ss { };
    ss << fs.rdbuf();
    return ss.str();
}

//------------------

kas::expected_string<std::string>
exec(const std::string& cmd)
{
    auto close_pipe {
        [](FILE* f) {
            if (f)
                pclose(f);
        }
    };
    std::unique_ptr<FILE, decltype(close_pipe)> file_uptr { popen(cmd.c_str(), "r") };

    if (!file_uptr) [[unlikely]]
        return kas::unexpected_string { "popen failed" };

    std::string result;
    std::array<char, 256> buffer;

    for (size_t bytes { }; (bytes = std::fread(buffer.data(), 1, buffer.size(), file_uptr.get())) > 0;) {
        result.append(buffer.data(), bytes);
    }

    if (std::ferror(file_uptr.get())) [[unlikely]]
        return kas::unexpected_string { "fread error" };

    const int32_t exit_status { pclose(file_uptr.release()) };

    if (exit_status == -1) [[unlikely]]
        return kas::unexpected_string { "pclose failed" };
    if (!WIFEXITED(exit_status)) [[unlikely]]
        return kas::unexpected_string { "terminated by signal" };

    if (WEXITSTATUS(exit_status) != 0) [[unlikely]]
        return kas::unexpected_string { "exit code: " + std::to_string(exit_status) };

    if (!result.empty() && result.back() == '\n')
        result.pop_back();

    return result;
}

//------------------

bool containsToken(std::string_view header, std::string_view token) noexcept
{
    std::size_t pos = 0;
    while (pos <= header.size()) {
        const auto comma = header.find(',', pos);
        const auto part = (comma == std::string_view::npos)
            ? header.substr(pos)
            : header.substr(pos, comma - pos);

        // trim OWS
        std::string_view trimmed = part;
        while (!trimmed.empty() && (trimmed.front() == ' ' || trimmed.front() == '\t'))
            trimmed.remove_prefix(1);
        while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\t'))
            trimmed.remove_suffix(1);

        if (iequals(trimmed, token))
            return true;

        if (comma == std::string_view::npos)
            break;
        pos = comma + 1;
    }
    return false;
}

//------------------

bool iequals(std::string_view a, std::string_view b) noexcept
{
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const auto ca = static_cast<unsigned char>(a[i]);
        const auto cb = static_cast<unsigned char>(b[i]);
        if (std::tolower(ca) != std::tolower(cb))
            return false;
    }
    return true;
}

//------------------

std::string
integerToHEX(std::uintmax_t num, bool is_upper_case, bool is_prefix, std::size_t max_setw)
{
    static constexpr char LOWER[] = "0123456789abcdef";
    static constexpr char UPPER[] = "0123456789ABCDEF";
    const char* digits = is_upper_case ? UPPER : LOWER;

    char buf[2 * sizeof(std::uintmax_t)];
    char* const end = buf + sizeof(buf);
    char* p = end;

    std::uintmax_t v = num;
    do {
        *--p = digits[v & 0xF];
        v >>= 4;
    } while (v != 0);

    const std::size_t width = std::min<std::size_t>(max_setw, 1024);
    const std::size_t digits_len = static_cast<std::size_t>(end - p);
    const std::size_t pad = (width > digits_len) ? (width - digits_len) : 0;

    std::string result;
    result.reserve((is_prefix ? 2u : 0u) + pad + digits_len);

    if (is_prefix)
        result += "0x";
    result.append(pad, '0');
    result.append(p, digits_len);

    return result;
}

//------------------

kas::expected<bool>
stringToFile(std::string_view filename, std::string_view str)
{
    std::ofstream fd { filename.data() };

    if (!fd.is_open()) [[unlikely]]
        return kas::unexpected { "Can not open file!" };

    fd.write(str.data(), static_cast<std::streamsize>(str.size()));
    fd.close();
    if (fd.fail())
        return kas::unexpected { "Can not write to the file!" };

    return true;
}

//------------------

std::string
toLower(std::string data)
{
    std::transform(
        std::begin(data), std::end(data), std::begin(data),
        [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

    return data;
}

//------------------

std::string
toUpper(std::string data)
{
    std::transform(
        std::begin(data), std::end(data), std::begin(data),
        [](unsigned char c) {
            return static_cast<char>(std::toupper(c));
        });

    return data;
}

//------------------

} // namespace kas::utils::v5