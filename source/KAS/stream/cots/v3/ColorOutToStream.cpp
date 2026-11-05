// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include "KAS/stream/cots/cots.h"

namespace kas::stream::cots::v3 {

//------------------

ColorOutToStream::ColorOutToStream(std::ostream& out, std::string_view delimiter)
    : m_delimiter { delimiter }
    , m_out_ptr { std::addressof(out) }
    , m_color_impl { ColorType<ColorTypes::None>::code, false }
    , m_color_properties_impl { ColorProperty<ColorProperties::None>::code, false }
{
}

//------------------

ColorOutToStream& ColorOutToStream::setDelimiter(std::string_view delimiter) &
{
    m_delimiter = delimiter;
    return *this;
}

//------------------

ColorOutToStream&& ColorOutToStream::setDelimiter(std::string_view delimiter) &&
{
    setDelimiter(delimiter);
    return std::move(*this);
}

//------------------

ColorOutToStream& ColorOutToStream::ln() &
{
    *m_out_ptr << '\n';
    return *this;
}

//------------------

ColorOutToStream&& ColorOutToStream::ln() &&
{
    ln();
    return std::move(*this);
}

//------------------

ColorOutToStream& ColorOutToStream::space() &
{
    *m_out_ptr << ' ';
    return *this;
}

//------------------

ColorOutToStream&& ColorOutToStream::space() &&
{
    space();
    return std::move(*this);
}

//------------------

ColorOutToStream& ColorOutToStream::tab(size_t count_tabs, size_t count_space) &
{
    std::string tab_str(count_space * count_tabs, ' ');
    return print(std::move(tab_str));
}

//------------------

ColorOutToStream&& ColorOutToStream::tab(size_t count_tabs, size_t count_space) &&
{
    tab(count_tabs, count_space);
    return std::move(*this);
}

//------------------

ColorOutToStream& ColorOutToStream::setOut(std::ostream& out) &
{
    m_out_ptr = std::addressof(out);
    return *this;
}

//------------------

ColorOutToStream&& ColorOutToStream::setOut(std::ostream& out) &&
{
    setOut(out);
    return std::move(*this);
}

//------------------

std::ostream& ColorOutToStream::getOut()
{
    return *m_out_ptr;
}

//------------------

ColorOutToStream& ColorOutToStream::print() &
{
    return *this;
}

//------------------

ColorOutToStream&& ColorOutToStream::print() &&
{
    return std::move(*this);
}

//------------------

ColorOutToStream& ColorOutToStream::printWithOutColor() &
{
    return *this;
}

//------------------

ColorOutToStream&& ColorOutToStream::printWithOutColor() &&
{
    return std::move(*this);
}

//------------------

ColorOutToStream& ColorOutToStream::colorReset() &
{
    m_color_impl.code = ColorType<ColorTypes::Default>::code;
    m_color_impl.has_printed = false;

    printCodesColor(*m_out_ptr);
    m_color_impl.has_printed = true;

    return *this;
}

//------------------

ColorOutToStream&& ColorOutToStream::colorReset() &&
{
    colorReset();
    return std::move(*this);
}

//------------------

ColorOutToStream& ColorOutToStream::reset() &
{
    *m_out_ptr << "\033[0m";

    m_color_impl.code = ColorType<ColorTypes::None>::code;
    m_color_impl.has_printed = false;

    m_color_properties_impl.code = ColorProperty<ColorProperties::None>::code;
    m_color_properties_impl.has_printed = false;

    return *this;
}

//------------------

ColorOutToStream&& ColorOutToStream::reset() &&
{
    reset();
    return std::move(*this);
}

//------------------

ColorOutToStream& ColorOutToStream::colorPropertyReset() &
{
    // Reset everything (drops both the property and the color)
    *m_out_ptr << "\033[0m";

    // Clear the properties
    m_color_properties_impl.code = ColorProperty<ColorProperties::None>::code;
    m_color_properties_impl.has_printed = true;

    // If a color was set, restore it
    if (!m_color_impl.code.empty()) {
        *m_out_ptr << "\033[" << m_color_impl.code << "m";
        m_color_impl.has_printed = true;
    }

    return *this;
}

//------------------

ColorOutToStream&& ColorOutToStream::colorPropertyReset() &&
{
    colorPropertyReset();
    return std::move(*this);
}

//------------------

void ColorOutToStream::printCodesColor(std::ostream& out)
{
    const auto p { m_color_properties_impl.code };
    const auto c { m_color_impl.code };

    if (p.empty() && c.empty())
        return;

    if (p.empty())
        out << "\033[" << c << "m";
    else if (c.empty())
        out << "\033[" << p << "m";
    else
        out << "\033[" << p << ";" << c << "m";
}

//------------------

void ColorOutToStream::flush()
{
    m_out_ptr->flush();
}

//------------------
} // namespace kas::stream::cots::v3