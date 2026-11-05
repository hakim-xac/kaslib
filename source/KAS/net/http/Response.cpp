// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include "KAS/net/http/Response.h"
#include "KAS/net/general.h"
#include "KAS/net/http/StaticFile.h"
#include "KAS/utils/utils.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <fcntl.h>
#include <poll.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <utility>

namespace kas::net::http {
namespace {

    /**
     * @brief Locale-independent ASCII tolower for HTTP header names.
     *
     * Only 'A'..'Z' are folded; all other bytes are returned unchanged.
     * Using this instead of std::tolower avoids dependency on the
     * current locale.
     *
     * @param c Input byte.
     * @return The lowercased byte, or @p c if it is not an ASCII letter.
     */
    inline unsigned char asciiToLower(unsigned char c) noexcept
    {
        return (c >= 'A' && c <= 'Z')
            ? static_cast<unsigned char>(c + ('a' - 'A'))
            : c;
    }

    /**
     * @brief Formats a time_t as an HTTP-date (IMF-fixdate).
     *
     * Produces the IMF-fixdate form required by RFC 9110, for example:
     *     Sun, 06 Nov 1994 08:49:37 GMT
     *
     * The time is converted to UTC via gmtime_r / gmtime_s.
     *
     * @param t Time value (seconds since the Unix epoch).
     * @return The formatted date string.
     */
    std::string formatHttpDate(std::time_t t)
    {
        std::tm tm { };
#if defined(_WIN32)
        gmtime_s(&tm, &t);
#else
        gmtime_r(&t, &tm);
#endif
        static constexpr const char* wday[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
        static constexpr const char* mon[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

        char buf[64];
        const int n = std::snprintf(
            buf, sizeof(buf), "%s, %02d %s %04d %02d:%02d:%02d GMT",
            wday[tm.tm_wday], tm.tm_mday, mon[tm.tm_mon], tm.tm_year + 1900,
            tm.tm_hour, tm.tm_min, tm.tm_sec);
        return std::string(buf, n > 0 ? static_cast<std::size_t>(n) : 0);
    }

    /**
     * @brief Sends a file window over a socket using sendfile(2).
     *
     * Opens @p path read-only and sends @p count_bytes starting at
     * @p start_offset. The transfer is done in chunks of up to 1 MiB,
     * with:
     *  - retries on EINTR;
     *  - poll(POLLOUT) and retry on EAGAIN/EWOULDBLOCK;
     *  - an overall timeout of 5 minutes to bound slow clients;
     *  - explicit errors on EPIPE/ECONNRESET (client disconnected),
     *    other sendfile errors, and unexpected EOF (sent == 0).
     *
     * @param fd           Socket file descriptor.
     * @param path         Path to the file.
     * @param start_offset Offset in the file to start from.
     * @param count_bytes  Number of bytes to send.
     * @return true on success, or an error string on failure.
     */
    expected_string<bool> sendFileData(
        int fd,
        const std::filesystem::path& path,
        std::size_t start_offset,
        std::size_t count_bytes)
    {
        const int file_fd = ::open(path.c_str(), O_RDONLY);
        if (file_fd < 0) {
            return unexpected_string {
                utils::stringify(
                    "sendFileData: open() failed: ",
                    std::generic_category().message(errno))
            };
        }

        // RAII close of the file.
        struct FileGuard {
            int fd;
            ~FileGuard()
            {
                if (fd >= 0)
                    ::close(fd);
            }
        } file_guard { file_fd };

        constexpr std::size_t MAX_CHUNK = 1024 * 1024; // 1 MiB
        constexpr auto OVERALL_TIMEOUT = std::chrono::minutes(5);

        off_t offset = static_cast<off_t>(start_offset);
        std::size_t remaining = count_bytes;
        const auto start_time = std::chrono::steady_clock::now();

        while (remaining > 0) {
            if (std::chrono::steady_clock::now() - start_time > OVERALL_TIMEOUT)
                return unexpected_string { "sendFileData: overall timeout" };

            const std::size_t chunk = std::min(remaining, MAX_CHUNK);
            const ssize_t sent = ::sendfile(fd, file_fd, &offset, chunk);

            if (sent > 0) {
                remaining -= static_cast<std::size_t>(sent);
                continue;
            }

            if (sent == -1) {
                if (errno == EINTR)
                    continue;

                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    pollfd pfd { fd, POLLOUT, 0 };
                    const int rc = ::poll(&pfd, 1, 1000);
                    if (rc < 0 && errno != EINTR) {
                        return unexpected_string {
                            utils::stringify(
                                "sendFileData: poll() error: ",
                                std::string(std::generic_category().message(errno)))
                        };
                    }
                    continue;
                }

                if (errno == EPIPE || errno == ECONNRESET)
                    return unexpected_string { "sendFileData: client disconnected" };

                return unexpected_string {
                    utils::stringify(
                        "sendFileData: sendfile() error: ",
                        std::string(std::generic_category().message(errno)))
                };
            }

            // sent == 0
            return unexpected_string { "sendFileData: unexpected EOF" };
        }

        return true;
    }

} // namespace

//------------------

Response& Response::setFileBody(std::filesystem::path path,
    std::size_t start_offset,
    std::size_t count_bytes) &
{
    std::error_code ec;
    const auto sz = std::filesystem::file_size(path, ec);
    if (ec)
        throw std::runtime_error("Response::setFileBody: " + ec.message());

    if (start_offset > static_cast<std::size_t>(sz))
        throw std::out_of_range("Response::setFileBody: start_offset > file_size");

    m_file_body_opt = FileBody {
        .path = std::move(path),
        .start_offset = start_offset,
        .count_bytes = count_bytes,
        .file_size = static_cast<std::size_t>(sz),
    };
    m_body.clear(); // файловое и строковое тело взаимоисключающие

    if (!hasHeader("Content-Type")) {
        const auto ext = m_file_body_opt->path.extension().string();
        setHeader("Content-Type", mimeByExtension(ext)); // ваш хелпер
    }
    return *this;
}

//------------------

Response&& Response::setFileBody(std::filesystem::path path,
    std::size_t start_offset,
    std::size_t count_bytes) &&
{
    setFileBody(std::move(path), start_offset, count_bytes);
    return std::move(*this);
}

//------------------

bool Response::hasFileBody() const noexcept
{
    return m_file_body_opt.has_value();
}

//------------------

const std::filesystem::path& Response::getFilePath() const
{
    if (!m_file_body_opt)
        throw std::logic_error("Response::getFilePath: no file body");
    return m_file_body_opt->path;
}

//------------------

std::size_t Response::getFileStartOffset() const noexcept
{
    return m_file_body_opt ? m_file_body_opt->start_offset : 0;
}

//------------------

std::size_t Response::getFileCountBytes() const noexcept
{
    return m_file_body_opt ? m_file_body_opt->count_bytes : 0;
}

//------------------

std::size_t Response::getFileSize() const noexcept
{
    return m_file_body_opt ? m_file_body_opt->file_size : 0;
}

//------------------

Response& Response::setFileRange(std::size_t start_offset, std::size_t count_bytes) &
{
    if (!m_file_body_opt)
        throw std::logic_error("Response::setFileRange: no file body");
    m_file_body_opt->start_offset = start_offset;
    m_file_body_opt->count_bytes = count_bytes;
    return *this;
}

//------------------

std::size_t ResponseHeaderHash::operator()(std::string_view sv) const noexcept
{
    std::size_t h = 1469598103934665603ull; // FNV-1a 64
    for (unsigned char c : sv) {
        h ^= static_cast<std::size_t>(asciiToLower(c));
        h *= 1099511628211ull;
    }
    return h;
}

//------------------

bool ResponseHeaderEqual::operator()(std::string_view a, std::string_view b) const noexcept
{
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (asciiToLower(static_cast<unsigned char>(a[i]))
            != asciiToLower(static_cast<unsigned char>(b[i])))
            return false;
    }
    return true;
}

//------------------

std::uint16_t Response::getStatusCode() const noexcept
{
    return m_status.code;
}

//------------------

std::string_view Response::getStatusText() const noexcept
{
    return m_status.text; // "404 Not Found"
}

//------------------

std::string_view Response::getReasonPhrase() const noexcept
{
    // m_status.text = "404 Not Found" -> "Not Found"
    const auto sp = m_status.text.find(' ');
    return (sp == std::string_view::npos) ? m_status.text : m_status.text.substr(sp + 1);
}

//------------------

std::string_view Response::getContentType() const noexcept
{
    const auto it = m_headers.find(std::string_view { "Content-Type" });
    if (it == m_headers.end())
        return { };
    return it->second;
}

//------------------

Response& Response::setHeader(std::string_view name, std::string_view value) &
{
    m_headers.insert_or_assign(std::string { name }, std::string { value });
    return *this;
}

//------------------

Response&& Response::setHeader(std::string_view name, std::string_view value) &&
{
    m_headers.insert_or_assign(std::string { name }, std::string { value });
    return std::move(*this);
}

//------------------

std::optional<std::string_view>
Response::getHeader(std::string_view name) const&
{
    const auto it = m_headers.find(name);
    return it != m_headers.end()
        ? std::optional<std::string_view> { it->second }
        : std::nullopt;
}

//------------------

bool Response::hasHeader(std::string_view name) const
{
    return m_headers.find(name) != m_headers.end();
}

//------------------

const Response::headers_t&
Response::getHeaders() const& noexcept
{
    return m_headers;
}

//------------------

Response& Response::removeHeader(std::string_view name) &
{
    m_headers.erase(std::string { name });
    return *this;
}

//------------------

Response& Response::setBody(std::string body) &
{
    m_body = std::move(body);
    m_file_body_opt.reset();
    return *this;
}

//------------------

Response&& Response::setBody(std::string body) &&
{
    m_body = std::move(body);
    m_file_body_opt.reset();
    return std::move(*this);
}

//------------------

const std::string& Response::getBody() const& noexcept
{
    return m_body;
}

//------------------

Response& Response::clearBody() &
{
    m_body.clear();
    m_file_body_opt.reset();
    return *this;
}

//------------------

std::size_t Response::getBodySize() const noexcept
{
    if (m_file_body_opt) {
        const auto& f = *m_file_body_opt;
        const std::size_t remaining = (f.start_offset <= f.file_size)
            ? (f.file_size - f.start_offset)
            : 0;
        return (f.count_bytes == 0) ? remaining
                                    : std::min(f.count_bytes, remaining);
    }
    return m_body.size();
}

//------------------

std::string Response::serialize() const
{
    std::string out;
    out.reserve(256);

    // Status line: "HTTP/1.1 404 Not Found\r\n"
    out += "HTTP/1.1 ";
    out += m_status.text;
    out += "\r\n";

    // User-provided headers.
    for (const auto& [name, value] : m_headers) {
        out += name;
        out += ": ";
        out += value;
        out += "\r\n";
    }

    // Content-Length: always present (0 if there is no body).
    if (!hasHeader("Content-Length")) {
        out += "Content-Length: ";
        out += std::to_string(getBodySize());
        out += "\r\n";
    }

    // Date: added if not already set.
    if (!hasHeader("Date")) {
        out += "Date: ";
        out += formatHttpDate(std::time(nullptr));
        out += "\r\n";
    }

    // End of headers.
    out += "\r\n";

    // The body is NOT appended here: it is sent by sendTo()
    // (sendfile or sendBytes).
    return out;
}

//------------------

expected_string<bool> Response::sendTo(int fd, bool skip_body) const
{
    const std::string head = serialize();
    auto hdr_exp = general::sendBytes(fd,
        std::span<const std::uint8_t> {
            reinterpret_cast<const std::uint8_t*>(head.data()), head.size() });
    if (!hdr_exp)
        return hdr_exp;

    if (skip_body)
        return true;

    if (m_file_body_opt) {
        const auto& f = *m_file_body_opt;
        const std::size_t count = getBodySize();
        if (count == 0)
            return true;
        return sendFileData(fd, f.path, f.start_offset, count);
    }

    if (m_body.empty())
        return true;
    return general::sendBytes(fd,
        std::span<const std::uint8_t> {
            reinterpret_cast<const std::uint8_t*>(m_body.data()), m_body.size() });
}

//------------------

} // namespace kas::net::http