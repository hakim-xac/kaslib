/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_NET_HTTP_RESPONSE_H
#define KASLIBCXX_NET_HTTP_RESPONSE_H

#include <KAS/expected/expected.h>

#include "ContentTypes.h"
#include "ResponseTypes.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace kas::net::http {

// ---------------------------------------------------------------------------
// Transparent hash/equality for header names (RFC 9110: case-insensitive).
// ---------------------------------------------------------------------------

/**
 * @brief Case-insensitive hash for HTTP response header names.
 *
 * Implements the transparent hashing requirement so that header lookups
 * can be performed with std::string_view keys without extra allocations.
 */
struct ResponseHeaderHash {
    using is_transparent = void;
    /**
     * @brief Hashes an ASCII string in a case-insensitive way.
     * @param sv Header name.
     * @return Hash value.
     */
    std::size_t operator()(std::string_view sv) const noexcept;
};

/**
 * @brief Case-insensitive equality for HTTP response header names.
 */
struct ResponseHeaderEqual {
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
// Logical HTTP response: status code + headers + body.
// The body may be a string (m_body) or a file (m_file_body_opt);
// these states are mutually exclusive.
//
// serialize() produces ONLY the headers (including Content-Length from
// getBodySize()); the body is sent via sendTo() - sendfile() for a file,
// sendBytes() for a string.
// ---------------------------------------------------------------------------

/**
 * @brief Logical HTTP response: status code, headers, and body.
 *
 * The body is either an in-memory string or a file, and these two states are
 * mutually exclusive. A file body supports an offset and a window size, which
 * makes it suitable for Range responses (206 Partial Content).
 *
 * Serialization is split into two steps:
 * - @ref serialize produces only the header block (status line and headers,
 *   including an automatically added Content-Length and, if missing, Date);
 * - @ref sendTo writes the headers to the socket and then the body, using
 *   sendfile() for a file body and sendBytes() for a string body, or writes
 *   only the headers when @c skip_body is true (for example, for HEAD).
 *
 * Header names are case-insensitive per RFC 9110, and the header map uses
 * transparent hashing and equality so lookups accept std::string_view
 * without allocating.
 */
class Response final {
public:
    /// Header map: case-insensitive names.
    using headers_t = std::unordered_map<
        std::string,
        std::string,
        ResponseHeaderHash,
        ResponseHeaderEqual>;

    /// Constructs an empty response with status 200 OK.
    Response() = default;

    Response(const Response&) = default;
    Response& operator=(const Response&) = default;
    Response(Response&&) noexcept = default;
    Response& operator=(Response&&) noexcept = default;
    ~Response() = default;

    // ---------------- status ----------------

    /**
     * @brief Returns the numeric status code.
     * @return Status code (for example, 200, 404).
     */
    [[nodiscard]] std::uint16_t getStatusCode() const noexcept;

    /**
     * @brief Returns the full status string: "404 Not Found".
     * @return Status code followed by the reason phrase.
     */
    [[nodiscard]] std::string_view getStatusText() const noexcept;

    /**
     * @brief Returns only the reason phrase: "Not Found".
     * @return Reason phrase without the numeric code.
     */
    [[nodiscard]] std::string_view getReasonPhrase() const noexcept;

    /**
     * @brief Sets the status from a compile-time response type tag.
     *
     * Example: @code setResponseCode<response_types::MethodNotAllowed>(); @endcode
     * The status code is checked at compile time to be in the range 100..599.
     *
     * @tparam T Response type tag (see @c ResponseTypes.h).
     * @return Reference to this response (lvalue overload).
     */
    template <concepts::as_response_type T>
    Response& setResponseCode() &
    {
        static_assert(T::code >= 100 && T::code <= 599,
            "HTTP status code out of range (must be 100..599)");
        m_status = ResponseTypeData { T::code, T::text };
        return *this;
    }

    /**
     * @brief Sets the status from a compile-time response type tag.
     *
     * @tparam T Response type tag (see @c ResponseTypes.h).
     * @return Moved reference to this response (rvalue overload).
     */
    template <concepts::as_response_type T>
    Response&& setResponseCode() &&
    {
        static_assert(T::code >= 100 && T::code <= 599,
            "HTTP status code out of range (must be 100..599)");
        m_status = ResponseTypeData { T::code, T::text };
        return std::move(*this);
    }

    // ---------------- content type ----------------

    /**
     * @brief Returns the Content-Type header value, or an empty view if unset.
     * @return Content-Type value.
     */
    [[nodiscard]] std::string_view getContentType() const noexcept;

    /**
     * @brief Sets Content-Type from a compile-time content type tag.
     *
     * Example: @code setContentType<ContentTypes::JSON>(); @endcode
     *
     * @tparam T Content type tag (see @c ContentTypes.h).
     * @return Reference to this response (lvalue overload).
     */
    template <concepts::as_content_type T>
    Response& setContentType() &
    {
        setHeader("Content-Type", T::type);
        return *this;
    }

    /**
     * @brief Sets Content-Type from a compile-time content type tag.
     *
     * @tparam T Content type tag (see @c ContentTypes.h).
     * @return Moved reference to this response (rvalue overload).
     */
    template <concepts::as_content_type T>
    Response&& setContentType() &&
    {
        setHeader("Content-Type", T::type);
        return std::move(*this);
    }

    // ---------------- headers ----------------

    /**
     * @brief Sets a header value, replacing any existing value.
     *
     * Header names are case-insensitive per RFC 9110.
     *
     * @param name  Header name.
     * @param value Header value.
     * @return Reference to this response (lvalue overload).
     */
    Response& setHeader(std::string_view name, std::string_view value) &;

    /**
     * @brief Sets a header value, replacing any existing value.
     *
     * @param name  Header name.
     * @param value Header value.
     * @return Moved reference to this response (rvalue overload).
     */
    Response&& setHeader(std::string_view name, std::string_view value) &&;

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

    /**
     * @brief Removes a header if present (case-insensitive).
     * @param name Header name.
     * @return Reference to this response.
     */
    Response& removeHeader(std::string_view name) &;

    // ---------------- string body ----------------

    /**
     * @brief Sets the body from a string, replacing any existing body.
     * @param body New body content.
     * @return Reference to this response (lvalue overload).
     */
    Response& setBody(std::string body) &;

    /**
     * @brief Sets the body from a string, replacing any existing body.
     * @param body New body content.
     * @return Moved reference to this response (rvalue overload).
     */
    Response&& setBody(std::string body) &&;

    /**
     * @brief Returns the string body (empty if the body is a file or unset).
     * @return Body content.
     */
    [[nodiscard]] const std::string& getBody() const& noexcept;

    // ---------------- file body ----------------

    /**
     * @brief Sets a file as the response body.
     *
     * - @p start_offset: offset inside the file (for 206 / partial responses);
     * - @p count_bytes: window size; 0 means "until the end of the file".
     *
     * Throws std::runtime_error if file_size() fails and std::out_of_range
     * if @p start_offset is greater than the file size. The Content-Type is
     * set from the file extension if not already present.
     *
     * @param path         Path to the file.
     * @param start_offset Offset inside the file.
     * @param count_bytes  Window size in bytes, 0 for "until end of file".
     * @return Reference to this response (lvalue overload).
     */
    Response& setFileBody(std::filesystem::path path,
        std::size_t start_offset = 0,
        std::size_t count_bytes = 0) &;
    /**
     * @brief Sets a file as the response body.
     *
     * @param path         Path to the file.
     * @param start_offset Offset inside the file.
     * @param count_bytes  Window size in bytes, 0 for "until end of file".
     * @return Moved reference to this response (rvalue overload).
     */
    Response&& setFileBody(std::filesystem::path path,
        std::size_t start_offset = 0,
        std::size_t count_bytes = 0) &&;

    /**
     * @brief Reports whether the body is a file.
     * @return true if a file body is set.
     */
    [[nodiscard]] bool hasFileBody() const noexcept;

    /**
     * @brief Returns the path of the file body.
     *
     * Intended for Range handling in HttpServer.
     *
     * @return File path.
     * @throws std::logic_error if no file body is set.
     */
    [[nodiscard]] const std::filesystem::path& getFilePath() const;

    /**
     * @brief Returns the start offset of the file body window.
     * @return Offset in bytes.
     */
    [[nodiscard]] std::size_t getFileStartOffset() const noexcept;

    /**
     * @brief Returns the size of the file body window.
     * @return Window size in bytes (0 means "until end of file").
     */
    [[nodiscard]] std::size_t getFileCountBytes() const noexcept;

    /**
     * @brief Returns the total size of the underlying file.
     * @return File size in bytes.
     */
    [[nodiscard]] std::size_t getFileSize() const noexcept;

    /**
     * @brief Changes the window inside an already configured file body.
     *
     * Intended for 206 responses with a single range.
     *
     * @param start_offset New start offset.
     * @param count_bytes  New window size.
     * @return Reference to this response.
     * @throws std::logic_error if no file body is set.
     */
    Response& setFileRange(std::size_t start_offset, std::size_t count_bytes) &;

    // ---------------- body (common) ----------------

    /**
     * @brief Returns the logical body size.
     *
     * For a file body: count_bytes, or file_size - start_offset when
     * count_bytes is 0. For a string body: m_body.size().
     *
     * @return Body size in bytes.
     */
    [[nodiscard]] std::size_t getBodySize() const noexcept;

    /**
     * @brief Clears both the string and the file body.
     * @return Reference to this response.
     */
    Response& clearBody() &;

    // ---------------- serialization ----------------

    /**
     * @brief Serializes the response headers into an HTTP/1.1 header block.
     *
     * Produces the status line followed by all headers, a Content-Length
     * (from getBodySize()) and a Date header (from the current time) if
     * they are not already present, and the terminating empty line
     * ("\r\n"). The body is NOT included: it is sent separately by
     * sendTo() (via sendfile() for a file body, or sendBytes() for a
     * string body).
     *
     * @return The header block, ending with "\r\n\r\n".
     */
    [[nodiscard]] std::string serialize() const;

    // ---------------- send ----------------

    /**
     * @brief Sends the headers and body to a socket.
     *
     * - @p skip_body = true: only headers are sent (for HEAD);
     * - file body: sendfile();
     * - string body: sendBytes().
     *
     * @param fd        Socket file descriptor.
     * @param skip_body Whether to skip the body (for HEAD responses).
     * @return An expected result indicating success or failure.
     */
    [[nodiscard]] expected_string<bool> sendTo(int fd, bool skip_body = false) const;

private:
    /// Runtime status storage: code and text taken from the compile-time tag T.
    struct ResponseTypeData {
        std::uint16_t code { 200 }; ///< Status code.
        std::string_view text { "200 OK" }; ///< Status line text.
    };

    /// File body description.
    struct FileBody {
        std::filesystem::path path; ///< File path.
        std::size_t start_offset { 0 }; ///< Offset inside the file.
        std::size_t count_bytes { 0 }; ///< Window size, 0 means "until end of file".
        std::size_t file_size { 0 }; ///< Total file size.
    };

    ResponseTypeData m_status { }; ///< Response status.
    headers_t m_headers; ///< Response headers.
    std::string m_body; ///< String body.
    std::optional<FileBody> m_file_body_opt; ///< File body, if set.
};

} // namespace kas::net::http

#endif // KASLIBCXX_NET_HTTP_RESPONSE_H