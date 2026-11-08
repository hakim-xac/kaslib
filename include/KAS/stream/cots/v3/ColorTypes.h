/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_STREAM_COTS_V3_COLOR_TYPES_H
#define KASLIBCXX_STREAM_COTS_V3_COLOR_TYPES_H

#include <string_view>

namespace kas::stream::cots::v3 {

/**
 * @brief ANSI SGR foreground/background colors.
 *
 * Each enumerator identifies one ANSI Select Graphic Rendition (SGR)
 * color code. The corresponding ColorType<T> specialization exposes
 * the numeric SGR code as a string_view, e.g. "31" for Red. The
 * ColorOutToStream wrapper writes these codes into the stream.
 *
 * Numeric values follow ECMA-48 / ANSI X3.64 for foreground colors:
 *  - Black   -> 30
 *  - Red     -> 31
 *  - Green   -> 32
 *  - Yellow  -> 33
 *  - Blue    -> 34
 *  - Magenta -> 35
 *  - Cyan    -> 36
 *  - White   -> 37
 *  - Reset   -> 0  (full SGR reset)
 *  - Default -> 39 (reset foreground only)
 *  - None    -> "" (empty; no code is emitted)
 *
 * @par Scope
 * These are foreground codes. Background codes are the same numbers
 * plus 10 (40..47, 49). This header currently exposes only foreground
 * codes; add dedicated enumerators if background support is needed.
 *
 * @par Reset vs Default
 * Reset maps to "0" and performs a full SGR reset (foreground,
 * background and all attributes). Default maps to "39" and resets
 * only the foreground color. None maps to an empty string, meaning
 * "no code": ColorOutToStream will not emit anything for None.
 */
enum class ColorTypes {
    Black, ///< Black foreground (SGR 30).
    Red, ///< Red foreground (SGR 31).
    Green, ///< Green foreground (SGR 32).
    Yellow, ///< Yellow foreground (SGR 33).
    Blue, ///< Blue foreground (SGR 34).
    Magenta, ///< Magenta foreground (SGR 35).
    Cyan, ///< Cyan foreground (SGR 36).
    White, ///< White foreground (SGR 37).
    Reset, ///< Full SGR reset (SGR 0).
    Default, ///< Reset foreground only (SGR 39).
    None ///< No color; emits an empty code.
};

/**
 * @brief Maps a ColorTypes enumerator to its ANSI SGR code.
 *
 * The primary template is left undefined on purpose: only the
 * enumerators listed in ColorTypes have a specialization. Using
 * ColorType<T> with an unsupported T is a compile-time error, which
 * keeps the mapping exhaustive.
 *
 * @tparam T A ColorTypes enumerator.
 */
template <ColorTypes T>
struct ColorType;

/// SGR code for ColorTypes::Black.
template <>
struct ColorType<ColorTypes::Black> {
    static constexpr std::string_view code { "30" };
};

/// SGR code for ColorTypes::Red.
template <>
struct ColorType<ColorTypes::Red> {
    static constexpr std::string_view code { "31" };
};

/// SGR code for ColorTypes::Green.
template <>
struct ColorType<ColorTypes::Green> {
    static constexpr std::string_view code { "32" };
};

/// SGR code for ColorTypes::Yellow.
template <>
struct ColorType<ColorTypes::Yellow> {
    static constexpr std::string_view code { "33" };
};

/// SGR code for ColorTypes::Blue.
template <>
struct ColorType<ColorTypes::Blue> {
    static constexpr std::string_view code { "34" };
};

/// SGR code for ColorTypes::Magenta.
template <>
struct ColorType<ColorTypes::Magenta> {
    static constexpr std::string_view code { "35" };
};

/// SGR code for ColorTypes::Cyan.
template <>
struct ColorType<ColorTypes::Cyan> {
    static constexpr std::string_view code { "36" };
};

/// SGR code for ColorTypes::White.
template <>
struct ColorType<ColorTypes::White> {
    static constexpr std::string_view code { "37" };
};

/// SGR code for ColorTypes::Reset (full SGR reset).
template <>
struct ColorType<ColorTypes::Reset> {
    static constexpr std::string_view code { "0" };
};

/// SGR code for ColorTypes::Default (reset foreground only).
template <>
struct ColorType<ColorTypes::Default> {
    static constexpr std::string_view code { "39" };
};

/// SGR code for ColorTypes::None (empty; no code is emitted).
template <>
struct ColorType<ColorTypes::None> {
    static constexpr std::string_view code { "" };
};

} // namespace kas::stream::cots::v3

#endif