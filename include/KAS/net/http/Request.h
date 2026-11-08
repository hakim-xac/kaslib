/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_NET_SOCKET_REQUEST_H
#define KASLIBCXX_NET_SOCKET_REQUEST_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace kas::net::socket {
class ClientConnection;
}

namespace kas::net::http {

class ClientConnection;

/**
 * @brief HTTP protocol version.
 */
enum class HttpVersion {
    HTTP_1_0, ///< HTTP/1.0.
    HTTP_1_1, ///< HTTP/1.1.
    HTTP_2_0, ///< HTTP/2.0.
};

// ---------------------------------------------------------------------------
// Transparent hash/equality for header names (RFC 9110: case-insensitive).
// is_transparent allows lookup by std::string_view without constructing
// a std::string.
// ---------------------------------------------------------------------------

/**
 * @brief Case-insensitive hash for HTTP header names.
 *
 * Implements the transparent hashing requirement so that header lookups
 * can be performed with std::string_view keys without extra allocations.
 */
struct CaseInsensitiveHash {
    using is_transparent = void;
    /**
     * @brief Hashes an ASCII string in a case-insensitive way.
     * @param sv Header name.
     * @return Hash value.
     */
    std::size_t operator()(std::string_view sv) const noexcept;
};

/**
 * @brief Case-insensitive equality for HTTP header names.
 */
struct CaseInsensitiveEqual {
    using is_transparent = void;
    /**
     * @brief Compares two ASCII strings ignoring case.
     * @param a First string.
     * @param b Second string.
     * @return true if the strings are equal ignoring ASCII case.
     */
    bool operator()(std::string_view a, std::string_view b) const noexcept;
};

// ---------------------------------------------------------------------------
// Transparent hash/equality for query parameters.
// IMPORTANT: query keys are CASE-SENSITIVE ("?User=1" != "?user=1"),
// so exact comparison is used here, not CaseInsensitive*.
// ---------------------------------------------------------------------------

/**
 * @brief Transparent hash for query parameter keys.
 *
 * Uses exact (case-sensitive) hashing of the key.
 */
struct TransparentStringHash {
    using is_transparent = void;
    /**
     * @brief Hashes a query parameter key.
     * @param sv Key.
     * @return Hash value.
     */
    std::size_t operator()(std::string_view sv) const noexcept
    {
        return std::hash<std::string_view> { }(sv);
    }
};

/**
 * @brief Transparent equality for query parameter keys.
 *
 * Uses exact (case-sensitive) comparison.
 */
struct TransparentStringEqual {
    using is_transparent = void;
    /**
     * @brief Compares two query parameter keys exactly.
     * @param a First key.
     * @param b Second key.
     * @return true if the keys are identical.
     */
    bool operator()(std::string_view a, std::string_view b) const noexcept
    {
        return a == b;
    }
};

/**
 * @brief Parsed HTTP request.
 *
 * Stores the request line (method, target, version), parsed path and query
 * string, headers, body, and per-connection information (remote address,
 * keep-alive flag). Header names are case-insensitive per RFC 9110; query
 * parameter names are case-sensitive. Both maps use transparent hashers and
 * equality comparators, so lookups can be performed with std::string_view
 * keys without allocating.
 */
class Request final {
public:
    /// Header map: case-insensitive names.
    using headers_t = std::unordered_map<
        std::string,
        std::string,
        CaseInsensitiveHash,
        CaseInsensitiveEqual>;

    /// Query parameter map: case-sensitive names.
    using query_params_t = std::unordered_map<
        std::string,
        std::string,
        TransparentStringHash,
        TransparentStringEqual>;

    /**
     * @brief Reads and parses a single HTTP request from @p connection.
     *
     * Steps:
     *  1. Reads the header block up to and including "\r\n\r\n"
     *     (readHeaders enforces a size limit).
     *  2. Parses the request line and headers (parseHeaderBlock).
     *  3. Determines the body length from Content-Length, if present
     *     (parseContentLength).
     *  4. Rejects chunked transfer encoding (not supported yet) and reads
     *     the body: any bytes already buffered after the header block are
     *     reused first, the rest is read with readExact.
     *  5. Computes the keep-alive flag from the HTTP version and the
     *     Connection header.
     *  6. Stores the client's remote address.
     *
     * @param connection Client connection to read from.
     * @return The parsed request, or std::nullopt on I/O error, malformed
     *         request, unsupported Transfer-Encoding, or missing/invalid
     *         Content-Length.
     */
    [[nodiscard]] static std::optional<Request> make(const kas::net::socket::ClientConnection& connection);

    /// Constructs an empty request (HTTP/1.1, keep-alive enabled).
    Request() = default;

    /**
     * @brief Constructs a request from its request line components.
     *
     * The target is split into path and query string, and query parameters
     * are parsed.
     *
     * @param method  HTTP method.
     * @param target  Request target (for example, "/path?x=1").
     * @param version HTTP version.
     */
    Request(std::string_view method, std::string_view target, HttpVersion version = HttpVersion::HTTP_1_1);

    Request(const Request&) = default;
    Request& operator=(const Request&) = default;
    Request(Request&&) noexcept = default;
    Request& operator=(Request&&) noexcept = default;
    ~Request() = default;

    // ---------------- method ----------------

    /**
     * @brief Returns the HTTP method.
     * @return Method string.
     */
    [[nodiscard]] const std::string& getMethod() const& noexcept;

    /**
     * @brief Sets the HTTP method.
     * @param method Method string.
     * @return Reference to this request (lvalue overload).
     */
    Request& setMethod(std::string method) &;

    /**
     * @brief Sets the HTTP method.
     * @param method Method string.
     * @return Moved reference to this request (rvalue overload).
     */
    Request&& setMethod(std::string method) &&;

    // ---------------- target / path / query ----------------

    /**
     * @brief Returns the raw request target.
     * @return Target string as received.
     */
    [[nodiscard]] const std::string& getTarget() const& noexcept;

    /**
     * @brief Sets the request target and re-parses path, query, and params.
     * @param target New request target.
     * @return Reference to this request.
     */
    Request& setTarget(std::string_view target) &;

    /**
     * @brief Returns the path component of the target.
     * @return Path (without query string).
     */
    [[nodiscard]] const std::string& getPath() const& noexcept;

    /**
     * @brief Returns the query string (without the leading '?').
     * @return Query string.
     */
    [[nodiscard]] const std::string& getQueryString() const& noexcept;

    /**
     * @brief Looks up a query parameter by key.
     *
     * Transparent lookup: no allocation for std::string_view keys. If the
     * key appears multiple times (for example, "?x=1&x=2"), the first
     * occurrence is returned.
     *
     * @param key Query parameter name (case-sensitive).
     * @return Parameter value, or std::nullopt if not present.
     */
    [[nodiscard]] std::optional<std::string_view> getQueryParam(std::string_view key) const&;

    /**
     * @brief Returns all parsed query parameters.
     * @return Query parameter map.
     */
    [[nodiscard]] const query_params_t& getQueryParams() const& noexcept;

    // ---------------- version ----------------

    /**
     * @brief Returns the HTTP version.
     * @return Protocol version.
     */
    [[nodiscard]] HttpVersion getVersion() const noexcept;

    /**
     * @brief Sets the HTTP version.
     * @param v New protocol version.
     * @return Reference to this request.
     */
    Request& setVersion(HttpVersion v) & noexcept;

    /**
     * @brief Converts a version enum to its string form (for example, "HTTP/1.1").
     * @param v Protocol version.
     * @return String representation.
     */
    [[nodiscard]] static std::string_view versionToString(HttpVersion v) noexcept;

    /**
     * @brief Returns the string form of this request's version.
     * @return String representation of the current version.
     */
    [[nodiscard]] std::string_view getVersionString() const noexcept;

    // ---------------- headers ----------------

    /**
     * @brief Sets a header value, replacing any existing value.
     *
     * Header names are case-insensitive per RFC 9110.
     *
     * @param name  Header name.
     * @param value Header value.
     * @return Reference to this request.
     */
    Request& setHeader(std::string_view name, std::string_view value) &;

    /**
     * @brief Looks up a header by name (case-insensitive).
     *
     * Transparent lookup: no allocation for std::string_view keys.
     *
     * @param name Header name.
     * @return Header value, or std::nullopt if not present.
     */
    [[nodiscard]] std::optional<std::string_view> getHeader(std::string_view name) const&;

    /**
     * @brief Checks whether a header is present (case-insensitive).
     * @param name Header name.
     * @return true if the header exists, false otherwise.
     */
    [[nodiscard]] bool hasHeader(std::string_view name) const;

    /**
     * @brief Returns all headers.
     * @return Header map.
     */
    [[nodiscard]] const headers_t& getHeaders() const& noexcept;

    // ---------------- body ----------------

    /**
     * @brief Sets the request body, replacing any existing content.
     * @param body New body.
     * @return Reference to this request.
     */
    Request& setBody(std::string body) &;

    /**
     * @brief Returns the request body.
     * @return Body content.
     */
    [[nodiscard]] const std::string& getBody() const& noexcept;

    /**
     * @brief Returns the body size in bytes.
     * @return Body size.
     */
    [[nodiscard]] std::size_t getBodySize() const noexcept;

    // ---------------- connection info ----------------

    /**
     * @brief Sets the remote peer address.
     * @param addr Remote address string.
     * @return Reference to this request.
     */
    Request& setRemoteAddress(std::string addr) &;

    /**
     * @brief Returns the remote peer address.
     * @return Remote address string.
     */
    [[nodiscard]] const std::string& getRemoteAddress() const& noexcept;

    /**
     * @brief Sets the keep-alive flag for the connection.
     * @param keep_alive Whether the connection may be reused.
     * @return Reference to this request.
     */
    Request& setKeepAlive(bool keep_alive) & noexcept;

    /**
     * @brief Reports whether the connection may be kept alive.
     * @return true if keep-alive is enabled.
     */
    [[nodiscard]] bool isKeepAlive() const noexcept;

private:
    /**
     * @brief Splits the request target into path and query, and parses the query.
     *
     * The target is expected to have the form "path[?query]":
     *  - if there is no '?', the whole target is the path, and the query
     *    string and query parameters are cleared;
     *  - otherwise, m_path is the part before the first '?', and m_query is
     *    everything after it.
     *
     * The query string is parsed as a sequence of "key=value" pairs
     * separated by '&'. A pair without '=' is stored with an empty value.
     * Duplicate keys keep only the first occurrence (subsequent duplicates
     * are ignored). No percent-decoding is performed.
     *
     * Must be called after m_target has been set; setTarget() does this
     * automatically.
     */
    void splitTarget();

private:
    std::string m_method; ///< HTTP method.
    std::string m_target; ///< Raw request target.
    std::string m_path; ///< Path component of the target.
    std::string m_query; ///< Query string component (without '?').
    query_params_t m_query_params; ///< Parsed query parameters.
    HttpVersion m_version = HttpVersion::HTTP_1_1; ///< HTTP version.
    headers_t m_headers; ///< Request headers.
    std::string m_body; ///< Request body.
    std::string m_remote; ///< Remote peer address.
    bool m_keep_alive = true; ///< Whether the connection may be reused.
};

} // namespace kas::net::http

#endif // KASLIBCXX_NET_SOCKET_REQUEST_H