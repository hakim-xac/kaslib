/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_STREAM_LOGGER_V1_LOGGER_SYNC_H
#define KASLIBCXX_STREAM_LOGGER_V1_LOGGER_SYNC_H

#include "LoggerBaseInterface.h"

#include <iostream>

namespace kas::stream::logger::v1 {

/**
 * @brief Logger that serializes writes to the output stream.
 *
 * LoggerSync is a LoggerBaseInterface<LoggerSync> implementation that
 * protects every write to the underlying std::ostream with an internal
 * mutex. This makes concurrent logging from multiple threads safe at
 * the stream level: records are not interleaved, and no data race
 * occurs inside ColorOutToStream or the stream buffer.
 *
 * The mutex is held for the duration of each record (color codes,
 * message, reset, newline), so records are atomic with respect to
 * other records written through the same LoggerSync instance.
 *
 * Two LoggerSync objects writing to the same stream are NOT
 * synchronized with each other: each has its own mutex. Use a single
 * shared LoggerSync, or an external lock, when several objects write
 * to the same stream.
 *
 * @par Thread safety
 * Public logging methods (printXxx) and flush() are safe to call
 * concurrently from multiple threads on the same LoggerSync object.
 * Level/enable accessors are atomic in the base class.
 *
 * @par Ownership
 * The output stream is not owned; it must outlive the LoggerSync.
 */
class LoggerSync : public LoggerBaseInterface<LoggerSync> {
public:
    /**
     * @brief Constructs a synchronized logger.
     * @param out Output stream. Must outlive *this; stored as a
     *            non-owning pointer. Defaults to std::cerr.
     */
    explicit LoggerSync(std::ostream& out = std::cerr)
        : m_out_ptr { std::addressof(out) }
        , m_mx { }
    {
    }

    /**
     * @brief Flushes the underlying stream under the internal mutex.
     * @return Const reference to *this.
     */
    const LoggerSync& flush() const&;

    /**
     * @brief Flushes the underlying stream (rvalue overload).
     * @return std::move(*this).
     */
    LoggerSync&& flush() &&;

protected:
    std::ostream* m_out_ptr; ///< Non-owning output stream.
    mutable std::mutex m_mx; ///< Serializes writes to m_out_ptr.

    /**
     * @brief Prints a record without color: message plus newline.
     *
     * Holds m_mx for the whole record.
     *
     * @param args Values to print.
     */
    template <typename... Args>
    void printWithOutColorStyleImpl(Args&&... args) const
    {
        std::lock_guard lk { m_mx };
        kas::stream::cots::v3::ColorOutToStream { *m_out_ptr }
            .print(std::forward<Args>(args)...)
            .ln();
    }

    /**
     * @brief Prints a usual-style record: "[time] (header) text".
     *
     * Holds m_mx for the whole record.
     *
     * @tparam TID  Tag type satisfying as_simple_color_t.
     * @tparam Args Message values.
     * @param time  Formatted timestamp.
     * @param args  Message values.
     */
    template <kas::concepts::as_simple_color_t TID, typename... Args>
    void printUsualStyleImpl(const std::string& time, Args&&... args) const
    {
        std::lock_guard lk { m_mx };
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
     * @brief Prints a verbose-style record with source location.
     *
     * Holds m_mx for the whole record.
     *
     * @tparam TID      Tag type satisfying as_verbose_color_t.
     * @tparam Args     Message values.
     * @param location  Source location of the log call.
     * @param time      Formatted timestamp.
     * @param args      Message values.
     */
    template <kas::concepts::as_verbose_color_t TID, typename... Args>
    void printVerboselStyleImpl(const std::source_location& location, const std::string& time, Args&&... args) const
    {
        std::lock_guard lk { m_mx };
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
    friend LoggerBaseInterface<LoggerSync>;

    /**
     * @brief CRTP hook: plain style.
     * @param args Values to print.
     */
    template <typename... Args>
    void printWithOutColorStyle(Args&&... args) const
    {
        printWithOutColorStyleImpl(std::forward<Args>(args)...);
    }

    /**
     * @brief CRTP hook: usual style.
     * @tparam TID  Tag type satisfying as_simple_color_t.
     * @param time  Formatted timestamp.
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
     * @param time      Formatted timestamp.
     * @param args      Message values.
     */
    template <kas::concepts::as_verbose_color_t TID, typename... Args>
    void printVerboselStyle(const std::source_location& location,
        const std::string& time, Args&&... args) const
    {
        printVerboselStyleImpl<TID>(location, time, std::forward<Args>(args)...);
    }
};

} // namespace kas::stream::logger::v1

namespace kas::stream::logger::v1 {

/**
 * @brief Flushes the underlying stream under the internal mutex.
 * @return Const reference to *this.
 */
inline const LoggerSync&
LoggerSync::flush() const&
{
    std::lock_guard lk { m_mx };
    m_out_ptr->flush();
    return *this;
}

/**
 * @brief Flushes the underlying stream (rvalue overload).
 * @return std::move(*this).
 */
inline LoggerSync&&
LoggerSync::flush() &&
{
    flush();
    return std::move(*this);
}

} // namespace kas::stream::logger::v1

#endif