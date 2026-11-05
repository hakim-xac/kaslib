// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include "KAS/serial/FileHandleScoped.h"

#include <fcntl.h>
#include <unistd.h>
#include <utility>

namespace kas::serial {
namespace {
    constexpr std::int32_t G_INVALID_HANDLE { -1 };
}

//------------------

std::optional<FileHandleScoped>
FileHandleScoped::make(const char* filename, int32_t flags)
{
    FileHandleScoped fd { filename, flags };
    return fd.isValid() ? std::optional { std::move(fd) } : std::nullopt;
}

//------------------

FileHandleScoped::FileHandleScoped(const char* filename, int32_t flags)
    : m_handle { open(filename, flags) }
{
}

//------------------

FileHandleScoped::FileHandleScoped(FileHandleScoped&& other) noexcept
    : m_handle { std::exchange(other.m_handle, G_INVALID_HANDLE) }
{
}

//------------------

FileHandleScoped&
FileHandleScoped::operator=(FileHandleScoped&& other) noexcept
{
    if (std::addressof(other) != this)
        swap(other);

    return *this;
}

//------------------

FileHandleScoped::~FileHandleScoped()
{
    close();
}

//------------------

void FileHandleScoped::close() noexcept
{
    if (!isValid()) [[unlikely]]
        return;

    ::close(m_handle);
    m_handle = G_INVALID_HANDLE;
}

//------------------

std::int32_t
FileHandleScoped::getHandle() const noexcept
{
    return m_handle;
}

//------------------

bool FileHandleScoped::isValid() const noexcept
{
    return m_handle != G_INVALID_HANDLE;
}

//------------------

void FileHandleScoped::swap(FileHandleScoped& other) noexcept
{
    std::swap(m_handle, other.m_handle);
}

//------------------
} // namespace kas::serial
