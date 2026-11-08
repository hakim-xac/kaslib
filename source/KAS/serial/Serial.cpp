// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include "KAS/serial/Serial.h"

#include "KAS/utils/utils.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <utility>

namespace kas::serial {
namespace {

    //------------------
    // enums -> POSIX termios constants
    //------------------

    constexpr tcflag_t toPosix(enums::NumDataBits v) noexcept
    {
        switch (v) {
        case enums::NumDataBits::Five:
            return CS5;
        case enums::NumDataBits::Six:
            return CS6;
        case enums::NumDataBits::Seven:
            return CS7;
        case enums::NumDataBits::Eight:
            return CS8;
        }
        return CS8; // unreachable
    }

    constexpr tcflag_t toPosix(enums::NumStopBits v) noexcept
    {
        return v == enums::NumStopBits::Two ? CSTOPB : 0;
    }

    constexpr tcflag_t toPosix(enums::Parity v) noexcept
    {
        switch (v) {
        case enums::Parity::None:
            return 0;
        case enums::Parity::Even:
            return PARENB;
        case enums::Parity::Odd:
            return PARENB | PARODD;
        }
        return 0;
    }

    constexpr speed_t toPosix(enums::BaudRate v) noexcept
    {
        switch (v) {
        case enums::BaudRate::B_0:
            return B0;
        case enums::BaudRate::B_50:
            return B50;
        case enums::BaudRate::B_75:
            return B75;
        case enums::BaudRate::B_110:
            return B110;
        case enums::BaudRate::B_134:
            return B134;
        case enums::BaudRate::B_150:
            return B150;
        case enums::BaudRate::B_200:
            return B200;
        case enums::BaudRate::B_300:
            return B300;
        case enums::BaudRate::B_600:
            return B600;
        case enums::BaudRate::B_1200:
            return B1200;
        case enums::BaudRate::B_1800:
            return B1800;
        case enums::BaudRate::B_2400:
            return B2400;
        case enums::BaudRate::B_4800:
            return B4800;
        case enums::BaudRate::B_9600:
            return B9600;
        case enums::BaudRate::B_19200:
            return B19200;
        case enums::BaudRate::B_38400:
            return B38400;
        case enums::BaudRate::B_57600:
            return B57600;
        case enums::BaudRate::B_115200:
            return B115200;
        case enums::BaudRate::B_230400:
            return B230400;
        case enums::BaudRate::B_460800:
            return B460800;
        case enums::BaudRate::B_500000:
            return B500000;
        case enums::BaudRate::B_576000:
            return B576000;
        case enums::BaudRate::B_921600:
            return B921600;
        case enums::BaudRate::B_1000000:
            return B1000000;
        case enums::BaudRate::B_1152000:
            return B1152000;
        case enums::BaudRate::B_1500000:
            return B1500000;
        case enums::BaudRate::B_2000000:
            return B2000000;
        case enums::BaudRate::B_2500000:
            return B2500000;
        case enums::BaudRate::B_3000000:
            return B3000000;
        case enums::BaudRate::B_3500000:
            return B3500000;
        case enums::BaudRate::B_4000000:
            return B4000000;
        }
        return B9600; // unreachable
    }

    constexpr tcflag_t toPosix(enums::SoftwareFlowControl v) noexcept
    {
        switch (v) {
        case enums::SoftwareFlowControl::F_IXON:
            return IXON;
        case enums::SoftwareFlowControl::F_IXOFF:
            return IXOFF;
        case enums::SoftwareFlowControl::F_IXANY:
            return IXANY;
        }
        return 0;
    }

    //------------------
    // Throws std::runtime_error on failure (errno is included in the message).
    //------------------
    void configureTermios(int32_t fd,
        enums::BaudRate baudrate,
        const structs::SerialConfig& cfg)
    {
        using namespace kas::utils;
        termios tio { };
        if (::tcgetattr(fd, &tio) != 0) {
            const int saved_errno = errno;
            throw std::runtime_error {
                utils::stringify(
                    "tcgetattr failed: ",
                    std::strerror(saved_errno))
            };
        }

        if (cfg.raw_mode) {
            ::cfmakeraw(&tio);
        }

        const speed_t speed = toPosix(baudrate);
        if (::cfsetispeed(&tio, speed) != 0) {
            const int saved_errno = errno;
            throw std::runtime_error {
                utils::stringify(
                    "cfsetispeed failed: ",
                    std::strerror(saved_errno))
            };
        }
        if (::cfsetospeed(&tio, speed) != 0) {
            const int saved_errno = errno;
            throw std::runtime_error {
                utils::stringify(
                    "cfsetospeed failed: ",
                    std::strerror(saved_errno))
            };
        }

        tio.c_cflag &= ~CSIZE;
        tio.c_cflag |= toPosix(cfg.data_bits);
        tio.c_cflag |= toPosix(cfg.stop_bits);
        tio.c_cflag |= toPosix(cfg.parity);
        tio.c_cflag |= (CREAD | CLOCAL);

        if (cfg.hardware_flow_control) {
            tio.c_cflag |= CRTSCTS;
        } else {
            tio.c_cflag &= ~CRTSCTS;
        }

        tio.c_iflag &= ~(IXON | IXOFF | IXANY);
        tio.c_iflag |= toPosix(cfg.software_flow_control_bits);

        if (::tcsetattr(fd, TCSANOW, &tio) != 0) {
            const int saved_errno = errno;
            throw std::runtime_error {
                utils::stringify(
                    "tcsetattr failed: ",
                    std::strerror(saved_errno))
            };
        }

        ::tcflush(fd, TCIOFLUSH);

        if (cfg.rts_enabled || cfg.rts_invert) {
            int rts = TIOCM_RTS;
            const int cmd = cfg.rts_invert ? TIOCMBIC : TIOCMBIS;
            if (::ioctl(fd, cmd, &rts) != 0) {
                const int saved_errno = errno;
                throw std::runtime_error {
                    utils::stringify(
                        "RTS ioctl failed: ",
                        std::strerror(saved_errno))
                };
            }
        }
    }

} // namespace

//------------------

Serial::Serial(std::string port, structs::BaudRate br,
    structs::SerialConfig cfg, FileHandleScoped handle)
    : m_port { std::move(port) }
    , m_baudrate { br.value }
    , m_config { cfg }
    , m_handle { std::move(handle) }
{
    // Validate configuration that cannot be expressed in the type system.
    if (m_config.hardware_flow_control && (m_config.rts_enabled || m_config.rts_invert)) {
        throw std::invalid_argument { "Serial: hardware_flow_control conflicts with manual RTS control" };
    }

    configureTermios(m_handle.getHandle(), m_baudrate, m_config);
}

//------------------

kas::expected_string<Serial>
Serial::make(structs::PortName port,
    structs::BaudRate baudrate,
    structs::SerialConfig config)
{
    if (port.value.empty()) {
        return kas::unexpected_string { "Serial: port name is empty" };
    }

    constexpr int32_t kFlags = O_RDWR | O_NOCTTY;
    auto handle = FileHandleScoped::make(port.value.c_str(), kFlags);
    if (!handle) {
        const int saved_errno = errno;
        return kas::unexpected_string {
            utils::stringify(
                "Serial: open(",
                port.value,
                ") failed: ",
                std::strerror(saved_errno))
        };
    }

    try {
        return Serial { std::move(port.value), baudrate, config, std::move(*handle) };
    } catch (const std::exception& ex) {
        // All std::exception-derived errors (including bad_alloc) are converted
        // to expected errors. If you want bad_alloc to propagate, catch only
        // system_error/invalid_argument/runtime_error.
        return kas::unexpected_string { ex.what() };
    } catch (...) {
        return kas::unexpected_string { "Serial: unknown error" };
    }
}

//------------------

bool Serial::write(std::span<const std::uint8_t> data) noexcept
{
    std::size_t total = 0;
    while (total < data.size()) {
        const ssize_t n = ::write(m_handle.getHandle(),
            data.data() + total, data.size() - total);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return false;
        }
        if (n == 0)
            return false;
        total += static_cast<std::size_t>(n);
    }
    return true;
}

//------------------

bool Serial::write(std::string_view data) noexcept
{
    return write(std::span<const std::uint8_t> { reinterpret_cast<const std::uint8_t*>(data.data()), data.size() });
}

//------------------

std::optional<std::vector<std::uint8_t>>
Serial::read(std::size_t max_bytes)
{
    std::vector<std::uint8_t> result;
    if (max_bytes > 0)
        result.reserve(max_bytes);

    std::array<std::uint8_t, 4096> buffer { };

    while (true) {
        // If a timeout is set, wait for readability first.
        if (m_timeout.count() > 0 && !waitReadable(m_timeout))
            break;

        std::size_t to_read = buffer.size();
        if (max_bytes > 0) {
            const std::size_t remaining = max_bytes - result.size();
            if (remaining == 0)
                break;
            to_read = std::min(buffer.size(), remaining);
        }

        const ssize_t n = ::read(m_handle.getHandle(), buffer.data(), to_read);
        if (n > 0) {
            result.insert(result.end(), buffer.begin(), buffer.begin() + n);
            // For "read up to max": stop when we got fewer bytes than requested.
            if (max_bytes > 0 && static_cast<std::size_t>(n) < to_read)
                break;
            continue;
        }
        if (n == 0)
            break; // EOF
        if (errno == EINTR)
            continue;
        // EAGAIN/EWOULDBLOCK — return what we have
        break;
    }

    if (result.empty())
        return std::nullopt;
    return result;
}

//------------------

std::optional<std::vector<std::uint8_t>>
Serial::readExact(std::size_t need_bytes)
{
    if (need_bytes == 0)
        return std::vector<std::uint8_t> { };

    std::vector<std::uint8_t> result;
    result.reserve(need_bytes);

    std::array<std::uint8_t, 4096> buffer { };

    while (result.size() < need_bytes) {
        // If a timeout is set, wait for readability first.
        if (m_timeout.count() > 0 && !waitReadable(m_timeout))
            return std::nullopt;

        const std::size_t remaining = need_bytes - result.size();
        const std::size_t to_read = std::min(buffer.size(), remaining);

        const ssize_t n = ::read(m_handle.getHandle(), buffer.data(), to_read);
        if (n > 0) {
            result.insert(result.end(), buffer.begin(), buffer.begin() + n);
            continue;
        }
        if (n == 0)
            return std::nullopt; // EOF before need_bytes
        if (errno == EINTR)
            continue;
        return std::nullopt;
    }

    return result;
}

//------------------

kas::expected_string<std::vector<std::uint8_t>>
Serial::query(std::span<const std::uint8_t> buffer, std::size_t need_bytes)
{
    if (!write(buffer))
        return kas::unexpected_string { "Can not write to serial port!" };

    auto read_result = readExact(need_bytes);
    if (!read_result)
        return kas::unexpected_string { "Can not read from serial port!" };

    return std::move(*read_result);
}

//------------------

Serial& Serial::setTimeout(std::chrono::milliseconds ms) &
{
    m_timeout = ms;
    return *this;
}

//------------------

bool Serial::waitReadable(std::chrono::milliseconds timeout) noexcept
{
    const auto ms = timeout.count();
    const int poll_timeout = ms > std::numeric_limits<int>::max()
        ? std::numeric_limits<int>::max()
        : static_cast<int>(ms);

    pollfd pfd { .fd = m_handle.getHandle(), .events = POLLIN, .revents = 0 };

    int rc;
    do {
        rc = ::poll(&pfd, 1, poll_timeout);
    } while (rc < 0 && errno == EINTR);

    if (rc < 0)
        return false;

    return rc > 0 && (pfd.revents & (POLLIN | POLLHUP));
}

//------------------

} // namespace kas::serial