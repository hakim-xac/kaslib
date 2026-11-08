/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_FUNCTIONAL_LOGTIME_H
#define KASLIBCXX_FUNCTIONAL_LOGTIME_H

#include "MoveOnlyFunction.h"

#include <chrono>
#include <iostream>
#include <optional>
#include <type_traits>

/**
 * @brief Concepts used across the library.
 */
namespace kas::functional::concepts {
/**
 * @brief Constrains a type to a std::chrono duration type.
 *
 * Accepts only the standard duration aliases defined by std::chrono
 * (nanoseconds through years).
 *
 * @tparam TimeType Type to check.
 */
template <typename TimeType>
concept as_time_type_t = std::same_as<TimeType, std::chrono::nanoseconds>
    || std::same_as<TimeType, std::chrono::microseconds>
    || std::same_as<TimeType, std::chrono::milliseconds>
    || std::same_as<TimeType, std::chrono::seconds>
    || std::same_as<TimeType, std::chrono::minutes>
    || std::same_as<TimeType, std::chrono::hours>
    || std::same_as<TimeType, std::chrono::days>
    || std::same_as<TimeType, std::chrono::weeks>
    || std::same_as<TimeType, std::chrono::months>
    || std::same_as<TimeType, std::chrono::years>;
}

namespace kas::functional {
/**
 * @brief Measures elapsed time over a scope and reports it via a callback.
 *
 * 'LogTime' is a RAII timer: it records the start time point on construction
 * and invokes the supplied callback with the elapsed duration on destruction
 * (or on an explicit 'printDifferenceTime()' call). The elapsed time is
 * cast to the duration type 'TTime' selected as the template parameter.
 *
 * The callback receives a single 'std::int64_t' argument: the elapsed time in
 * units of 'TTime' (for example, milliseconds by default).
 *
 * Any exception thrown by the callback is caught and logged to std::cerr;
 * it does not propagate out of the destructor.
 *
 * The class is move-only: the callback is stored in a MoveOnlyFunction.
 *
 * @tparam TTime Duration type used for the reported elapsed time.
 *               Defaults to std::chrono::milliseconds.
 *
 * Example:
 * @code
 * {
 *     LogTime timer { [](std::int64_t ms) {
 *         std::cout << "elapsed: " << ms << " ms\n";
 *     } };
 *     doWork();
 * } // callback is invoked with the elapsed time in milliseconds
 * @endcode
 */
template <concepts::as_time_type_t TTime = std::chrono::milliseconds>
class LogTime final {
public:
    /**
     * @brief Constructs the timer and records the start time point.
     *
     * @tparam TCallback Callable invoked on destruction or on explicit
     *                   printDifferenceTime(); must accept std::int64_t.
     * @param call Callback receiving the elapsed time in units of TTime.
     */
    template <std::invocable<std::int64_t> TCallback>
    explicit LogTime(TCallback&& call)
        : m_callback { std::forward<TCallback>(call) }
        , m_start_time_opt { std::chrono::steady_clock::now() }
    {
    }

    /**
     * @brief Sets a new start time point, restarting the measurement.
     *
     * @param time New start time point; defaults to now.
     * @return *this
     */
    LogTime& setNewTimePoint(std::chrono::steady_clock::time_point time = std::chrono::steady_clock::now()) &
    {
        m_start_time_opt.emplace(time);
        return *this;
    }

    /**
     * @brief Clears the start time point, disabling the callback on
     *        destruction.
     *
     * After this call, printDifferenceTime() is a no-op until a new time
     * point is set via setNewTimePoint().
     *
     * @return *this
     */
    LogTime& resetTimePoint() &
    {
        m_start_time_opt.reset();
        return *this;
    }

    LogTime(const LogTime&) = delete;
    LogTime(LogTime&&) noexcept = default;
    LogTime& operator=(const LogTime&) = delete;
    LogTime& operator=(LogTime&&) noexcept = default;

    /**
     * @brief Invokes the callback with the elapsed time, if a start point
     *        is set.
     *
     * Never throws: exceptions from the callback are caught and reported
     * to std::cerr.
     */
    ~LogTime()
    {
        printDifferenceTime();
    }

    /**
     * @brief Computes the elapsed time and passes it to the callback.
     *
     * Does nothing if no start time point is set (for example, after
     * resetTimePoint()).
     *
     * Exceptions from the callback are caught: std::exception-derived
     * exceptions are logged via ex.what(), others are logged as an
     * "unknown exception" message.
     *
     * @return *this
     */
    LogTime& printDifferenceTime() &
    {
        try {
            if (m_start_time_opt) {
                const std::int64_t diff {
                    [time_point = m_start_time_opt.value()] {
                        const auto now { std::chrono::steady_clock::now() };
                        return std::chrono::duration_cast<TTime>(now - time_point).count();
                    }()
                };

                m_callback(diff);
            }
        } catch (const std::exception& ex) {
            std::cerr << ex.what() << '\n';
        } catch (...) {
            std::cerr << "unknown exception in LogTime callback\n";
        }

        return *this;
    }

private:
    MoveOnlyFunction<void(std::int64_t)> m_callback;
    std::optional<std::chrono::steady_clock::time_point> m_start_time_opt;
};
} // namespace kas::functional

#endif