/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_STREAM_COTS_V3_COLOR_PROPERTIES_H
#define KASLIBCXX_STREAM_COTS_V3_COLOR_PROPERTIES_H

#include <string_view>

namespace kas::stream::cots::v3 {

/**
 * @brief ANSI SGR color properties (text attributes).
 *
 * Each enumerator identifies one ANSI Select Graphic Rendition (SGR)
 * attribute. The corresponding ColorProperty<T> specialization exposes
 * the numeric SGR code as a string_view, e.g. "1" for Bold. The
 * ColorOutToStream wrapper writes these codes into the stream.
 *
 * Numeric values follow ECMA-48 / ANSI X3.64:
 *  - Bold          -> 1
 *  - Faint         -> 2
 *  - Italic        -> 3
 *  - Underline     -> 4
 *  - CrossedOut    -> 9
 *  - Reset         -> 0
 *  - ResetFg       -> 39
 *  - ResetBg       -> 49
 *  - Default       -> 0
 *  - None          -> "" (empty; no code is emitted)
 *
 * @par Default vs None
 * Default and Reset both map to "0" (full SGR reset). None maps to
 * an empty string, meaning "no code": ColorOutToStream will not emit
 * anything for None.
 */
enum class ColorProperties {
    Bold, ///< Bold text (SGR 1).
    Faint, ///< Faint/dim text (SGR 2).
    Italic, ///< Italic text (SGR 3).
    Underline, ///< Underlined text (SGR 4).
    CrossedOut, ///< Struck-through text (SGR 9).
    Reset, ///< Full reset (SGR 0).
    ResetFg, ///< Reset foreground color (SGR 39).
    ResetBg, ///< Reset background color (SGR 49).
    None, ///< No property; emits an empty code.
    Default ///< Default/reset (SGR 0).
};

/**
 * @brief Maps a ColorProperties enumerator to its ANSI SGR code.
 *
 * The primary template is left undefined on purpose: only the
 * enumerators listed in ColorProperties have a specialization. Using
 * ColorProperty<T> with an unsupported T is a compile-time error,
 * which keeps the mapping exhaustive.
 *
 * @tparam T A ColorProperties enumerator.
 */
template <ColorProperties T>
struct ColorProperty;

/// SGR code for ColorProperties::Default (full reset).
template <>
struct ColorProperty<ColorProperties::Default> {
    static constexpr std::string_view code { "0" };
};

/// SGR code for ColorProperties::Bold.
template <>
struct ColorProperty<ColorProperties::Bold> {
    static constexpr std::string_view code { "1" };
};

/// SGR code for ColorProperties::Faint.
template <>
struct ColorProperty<ColorProperties::Faint> {
    static constexpr std::string_view code { "2" };
};

/// SGR code for ColorProperties::Italic.
template <>
struct ColorProperty<ColorProperties::Italic> {
    static constexpr std::string_view code { "3" };
};

/// SGR code for ColorProperties::Underline.
template <>
struct ColorProperty<ColorProperties::Underline> {
    static constexpr std::string_view code { "4" };
};

/// SGR code for ColorProperties::CrossedOut.
template <>
struct ColorProperty<ColorProperties::CrossedOut> {
    static constexpr std::string_view code { "9" };
};

/// SGR code for ColorProperties::Reset (full reset).
template <>
struct ColorProperty<ColorProperties::Reset> {
    static constexpr std::string_view code { "0" };
};

/// SGR code for ColorProperties::ResetFg (reset foreground).
template <>
struct ColorProperty<ColorProperties::ResetFg> {
    static constexpr std::string_view code { "39" };
};

/// SGR code for ColorProperties::ResetBg (reset background).
template <>
struct ColorProperty<ColorProperties::ResetBg> {
    static constexpr std::string_view code { "49" };
};

/// SGR code for ColorProperties::None (empty; no code is emitted).
template <>
struct ColorProperty<ColorProperties::None> {
    static constexpr std::string_view code { "" };
};

} // namespace kas::stream::cots::v3

#endif