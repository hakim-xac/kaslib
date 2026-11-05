/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_STREAM_LOGGER_V1_LOGGER_H
#define KASLIBCXX_STREAM_LOGGER_V1_LOGGER_H

#include "LoggerBaseInterface.h"

#include <ostream>

namespace kas::stream::logger::v1 {

/**
 * @brief Concrete logger that writes formatted log records to a stream.
 *
 * Logger implements the formatting hooks required by
 * LoggerBaseInterface<Logger> and forwards the actual output to a
 * non-owning std::ostream (std::cerr by default). Three record styles
 * are supported, selected by the interface:
 *
 *  - plain style: no color, just the message followed by a newline;
 *  - usual style: colored "[time] (header) text" record;
 *  - verbose style: like usual, plus source location and function
 *    name.
 *
 * The output stream is stored as a pointer and must outlive the
 * Logger. The default is std::cerr.
 *
 * @par Thread safety
 * Logger does not synchronize access to the output stream. Concurrent
 * logging from multiple threads requires external synchronization
 * (a mutex or a synchronized stream such as std::osyncstream).
 *
 * @par Ownership
 * Logger does not own the output stream. It is a lightweight,
 * copyable handle; copying is allowed and copies share the same
 * underlying stream.
 */
class Logger : public LoggerBaseInterface<Logger> {
public:
    /**
     * @brief Constructs a logger writing to the given stream.
     * @param out Output stream. Must outlive *this; stored as a
     *            non-owning pointer. Defaults to std::cerr.
     */
    explicit Logger(std::ostream& out = std::cerr)
        : m_out_ptr { std::addressof(out) }
    {
    }

    /**
     * @brief Flushes the underlying stream.
     * @return Const reference to *this, to allow chaining on an lvalue.
     */
    const Logger& flush() const&;

    /**
     * @brief Flushes the underlying stream (rvalue overload).
     * @return std::move(*this), to allow chaining on a temporary.
     */
    Logger&& flush() &&;

protected:
    std::ostream* m_out_ptr; ///< Non-owning output stream.

    /**
     * @brief Prints a record without color: message plus newline.
     *
     * Forwards all arguments to ColorOutToStream::print(), so multiple
     * values are separated by the default delimiter (" "), then
     * terminates the record with a newline.
     *
     * @param args Values to print.
     */
    template <typename... Args>
    void printWithOutColorStyleImpl(Args&&... args) const
    {
        kas::stream::cots::v3::ColorOutToStream { *m_out_ptr }
            .print(std::forward<Args>(args)...)
            .ln();
    }

    /**
     * @brief Prints a usual-style record: "[time] (header) text".
     *
     * Time and header are colored according to TID, the message is
     * colored according to TID's text color, then the color state is
     * reset and a newline is written.
     *
     * @tparam TID  Tag type satisfying as_simple_color_t, providing
     *              colors and header text.
     * @tparam Args Message values.
     * @param time  Formatted timestamp string.
     * @param args  Message values.
     */
    template <kas::concepts::as_simple_color_t TID, typename... Args>
    void printUsualStyleImpl(const std::string& time, Args&&... args) const
    {
        kas::stream::cots::v3::ColorOutToStream { *m_out_ptr }
            .template setColor<TID::color_time>()
            .template setColorProperty<TID::propery_time>()
            .template print("[")
            .template print(time)
            .template print("]")
            .space()
            .template setColor<TID::color_header>()
            .template setColorProperty<TID::propery_header>()
            .template print("(")
            .template print(TID::header)
            .template print(")")
            .space()
            .template setColor<TID::color_text>()
            .template setColorProperty<TID::propery_text>()
            .template print(std::forward<Args>(args)...)
            .reset()
            .ln();
    }

    /**
     * @brief Prints a verbose-style record.
     *
     * Like the usual style, plus source location and function name
     * after the message, each with its own color from TID.
     *
     * @tparam TID      Tag type satisfying as_verbose_color_t, providing
     *                  colors for time, header, text, location and
     *                  function, plus header text.
     * @tparam Args     Message values.
     * @param location  Source location of the log call.
     * @param time      Formatted timestamp string.
     * @param args      Message values.
     */
    template <kas::concepts::as_verbose_color_t TID, typename... Args>
    void printVerboselStyleImpl(const std::source_location& location, const std::string& time, Args&&... args) const
    {
        kas::stream::cots::v3::ColorOutToStream { *m_out_ptr }
            .template setColor<TID::color_time>()
            .template setColorProperty<TID::propery_time>()
            .template print("[")
            .template print(time)
            .template print("]")
            .space()
            .template setColor<TID::color_header>()
            .template setColorProperty<TID::propery_header>()
            .template print("(")
            .template print(TID::header)
            .template print(")")
            .space()
            .template setColor<TID::color_text>()
            .template setColorProperty<TID::propery_text>()
            .template print(std::forward<Args>(args)...)
            .space()
            .template setColor<TID::color_location>()
            .template setColorProperty<TID::propery_location>()
            .template print("(", location.file_name(), ":", location.line(), ")")
            .space()
            .template setColor<TID::color_function>()
            .template setColorProperty<TID::propery_function>()
            .template print("->", location.function_name())
            .reset()
            .ln();
    }

private:
    friend LoggerBaseInterface<Logger>;

    /**
     * @brief CRTP hook: plain style without color.
     * @param args Message values.
     */
    template <typename... Args>
    void printWithOutColorStyle(Args&&... args) const
    {
        printWithOutColorStyleImpl(std::forward<Args>(args)...);
    }

    /**
     * @brief CRTP hook: usual style.
     * @tparam TID  Tag type satisfying as_simple_color_t.
     * @param time  Formatted timestamp string.
     * @param args  Message values.
     */
    template <kas::concepts::as_simple_color_t TID, typename... Args>
    void printUsualStyle(const std::string& time, Args&&... args) const
    {
        printUsualStyleImpl<TID>(time, std::forward<Args>(args)...);
    }

    /**
     * @brief CRTP hook: verbose style.
     * @tparam TID      Tag type satisfying as_verbose_color_t.
     * @param location  Source location of the log call.
     * @param time      Formatted timestamp string.
     * @param args      Message values.
     */
    template <kas::concepts::as_verbose_color_t TID, typename... Args>
    void printVerboselStyle(const std::source_location& location,
        const std::string& time, Args&&... args) const
    {
        printVerboselStyleImpl<TID>(location, time, std::forward<Args>(args)...);
    }
};
}

namespace kas::stream::logger::v1 {

/**
 * @brief Flushes the underlying stream (lvalue overload).
 * @return Const reference to *this.
 */
inline const Logger& Logger::flush() const&
{
    m_out_ptr->flush();
    return *this;
}

/**
 * @brief Flushes the underlying stream (rvalue overload).
 * @return std::move(*this).
 */
inline Logger&& Logger::flush() &&
{
    flush();
    return std::move(*this);
}
}

#endif