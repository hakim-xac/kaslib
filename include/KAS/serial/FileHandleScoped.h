/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_FILE_HANDLE_SCOPED_H
#define KASLIBCXX_FILE_HANDLE_SCOPED_H

#include <cstdint>
#include <optional>

namespace kas::serial {
class FileHandleScoped final {
public:
    [[nodiscard]] static std::optional<FileHandleScoped> make(const char* filename, int32_t flags);

    ~FileHandleScoped();
    FileHandleScoped(const FileHandleScoped& other) = delete;
    FileHandleScoped(FileHandleScoped&& other) noexcept;
    FileHandleScoped& operator=(const FileHandleScoped& other) = delete;
    FileHandleScoped& operator=(FileHandleScoped&& other) noexcept;

    void swap(FileHandleScoped& other) noexcept;

    [[nodiscard]] int32_t getHandle() const noexcept;

    [[nodiscard]] bool isValid() const noexcept;

private:
    explicit FileHandleScoped(const char* filename, int32_t flags);
    void close() noexcept;

private:
    int32_t m_handle;
};

inline void swap(FileHandleScoped& a, FileHandleScoped& b) noexcept { a.swap(b); }
} // namespace kas::serial

#endif