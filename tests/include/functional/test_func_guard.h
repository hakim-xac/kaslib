/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_FUNCTIONAL_FUNC_GUARD_TESTS_H
#define KASLIBCXX_FUNCTIONAL_FUNC_GUARD_TESTS_H

#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include "KAS/functional/FuncGuard.h"
#include "tests/tests.h"

/**
 * @brief Tests for kas::functional::FuncGuard.
 */
namespace kas::functional::func_guard::tests {

namespace {
    using namespace kas::tests;
    using kas::functional::FuncGuard;

    // ==================================================================
    // Compile-time checks
    //
    // FuncGuard is intentionally non-copyable AND non-movable:
    // it is bound to the scope it protects, and moving it would
    // break the association between the setup and cleanup actions.
    // ==================================================================

    using SimpleDtor = decltype([] { });

    static_assert(!std::is_copy_constructible_v<FuncGuard<SimpleDtor>>,
        "FuncGuard must not be copy-constructible");
    static_assert(!std::is_copy_assignable_v<FuncGuard<SimpleDtor>>,
        "FuncGuard must not be copy-assignable");
    static_assert(!std::is_move_constructible_v<FuncGuard<SimpleDtor>>,
        "FuncGuard must not be move-constructible (scope-bound)");
    static_assert(!std::is_move_assignable_v<FuncGuard<SimpleDtor>>,
        "FuncGuard must not be move-assignable (scope-bound)");

    // ==================================================================
    // 1. ctor -> body -> dtor order
    // ==================================================================
    inline void ctorBodyDtorOrder(TestContext& ctx)
    {
        std::vector<int> order;
        {
            FuncGuard guard {
                [&order] { order.push_back(1); },
                [&order] { order.push_back(2); }
            };
            order.push_back(3);
        }
        KAS_TEST_CHECK_IMPL(ctx, order.size() == 3u);
        KAS_TEST_CHECK_IMPL(ctx, order[0] == 1); // ctor
        KAS_TEST_CHECK_IMPL(ctx, order[1] == 3); // body
        KAS_TEST_CHECK_IMPL(ctx, order[2] == 2); // dtor
    }

    // ==================================================================
    // 2. dtor runs on normal scope exit
    // ==================================================================
    inline void dtorOnScopeExit(TestContext& ctx)
    {
        int calls = 0;
        {
            FuncGuard guard {
                [] { },
                [&calls] { ++calls; }
            };
            KAS_TEST_CHECK_IMPL(ctx, calls == 0);
        }
        KAS_TEST_CHECK_IMPL(ctx, calls == 1);
    }

    // ==================================================================
    // 3. dtor runs when body throws
    // ==================================================================
    inline void dtorOnBodyException(TestContext& ctx)
    {
        int calls = 0;
        bool caught = false;
        try {
            FuncGuard guard {
                [] { },
                [&calls] { ++calls; }
            };
            throw std::runtime_error("body");
        } catch (const std::runtime_error&) {
            caught = true;
        }
        KAS_TEST_CHECK_IMPL(ctx, caught);
        KAS_TEST_CHECK_IMPL(ctx, calls == 1);
    }

    // ==================================================================
    // 4. if func_in_ctor throws, dtor is NOT invoked
    // ==================================================================
    inline void ctorThrowsDtorNotCalled(TestContext& ctx)
    {
        int dtor_calls = 0;
        bool caught = false;
        try {
            FuncGuard guard {
                [] { throw std::runtime_error("ctor"); },
                [&dtor_calls] { ++dtor_calls; }
            };
        } catch (const std::runtime_error&) {
            caught = true;
        }
        KAS_TEST_CHECK_IMPL(ctx, caught);
        KAS_TEST_CHECK_IMPL(ctx, dtor_calls == 0);
    }

    // ==================================================================
    // 5. both callbacks capture by reference
    // ==================================================================
    inline void captureByRef(TestContext& ctx)
    {
        int in_calls = 0;
        int out_calls = 0;
        {
            FuncGuard guard {
                [&in_calls] { ++in_calls; },
                [&out_calls] { ++out_calls; }
            };
            KAS_TEST_CHECK_IMPL(ctx, in_calls == 1);
            KAS_TEST_CHECK_IMPL(ctx, out_calls == 0);
        }
        KAS_TEST_CHECK_IMPL(ctx, in_calls == 1);
        KAS_TEST_CHECK_IMPL(ctx, out_calls == 1);
    }

    // ==================================================================
    // 6. move-only lambda in dtor (shared_ptr observed via weak_ptr)
    // ==================================================================
    inline void moveOnlyDtor(TestContext& ctx)
    {
        auto ptr = std::make_shared<int>(0);
        std::weak_ptr<int> observer = ptr;
        int observed = -1;

        KAS_TEST_CHECK_IMPL(ctx, !observer.expired());

        {
            FuncGuard guard {
                [] { },
                [p = std::move(ptr), &observed] { observed = ++(*p); }
            };
            KAS_TEST_CHECK_IMPL(ctx, !observer.expired());
        }

        KAS_TEST_CHECK_IMPL(ctx, observed == 1); // deleter ran, ++(*p) == 1
        KAS_TEST_CHECK_IMPL(ctx, observer.expired()); // lambda + its capture destroyed
    }

    // ==================================================================
    // 7. move-only lambdas in BOTH ctor and dtor
    // ==================================================================
    inline void moveOnlyCtorAndDtor(TestContext& ctx)
    {
        int ctor_ran = 0;
        int dtor_ran = 0;
        auto cptr = std::make_unique<int>(1);
        auto dptr = std::make_unique<int>(2);
        {
            FuncGuard guard {
                [p = std::move(cptr), &ctor_ran] { ctor_ran = *p; },
                [p = std::move(dptr), &dtor_ran] { dtor_ran = *p; }
            };
            KAS_TEST_CHECK_IMPL(ctx, ctor_ran == 1);
            KAS_TEST_CHECK_IMPL(ctx, dtor_ran == 0);
        }
        KAS_TEST_CHECK_IMPL(ctx, dtor_ran == 2);
    }

    // ==================================================================
    // Function pointers as callbacks
    // ==================================================================

    int g_calls = 0;
    void freeFuncIn() { ++g_calls; }
    void freeFuncOut() { g_calls += 10; }

    // ==================================================================
    // 8. function pointers as callbacks
    // ==================================================================
    inline void functionPointers(TestContext& ctx)
    {
        g_calls = 0;
        {
            FuncGuard guard { &freeFuncIn, &freeFuncOut };
            KAS_TEST_CHECK_IMPL(ctx, g_calls == 1);
        }
        KAS_TEST_CHECK_IMPL(ctx, g_calls == 11); // 1 + 10
    }

    // ==================================================================
    // 9. nested guards - LIFO destruction order
    // ==================================================================
    inline void nestedGuards(TestContext& ctx)
    {
        std::vector<int> order;
        {
            FuncGuard outer {
                [&order] { order.push_back(1); },
                [&order] { order.push_back(4); }
            };
            {
                FuncGuard inner {
                    [&order] { order.push_back(2); },
                    [&order] { order.push_back(3); }
                };
            }
        }
        KAS_TEST_CHECK_IMPL(ctx, order.size() == 4u);
        KAS_TEST_CHECK_IMPL(ctx, order[0] == 1);
        KAS_TEST_CHECK_IMPL(ctx, order[1] == 2);
        KAS_TEST_CHECK_IMPL(ctx, order[2] == 3);
        KAS_TEST_CHECK_IMPL(ctx, order[3] == 4);
    }

} // namespace

// ============================================================
// Entry point
// ============================================================
/**
 * @brief Runs all FuncGuard tests and prints a summary.
 */
inline bool runTests()
{
    TestContext ctx;

    ctorBodyDtorOrder(ctx);
    dtorOnScopeExit(ctx);
    dtorOnBodyException(ctx);

    ctorThrowsDtorNotCalled(ctx);

    captureByRef(ctx);
    moveOnlyDtor(ctx);
    moveOnlyCtorAndDtor(ctx);

    functionPointers(ctx);
    nestedGuards(ctx);

    return printSummary("kas::functional::func_guard::tests", ctx);
}

} // namespace kas::functional::func_guard::tests

#endif