/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_FUNCTIONAL_TYPE_GUARD_TESTS_H
#define KASLIBCXX_FUNCTIONAL_TYPE_GUARD_TESTS_H

#include "KAS/functional/TypeGuard.h"
#include "tests/tests.h"

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

namespace kas::functional::type_guard::tests {

namespace {
    using namespace kas::tests;
    using kas::functional::TypeGuard;

    // ============================================================
    // Helpers
    // ============================================================

    /// Shared log for deleter invocations.
    struct CallLog {
        int count = 0;
        int lastValue = 0;
    };

    auto makeLoggingDeleter(std::shared_ptr<CallLog> log)
    {
        return [log](int& v) {
            ++log->count;
            log->lastValue = v;
        };
    }

    // ============================================================
    // Compile-time checks
    //
    // TypeGuard is move-constructible (so it can be returned from
    // factories and stored in containers) but not move-assignable
    // (assigning would silently drop the target's old resource
    // instead of invoking its deleter, leaking it). Copy is deleted
    // entirely: the guard is scope-bound and must not be duplicated.
    // ============================================================
    void runCompileTimeChecks()
    {
        static_assert(!std::is_copy_constructible_v<TypeGuard<int>>,
            "TypeGuard must not be copy-constructible");
        static_assert(!std::is_copy_assignable_v<TypeGuard<int>>,
            "TypeGuard must not be copy-assignable");
        static_assert(std::is_move_constructible_v<TypeGuard<int>>,
            "TypeGuard must be move-constructible");
        static_assert(!std::is_move_assignable_v<TypeGuard<int>>,
            "TypeGuard must not be move-assignable (would leak old resource)");
        static_assert(!std::is_default_constructible_v<TypeGuard<int>>,
            "TypeGuard requires a deleter");

        static_assert(noexcept(std::declval<TypeGuard<int>&>().release()),
            "release() must be noexcept for int");
        static_assert(noexcept(std::declval<TypeGuard<int>&>().dismiss()),
            "dismiss() must be noexcept");
        static_assert(noexcept(std::declval<const TypeGuard<int>&>().get()),
            "const get() must be noexcept");
        static_assert(noexcept(std::declval<TypeGuard<int>&>().get()),
            "non-const get() must be noexcept");
    }

    // ============================================================
    // 1. deleter called exactly once on destruction
    // ============================================================
    void testDeleterCalledOnDestruction(TestContext& ctx)
    {
        auto log = std::make_shared<CallLog>();
        {
            TypeGuard<int> g { makeLoggingDeleter(log), 42 };
            KAS_TEST_CHECK_IMPL(ctx, g.get() == 42);
            KAS_TEST_CHECK_IMPL(ctx, *g == 42);
            KAS_TEST_CHECK_IMPL(ctx, log->count == 0);
        }
        KAS_TEST_CHECK_IMPL(ctx, log->count == 1);
        KAS_TEST_CHECK_IMPL(ctx, log->lastValue == 42);
    }

    // ============================================================
    // 2. operator-> on user type
    // ============================================================
    void testArrowOperator(TestContext& ctx)
    {
        auto log = std::make_shared<CallLog>();
        TypeGuard<std::string> g { [log](std::string&) { ++log->count; }, "hello" };
        KAS_TEST_CHECK_IMPL(ctx, g->size() == 5);
        KAS_TEST_CHECK_IMPL(ctx, g->substr(1, 3) == "ell");
        KAS_TEST_CHECK_IMPL(ctx, log->count == 0);
    }

    // ============================================================
    // 3. dismiss() cancels deleter, value stays accessible
    // ============================================================
    void testDismissCancelsDeleter(TestContext& ctx)
    {
        auto log = std::make_shared<CallLog>();
        {
            TypeGuard<int> g { makeLoggingDeleter(log), 7 };
            g.dismiss();
            KAS_TEST_CHECK_IMPL(ctx, g.get() == 7);
            KAS_TEST_CHECK_IMPL(ctx, *g == 7);
        }
        KAS_TEST_CHECK_IMPL(ctx, log->count == 0);
    }

    // ============================================================
    // 4. release() returns value without invoking deleter
    // ============================================================
    void testReleaseReturnsValue(TestContext& ctx)
    {
        auto log = std::make_shared<CallLog>();
        {
            TypeGuard<int> g { makeLoggingDeleter(log), 99 };
            const int v = g.release();
            KAS_TEST_CHECK_IMPL(ctx, v == 99);
        }
        KAS_TEST_CHECK_IMPL(ctx, log->count == 0);
    }

    // ============================================================
    // 5. release() on std::string leaves moved-from value
    // ============================================================
    void testReleaseLeavesMovedFrom(TestContext& ctx)
    {
        auto log = std::make_shared<CallLog>();
        {
            TypeGuard<std::string> g { [log](std::string&) { ++log->count; }, "hello" };
            const std::string s = g.release();
            KAS_TEST_CHECK_IMPL(ctx, s == "hello");
            KAS_TEST_CHECK_IMPL(ctx, g.get().empty());
        }
        KAS_TEST_CHECK_IMPL(ctx, log->count == 0);
    }

    // ============================================================
    // 6. empty deleter makes guard a no-op
    // ============================================================
    void testEmptyDeleterIsNoOp(TestContext& ctx)
    {
        TypeGuard<int> g { MoveOnlyFunction<void(int&)> { nullptr }, 5 };
        KAS_TEST_CHECK_IMPL(ctx, g.get() == 5);
        // Destructor must not crash, throw, or terminate.
    }

    // ============================================================
    // 7. const accessors
    // ============================================================
    void testConstAccessors(TestContext& ctx)
    {
        auto log = std::make_shared<CallLog>();
        const TypeGuard<int> g { makeLoggingDeleter(log), 42 };
        KAS_TEST_CHECK_IMPL(ctx, g.get() == 42);
        KAS_TEST_CHECK_IMPL(ctx, *g == 42);
        const int x = *g;
        KAS_TEST_CHECK_IMPL(ctx, x == 42);
    }

    // ============================================================
    // 8. multiple guards invoke their own deleters
    // ============================================================
    void testMultipleGuards(TestContext& ctx)
    {
        auto log1 = std::make_shared<CallLog>();
        auto log2 = std::make_shared<CallLog>();
        {
            TypeGuard<int> g1 { makeLoggingDeleter(log1), 1 };
            TypeGuard<int> g2 { makeLoggingDeleter(log2), 2 };
        }
        KAS_TEST_CHECK_IMPL(ctx, log1->count == 1);
        KAS_TEST_CHECK_IMPL(ctx, log2->count == 1);
    }

    // ============================================================
    // 9. release() and dismiss() are idempotent
    // ============================================================
    void testIdempotentCancel(TestContext& ctx)
    {
        auto log = std::make_shared<CallLog>();
        TypeGuard<int> g { makeLoggingDeleter(log), 1 };
        g.dismiss();
        g.dismiss(); // second dismiss is a no-op
        const int v = g.release(); // release after dismiss still returns value
        KAS_TEST_CHECK_IMPL(ctx, v == 1);
        KAS_TEST_CHECK_IMPL(ctx, log->count == 0);
    }

    // ============================================================
    // 10. deleter observes value modifications
    // ============================================================
    void testDeleterSeesModifications(TestContext& ctx)
    {
        int seen = 0;
        {
            TypeGuard<int> g { [&seen](int& v) { seen = v; }, 10 };
            *g = 20;
        }
        KAS_TEST_CHECK_IMPL(ctx, seen == 20);
    }

    // ============================================================
    // 11. release() value is intact for non-trivial T
    // ============================================================
    void testReleaseValueIntact(TestContext& ctx)
    {
        auto log = std::make_shared<CallLog>();
        TypeGuard<std::string> g { [log](std::string&) { ++log->count; }, "payload" };
        const std::string out = g.release();
        KAS_TEST_CHECK_IMPL(ctx, out == "payload");
        KAS_TEST_CHECK_IMPL(ctx, g.get().empty());
        KAS_TEST_CHECK_IMPL(ctx, log->count == 0);
    }

    // ============================================================
    // 12. Nested scopes: inner guard fires before outer
    // ============================================================
    void testNestedScopeOrdering(TestContext& ctx)
    {
        std::string order;
        {
            TypeGuard<int> outer { [&order](int&) { order += 'O'; }, 1 };
            {
                TypeGuard<int> inner { [&order](int&) { order += 'I'; }, 2 };
            }
            order += '-';
        }
        KAS_TEST_CHECK_IMPL(ctx, order == "I-O");
    }

    // ============================================================
    // 13. move-ctor transfers deleter ownership (no double call)
    // ============================================================
    void testMoveConstruction(TestContext& ctx)
    {
        auto log = std::make_shared<CallLog>();
        {
            TypeGuard<int> a { makeLoggingDeleter(log), 10 };
            TypeGuard<int> b { std::move(a) };
            KAS_TEST_CHECK_IMPL(ctx, log->count == 0);
            KAS_TEST_CHECK_IMPL(ctx, b.get() == 10);
        }
        KAS_TEST_CHECK_IMPL(ctx, log->count == 1);
    }

    // ============================================================
    // 14. move-ctor with non-trivial T
    // ============================================================
    void testMoveConstructString(TestContext& ctx)
    {
        auto log = std::make_shared<CallLog>();
        {
            TypeGuard<std::string> a { [log](std::string&) { ++log->count; }, "abc" };
            TypeGuard<std::string> b { std::move(a) };
            KAS_TEST_CHECK_IMPL(ctx, b.get() == "abc");
            KAS_TEST_CHECK_IMPL(ctx, a.get().empty());
        }
        KAS_TEST_CHECK_IMPL(ctx, log->count == 1);
    }

    // ============================================================
    // 15. move-ctor enables factory returns
    // ============================================================
    void testFactoryReturn(TestContext& ctx)
    {
        auto log = std::make_shared<CallLog>();
        {
            auto makeGuard = [&log]() {
                TypeGuard<int> g { makeLoggingDeleter(log), 77 };
                return g;
            };
            TypeGuard<int> g = makeGuard();
            KAS_TEST_CHECK_IMPL(ctx, g.get() == 77);
            KAS_TEST_CHECK_IMPL(ctx, log->count == 0);
        }
        KAS_TEST_CHECK_IMPL(ctx, log->count == 1);
    }

} // namespace

// ============================================================
// Entry point
// ============================================================
/**
 * @brief Runs all TypeGuard tests and prints a summary.
 */
inline bool runTests()
{
    TestContext ctx;

    runCompileTimeChecks();

    testDeleterCalledOnDestruction(ctx);
    testArrowOperator(ctx);
    testDismissCancelsDeleter(ctx);
    testReleaseReturnsValue(ctx);
    testReleaseLeavesMovedFrom(ctx);
    testEmptyDeleterIsNoOp(ctx);
    testConstAccessors(ctx);
    testMultipleGuards(ctx);
    testIdempotentCancel(ctx);
    testDeleterSeesModifications(ctx);
    testReleaseValueIntact(ctx);
    testNestedScopeOrdering(ctx);
    testMoveConstruction(ctx);
    testMoveConstructString(ctx);
    testFactoryReturn(ctx);

    return printSummary("kas::functional::type_guard::tests", ctx);
}

} // namespace kas::functional::type_guard::tests

#endif