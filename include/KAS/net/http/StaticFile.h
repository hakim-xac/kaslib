/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_NET_HTTP_STATIC_FILE_H
#define KASLIBCXX_NET_HTTP_STATIC_FILE_H

#include "ContentTypes.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace kas::net::http {

// ---------------------------------------------------------------------------
// MIME type by file extension.
// Returns ContentTypes::OCTET_STREAM::type if the extension is unknown.
// Extension comparison is case-insensitive.
// ---------------------------------------------------------------------------

/**
 * @brief Resolves a MIME type from a file extension.
 *
 * Extracts the extension from @p path (the part after the last '.'), folds
 * it to lowercase, and looks it up in a static table that maps extensions
 * to @c ContentTypes tags. Unknown or missing extensions map to
 * @c ContentTypes::OCTET_STREAM::type.
 *
 * Extensions longer than 15 characters are treated as unknown without
 * building the lowercase buffer.
 *
 * @param path File path or name.
 * @return MIME type as a string_view (static storage).
 */
inline std::string_view mimeByExtension(std::string_view path) noexcept
{
    const auto dot = path.find_last_of('.');
    if (dot == std::string_view::npos || dot + 1 >= path.size())
        return ContentTypes::OCTET_STREAM::type;

    std::string_view ext = path.substr(dot + 1);

    // Fold to lowercase. Extensions longer than 15 characters do not occur,
    // so return octet-stream immediately.
    char lower[16] { };
    if (ext.size() >= sizeof(lower))
        return ContentTypes::OCTET_STREAM::type;
    for (std::size_t i = 0; i < ext.size(); ++i) {
        const char c = ext[i];
        lower[i] = (c >= 'A' && c <= 'Z')
            ? static_cast<char>(c - 'A' + 'a')
            : c;
    }
    ext = std::string_view { lower, ext.size() };

    // Extension-to-ContentTypes table. MIME strings are not duplicated;
    // they are taken from ContentTypes.
    struct Entry {
        std::string_view ext;
        std::string_view type;
    };
    static constexpr Entry table[] = {
        { "txt", ContentTypes::TXT::type },
        { "html", ContentTypes::HTML::type },
        { "htm", ContentTypes::HTML::type },
        { "css", ContentTypes::CSS::type },
        { "js", ContentTypes::JS::type },
        { "json", ContentTypes::JSON::type },
        { "xml", ContentTypes::XML::type },
        { "csv", ContentTypes::CSV::type },
        { "pdf", ContentTypes::PDF::type },
        { "png", ContentTypes::PNG::type },
        { "jpg", ContentTypes::JPEG::type },
        { "jpeg", ContentTypes::JPEG::type },
        { "gif", ContentTypes::GIF::type },
        { "svg", ContentTypes::SVG::type },
        { "webp", ContentTypes::WEBP::type },
        { "aac", ContentTypes::AAC::type },
        { "avi", ContentTypes::AVI::type },
        { "bmp", ContentTypes::BMP::type },
        { "bz", ContentTypes::BZ::type },
        { "bz2", ContentTypes::BZ2::type },
        { "doc", ContentTypes::DOC::type },
        { "docx", ContentTypes::DOCX::type },
        { "ico", ContentTypes::ICO::type },
        { "gz", ContentTypes::GZ::type },
        { "jar", ContentTypes::JAR::type },
        { "m3u8", ContentTypes::M3U8::type },
        { "md", ContentTypes::MD::type },
        { "mp3", ContentTypes::MP3::type },
        { "mp4", ContentTypes::MP4::type },
        { "mpeg", ContentTypes::MPEG::type },
        { "tar", ContentTypes::TAR::type },
        { "tiff", ContentTypes::TIFF::type },
        { "ts", ContentTypes::TS::type },
        { "ttf", ContentTypes::TTF::type },
        { "wav", ContentTypes::WAV::type },
        { "weba", ContentTypes::WEBA::type },
        { "webm", ContentTypes::WEBM::type },
        { "woff", ContentTypes::WOFF::type },
        { "woff2", ContentTypes::WOFF2::type },
        { "xhtml", ContentTypes::XHTML::type },
        { "xls", ContentTypes::XLS::type },
        { "xlsx", ContentTypes::XLSX::type },
        { "7z", ContentTypes::_7Z::type },
    };

    for (const auto& e : table) {
        if (e.ext == ext)
            return e.type;
    }

    return ContentTypes::OCTET_STREAM::type;
}

// ---------------------------------------------------------------------------
// File name from a path (for Content-Disposition).
// ---------------------------------------------------------------------------

/**
 * @brief Extracts the file name from a path.
 *
 * Returns everything after the last '/' or '\'.
 *
 * @param path File path.
 * @return File name (a suffix of @p path).
 */
inline std::string_view fileNameFromPath(std::string_view path) noexcept
{
    const auto slash = path.find_last_of("/\\");
    return (slash == std::string_view::npos) ? path : path.substr(slash + 1);
}

// ---------------------------------------------------------------------------
// Content-Disposition: "<type>; filename=\"...\""
// type: "attachment" | "inline".
// Quotes, '\', CR, and LF are stripped from the file name.
// ---------------------------------------------------------------------------

/**
 * @brief Builds a Content-Disposition header value.
 *
 * Produces "<type>; filename=\"<sanitized-name>\"". Double quotes,
 * backslashes, CR, and LF are removed from @p filename to avoid header
 * injection and quoting issues.
 *
 * @param filename File name to embed.
 * @param type     Disposition type, "attachment" (default) or "inline".
 * @return Content-Disposition header value.
 */
inline std::string makeContentDisposition(
    std::string_view filename,
    std::string_view type = "attachment")
{
    std::string out;
    out.reserve(type.size() + filename.size() + 32);
    out += type;
    out += "; filename=\"";
    for (char c : filename) {
        if (c == '"' || c == '\\' || c == '\r' || c == '\n')
            continue;
        out += c;
    }
    out += '"';
    return out;
}

// ---------------------------------------------------------------------------
// Reads a whole file into memory.
// Returns std::nullopt if the file cannot be opened or size > max_bytes.
// Used when a file must be served via setBody() (small files).
// ---------------------------------------------------------------------------

/**
 * @brief Reads an entire file into memory.
 *
 * @param path      Path to the file.
 * @param max_bytes Maximum allowed file size.
 * @return File contents, or std::nullopt if the file cannot be opened or
 *         its size exceeds @p max_bytes.
 */
inline std::optional<std::string> readFileToString(
    const std::filesystem::path& path,
    std::size_t max_bytes)
{
    std::error_code ec;
    const auto sz = std::filesystem::file_size(path, ec);
    if (ec)
        return std::nullopt;
    if (static_cast<std::size_t>(sz) > max_bytes)
        return std::nullopt;

    std::ifstream in(path, std::ios::binary);
    if (!in)
        return std::nullopt;

    std::string data(static_cast<std::size_t>(sz), '\0');
    if (!data.empty()
        && !in.read(data.data(), static_cast<std::streamsize>(data.size()))) {
        return std::nullopt;
    }
    return data;
}

// ---------------------------------------------------------------------------
// Resolves a URL path to a file inside root_dir, protecting against
// path traversal.
//
// - endpoint: path from the request, for example "/css/app.css" (leading
//   '/' characters are stripped).
// - root_dir: static files root.
//
// Returns nullopt if:
//   - endpoint escapes root_dir ("..", absolute path, etc.);
//   - the target is not a regular file (directory, symlink, ...).
//
// The returned path is already normalized (lexically_normal).
// ---------------------------------------------------------------------------

/**
 * @brief Resolves a URL path to a regular file inside a static root.
 *
 * Strips leading '/' characters from @p endpoint, normalizes the relative
 * path, joins it with @p root_dir, and verifies that the result is still
 * inside @p root_dir (protection against path traversal). Only regular
 * files are accepted; directories and symlinks pointing outside are
 * rejected.
 *
 * @param endpoint Request path (for example, "/css/app.css").
 * @param root_dir Static files root directory.
 * @return Normalized path to the file, or std::nullopt if the path escapes
 *         the root, the target is not a regular file, or the endpoint is
 *         empty after stripping.
 */
inline std::optional<std::filesystem::path> tryResolveStaticPath(
    std::string_view endpoint,
    const std::filesystem::path& root_dir)
{
    // Strip leading '/' characters; otherwise filesystem::path{"/css"} is
    // absolute and base / rel yields "/css" instead of "base/css".
    std::string_view rel_view = endpoint;
    while (!rel_view.empty() && rel_view.front() == '/')
        rel_view.remove_prefix(1);
    if (rel_view.empty())
        return std::nullopt;

    const auto rel = std::filesystem::path { rel_view }.lexically_normal();
    if (rel.empty())
        return std::nullopt;

    const auto file_path = (root_dir / rel).lexically_normal();

    // Prefix check: file_path must start with root_dir.
    auto base_it = root_dir.begin();
    auto file_it = file_path.begin();
    for (; base_it != root_dir.end(); ++base_it, ++file_it) {
        if (file_it == file_path.end() || *file_it != *base_it)
            return std::nullopt; // escaped root_dir
    }

    // Regular files only. Symlinks pointing outside are rejected.
    std::error_code ec;
    if (!std::filesystem::is_regular_file(file_path, ec) || ec)
        return std::nullopt;

    return file_path;
}

} // namespace kas::net::http

#endif // KASLIBCXX_NET_HTTP_STATIC_FILE_H