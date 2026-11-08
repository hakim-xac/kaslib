// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include "KAS/net/http/Request.h"
#include "KAS/net/socket/ClientConnection.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <charconv>
#include <cstring>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>

namespace kas::net::http {
namespace {

    // ------------------------------------------------------------
    //  Low-level socket reading
    // ------------------------------------------------------------

    /**
     * @brief Reads up to @p max_bytes from @p fd into @p buf.
     *
     * Retries on EINTR. A single recv(2) call is performed; the result
     * may be a short read, so callers that need a specific number of
     * bytes must loop.
     *
     * @param fd        Socket file descriptor.
     * @param buf       Destination buffer.
     * @param max_bytes Maximum number of bytes to read.
     * @return Number of bytes read (> 0), 0 on EOF, or -1 on error.
     */
    ssize_t readSome(int fd, char* buf, std::size_t max_bytes) noexcept
    {
        ssize_t n = 0;
        do {
            n = ::recv(fd, buf, max_bytes, 0);
        } while (n < 0 && errno == EINTR);
        return n;
    }

    /**
     * @brief Reads exactly @p n bytes into @p out (appending).
     *
     * Used for the request body when Content-Length is known.
     * Reserves space for the whole body up front and reads it in
     * chunks of up to 4096 bytes.
     *
     * @param fd  Socket file descriptor.
     * @param out Destination string; new bytes are appended.
     * @param n   Number of bytes to read.
     * @return true on success, false on EOF or error before @p n bytes
     *         were read.
     */
    bool readExact(int fd, std::string& out, std::size_t n)
    {
        out.reserve(out.size() + n);
        char buf[4096];
        std::size_t remaining = n;
        while (remaining > 0) {
            const std::size_t chunk = std::min(remaining, sizeof(buf));
            const ssize_t got = readSome(fd, buf, chunk);
            if (got <= 0)
                return false;
            out.append(buf, static_cast<std::size_t>(got));
            remaining -= static_cast<std::size_t>(got);
        }
        return true;
    }

    /**
     * @brief Reads from @p fd until the end of the header block.
     *
     * Appends bytes to @p out and stops as soon as "\r\n\r\n" is seen
     * (the terminator is included). The header block is capped at
     * @p max_header_bytes to bound memory use for malformed or
     * malicious clients.
     *
     * @param fd              Socket file descriptor.
     * @param out             Destination string; new bytes are appended.
     * @param max_header_bytes Maximum size of the header block.
     * @return true if the header terminator was found within the limit,
     *         false on EOF, error, or limit exceeded.
     */
    bool readHeaders(int fd, std::string& out, std::size_t max_header_bytes = 64 * 1024)
    {
        char buf[4096];
        while (out.size() < max_header_bytes) {
            const ssize_t got = readSome(fd, buf, sizeof(buf));
            if (got <= 0)
                return false;
            out.append(buf, static_cast<std::size_t>(got));

            // look for the header terminator
            if (out.find("\r\n\r\n") != std::string::npos)
                return true;
        }
        return false; // limit exceeded
    }

    // ------------------------------------------------------------
    //  Parsing
    // ------------------------------------------------------------

    /**
     * @brief Trims spaces and tabs from both ends of @p s.
     *
     * @param s Input view.
     * @return A view into @p s with leading and trailing SP/HTAB removed.
     */
    std::string_view trim(std::string_view s) noexcept
    {
        while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
            s.remove_prefix(1);
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
            s.remove_suffix(1);
        return s;
    }

    /**
     * @brief Parses an HTTP version string.
     *
     * Accepts "HTTP/1.0", "HTTP/1.1", "HTTP/2.0", and "HTTP/2".
     *
     * @param v Version string.
     * @return The corresponding HttpVersion, or std::nullopt on a
     *         malformed or unsupported value.
     */
    std::optional<HttpVersion> parseVersion(std::string_view v) noexcept
    {
        if (v == "HTTP/1.0")
            return HttpVersion::HTTP_1_0;
        if (v == "HTTP/1.1")
            return HttpVersion::HTTP_1_1;
        if (v == "HTTP/2.0" || v == "HTTP/2")
            return HttpVersion::HTTP_2_0;
        return std::nullopt;
    }

    /**
     * @brief Checks whether @p s is a valid HTTP token (RFC 9110).
     *
     * A token is a non-empty sequence of tchar characters:
     * "!" / "#" / "$" / "%" / "&" / "'" / "*" / "+" / "-" / "." /
     * "^" / "_" / "`" / "|" / "~" / DIGIT / ALPHA.
     *
     * @param s Candidate string.
     * @return true if @p s is a valid token, false otherwise.
     */
    bool isToken(std::string_view s) noexcept
    {
        if (s.empty())
            return false;
        for (unsigned char c : s) {
            // token = 1*tchar; tchar = "!" / "#" / "$" / "%" / "&" / "'" /
            //   "*" / "+" / "-" / "." / "^" / "_" / "`" / "|" / "~" /
            //   DIGIT / ALPHA
            const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                || (c >= '0' && c <= '9')
                || std::strchr("!#$%&'*+-.^_`|~", c) != nullptr;
            if (!ok)
                return false;
        }
        return true;
    }

    /**
     * @brief Parses "METHOD SP target SP version" (without CRLF).
     *
     * Validates the method as an HTTP token, requires a non-empty
     * target, and parses the version. On success, fills @p req with
     * method, target, and version.
     *
     * @param line Request line without the trailing CRLF.
     * @param req  Request to populate.
     * @return true on success, false on a malformed request line.
     */
    bool parseRequestLine(std::string_view line, Request& req)
    {
        // line without CRLF
        const auto sp1 = line.find(' ');
        if (sp1 == std::string_view::npos)
            return false;
        const auto sp2 = line.find(' ', sp1 + 1);
        if (sp2 == std::string_view::npos)
            return false;

        const std::string_view method = line.substr(0, sp1);
        const std::string_view target = line.substr(sp1 + 1, sp2 - sp1 - 1);
        const std::string_view version = line.substr(sp2 + 1);

        if (!isToken(method))
            return false;
        if (target.empty())
            return false;

        const auto ver = parseVersion(version);
        if (!ver)
            return false;

        req.setMethod(std::string(method));
        req.setTarget(target);
        req.setVersion(*ver);
        return true;
    }

    /**
     * @brief Parses a single "Name: value" header line.
     *
     * The value is trimmed of surrounding spaces/tabs. An empty name
     * (no ':' or ':' at position 0) is rejected.
     *
     * @param line Header line without CRLF.
     * @param req  Request to add the header to.
     * @return true on success, false on a malformed header line.
     */
    bool parseHeaderLine(std::string_view line, Request& req)
    {
        const auto colon = line.find(':');
        if (colon == std::string_view::npos || colon == 0)
            return false;
        const std::string_view name = line.substr(0, colon);
        const std::string_view value = trim(line.substr(colon + 1));
        req.setHeader(name, value);
        return true;
    }

    /**
     * @brief Parses a Content-Length value into a size_t.
     *
     * The whole (trimmed) value must be a decimal number that fits into
     * size_t.
     *
     * @param s Header value.
     * @return The parsed length, or std::nullopt on empty/invalid input
     *         or overflow.
     */
    std::optional<std::size_t> parseContentLength(std::string_view s) noexcept
    {
        s = trim(s);
        if (s.empty())
            return std::nullopt;
        std::size_t value = 0;
        const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
        if (ec != std::errc { } || ptr != s.data() + s.size())
            return std::nullopt;
        return value;
    }

    /**
     * @brief Computes the keep-alive flag from the version and headers.
     *
     * A "Connection: close" header (case-insensitive, exactly "close")
     * disables keep-alive. Otherwise, HTTP/1.1 and HTTP/2 keep the
     * connection alive, HTTP/1.0 does not (matching the defaults
     * defined by RFC 9110).
     *
     * @param v   Request HTTP version.
     * @param req Parsed request.
     * @return true if the connection should be kept alive.
     */
    bool computeKeepAlive(HttpVersion v, const Request& req) noexcept
    {
        if (const auto conn = req.getHeader("Connection")) {
            // case-insensitive "close"
            std::string_view c = *conn;
            if (c.size() == 5
                && (c[0] == 'c' || c[0] == 'C') && (c[1] == 'l' || c[1] == 'L')
                && (c[2] == 'o' || c[2] == 'O') && (c[3] == 's' || c[3] == 'S')
                && (c[4] == 'e' || c[4] == 'E')) {
                return false;
            }
        }
        return v != HttpVersion::HTTP_1_0;
    }

    // ------------------------------------------------------------
    //  Full buffer parsing: header + body
    // ------------------------------------------------------------

    /**
     * @brief Parses the header block of a request.
     *
     * Splits @p block at the first "\r\n\r\n", parses the request line
     * and each header line, and fills @p req. On success, returns the
     * length of the header block including the terminator, so the
     * caller can locate the body.
     *
     * @param block Buffer containing the request line and headers, up
     *              to and including "\r\n\r\n".
     * @param req   Request to populate.
     * @return The header block length (including the terminating
     *         "\r\n\r\n") on success, or std::nullopt if the terminator
     *         is missing or the header block is malformed.
     */
    std::optional<std::size_t> parseHeaderBlock(std::string_view block, Request& req)
    {
        const auto header_end = block.find("\r\n\r\n");
        if (header_end == std::string_view::npos)
            return std::nullopt;

        const std::string_view headers = block.substr(0, header_end);
        std::size_t pos = 0;

        // first line - request line
        const auto eol = headers.find("\r\n");
        if (eol == std::string_view::npos) {
            // single line, no headers
            if (!parseRequestLine(headers, req))
                return std::nullopt;
            return header_end + 4;
        }
        if (!parseRequestLine(headers.substr(0, eol), req))
            return std::nullopt;
        pos = eol + 2;

        // remaining lines - headers
        while (pos < headers.size()) {
            const auto next = headers.find("\r\n", pos);
            const std::string_view line = (next == std::string_view::npos)
                ? headers.substr(pos)
                : headers.substr(pos, next - pos);
            if (!line.empty() && !parseHeaderLine(line, req))
                return std::nullopt;
            if (next == std::string_view::npos)
                break;
            pos = next + 2;
        }

        return header_end + 4;
    }

} // namespace

//------------------

std::size_t CaseInsensitiveHash::operator()(std::string_view sv) const noexcept
{
    // FNV-1a по lowercase-символам.
    std::size_t h = 1469598103934665603ULL;
    for (unsigned char c : sv) {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<unsigned char>(c - 'A' + 'a');
        h ^= c;
        h *= 1099511628211ULL;
    }
    return h;
}

//------------------

bool CaseInsensitiveEqual::operator()(std::string_view a, std::string_view b) const noexcept
{
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        unsigned char ca = static_cast<unsigned char>(a[i]);
        unsigned char cb = static_cast<unsigned char>(b[i]);
        if (ca >= 'A' && ca <= 'Z')
            ca = static_cast<unsigned char>(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z')
            cb = static_cast<unsigned char>(cb - 'A' + 'a');
        if (ca != cb)
            return false;
    }
    return true;
}

//------------------

Request::Request(std::string_view method, std::string_view target, HttpVersion version)
    : m_method { method }
    , m_version { version }
{
    setTarget(target);
}

//------------------

const std::string& Request::getMethod() const& noexcept
{
    return m_method;
}

//------------------

Request& Request::setMethod(std::string method) &
{
    m_method = std::move(method);
    return *this;
}

//------------------

Request&& Request::setMethod(std::string method) &&
{
    m_method = std::move(method);
    return std::move(*this);
}

//------------------

const std::string& Request::getTarget() const& noexcept
{
    return m_target;
}

//------------------

Request& Request::setTarget(std::string_view target) &
{
    m_target.assign(target);
    splitTarget();
    return *this;
}

//------------------

const std::string& Request::getPath() const& noexcept
{
    return m_path;
}

//------------------

const std::string& Request::getQueryString() const& noexcept
{
    return m_query;
}

//------------------

std::optional<std::string_view>
Request::getQueryParam(std::string_view key) const&
{
    const auto it = m_query_params.find(key);
    if (it == m_query_params.end())
        return std::nullopt;
    return std::string_view { it->second };
}

//------------------

const Request::query_params_t& Request::getQueryParams() const& noexcept
{
    return m_query_params;
}

//------------------

HttpVersion Request::getVersion() const noexcept
{
    return m_version;
}

//------------------

Request& Request::setVersion(HttpVersion v) & noexcept
{
    m_version = v;
    return *this;
}

//------------------

std::string_view
Request::versionToString(HttpVersion v) noexcept
{
    switch (v) {
    case HttpVersion::HTTP_1_0:
        return "HTTP/1.0";
    case HttpVersion::HTTP_1_1:
        return "HTTP/1.1";
    case HttpVersion::HTTP_2_0:
        return "HTTP/2.0";
    }
    return "HTTP/1.1";
}

//------------------

std::string_view
Request::getVersionString() const noexcept
{
    return versionToString(m_version);
}

//------------------

Request& Request::setHeader(std::string_view name, std::string_view value) &
{
    m_headers.insert_or_assign(std::string { name }, std::string { value });
    return *this;
}

//------------------

std::optional<std::string_view>
Request::getHeader(std::string_view name) const&
{
    const auto it = m_headers.find(name);
    if (it == m_headers.end())
        return std::nullopt;
    return std::string_view { it->second };
}

//------------------

bool Request::hasHeader(std::string_view name) const
{
    return m_headers.find(name) != m_headers.end();
}

//------------------

const Request::headers_t& Request::getHeaders() const& noexcept
{
    return m_headers;
}

//------------------

Request& Request::setBody(std::string body) &
{
    m_body = std::move(body);
    return *this;
}

//------------------

const std::string& Request::getBody() const& noexcept
{
    return m_body;
}

//------------------

std::size_t Request::getBodySize() const noexcept
{
    return m_body.size();
}

//------------------

Request& Request::setRemoteAddress(std::string addr) &
{
    m_remote = std::move(addr);
    return *this;
}

//------------------

const std::string& Request::getRemoteAddress() const& noexcept
{
    return m_remote;
}

//------------------

Request& Request::setKeepAlive(bool keep_alive) & noexcept
{
    m_keep_alive = keep_alive;
    return *this;
}

//------------------

bool Request::isKeepAlive() const noexcept
{
    return m_keep_alive;
}

//------------------

void Request::splitTarget()
{
    // Target format: path[?query]
    const auto qpos = m_target.find('?');
    if (qpos == std::string::npos) {
        m_path = m_target;
        m_query.clear();
        m_query_params.clear();
        return;
    }

    m_path.assign(m_target, 0, qpos);
    m_query.assign(m_target, qpos + 1, std::string::npos);

    m_query_params.clear();

    // Parse query: key=value&key=value...
    std::size_t pos = 0;
    while (pos <= m_query.size()) {
        const auto amp = m_query.find('&', pos);
        const std::size_t end = (amp == std::string::npos) ? m_query.size() : amp;
        const std::string_view pair { m_query.data() + pos, end - pos };

        if (!pair.empty()) {
            const auto eq = pair.find('=');
            std::string_view key = (eq == std::string_view::npos)
                ? pair
                : pair.substr(0, eq);
            std::string_view value = (eq == std::string_view::npos)
                ? std::string_view { }
                : pair.substr(eq + 1);

            // Duplicates: keep the FIRST occurrence.
            if (!m_query_params.contains(key))
                m_query_params.emplace(std::string { key }, std::string { value });
        }

        if (amp == std::string::npos)
            break;
        pos = amp + 1;
    }
}

//------------------

std::optional<Request>
Request::make(const kas::net::socket::ClientConnection& connection)
{
    const int fd = connection.getSocketID();
    if (fd < 0)
        return std::nullopt;

    // 1. Read the header block (up to \r\n\r\n).
    std::string buffer;
    if (!readHeaders(fd, buffer))
        return std::nullopt;

    // 2. Parse the header block.
    Request req;
    const auto header_len_opt = parseHeaderBlock(buffer, req);
    if (!header_len_opt)
        return std::nullopt;
    const std::size_t header_len = *header_len_opt;

    // 3. Determine the body length.
    std::size_t body_len = 0;
    if (const auto cl = req.getHeader("Content-Length")) {
        const auto parsed = parseContentLength(*cl);
        if (!parsed)
            return std::nullopt;
        body_len = *parsed;
    }

    // 4. Read the body (if any).
    //    NB: chunked is not supported - TODO. If Transfer-Encoding:
    //    chunked is present, return nullopt (an explicit error is
    //    better than garbage).
    if (const auto te = req.getHeader("Transfer-Encoding")) {
        if (trim(*te).find("chunked") != std::string_view::npos)
            return std::nullopt; // TODO: implement chunked
    }

    std::string body;
    if (body_len > 0) {
        // buffer may already contain part of the body (read in chunks).
        const std::size_t already = buffer.size() - header_len;
        if (already > 0) {
            const std::size_t take = std::min(already, body_len);
            body.assign(buffer, header_len, take);
        }
        if (body.size() < body_len) {
            if (!readExact(fd, body, body_len - body.size()))
                return std::nullopt;
        }
        req.setBody(std::move(body));
    }

    // 5. Keep-alive.
    //    NOTE: computeKeepAlive is defined in the anonymous namespace
    //    inside kas::net, so it must NOT be qualified as
    //    kas::net::computeKeepAlive.
    req.setKeepAlive(computeKeepAlive(req.getVersion(), req));

    // 6. Client address.
    return req.setRemoteAddress(connection.getRemoteAddress().toString());
}

//------------------

} // namespace kas::net