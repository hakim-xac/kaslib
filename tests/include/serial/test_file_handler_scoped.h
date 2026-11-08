/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_FILE_HANDLE_SCOPED_TESTS_H
#define KASLIBCXX_FILE_HANDLE_SCOPED_TESTS_H

#include "KAS/serial/FileHandleScoped.h"
#include "tests/tests.h"

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <string>
#include <type_traits>
#include <utility>

namespace kas::serial::test_fhs::tests {

namespace {
    using namespace kas::tests;
    using kas::serial::FileHandleScoped;

    // ============================================================
    // Compile-time checks
    // ============================================================
    void runCompileTimeChecks()
    {
        static_assert(!std::is_copy_constructible_v<FileHandleScoped>,
            "FileHandleScoped must not be copy-constructible");
        static_assert(!std::is_copy_assignable_v<FileHandleScoped>,
            "FileHandleScoped must not be copy-assignable");
        static_assert(std::is_nothrow_move_constructible_v<FileHandleScoped>,
            "FileHandleScoped must be nothrow-move-constructible");
        static_assert(std::is_nothrow_move_assignable_v<FileHandleScoped>,
            "FileHandleScoped must be nothrow-move-assignable");
        static_assert(!std::is_default_constructible_v<FileHandleScoped>,
            "FileHandleScoped must not be default-constructible");

        static_assert(noexcept(std::declval<FileHandleScoped&>().getHandle()),
            "getHandle() must be noexcept");
        static_assert(noexcept(std::declval<const FileHandleScoped&>().isValid()),
            "isValid() must be noexcept");
        static_assert(noexcept(std::declval<FileHandleScoped&>().swap(
                          std::declval<FileHandleScoped&>())),
            "swap() must be noexcept");
    }

    // ============================================================
    // 1. Successful open returns a valid handle
    // ============================================================
    void testSuccessfulOpen(TestContext& ctx)
    {
        auto h = FileHandleScoped::make("/dev/null", O_RDWR);
        KAS_TEST_CHECK_IMPL(ctx, h.has_value());
        if (!h)
            return;
        KAS_TEST_CHECK_IMPL(ctx, h->isValid());
        KAS_TEST_CHECK_IMPL(ctx, h->getHandle() >= 0);
    }

    // ============================================================
    // 2. Non-existent path -> nullopt
    // ============================================================
    void testNonexistentPath(TestContext& ctx)
    {
        auto h = FileHandleScoped::make("/dev/does-not-exist-kaslib", O_RDONLY);
        KAS_TEST_CHECK_IMPL(ctx, !h.has_value());
    }

    // ============================================================
    // 3. Directory with O_RDONLY succeeds on Linux (fd to dir),
    //    but O_RDWR fails with EISDIR. Use O_RDWR for a clean failure.
    // ============================================================
    void testDirectoryOpenRwFails(TestContext& ctx)
    {
        auto h = FileHandleScoped::make("/tmp", O_RDWR);
        KAS_TEST_CHECK_IMPL(ctx, !h.has_value());
    }

    // ============================================================
    // 4. move-ctor: source becomes invalid, target owns the fd
    // ============================================================
    void testMoveConstruction(TestContext& ctx)
    {
        auto a = FileHandleScoped::make("/dev/null", O_RDWR);
        KAS_TEST_CHECK_IMPL(ctx, a.has_value());
        if (!a)
            return;

        const int32_t original = a->getHandle();
        KAS_TEST_CHECK_IMPL(ctx, original >= 0);

        FileHandleScoped b { std::move(*a) };
        KAS_TEST_CHECK_IMPL(ctx, b.isValid());
        KAS_TEST_CHECK_IMPL(ctx, b.getHandle() == original);

        // Source is moved-from: no longer valid, and its fd is -1.
        // The fd is now owned by `b`, so the destructor of `a` must not close it.
        KAS_TEST_CHECK_IMPL(ctx, !a->isValid());
        KAS_TEST_CHECK_IMPL(ctx, a->getHandle() == -1);
    }

    // ============================================================
    // 5. move-assign: target's old fd goes to source (swap),
    //    source's fd moves into target.
    // ============================================================
    void testMoveAssignment(TestContext& ctx)
    {
        auto a_opt = FileHandleScoped::make("/dev/null", O_RDWR);
        auto b_opt = FileHandleScoped::make("/dev/null", O_RDWR);
        KAS_TEST_CHECK_IMPL(ctx, a_opt.has_value() && b_opt.has_value());
        if (!a_opt || !b_opt)
            return;

        const int32_t fd_a = a_opt->getHandle();
        const int32_t fd_b = b_opt->getHandle();
        KAS_TEST_CHECK_IMPL(ctx, fd_a != fd_b);

        *a_opt = std::move(*b_opt);

        // After swap-based move-assign: a owns fd_b, b owns fd_a (old fd of a).
        KAS_TEST_CHECK_IMPL(ctx, a_opt->getHandle() == fd_b);
        KAS_TEST_CHECK_IMPL(ctx, b_opt->getHandle() == fd_a);
        KAS_TEST_CHECK_IMPL(ctx, a_opt->isValid());
        KAS_TEST_CHECK_IMPL(ctx, b_opt->isValid());

        // Both fds are still open (just swapped between objects).
        KAS_TEST_CHECK_IMPL(ctx, ::fcntl(fd_a, F_GETFD) != -1);
        KAS_TEST_CHECK_IMPL(ctx, ::fcntl(fd_b, F_GETFD) != -1);
    }

    // ============================================================
    // 6. self-swap is a no-op (guards the self-check in operator=)
    // ============================================================
    void testSelfSwapIsNoOp(TestContext& ctx)
    {
        auto h = FileHandleScoped::make("/dev/null", O_RDWR);
        KAS_TEST_CHECK_IMPL(ctx, h.has_value());
        if (!h)
            return;

        const int32_t fd = h->getHandle();
        h->swap(*h); // self-swap must not corrupt the handle

        KAS_TEST_CHECK_IMPL(ctx, h->isValid());
        KAS_TEST_CHECK_IMPL(ctx, h->getHandle() == fd);
        KAS_TEST_CHECK_IMPL(ctx, ::fcntl(fd, F_GETFD) != -1);
    }

    // ============================================================
    // 7. swap exchanges handles between two objects
    // ============================================================
    void testSwap(TestContext& ctx)
    {
        auto a_opt = FileHandleScoped::make("/dev/null", O_RDWR);
        auto b_opt = FileHandleScoped::make("/dev/null", O_RDWR);
        KAS_TEST_CHECK_IMPL(ctx, a_opt.has_value() && b_opt.has_value());
        if (!a_opt || !b_opt)
            return;

        const int32_t fd_a = a_opt->getHandle();
        const int32_t fd_b = b_opt->getHandle();

        a_opt->swap(*b_opt);

        KAS_TEST_CHECK_IMPL(ctx, a_opt->getHandle() == fd_b);
        KAS_TEST_CHECK_IMPL(ctx, b_opt->getHandle() == fd_a);
    }

    // ============================================================
    // 8. free-function swap (ADL) behaves like member swap
    // ============================================================
    void testFreeFunctionSwap(TestContext& ctx)
    {
        auto a_opt = FileHandleScoped::make("/dev/null", O_RDWR);
        auto b_opt = FileHandleScoped::make("/dev/null", O_RDWR);
        KAS_TEST_CHECK_IMPL(ctx, a_opt.has_value() && b_opt.has_value());
        if (!a_opt || !b_opt)
            return;

        const int32_t fd_a = a_opt->getHandle();
        const int32_t fd_b = b_opt->getHandle();

        using std::swap;
        swap(*a_opt, *b_opt); // ADL finds kas::serial::swap

        KAS_TEST_CHECK_IMPL(ctx, a_opt->getHandle() == fd_b);
        KAS_TEST_CHECK_IMPL(ctx, b_opt->getHandle() == fd_a);
    }

    // ============================================================
    // 9. Destructor closes the fd.
    //    Verify by checking that fcntl(fd, F_GETFD) returns EBADF
    //    after the guard is destroyed.
    // ============================================================
    void testDestructorClosesFd(TestContext& ctx)
    {
        int32_t fd = -1;
        {
            auto h = FileHandleScoped::make("/dev/null", O_RDWR);
            KAS_TEST_CHECK_IMPL(ctx, h.has_value());
            if (!h)
                return;
            fd = h->getHandle();
            KAS_TEST_CHECK_IMPL(ctx, ::fcntl(fd, F_GETFD) != -1);
        }
        // Destructor ran: fd should be closed now.
        errno = 0;
        const int rc = ::fcntl(fd, F_GETFD);
        KAS_TEST_CHECK_IMPL(ctx, rc == -1);
        KAS_TEST_CHECK_IMPL(ctx, errno == EBADF);
    }

    // ============================================================
    // 10. Moved-from guard does not close the fd (double-close guard).
    // ============================================================
    void testMovedFromDoesNotCloseFd(TestContext& ctx)
    {
        int32_t fd = -1;
        {
            auto a = FileHandleScoped::make("/dev/null", O_RDWR);
            KAS_TEST_CHECK_IMPL(ctx, a.has_value());
            if (!a)
                return;

            FileHandleScoped b { std::move(*a) };
            fd = b.getHandle();
            KAS_TEST_CHECK_IMPL(ctx, fd >= 0);

            // `a` is moved-from; its destructor runs at the end of this scope.
            // The fd must remain open because `b` still owns it.
        }
        // Both `a` (moved-from) and `b` (owner) are destroyed.
        // `b` should have closed fd.
        errno = 0;
        const int rc = ::fcntl(fd, F_GETFD);
        KAS_TEST_CHECK_IMPL(ctx, rc == -1);
        KAS_TEST_CHECK_IMPL(ctx, errno == EBADF);
    }

    // ============================================================
    // 11. move-assign: source's old fd is closed when source is destroyed
    // ============================================================
    void testMoveAssignClosesOldFd(TestContext& ctx)
    {
        int32_t fd_a = -1;
        int32_t fd_b = -1;
        {
            auto a_opt = FileHandleScoped::make("/dev/null", O_RDWR);
            auto b_opt = FileHandleScoped::make("/dev/null", O_RDWR);
            KAS_TEST_CHECK_IMPL(ctx, a_opt.has_value() && b_opt.has_value());
            if (!a_opt || !b_opt)
                return;

            fd_a = a_opt->getHandle();
            fd_b = b_opt->getHandle();

            *a_opt = std::move(*b_opt);

            // Now: a owns fd_b, b owns fd_a.
            KAS_TEST_CHECK_IMPL(ctx, a_opt->getHandle() == fd_b);
            KAS_TEST_CHECK_IMPL(ctx, b_opt->getHandle() == fd_a);
        }
        // Both closed. No double-close, no leak.
        errno = 0;
        KAS_TEST_CHECK_IMPL(ctx, ::fcntl(fd_a, F_GETFD) == -1);
        KAS_TEST_CHECK_IMPL(ctx, errno == EBADF);

        errno = 0;
        KAS_TEST_CHECK_IMPL(ctx, ::fcntl(fd_b, F_GETFD) == -1);
        KAS_TEST_CHECK_IMPL(ctx, errno == EBADF);
    }

} // namespace

// ============================================================
// Entry point
// ============================================================
/**
 * @brief Runs all FileHandleScoped tests and prints a summary.
 */
inline bool runTests()
{
    TestContext ctx;

    runCompileTimeChecks();

    testSuccessfulOpen(ctx);
    testNonexistentPath(ctx);
    testDirectoryOpenRwFails(ctx);
    testMoveConstruction(ctx);
    testMoveAssignment(ctx);
    testSelfSwapIsNoOp(ctx);
    testSwap(ctx);
    testFreeFunctionSwap(ctx);
    testDestructorClosesFd(ctx);
    testMovedFromDoesNotCloseFd(ctx);
    testMoveAssignClosesOldFd(ctx);

    return printSummary("kas::serial::test_fhs::tests", ctx);
}

} // namespace kas::serial::test_fhs::tests

#endif