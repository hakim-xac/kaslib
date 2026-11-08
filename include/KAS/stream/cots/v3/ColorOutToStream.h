/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_STREAM_COTS_V3_H
#define KASLIBCXX_STREAM_COTS_V3_H

#include "ColorProperties.h"
#include "ColorTypes.h"

#include <iomanip>
#include <iostream>
#include <string_view>

namespace kas::stream::cots::v3 {

/**
 * @brief Stream wrapper that prints colored values with a delimiter.
 *
 * ColorOutToStream forwards values to an output stream, inserting a
 * delimiter between consecutive values. It also supports ANSI color
 * codes and color properties (see ColorTypes and ColorProperties).
 * Color codes are emitted lazily: they are written to the stream only
 * before the first value of a print() call, not when setColor() or
 * setColorProperty() is called.
 *
 * The class is designed for chained use. Every mutator has both an
 * lvalue (&) and an rvalue (&&) overload so a chain can continue on a
 * temporary:
 *
 * @code
 * ColorOutToStream(std::cout)
 *     .setColor<ColorType::RED>()
 *     .print("error:", 42)
 *     .ln();
 * @endcode
 *
 * The output stream is not owned: it is stored as a pointer and must
 * outlive the ColorOutToStream object.
 *
 * @par Delimiter
 * The delimiter is a non-owning std::string_view used between values.
 * The caller must keep the delimiter storage alive as long as the
 * ColorOutToStream object may use it.
 *
 * @par Thread safety
 * ColorOutToStream is not thread-safe. All calls on the same object
 * must be serialized by the caller.
 */
class ColorOutToStream final {
public:
    /**
     * @brief Constructs the wrapper.
     * @param out       Output stream. Must outlive *this; stored as a
     *                  non-owning pointer.
     * @param delimiter Delimiter inserted between values. Non-owning;
     *                  must outlive *this.
     */
    explicit ColorOutToStream(std::ostream& out, std::string_view delimiter = " ");

    /**
     * @brief Sets the delimiter between values.
     * @param delimiter New delimiter. Non-owning; must outlive *this.
     * @return *this, to allow chaining.
     */
    ColorOutToStream& setDelimiter(std::string_view delimiter) &;

    /**
     * @brief Sets the delimiter on a temporary.
     * @param delimiter New delimiter. Non-owning; must outlive the
     *                  resulting temporary.
     * @return std::move(*this), to allow chaining on a temporary.
     */
    ColorOutToStream&& setDelimiter(std::string_view delimiter) &&;

    /// Writes a single space. @return *this.
    ColorOutToStream& space() &;

    /// Writes a single space on a temporary. @return std::move(*this).
    ColorOutToStream&& space() &&;

    /**
     * @brief Writes indentation.
     * @param count_tabs  Number of tab stops.
     * @param count_space Number of spaces per tab stop.
     * @return *this.
     */
    ColorOutToStream& tab(size_t count_tabs = 1, size_t count_space = 4) &;

    /**
     * @brief Writes indentation on a temporary.
     * @param count_tabs  Number of tab stops.
     * @param count_space Number of spaces per tab stop.
     * @return std::move(*this).
     */
    ColorOutToStream&& tab(size_t count_tabs = 1, size_t count_space = 4) &&;

    /// Writes a newline. @return *this.
    ColorOutToStream& ln() &;

    /// Writes a newline on a temporary. @return std::move(*this).
    ColorOutToStream&& ln() &&;

    /**
     * @brief Replaces the output stream.
     * @param out New output stream. Must outlive *this.
     * @return *this.
     */
    ColorOutToStream& setOut(std::ostream& out) &;

    /**
     * @brief Replaces the output stream on a temporary.
     * @param out New output stream. Must outlive the resulting temporary.
     * @return std::move(*this).
     */
    ColorOutToStream&& setOut(std::ostream& out) &&;

    /// Returns the current output stream.
    [[nodiscard]] std::ostream& getOut();

    /// Emits the ANSI reset sequence and clears the printed flags.
    ColorOutToStream& colorReset() &;

    /// Emits the ANSI reset sequence on a temporary.
    ColorOutToStream&& colorReset() &&;

    /// Resets all color state (codes and printed flags).
    ColorOutToStream& reset() &;

    /// Resets all color state on a temporary.
    ColorOutToStream&& reset() &&;

    /// Resets only the color-property state.
    ColorOutToStream& colorPropertyReset() &;

    /// Resets only the color-property state on a temporary.
    ColorOutToStream&& colorPropertyReset() &&;

    /**
     * @brief Emits pending color codes, if any.
     *
     * Prints the currently selected color and color-property codes to
     * the output stream once. Subsequent calls are no-ops until the
     * state is reset or a new color is selected.
     *
     * @return *this.
     */
    ColorOutToStream& print() &;

    /// Emits pending color codes on a temporary.
    ColorOutToStream&& print() &&;

    /**
     * @brief Prints values without emitting color codes.
     *
     * Values are separated by the delimiter. Color state is ignored.
     *
     * @return *this.
     */
    ColorOutToStream& printWithOutColor() &;

    /// Prints values without color on a temporary.
    ColorOutToStream&& printWithOutColor() &&;

    /// Flushes the underlying output stream.
    void flush();

    /**
     * @brief Selects a foreground/background color.
     *
     * The code is stored but not written until print() is called.
     *
     * @tparam TColor Color identifier from ColorTypes.
     * @return *this.
     */
    template <ColorTypes TColor>
    ColorOutToStream& setColor() &;

    /// Selects a color on a temporary.
    template <ColorTypes TColor>
    ColorOutToStream&& setColor() &&;

    /**
     * @brief Selects a color property (bold, underline, etc.).
     *
     * The code is stored but not written until print() is called.
     *
     * @tparam TColorProperty Property identifier from ColorProperties.
     * @return *this.
     */
    template <ColorProperties TColorProperty>
    ColorOutToStream& setColorProperty() &;

    /// Selects a color property on a temporary.
    template <ColorProperties TColorProperty>
    ColorOutToStream&& setColorProperty() &&;

    /**
     * @brief Prints values separated by the delimiter, without color.
     * @param arg  First value.
     * @param args Remaining values.
     * @return *this.
     */
    template <typename Arg, typename... Args>
    ColorOutToStream& printWithOutColor(Arg&& arg, Args&&... args) &;

    /// Prints values without color on a temporary.
    template <typename Arg, typename... Args>
    ColorOutToStream&& printWithOutColor(Arg&& arg, Args&&... args) &&;

    /**
     * @brief Prints values separated by the delimiter, with color.
     *
     * Before the first value, emits the currently selected color and
     * color-property codes (if any) once. Then writes each value,
     * inserting the delimiter between consecutive values.
     *
     * @param arg  First value.
     * @param args Remaining values.
     * @return *this.
     */
    template <typename Arg, typename... Args>
    ColorOutToStream& print(Arg&& arg, Args&&... args) &;

    /// Prints values with color on a temporary.
    template <typename Arg, typename... Args>
    ColorOutToStream&& print(Arg&& arg, Args&&... args) &&;

private:
    /// Writes the pending color and color-property codes to out.
    void printCodesColor(std::ostream& out);

    /// One stored ANSI code plus a "already printed" flag.
    struct Info {
        std::string_view code; ///< ANSI escape sequence; empty means "none".
        bool has_printed; ///< True once the code has been emitted.
    };

    std::string_view m_delimiter; ///< Delimiter between values (non-owning).
    std::ostream* m_out_ptr; ///< Output stream (non-owning).
    Info m_color_impl; ///< Selected color code.
    Info m_color_properties_impl; ///< Selected color-property code.
};
} // namespace kas::stream::cots::v3

namespace kas::stream::cots::v3 {
//------------------

/**
 * @brief Selects a color (lvalue overload).
 * @tparam TColor Color identifier from ColorTypes.
 * @return *this.
 */
template <ColorTypes TColor>
ColorOutToStream& ColorOutToStream::setColor() &
{
    m_color_impl.code = ColorType<TColor>::code;
    m_color_impl.has_printed = false;

    return *this;
}

//------------------

/**
 * @brief Selects a color (rvalue overload).
 * @tparam TColor Color identifier from ColorTypes.
 * @return std::move(*this).
 */
template <ColorTypes TColor>
ColorOutToStream&& ColorOutToStream::setColor() &&
{
    return std::move(this->setColor<TColor>());
}

//------------------

/**
 * @brief Selects a color property (lvalue overload).
 * @tparam TColorProperty Property identifier from ColorProperties.
 * @return *this.
 */
template <ColorProperties TColorProperty>
ColorOutToStream& ColorOutToStream::setColorProperty() &
{
    m_color_properties_impl.code = ColorProperty<TColorProperty>::code;
    m_color_properties_impl.has_printed = false;

    return *this;
}

//------------------

/**
 * @brief Selects a color property (rvalue overload).
 * @tparam TColorProperty Property identifier from ColorProperties.
 * @return std::move(*this).
 */
template <ColorProperties TColorProperty>
ColorOutToStream&& ColorOutToStream::setColorProperty() &&
{
    return std::move(this->setColorProperty<TColorProperty>());
}

//------------------

/**
 * @brief Prints values with color (lvalue overload).
 *
 * Emits the pending color codes once, before the first value, then
 * writes each value separated by the delimiter. If more than one
 * value is passed, the function recurses so that a delimiter is
 * inserted between consecutive values.
 *
 * @param arg  First value.
 * @param args Remaining values.
 * @return *this.
 */
template <typename Arg, typename... Args>
ColorOutToStream& ColorOutToStream::print(Arg&& arg, Args&&... args) &
{
    if (!m_color_impl.has_printed || !m_color_properties_impl.has_printed) {
        printCodesColor(*m_out_ptr);
        m_color_impl.has_printed = true;
        m_color_properties_impl.has_printed = true;
    }

    *m_out_ptr << std::forward<Arg>(arg);

    if constexpr (sizeof...(args) > 0) {
        *m_out_ptr << m_delimiter;
        return print(std::forward<Args>(args)...);
    }

    return *this;
}

//------------------

/**
 * @brief Prints values with color (rvalue overload).
 * @return std::move(*this).
 */
template <typename Arg, typename... Args>
ColorOutToStream&& ColorOutToStream::print(Arg&& arg, Args&&... args) &&
{
    return std::move(this->print(std::forward<Arg>(arg), std::forward<Args>(args)...));
}

//------------------

/**
 * @brief Prints values without color (lvalue overload).
 *
 * Writes each value, inserting the delimiter between consecutive
 * values. Does not emit color codes.
 *
 * @param arg  First value.
 * @param args Remaining values.
 * @return *this.
 */
template <typename Arg, typename... Args>
ColorOutToStream& ColorOutToStream::printWithOutColor(Arg&& arg, Args&&... args) &
{
    *m_out_ptr << std::forward<Arg>(arg);
    if constexpr (sizeof...(args) > 0) {
        *m_out_ptr << m_delimiter;
        return printWithOutColor(std::forward<Args>(args)...);
    }

    return *this;
}

//------------------

/**
 * @brief Prints values without color (rvalue overload).
 * @return std::move(*this).
 */
template <typename Arg, typename... Args>
ColorOutToStream&& ColorOutToStream::printWithOutColor(Arg&& arg, Args&&... args) &&
{
    return std::move(this->printWithOutColor(std::forward<Arg>(arg), std::forward<Args>(args)...));
}

//------------------
} // namespace kas::stream::cots::v3

#endif