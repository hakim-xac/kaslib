/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_SERIAL_SERIAL_H
#define KASLIBCXX_SERIAL_SERIAL_H

#include "FileHandleScoped.h"
#include "helper.h"

#include <KAS/expected/expected.h>

#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace kas::serial {
class Serial final {
public:
    [[nodiscard]] static kas::expected_string<Serial> make(structs::PortName port,
        structs::BaudRate baudrate,
        structs::SerialConfig config = { });

    Serial(const Serial&) = delete;
    Serial(Serial&&) noexcept = default;
    Serial& operator=(const Serial&) = delete;
    Serial& operator=(Serial&&) noexcept = default;
    ~Serial() = default;

    /// Sends all bytes. Returns false on write failure.
    [[nodiscard]] bool write(std::span<const std::uint8_t> data) noexcept;

    /// Sends a string.
    [[nodiscard]] bool write(std::string_view data) noexcept;

    /// Reads up to `max_bytes`. Returns nullopt on error.
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> read(std::size_t max_bytes);

    /// Reads exactly `need_bytes`. Returns nullopt on error.
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> readExact(std::size_t need_bytes);

    /// Sends `buffer` and reads exactly `need_bytes` back.
    [[nodiscard]] kas::expected_string<std::vector<std::uint8_t>>
    query(std::span<const std::uint8_t> buffer, std::size_t need_bytes);

    Serial& setTimeout(std::chrono::milliseconds ms) &;

    [[nodiscard]] enums::BaudRate getBaudrate() const noexcept { return m_baudrate; }
    [[nodiscard]] const std::string& getPort() const& noexcept { return m_port; }
    [[nodiscard]] const structs::SerialConfig& getSerialConfig() const& noexcept { return m_config; }

    /// Blocks until the fd becomes readable or the timeout expires.
    ///
    /// Uses poll(2) with POLLIN. A negative timeout blocks indefinitely
    /// (POSIX semantics); zero returns immediately. Retries on EINTR.
    /// Returns true if data is available, or if the peer closed the
    /// connection (POLLHUP — e.g. USB serial disconnected). Returns false
    /// on timeout or error.
    bool waitReadable(std::chrono::milliseconds timeout) noexcept;

private:
    /// Throws std::runtime_error on failure. Only called from make().
    Serial(std::string port, structs::BaudRate br,
        structs::SerialConfig cfg, FileHandleScoped handle);

private:
    std::string m_port;
    enums::BaudRate m_baudrate;
    structs::SerialConfig m_config;
    FileHandleScoped m_handle;
    std::chrono::milliseconds m_timeout { 0 };
};
}

#endif