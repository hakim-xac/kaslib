/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_STREAM_LOGGER_V1_LOGGER_BASE_INTERFACE_H
#define KASLIBCXX_STREAM_LOGGER_V1_LOGGER_BASE_INTERFACE_H

#include "tags.h"

#include <atomic>
#include <chrono>
#include <optional>
#include <source_location>

namespace kas::stream::logger::v1 {

/**
 * @brief CRTP base providing level filtering and record formatting.
 *
 * LoggerBaseInterface<LoggerType> implements the public logging API
 * (print, printTrace, printDebug, ..., printFatal, printException)
 * and the level/enable filtering. The actual output is delegated to
 * the derived LoggerType through the CRTP hooks:
 *
 *  - printWithOutColorStyle(args...)       - plain style;
 *  - printUsualStyle<TID>(time, args...)   - "[time] (header) text";
 *  - printVerboselStyle<TID>(loc, time, args...)
 *                                          - usual + source location
 *                                            and function name.
 *
 * Each printXxx() call:
 *  1. checks whether logging is enabled (setEnable/setDisabled);
 *  2. checks whether the level is allowed (setLogLevel/checkLevel);
 *  3. if both pass, formats the current time and forwards to the
 *     derived class.
 *
 * @par Log levels
 * Level filtering uses a bitmask: a record with code C is emitted
 * when the configured level is 0 (meaning "all") or when
 * (configured & C) != 0. The tags in tags.h define the codes.
 *
 * @par Thread safety
 * enable flag and level are std::atomic, so setEnable/setDisabled/
 * setLogLevel/getEnable/getLogLevel are safe to call concurrently
 * with logging from other threads. The derived class is responsible
 * for synchronizing the actual output stream.
 *
 * @par Lifetime
 * The object does not own the output stream. The derived LoggerType
 * controls where records are written.
 */
template <typename LoggerType>
class LoggerBaseInterface {
public:
    /**
     * @brief Constructs the interface with logging enabled and
     *        level 0 (emit everything).
     */
    LoggerBaseInterface()
        : m_log_level_at { 0 }
        , m_log_enabling_at { true }
    {
    }

    /**
     * @brief Returns whether logging is enabled.
     */
    [[nodiscard]] inline bool getEnable() const&
    {
        return m_log_enabling_at.load();
    }

    /**
     * @brief Enables or disables logging.
     * @param flag true to enable, false to disable.
     */
    inline void setEnable(bool flag) &
    {
        m_log_enabling_at.store(flag);
    }

    /// Enables logging.
    inline void setEnabled() &
    {
        m_log_enabling_at.store(true);
    }

    /// Disables logging.
    inline void setDisabled() &
    {
        m_log_enabling_at.store(false);
    }

    /**
     * @brief Sets the level bitmask.
     * @param log_level Bitmask of allowed levels; 0 means "all".
     */
    inline void setLogLevel(size_t log_level) &
    {
        m_log_level_at.store(log_level);
    }

    /**
     * @brief Returns the current level bitmask.
     */
    inline size_t getLogLevel() const noexcept
    {
        return m_log_level_at.load();
    }

    /**
     * @brief Sets the level bitmask from a list of tag types.
     *
     * The stored value is the sum of the tags' code members. With
     * power-of-two codes this is equivalent to a bitwise OR; with
     * non-power-of-two codes the sum is used as the mask.
     *
     * @tparam TypeErrorLevels Tag types, each providing `code`.
     */
    template <typename... TypeErrorLevels>
    void setLogLevel() &
    {
        m_log_level_at.store((getErrorLevel<TypeErrorLevels>() + ...));
    }

    /**
     * @brief Prints a record in plain style (no color).
     * @param args Values to print.
     */
    template <typename... Args>
    void print(Args&&... args) const
    {
        printWithOutColorStyle(std::forward<Args>(args)...);
    }

    /**
     * @brief Prints a Trace-level record in usual style.
     * @param args Message values.
     */
    template <typename... Args>
    void printTrace(Args&&... args) const
    {
        printUsualStyleImpl<tags::Trace>(std::forward<Args>(args)...);
    }

    /**
     * @brief Prints a Debug-level record in usual style.
     * @param args Message values.
     */
    template <typename... Args>
    void printDebug(Args&&... args) const
    {
        printUsualStyleImpl<tags::Debug>(std::forward<Args>(args)...);
    }

    /**
     * @brief Prints a Log-level record in usual style.
     * @param args Message values.
     */
    template <typename... Args>
    void printLog(Args&&... args) const
    {
        printUsualStyleImpl<tags::Log>(std::forward<Args>(args)...);
    }

    /**
     * @brief Prints an Info-level record in usual style.
     * @param args Message values.
     */
    template <typename... Args>
    void printInfo(Args&&... args) const
    {
        printUsualStyleImpl<tags::Info>(std::forward<Args>(args)...);
    }

    /**
     * @brief Prints a Warning-level record in usual style.
     * @param args Message values.
     */
    template <typename... Args>
    void printWarn(Args&&... args) const
    {
        printUsualStyleImpl<tags::Warning>(std::forward<Args>(args)...);
    }

    /**
     * @brief Prints an Error-level record in verbose style.
     *
     * The source location defaults to the caller's location via
     * std::source_location::current(), so the printed location points
     * at the user's printError() call, not at this header.
     *
     * @param arg      Message value.
     * @param location Call site (defaults to current()).
     */
    template <typename Arg>
    void printError(Arg&& arg, const std::source_location& location = std::source_location::current()) const
    {
        printVerboselStyleImpl<tags::Error>(location, std::forward<Arg>(arg));
    }

    /**
     * @brief Prints a Fatal-level record in verbose style.
     * @param arg      Message value.
     * @param location Call site (defaults to current()).
     */
    template <typename Arg>
    void printFatal(Arg&& arg, const std::source_location& location = std::source_location::current()) const
    {
        printVerboselStyleImpl<tags::Fatal>(location, std::forward<Arg>(arg));
    }

    /**
     * @brief Prints an Exception-level record in verbose style.
     * @param arg      Message value.
     * @param location Call site (defaults to current()).
     */
    template <typename Arg>
    void printException(Arg&& arg, const std::source_location& location = std::source_location::current()) const
    {
        printVerboselStyleImpl<tags::Exception>(location, std::forward<Arg>(arg));
    }

private:
    /**
     * @brief CRTP dispatch: plain style.
     * @param args Values to print.
     */
    template <typename... Args>
    void printWithOutColorStyle(Args&&... args) const
    {
        static_cast<const LoggerType*>(this)->printWithOutColorStyle(std::forward<Args>(args)...);
    }

    /**
     * @brief Filters and forwards a usual-style record.
     *
     * Does nothing if logging is disabled or the level is filtered
     * out. Otherwise formats the current time and calls the derived
     * printUsualStyle<TID>().
     *
     * @tparam TID  Tag type satisfying as_simple_color_t.
     * @tparam Args Message values.
     * @param args  Message values.
     */
    template <kas::concepts::as_simple_color_t TID, typename... Args>
    void printUsualStyleImpl(Args&&... args) const
    {
        if (!m_log_enabling_at) [[unlikely]]
            return;

        if (checkLevel(TID::code)) {
            static_cast<const LoggerType*>(this)->template printUsualStyle<TID>(
                convertTimePointToString(std::chrono::system_clock::now()).value_or("--"),
                std::forward<Args>(args)...);
        }
    }

    /**
     * @brief Filters and forwards a verbose-style record.
     *
     * Does nothing if logging is disabled or the level is filtered
     * out. Otherwise formats the current time and calls the derived
     * printVerboselStyle<TID>().
     *
     * @tparam TID  Tag type satisfying as_verbose_color_t.
     * @tparam Arg  Message value type.
     * @param location Call site.
     * @param arg      Message value.
     */
    template <kas::concepts::as_verbose_color_t TID, typename Arg>
    void printVerboselStyleImpl(const std::source_location& location, Arg&& arg) const
    {
        if (!m_log_enabling_at) [[unlikely]]
            return;

        if (checkLevel(TID::code)) {
            static_cast<const LoggerType*>(this)->template printVerboselStyle<TID>(
                location,
                convertTimePointToString(std::chrono::system_clock::now()).value_or("--"),
                std::forward<Arg>(arg));
        }
    }

    /**
     * @brief Returns the `code` of a tag type.
     * @tparam TypeErrorLevel Tag type providing `code`.
     */
    template <typename TypeErrorLevel>
    [[nodiscard]] size_t getErrorLevel() const noexcept
    {
        return TypeErrorLevel::code;
    }

    /**
     * @brief Checks whether a record with the given level is allowed.
     *
     * Level 0 means "all records are allowed". Otherwise the level
     * mask is tested with a bitwise AND.
     *
     * @param log_level Level mask of the record.
     */
    [[nodiscard]] constexpr bool checkLevel(std::size_t log_level) const noexcept
    {
        const size_t current_log_level { m_log_level_at.load() };
        if (current_log_level == 0)
            return true;

        return (current_log_level & log_level) != 0;
    }

    /**
     * @brief Formats a system_clock time_point as a string in local time.
     *
     * Converts @p time_point to local time and formats it with strftime()
     * using @p format. The result is written into a fixed 64-byte buffer,
     * so the formatted output must fit into 63 characters plus the null
     * terminator.
     *
     * Thread safety:
     *  - Uses localtime_r() on POSIX and localtime_s() on Windows, so
     *    concurrent calls from multiple threads are safe (no shared
     *    static buffer).
     *
     * Locale:
     *  - The output follows the C locale unless setlocale() was called
     *    to change it. Format specifiers are interpreted by strftime().
     *
     * @param time_point Time point to format.
     * @param format     strftime() format string. Defaults to
     *                   "%Y-%m-%d %H:%M:%S".
     *
     * @return The formatted string, or std::nullopt if:
     *         - localtime_r()/localtime_s() failed, or
     *         - strftime() produced no output (empty format, unsupported
     *           specifier, or the result did not fit into the buffer).
     *
     * @note The 64-byte buffer is a deliberate fixed limit. If a longer
     *       format is needed, increase the buffer or switch to
     *       std::format / std::vformat with chrono (C++20).
     */
    [[nodiscard]] inline std::optional<std::string>
    convertTimePointToString(
        const std::chrono::system_clock::time_point& time_point,
        const std::string& format = "%Y-%m-%d %H:%M:%S") const
    {
        const std::time_t time { std::chrono::system_clock::to_time_t(time_point) };
        std::tm timeinfo { };
#if defined(_WIN32)
        if (localtime_s(&timeinfo, &time) != 0)
            return std::nullopt;
#else
        if (localtime_r(&time, &timeinfo) == nullptr)
            return std::nullopt;
#endif

        char buffer[64] { };
        const std::size_t write_bytes = std::strftime(buffer, sizeof(buffer), format.c_str(), &timeinfo);
        if (write_bytes == 0) [[unlikely]]
            return std::nullopt;

        return std::string { buffer };
    }

    std::atomic<size_t> m_log_level_at; ///< Level bitmask; 0 means "all".
    std::atomic_bool m_log_enabling_at; ///< Global enable flag.
};

} // namespace kas::stream::logger::v1

#endif