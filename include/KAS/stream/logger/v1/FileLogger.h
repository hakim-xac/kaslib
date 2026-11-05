/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_STREAM_FILE_LOGGER_V1_H
#define KASLIBCXX_STREAM_FILE_LOGGER_V1_H

#include "LoggerSync.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <utility>

namespace kas::stream::logger::v1 {

/**
 * @brief Logger that writes records to a file on disk.
 *
 * FileLogger owns an std::ofstream and forwards every log call to an
 * internal LoggerSync bound to that stream. The stream is opened in
 * append mode. If the parent directories do not exist, they are
 * created before opening the file.
 *
 * Size-based rotation: if a non-zero max_filesize is configured, the
 * file is truncated (reopened with std::ios::trunc) before a write
 * once its size reaches max_filesize. This is a "rotate by truncate"
 * policy, not a numbered-backup rotation: old content is discarded,
 * not moved aside. Use max_filesize == 0 (the default) to disable the
 * check entirely.
 *
 * Thread safety: every public method takes the internal m_file_mx
 * mutex and then delegates to LoggerSync, which has its own mutex.
 * Concurrent logging from multiple threads through the same
 * FileLogger object is safe. Two FileLogger objects writing to the
 * same path are NOT synchronized with each other.
 *
 * The class is non-copyable; it owns an open stream and a mutex.
 */
class FileLogger final {
public:
    /**
     * @brief Opens the log file and constructs the logger.
     *
     * Creates the parent directory if needed, then opens the file in
     * append mode.
     *
     * @param filename     Path to the log file.
     * @param max_filesize Maximum file size in bytes before truncation.
     *                     0 (default) disables size-based rotation.
     *
     * @throws std::runtime_error if the file cannot be opened.
     */
    explicit FileLogger(std::filesystem::path filename, std::size_t max_filesize = 0)
        : m_filename { std::move(filename) }
        , m_file { openFile(m_filename) }
        , m_logger { m_file }
        , m_max_filesize { max_filesize }
        , m_file_mx { }
    {
    }

    /// Copying is disabled: FileLogger owns the file stream and a mutex.
    FileLogger(const FileLogger&) = delete;
    /// Copy assignment is disabled.
    FileLogger& operator=(const FileLogger&) = delete;

    /**
     * @brief Prints a record in plain style (no color).
     * @param args Values to print.
     */
    template <typename... Args>
    void print(Args&&... args)
    {
        std::lock_guard lk { m_file_mx };
        checkAndRotateIfNeededLocked();
        m_logger.print(std::forward<Args>(args)...);
    }

    /**
     * @brief Prints a Trace-level record.
     * @param args Message values.
     */
    template <typename... Args>
    void printTrace(Args&&... args)
    {
        std::lock_guard lk { m_file_mx };
        checkAndRotateIfNeededLocked();
        m_logger.printTrace(std::forward<Args>(args)...);
    }

    /**
     * @brief Prints a Debug-level record.
     * @param args Message values.
     */
    template <typename... Args>
    void printDebug(Args&&... args)
    {
        std::lock_guard lk { m_file_mx };
        checkAndRotateIfNeededLocked();
        m_logger.printDebug(std::forward<Args>(args)...);
    }

    /**
     * @brief Prints a Log-level record.
     * @param args Message values.
     */
    template <typename... Args>
    void printLog(Args&&... args)
    {
        std::lock_guard lk { m_file_mx };
        checkAndRotateIfNeededLocked();
        m_logger.printLog(std::forward<Args>(args)...);
    }

    /**
     * @brief Prints an Info-level record.
     * @param args Message values.
     */
    template <typename... Args>
    void printInfo(Args&&... args)
    {
        std::lock_guard lk { m_file_mx };
        checkAndRotateIfNeededLocked();
        m_logger.printInfo(std::forward<Args>(args)...);
    }

    /**
     * @brief Prints a Warning-level record.
     * @param args Message values.
     */
    template <typename... Args>
    void printWarn(Args&&... args)
    {
        std::lock_guard lk { m_file_mx };
        checkAndRotateIfNeededLocked();
        m_logger.printWarn(std::forward<Args>(args)...);
    }

    /**
     * @brief Prints an Error-level record (verbose style with location).
     * @param args Message value and optional source location.
     */
    template <typename... Args>
    void printError(Args&&... args)
    {
        std::lock_guard lk { m_file_mx };
        checkAndRotateIfNeededLocked();
        m_logger.printError(std::forward<Args>(args)...);
    }

    /**
     * @brief Prints a Fatal-level record (verbose style with location).
     * @param args Message value and optional source location.
     */
    template <typename... Args>
    void printFatal(Args&&... args)
    {
        std::lock_guard lk { m_file_mx };
        checkAndRotateIfNeededLocked();
        m_logger.printFatal(std::forward<Args>(args)...);
    }

    /**
     * @brief Prints an Exception-level record (verbose style with location).
     * @param args Message value and optional source location.
     */
    template <typename... Args>
    void printException(Args&&... args)
    {
        std::lock_guard lk { m_file_mx };
        checkAndRotateIfNeededLocked();
        m_logger.printException(std::forward<Args>(args)...);
    }

    /// Enables or disables logging.
    void setEnable(bool flag) { m_logger.setEnable(flag); }
    /// Enables logging.
    void setEnabled() { m_logger.setEnabled(); }
    /// Disables logging.
    void setDisabled() { m_logger.setDisabled(); }
    /// Returns whether logging is enabled.
    [[nodiscard]] bool getEnable() const { return m_logger.getEnable(); }

    /// Sets the level bitmask; 0 means "all".
    void setLogLevel(std::size_t level) { m_logger.setLogLevel(level); }
    /// Returns the current level bitmask.
    [[nodiscard]] std::size_t getLogLevel() const { return m_logger.getLogLevel(); }

    /**
     * @brief Sets the level bitmask from a list of tag types.
     * @tparam TypeErrorLevels Tag types, each providing `code`.
     */
    template <typename... TypeErrorLevels>
    void setLogLevel()
    {
        m_logger.template setLogLevel<TypeErrorLevels...>();
    }

    /// Flushes the file buffer to disk.
    void flush()
    {
        std::lock_guard lk { m_file_mx };
        checkAndRotateIfNeededLocked();
        if (m_file.is_open())
            m_file.flush();
    }

    /// Truncates the log file.
    void clearLogFile()
    {
        std::lock_guard lk { m_file_mx };
        reopen(std::ios::trunc);
    }

    /**
     * @brief Switches to a new log file path.
     *
     * Flushes and closes the current file, updates the path, then
     * reopens the file in append mode. The parent directory of the
     * new path is not created; use the constructor for that.
     *
     * @param filename New log file path.
     * @throws std::runtime_error if the new file cannot be opened.
     */
    void setFilename(std::filesystem::path filename)
    {
        std::lock_guard lk { m_file_mx };

        // Flush and close the current file so LoggerSync does not
        // write to a closed stream while the path is being changed.
        if (m_file.is_open()) {
            m_file.flush();
            m_file.close();
        }

        m_filename = std::move(filename);
        m_file.open(m_filename, std::ios::out | std::ios::app);
        if (!m_file.is_open()) {
            throw std::runtime_error(
                "FileLogger: cannot open file: " + m_filename.string());
        }
    }

    /**
     * @brief Returns the current log file path.
     *
     * Returns a copy of the stored path, taken under m_file_mx. The
     * copy is safe to use after the lock is released; concurrent
     * setFilename() calls cannot affect it.
     *
     * @return Copy of the current log file path.
     */
    [[nodiscard]] std::filesystem::path getFilename() const
    {
        std::lock_guard lk { m_file_mx };
        return m_filename;
    }

    /// Sets the maximum file size before truncation; 0 disables it.
    void setMaxFilesize(std::size_t max_filesize) noexcept
    {
        std::lock_guard lk { m_file_mx };
        m_max_filesize = max_filesize;
    }

    /// Disables size-based rotation (sets the limit to 0).
    void resetMaxFilesize() noexcept
    {
        std::lock_guard lk { m_file_mx };
        m_max_filesize = 0;
    }

    /// Returns the current maximum file size (0 means "no limit").
    [[nodiscard]] std::size_t getMaxFilesize() const noexcept
    {
        std::lock_guard lk { m_file_mx };
        return m_max_filesize;
    }

private:
    /**
     * @brief Creates parent directories and opens the file for append.
     * @param filename Path to open.
     * @return Open std::ofstream.
     * @throws std::runtime_error if the file cannot be opened.
     */
    static std::ofstream openFile(const std::filesystem::path& filename)
    {
        std::error_code ec;
        const auto parent = filename.parent_path();
        if (!parent.empty())
            std::filesystem::create_directories(parent, ec);

        std::ofstream f { filename, std::ios::out | std::ios::app };
        if (!f.is_open())
            throw std::runtime_error("FileLogger: cannot open file: " + filename.string());
        return f;
    }

    /**
     * @brief Reopens m_file with the given mode.
     *
     * Flushes and closes the current stream first, then opens it again
     * with mode | std::ios::out.
     *
     * @param mode Additional open mode bits.
     * @throws std::runtime_error if the file cannot be reopened.
     * @note Must be called with m_file_mx held.
     */
    void reopen(std::ios::openmode mode)
    {
        if (m_file.is_open()) {
            m_file.flush();
            m_file.close();
        }
        m_file.open(m_filename, mode | std::ios::out);
        if (!m_file.is_open()) {
            throw std::runtime_error(
                "FileLogger: cannot reopen file: " + m_filename.string());
        }
    }

    /**
     * @brief Truncates the file if it reached m_max_filesize.
     *
     * A no-op when m_max_filesize == 0. Flushes the current stream,
     * queries the file size, and reopens with std::ios::trunc when the
     * size is at or above the limit. Filesystem errors are ignored.
     *
     * @note Must be called with m_file_mx held.
     */
    void checkAndRotateIfNeededLocked()
    {
        if (m_max_filesize == 0)
            return;

        if (m_file.is_open())
            m_file.flush();

        std::error_code ec;
        const auto size = std::filesystem::file_size(m_filename, ec);
        if (ec)
            return;

        if (size >= m_max_filesize)
            reopen(std::ios::trunc);
    }

    /**
     * @brief Locks m_file_mx and calls checkAndRotateIfNeededLocked().
     *
     * @note Currently unused: every public method calls
     *       checkAndRotateIfNeededLocked() directly while already
     *       holding the lock. Kept for potential future use.
     */
    void checkAndRotateIfNeeded()
    {
        std::lock_guard lk { m_file_mx };
        checkAndRotateIfNeededLocked();
    }

    std::filesystem::path m_filename; ///< Path to the current log file.
    std::ofstream m_file; ///< Open file stream (append mode).
    LoggerSync m_logger; ///< Synchronized logger bound to m_file.
    std::size_t m_max_filesize; ///< Rotation threshold; 0 disables.

    mutable std::mutex m_file_mx; ///< Serializes file operations and rotation.
};

} // namespace kas::stream::logger::v1

#endif