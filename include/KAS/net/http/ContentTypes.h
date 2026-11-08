/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_NET_HTTP_CONTENT_TYPES_H
#define KASLIBCXX_NET_HTTP_CONTENT_TYPES_H

#include <cstdint>
#include <string_view>

namespace kas::net::http::concepts {

/**
 * @brief Concept for types that describe an HTTP content type.
 *
 * A conforming type must provide:
 * - a nested type alias @c content_t;
 * - a static member @c type convertible to std::string_view (the MIME type).
 */
template <typename T>
concept as_content_type = requires {
    typename T::content_t;
    { T::type } -> std::convertible_to<std::string_view>;
};

/**
 * @brief Concept for content type types that additionally declare a file extension.
 *
 * A conforming type must provide a static member @c extension convertible
 * to std::string_view (for example, "html", "png", "json").
 */
template <typename T>
concept has_extension = requires {
    { T::extension } -> std::convertible_to<std::string_view>;
};
} // namespace kas::net::http::concepts

namespace kas::net::http {
/**
 * @brief Well-known HTTP media types as tag types.
 *
 * Each tag type exposes:
 * - @c content_t: the associated payload type (currently void for all tags);
 * - @c type: the MIME type string (static, constexpr);
 * - @c extension: the conventional file extension (where applicable).
 *
 * Tags satisfy @ref concepts::as_content_type and, for those that provide
 * @c extension, @ref concepts::has_extension. They are intended to be used
 * as compile-time descriptors of response bodies and file formats.
 */
namespace ContentTypes {

    /**
     * @brief application/octet-stream - arbitrary binary data.
     */
    struct OCTET_STREAM {
        using content_t = void;
        static constexpr std::string_view type { "application/octet-stream" };
    };

    /**
     * @brief application/x-www-form-urlencoded - HTML form submission encoding.
     */
    struct FORM_URLENCODED {
        using content_t = void;
        static constexpr std::string_view type { "application/x-www-form-urlencoded" };
    };

    /**
     * @brief multipart/form-data - multipart form submission.
     */
    struct MULTIPART_FORM_DATA {
        using content_t = void;
        static constexpr std::string_view type { "multipart/form-data" };
    };

    /**
     * @brief text/plain - plain text.
     */
    struct TXT {
        using content_t = void;
        static constexpr std::string_view type { "text/plain" };
        static constexpr std::string_view extension { "txt" };
    };

    /**
     * @brief text/html - HTML document.
     */
    struct HTML {
        using content_t = void;
        static constexpr std::string_view type { "text/html" };
        static constexpr std::string_view extension { "html" };
    };

    /**
     * @brief text/css - CSS stylesheet.
     */
    struct CSS {
        using content_t = void;
        static constexpr std::string_view type { "text/css" };
        static constexpr std::string_view extension { "css" };
    };

    /**
     * @brief application/javascript - JavaScript source.
     */
    struct JS {
        using content_t = void;
        static constexpr std::string_view type { "application/javascript" };
        static constexpr std::string_view extension { "js" };
    };

    /**
     * @brief application/json - JSON document.
     */
    struct JSON {
        using content_t = void;
        static constexpr std::string_view type { "application/json" };
        static constexpr std::string_view extension { "json" };
    };

    /**
     * @brief application/xml - XML document.
     */
    struct XML {
        using content_t = void;
        static constexpr std::string_view type { "application/xml" };
        static constexpr std::string_view extension { "xml" };
    };

    /**
     * @brief text/csv - comma-separated values.
     */
    struct CSV {
        using content_t = void;
        static constexpr std::string_view type { "text/csv" };
        static constexpr std::string_view extension { "csv" };
    };

    /**
     * @brief application/pdf - PDF document.
     */
    struct PDF {
        using content_t = void;
        static constexpr std::string_view type { "application/pdf" };
        static constexpr std::string_view extension { "pdf" };
    };

    /**
     * @brief image/png - PNG image.
     */
    struct PNG {
        using content_t = void;
        static constexpr std::string_view type { "image/png" };
        static constexpr std::string_view extension { "png" };
    };

    /**
     * @brief image/jpeg - JPEG image.
     */
    struct JPEG {
        using content_t = void;
        static constexpr std::string_view type { "image/jpeg" };
        static constexpr std::string_view extension { "jpg" };
    };

    /**
     * @brief image/gif - GIF image.
     */
    struct GIF {
        using content_t = void;
        static constexpr std::string_view type { "image/gif" };
        static constexpr std::string_view extension { "gif" };
    };

    /**
     * @brief image/svg+xml - SVG vector image.
     */
    struct SVG {
        using content_t = void;
        static constexpr std::string_view type { "image/svg+xml" };
        static constexpr std::string_view extension { "svg" };
    };

    /**
     * @brief image/webp - WebP image.
     */
    struct WEBP {
        using content_t = void;
        static constexpr std::string_view type { "image/webp" };
        static constexpr std::string_view extension { "webp" };
    };

    /**
     * @brief audio/aac - AAC audio.
     */
    struct AAC {
        using content_t = void;
        static constexpr std::string_view type { "audio/aac" };
        static constexpr std::string_view extension { "aac" };
    };

    /**
     * @brief video/x-msvideo - AVI video.
     */
    struct AVI {
        using content_t = void;
        static constexpr std::string_view type { "video/x-msvideo" };
        static constexpr std::string_view extension { "avi" };
    };

    /**
     * @brief image/bmp - BMP image.
     */
    struct BMP {
        using content_t = void;
        static constexpr std::string_view type { "image/bmp" };
        static constexpr std::string_view extension { "bmp" };
    };

    /**
     * @brief application/x-bzip - bzip archive.
     */
    struct BZ {
        using content_t = void;
        static constexpr std::string_view type { "application/x-bzip" };
        static constexpr std::string_view extension { "bz" };
    };

    /**
     * @brief application/x-bzip2 - bzip2 archive.
     */
    struct BZ2 {
        using content_t = void;
        static constexpr std::string_view type { "application/x-bzip2" };
        static constexpr std::string_view extension { "bz2" };
    };

    /**
     * @brief application/msword - legacy Word document.
     */
    struct DOC {
        using content_t = void;
        static constexpr std::string_view type { "application/msword" };
        static constexpr std::string_view extension { "doc" };
    };

    /**
     * @brief application/vnd.openxmlformats-officedocument.wordprocessingml.document - Word document (OOXML).
     */
    struct DOCX {
        using content_t = void;
        static constexpr std::string_view type { "application/vnd.openxmlformats-officedocument.wordprocessingml.document" };
        static constexpr std::string_view extension { "docx" };
    };

    /**
     * @brief image/x-icon - ICO icon.
     */
    struct ICO {
        using content_t = void;
        static constexpr std::string_view type { "image/x-icon" };
        static constexpr std::string_view extension { "ico" };
    };

    /**
     * @brief application/gzip - gzip archive.
     */
    struct GZ {
        using content_t = void;
        static constexpr std::string_view type { "application/gzip" };
        static constexpr std::string_view extension { "gz" };
    };

    /**
     * @brief application/java-archive - Java JAR archive.
     */
    struct JAR {
        using content_t = void;
        static constexpr std::string_view type { "application/java-archive" };
        static constexpr std::string_view extension { "jar" };
    };

    /**
     * @brief text/javascript - JavaScript source (text variant).
     */
    struct JS_TEXT {
        using content_t = void;
        static constexpr std::string_view type { "text/javascript" };
        static constexpr std::string_view extension { "js" };
    };

    /**
     * @brief application/vnd.apple.mpegurl - HLS playlist.
     */
    struct M3U8 {
        using content_t = void;
        static constexpr std::string_view type { "application/vnd.apple.mpegurl" };
        static constexpr std::string_view extension { "m3u8" };
    };

    /**
     * @brief text/markdown - Markdown document.
     */
    struct MD {
        using content_t = void;
        static constexpr std::string_view type { "text/markdown" };
        static constexpr std::string_view extension { "md" };
    };

    /**
     * @brief audio/mpeg - MP3 audio.
     */
    struct MP3 {
        using content_t = void;
        static constexpr std::string_view type { "audio/mpeg" };
        static constexpr std::string_view extension { "mp3" };
    };

    /**
     * @brief video/mp4 - MP4 video.
     */
    struct MP4 {
        using content_t = void;
        static constexpr std::string_view type { "video/mp4" };
        static constexpr std::string_view extension { "mp4" };
    };

    /**
     * @brief video/mpeg - MPEG video.
     */
    struct MPEG {
        using content_t = void;
        static constexpr std::string_view type { "video/mpeg" };
        static constexpr std::string_view extension { "mpeg" };
    };

    /**
     * @brief application/x-tar - tar archive.
     */
    struct TAR {
        using content_t = void;
        static constexpr std::string_view type { "application/x-tar" };
        static constexpr std::string_view extension { "tar" };
    };

    /**
     * @brief image/tiff - TIFF image.
     */
    struct TIFF {
        using content_t = void;
        static constexpr std::string_view type { "image/tiff" };
        static constexpr std::string_view extension { "tiff" };
    };

    /**
     * @brief video/mp2t - MPEG transport stream.
     */
    struct TS {
        using content_t = void;
        static constexpr std::string_view type { "video/mp2t" };
        static constexpr std::string_view extension { "ts" };
    };

    /**
     * @brief font/ttf - TrueType font.
     */
    struct TTF {
        using content_t = void;
        static constexpr std::string_view type { "font/ttf" };
        static constexpr std::string_view extension { "ttf" };
    };

    /**
     * @brief audio/wav - WAV audio.
     */
    struct WAV {
        using content_t = void;
        static constexpr std::string_view type { "audio/wav" };
        static constexpr std::string_view extension { "wav" };
    };

    /**
     * @brief audio/webm - WebM audio.
     */
    struct WEBA {
        using content_t = void;
        static constexpr std::string_view type { "audio/webm" };
        static constexpr std::string_view extension { "weba" };
    };

    /**
     * @brief video/webm - WebM video.
     */
    struct WEBM {
        using content_t = void;
        static constexpr std::string_view type { "video/webm" };
        static constexpr std::string_view extension { "webm" };
    };

    /**
     * @brief font/woff - WOFF font.
     */
    struct WOFF {
        using content_t = void;
        static constexpr std::string_view type { "font/woff" };
        static constexpr std::string_view extension { "woff" };
    };

    /**
     * @brief font/woff2 - WOFF2 font.
     */
    struct WOFF2 {
        using content_t = void;
        static constexpr std::string_view type { "font/woff2" };
        static constexpr std::string_view extension { "woff2" };
    };

    /**
     * @brief application/xhtml+xml - XHTML document.
     */
    struct XHTML {
        using content_t = void;
        static constexpr std::string_view type { "application/xhtml+xml" };
        static constexpr std::string_view extension { "xhtml" };
    };

    /**
     * @brief application/vnd.ms-excel - legacy Excel spreadsheet.
     */
    struct XLS {
        using content_t = void;
        static constexpr std::string_view type { "application/vnd.ms-excel" };
        static constexpr std::string_view extension { "xls" };
    };

    /**
     * @brief application/vnd.openxmlformats-officedocument.spreadsheetml.sheet - Excel spreadsheet (OOXML).
     */
    struct XLSX {
        using content_t = void;
        static constexpr std::string_view type { "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet" };
        static constexpr std::string_view extension { "xlsx" };
    };

    /**
     * @brief application/x-7z-compressed - 7-Zip archive.
     */
    struct _7Z {
        using content_t = void;
        static constexpr std::string_view type { "application/x-7z-compressed" };
        static constexpr std::string_view extension { "7z" };
    };

} // namespace ContentTypes
} // namespace kas::net::http
#endif