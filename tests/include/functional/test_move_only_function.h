/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_FUNCTIONAL_MOVE_ONLY_FUNCTION_TESTS_H
#define KASLIBCXX_FUNCTIONAL_MOVE_ONLY_FUNCTION_TESTS_H

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "KAS/functional/MoveOnlyFunction.h"
#include "tests/tests.h"

/**
 * @brief Tests for kas::functional::MoveOnlyFunction.
 */
namespace kas::functional::mof::tests {

namespace {
    using namespace kas::tests;
    using kas::functional::MoveOnlyFunction;

    // ==================================================================
    // Construction / operator bool
    // ==================================================================

    // --- 1. default - empty ------------------------------------------
    inline void defaultConstruct(TestContext& ctx)
    {
        MoveOnlyFunction<int()> f;
        KAS_TEST_CHECK_IMPL(ctx, !static_cast<bool>(f));
    }

    // --- 2. nullptr - empty ------------------------------------------
    inline void nullConstruct(TestContext& ctx)
    {
        MoveOnlyFunction<int()> f { nullptr };
        KAS_TEST_CHECK_IMPL(ctx, !static_cast<bool>(f));
    }

    // --- 3. lambda - non-empty, call works ---------------------------
    inline void lambdaConstruct(TestContext& ctx)
    {
        MoveOnlyFunction<int()> f { [] { return 42; } };
        KAS_TEST_CHECK_IMPL(ctx, static_cast<bool>(f));
        KAS_TEST_CHECK_IMPL(ctx, f() == 42);
    }

    // --- 4. function pointer -----------------------------------------
    inline void functionPointer(TestContext& ctx)
    {
        MoveOnlyFunction<int()> f { +[] { return 7; } };
        KAS_TEST_CHECK_IMPL(ctx, f() == 7);
    }

    // --- 5. lambda with capture by value -----------------------------
    inline void lambdaWithCapture(TestContext& ctx)
    {
        int x = 10;
        MoveOnlyFunction<int()> f { [x] { return x * 2; } };
        KAS_TEST_CHECK_IMPL(ctx, f() == 20);
    }

    // ==================================================================
    // operator() - arguments and return
    // ==================================================================

    // --- 6. arguments by value ---------------------------------------
    inline void argsByValue(TestContext& ctx)
    {
        MoveOnlyFunction<int(int, int)> f { [](int a, int b) { return a + b; } };
        KAS_TEST_CHECK_IMPL(ctx, f(2, 3) == 5);
    }

    // --- 7. argument by lvalue reference -----------------------------
    inline void argsByLvalueRef(TestContext& ctx)
    {
        MoveOnlyFunction<int(const std::string&)> f {
            [](const std::string& s) { return static_cast<int>(s.size()); }
        };
        const std::string s = "hello";
        KAS_TEST_CHECK_IMPL(ctx, f(s) == 5);
    }

    // --- 8. argument by rvalue reference (move) ----------------------
    inline void argsByRvalueRef(TestContext& ctx)
    {
        MoveOnlyFunction<std::string(std::string)> f {
            [](std::string s) { return s + "!"; }
        };
        KAS_TEST_CHECK_IMPL(ctx, f(std::string { "hi" }) == "hi!");
    }

    // --- 9. void return ----------------------------------------------
    inline void returnsVoid(TestContext& ctx)
    {
        int calls = 0;
        MoveOnlyFunction<void()> f { [&calls] { ++calls; } };
        f();
        KAS_TEST_CHECK_IMPL(ctx, calls == 1);
        f();
        KAS_TEST_CHECK_IMPL(ctx, calls == 2);
    }

    // --- 10. return by value - complex type --------------------------
    inline void returnsComplex(TestContext& ctx)
    {
        MoveOnlyFunction<std::vector<int>()> f {
            [] { return std::vector<int> { 1, 2, 3 }; }
        };
        const auto v = f();
        KAS_TEST_CHECK_IMPL(ctx, v.size() == 3u);
        KAS_TEST_CHECK_IMPL(ctx, v[0] == 1);
        KAS_TEST_CHECK_IMPL(ctx, v[2] == 3);
    }

    // ==================================================================
    // Move semantics
    // ==================================================================

    // --- 11. move constructor - source becomes empty -----------------
    inline void moveConstruct(TestContext& ctx)
    {
        MoveOnlyFunction<int()> a { [] { return 1; } };
        MoveOnlyFunction<int()> b { std::move(a) };

        KAS_TEST_CHECK_IMPL(ctx, !static_cast<bool>(a)); // "valid but unspecified" - ours is empty
        KAS_TEST_CHECK_IMPL(ctx, static_cast<bool>(b));
        KAS_TEST_CHECK_IMPL(ctx, b() == 1);
    }

    // --- 12. move-assign ---------------------------------------------
    inline void moveAssign(TestContext& ctx)
    {
        MoveOnlyFunction<int()> a { [] { return 10; } };
        MoveOnlyFunction<int()> b;
        b = std::move(a);

        KAS_TEST_CHECK_IMPL(ctx, !static_cast<bool>(a));
        KAS_TEST_CHECK_IMPL(ctx, b() == 10);
    }

    // --- 13. move-only lambda (unique_ptr) ---------------------------
    inline void moveOnlyLambda(TestContext& ctx)
    {
        auto ptr = std::make_unique<int>(42);
        MoveOnlyFunction<int()> f { [p = std::move(ptr)] { return *p; } };
        KAS_TEST_CHECK_IMPL(ctx, f() == 42);
    }

    // ==================================================================
    // reset / operator=(nullptr)
    // ==================================================================

    // --- 14. reset - empty -------------------------------------------
    inline void resetFn(TestContext& ctx)
    {
        MoveOnlyFunction<int()> f { [] { return 1; } };
        KAS_TEST_CHECK_IMPL(ctx, static_cast<bool>(f));
        f.reset();
        KAS_TEST_CHECK_IMPL(ctx, !static_cast<bool>(f));
    }

    // --- 15. operator=(nullptr) - empty ------------------------------
    inline void assignNullptr(TestContext& ctx)
    {
        MoveOnlyFunction<int()> f { [] { return 1; } };
        f = nullptr;
        KAS_TEST_CHECK_IMPL(ctx, !static_cast<bool>(f));
    }

    // ==================================================================
    // bad_function_call
    // ==================================================================

    // --- 16. calling an empty wrapper - std::bad_function_call -------
    inline void emptyCallThrows(TestContext& ctx)
    {
        MoveOnlyFunction<int()> f;
        bool caught = false;
        try {
            (void)f();
        } catch (const std::bad_function_call&) {
            caught = true;
        }
        KAS_TEST_CHECK_IMPL(ctx, caught);
    }

    // --- 17. after reset - also throws -------------------------------
    inline void callAfterResetThrows(TestContext& ctx)
    {
        MoveOnlyFunction<int()> f { [] { return 1; } };
        f.reset();
        bool caught = false;
        try {
            (void)f();
        } catch (const std::bad_function_call&) {
            caught = true;
        }
        KAS_TEST_CHECK_IMPL(ctx, caught);
    }

    // ==================================================================
    // result_type
    // ==================================================================

    // --- 18. result_type - return type -------------------------------
    inline void resultType(TestContext& ctx)
    {
        using fn_t = MoveOnlyFunction<std::string(int)>;
        static_assert(std::is_same_v<fn_t::result_type, std::string>);
        KAS_TEST_CHECK_IMPL(ctx, true);
    }

    // ==================================================================
    // Replacing the function (move-assign from a lambda)
    // ==================================================================

    // --- 19. replacement via move-assign -----------------------------
    inline void replaceFn(TestContext& ctx)
    {
        MoveOnlyFunction<int()> f { [] { return 1; } };
        KAS_TEST_CHECK_IMPL(ctx, f() == 1);

        f = MoveOnlyFunction<int()> { [] { return 2; } };
        KAS_TEST_CHECK_IMPL(ctx, f() == 2);
    }

    // ==================================================================
    // Exception from the invoked function
    // ==================================================================

    // --- 20. exception propagates ------------------------------------
    inline void exceptionPropagates(TestContext& ctx)
    {
        MoveOnlyFunction<int()> f {
            []() -> int { throw std::runtime_error("boom"); }
        };

        bool caught = false;
        try {
            (void)f();
        } catch (const std::runtime_error& ex) {
            caught = true;
            KAS_TEST_CHECK_IMPL(ctx, std::string_view(ex.what()) == "boom");
        }
        KAS_TEST_CHECK_IMPL(ctx, caught);
    }

    // ==================================================================
    // Composition: MoveOnlyFunction returning MoveOnlyFunction
    // ==================================================================

    // --- 21. nested MoveOnlyFunction ---------------------------------
    inline void nestedFn(TestContext& ctx)
    {
        MoveOnlyFunction<MoveOnlyFunction<int()>()> outer {
            [] { return MoveOnlyFunction<int()> { [] { return 99; } }; }
        };
        auto inner = outer();
        KAS_TEST_CHECK_IMPL(ctx, static_cast<bool>(inner));
        KAS_TEST_CHECK_IMPL(ctx, inner() == 99);
    }

    // ==================================================================
    // Many arguments
    // ==================================================================

    // --- 22. variadic args -------------------------------------------
    inline void variadicArgs(TestContext& ctx)
    {
        MoveOnlyFunction<int(int, int, int)> f {
            [](int a, int b, int c) { return a * 100 + b * 10 + c; }
        };
        KAS_TEST_CHECK_IMPL(ctx, f(1, 2, 3) == 123);
    }

} // namespace

// ============================================================
// Entry point
// ============================================================
/**
 * @brief Runs all MoveOnlyFunction tests and prints a summary.
 */
inline bool runTests()
{
    TestContext ctx;

    defaultConstruct(ctx);
    nullConstruct(ctx);
    lambdaConstruct(ctx);
    functionPointer(ctx);
    lambdaWithCapture(ctx);

    argsByValue(ctx);
    argsByLvalueRef(ctx);
    argsByRvalueRef(ctx);
    returnsVoid(ctx);
    returnsComplex(ctx);

    moveConstruct(ctx);
    moveAssign(ctx);
    moveOnlyLambda(ctx);

    resetFn(ctx);
    assignNullptr(ctx);

    emptyCallThrows(ctx);
    callAfterResetThrows(ctx);

    resultType(ctx);

    replaceFn(ctx);
    exceptionPropagates(ctx);
    nestedFn(ctx);
    variadicArgs(ctx);

    return printSummary("kas::functional::mof::tests", ctx);
}

} // namespace kas::functional::mof::tests

#endif