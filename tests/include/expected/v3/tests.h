/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_EXPECTED_V3_TESTS_H
#define KASLIBCXX_EXPECTED_V3_TESTS_H

#include <iostream>
#include <string>
#include <type_traits>
#include <utility>

#include "KAS/expected/v3/expected.h"
#include "tests/tests.h"

/**
 * @brief Tests for the expected v3 type.
 */
namespace kas::test_expected::v3 {

namespace {
    using namespace kas::tests;
    using kas::v3::expected;
    using kas::v3::unexpected;

    /**
     * @brief Simple aggregate used to test operator->.
     */
    struct Point {
        int x = 0;
        int y = 0;
    };

    /**
     * @brief Type with an overloaded operator& used to test std::addressof.
     */
    struct Tricky {
        int v = 5;
        Tricky* operator&() { return nullptr; } // breaks a naive &
    };

    /**
     * @brief Move-only type used to test expected with non-copyable T.
     */
    struct MoveOnly {
        MoveOnly() = default;
        MoveOnly(MoveOnly&&) = default;
        MoveOnly& operator=(MoveOnly&&) = default;
        MoveOnly(const MoveOnly&) = delete;
        MoveOnly& operator=(const MoveOnly&) = delete;
        int v = 7;
    };

    // --- 1. Default constructor --------------------------------------
    inline void ctorDefault(TestContext& ctx)
    {
        const expected<int, std::string> e;
        KAS_TEST_CHECK_IMPL(ctx, e.has_value());
        KAS_TEST_CHECK_IMPL(ctx, !e.has_error());
        KAS_TEST_CHECK_IMPL(ctx, static_cast<bool>(e));
        KAS_TEST_CHECK_IMPL(ctx, e.value() == 0);
    }

    // --- 2. Constructor from a value ---------------------------------
    inline void ctorValue(TestContext& ctx)
    {
        const expected<int, std::string> e { 42 };
        KAS_TEST_CHECK_IMPL(ctx, e.has_value());
        KAS_TEST_CHECK_IMPL(ctx, e.value() == 42);
        KAS_TEST_CHECK_IMPL(ctx, *e == 42);
    }

    // --- 3. Constructor from a convertible value ---------------------
    inline void ctorValueConvertible(TestContext& ctx)
    {
        // U = short, T = int
        const expected<int, std::string> e { static_cast<short>(7) };
        KAS_TEST_CHECK_IMPL(ctx, e.has_value());
        KAS_TEST_CHECK_IMPL(ctx, e.value() == 7);
    }

    // --- 4. Constructor from unexpected ------------------------------
    inline void ctorError(TestContext& ctx)
    {
        const expected<int, std::string> e = unexpected<std::string> { "boom" };
        KAS_TEST_CHECK_IMPL(ctx, e.has_error());
        KAS_TEST_CHECK_IMPL(ctx, !e.has_value());
        KAS_TEST_CHECK_IMPL(ctx, !static_cast<bool>(e));
        KAS_TEST_CHECK_IMPL(ctx, e.error() == "boom");
    }

    // --- 5. Constructor from a convertible unexpected ----------------
    inline void ctorErrorConvertible(TestContext& ctx)
    {
        // U = const char*, E = std::string
        const expected<int, std::string> e = unexpected<const char*> { "oops" };
        KAS_TEST_CHECK_IMPL(ctx, e.has_error());
        KAS_TEST_CHECK_IMPL(ctx, e.error() == "oops");
    }

    // --- 6. Copy and move --------------------------------------------
    inline void copyAndMove(TestContext& ctx)
    {
        expected<std::string, int> a { std::string("hello") };
        expected<std::string, int> b = a;
        KAS_TEST_CHECK_IMPL(ctx, b.value() == "hello");

        expected<std::string, int> c = std::move(a);
        KAS_TEST_CHECK_IMPL(ctx, c.value() == "hello");

        expected<std::string, int> d = unexpected<int> { 7 };
        expected<std::string, int> e = d;
        KAS_TEST_CHECK_IMPL(ctx, e.error() == 7);
    }

    // --- 7. value() - lvalue ref -------------------------------------
    inline void valueLvalueRef(TestContext& ctx)
    {
        expected<int, std::string> e { 1 };
        e.value() = 10;
        KAS_TEST_CHECK_IMPL(ctx, e.value() == 10);

        int& ref = e.value();
        ref = 20;
        KAS_TEST_CHECK_IMPL(ctx, *e == 20);
    }

    // --- 8. value() - const lvalue ref -------------------------------
    inline void valueConstRef(TestContext& ctx)
    {
        const expected<int, std::string> e { 5 };
        const int& ref = e.value();
        KAS_TEST_CHECK_IMPL(ctx, ref == 5);
        KAS_TEST_CHECK_IMPL(ctx, *e == 5);
    }

    // --- 9. value() - rvalue ref -------------------------------------
    inline void valueRvalueRef(TestContext& ctx)
    {
        expected<std::string, int> e { std::string("move me") };
        std::string s = std::move(e).value();
        KAS_TEST_CHECK_IMPL(ctx, s == "move me");
    }

    // --- 10. error() - lvalue ref ------------------------------------
    inline void errorLvalueRef(TestContext& ctx)
    {
        expected<int, std::string> e = unexpected<std::string> { "err" };
        e.error() = "new err";
        KAS_TEST_CHECK_IMPL(ctx, e.error() == "new err");
    }

    // --- 11. error() - const lvalue ref ------------------------------
    inline void errorConstRef(TestContext& ctx)
    {
        const expected<int, std::string> e = unexpected<std::string> { "const err" };
        const std::string& ref = e.error();
        KAS_TEST_CHECK_IMPL(ctx, ref == "const err");
    }

    // --- 12. error() - rvalue ref ------------------------------------
    inline void errorRvalueRef(TestContext& ctx)
    {
        expected<int, std::string> e = unexpected<std::string> { std::string("moved err") };
        std::string s = std::move(e).error();
        KAS_TEST_CHECK_IMPL(ctx, s == "moved err");
    }

    // --- 13. operator* - lvalue --------------------------------------
    inline void derefLvalue(TestContext& ctx)
    {
        expected<int, std::string> e { 42 };
        KAS_TEST_CHECK_IMPL(ctx, *e == 42);

        *e = 7;
        KAS_TEST_CHECK_IMPL(ctx, *e == 7);
    }

    // --- 14. operator* - const ---------------------------------------
    inline void derefConst(TestContext& ctx)
    {
        const expected<int, std::string> e { 42 };
        KAS_TEST_CHECK_IMPL(ctx, *e == 42);
    }

    // --- 15. operator* - rvalue (move) -------------------------------
    inline void derefRvalue(TestContext& ctx)
    {
        expected<std::string, int> e { std::string("hello") };
        std::string moved = *std::move(e);
        KAS_TEST_CHECK_IMPL(ctx, moved == "hello");
    }

    // --- 16. operator-> - lvalue -------------------------------------
    inline void arrowLvalue(TestContext& ctx)
    {
        expected<Point, int> e { Point { 3, 4 } };
        KAS_TEST_CHECK_IMPL(ctx, e->x == 3);
        KAS_TEST_CHECK_IMPL(ctx, e->y == 4);

        e->x = 100;
        KAS_TEST_CHECK_IMPL(ctx, e->x == 100);
    }

    // --- 17. operator-> - const --------------------------------------
    inline void arrowConst(TestContext& ctx)
    {
        const expected<Point, int> e { Point { 1, 2 } };
        KAS_TEST_CHECK_IMPL(ctx, e->x == 1);
        KAS_TEST_CHECK_IMPL(ctx, e->y == 2);
    }

    // --- 18. operator-> uses std::addressof --------------------------
    inline void arrowUsesAddressof(TestContext& ctx)
    {
        expected<Tricky, int> e { Tricky { } };
        KAS_TEST_CHECK_IMPL(ctx, e->v == 5); // no UB with overloaded operator&
    }

    // --- 19. operator* throws on error -------------------------------
    inline void derefThrowsOnError(TestContext& ctx)
    {
        expected<int, std::string> e = unexpected<std::string> { "boom" };

        bool caught = false;
        try {
            (void)*e;
        } catch (const std::runtime_error& ex) {
            caught = true;
            KAS_TEST_CHECK_IMPL(ctx, std::string_view(ex.what()) == "Expected object has no value!");
        } catch (const std::exception& ex) {
            KAS_TEST_CHECK_IMPL(ctx, false); // unexpected exception type
            (void)ex;
        }

        KAS_TEST_CHECK_IMPL(ctx, caught); // was an exception thrown at all?
    }

    // --- 20. operator-> throws on error ------------------------------
    inline void arrowThrowsOnError(TestContext& ctx)
    {
        expected<Point, std::string> e = unexpected<std::string> { "boom" };

        bool caught = false;
        try {
            (void)e->x;
        } catch (const std::runtime_error& ex) {
            caught = true;
            KAS_TEST_CHECK_IMPL(ctx, std::string_view(ex.what()) == "Expected object has no value!");
        } catch (const std::exception& ex) {
            KAS_TEST_CHECK_IMPL(ctx, false); // unexpected exception type
            (void)ex;
        }

        KAS_TEST_CHECK_IMPL(ctx, caught);
    }

    // --- 21. After reset() - neither value nor error, everything throws
    inline void derefThrowsAfterReset(TestContext& ctx)
    {
        expected<int, std::string> e { 1 };
        e.reset();
        KAS_TEST_CHECK_IMPL(ctx, !e.has_value());
        KAS_TEST_CHECK_IMPL(ctx, !e.has_error());

        // *e -> "Expected object has no value!"
        {
            bool caught = false;
            try {
                (void)*e;
            } catch (const std::runtime_error& ex) {
                caught = true;
                KAS_TEST_CHECK_IMPL(ctx, std::string_view(ex.what()) == "Expected object has no value!");
            } catch (const std::exception& ex) {
                KAS_TEST_CHECK_IMPL(ctx, false);
                (void)ex;
            }
            KAS_TEST_CHECK_IMPL(ctx, caught);
        }

        // e.value() -> "Expected object has no value!"
        {
            bool caught = false;
            try {
                (void)e.value();
            } catch (const std::runtime_error& ex) {
                caught = true;
                KAS_TEST_CHECK_IMPL(ctx, std::string_view(ex.what()) == "Expected object has no value!");
            } catch (const std::exception& ex) {
                KAS_TEST_CHECK_IMPL(ctx, false);
                (void)ex;
            }
            KAS_TEST_CHECK_IMPL(ctx, caught);
        }

        // e.error() -> "Expected object has no error!"
        {
            bool caught = false;
            try {
                (void)e.error();
            } catch (const std::runtime_error& ex) {
                caught = true;
                KAS_TEST_CHECK_IMPL(ctx, std::string_view(ex.what()) == "Expected object has no error!");
            } catch (const std::exception& ex) {
                KAS_TEST_CHECK_IMPL(ctx, false);
                (void)ex;
            }
            KAS_TEST_CHECK_IMPL(ctx, caught);
        }
    }

    // --- 22. emplace(value) from the error state ---------------------
    inline void emplaceValue(TestContext& ctx)
    {
        expected<int, std::string> e = unexpected<std::string> { "err" };
        KAS_TEST_CHECK_IMPL(ctx, e.has_error());

        e.emplace(42);
        KAS_TEST_CHECK_IMPL(ctx, e.has_value());
        KAS_TEST_CHECK_IMPL(ctx, e.value() == 42);
    }

    // --- 23. emplace(value) from a convertible value -----------------
    inline void emplaceValueConvertible(TestContext& ctx)
    {
        expected<std::string, int> e { std::string("old") };
        e.emplace("new");
        KAS_TEST_CHECK_IMPL(ctx, e.value() == "new");
    }

    // --- 24. emplace(unexpected) from the value state ----------------
    inline void emplaceError(TestContext& ctx)
    {
        expected<int, std::string> e { 1 };
        KAS_TEST_CHECK_IMPL(ctx, e.has_value());

        e.emplace(unexpected<std::string> { "bad" });
        KAS_TEST_CHECK_IMPL(ctx, e.has_error());
        KAS_TEST_CHECK_IMPL(ctx, e.error() == "bad");
    }

    // --- 25. emplace returns *this -----------------------------------
    inline void emplaceReturnsSelf(TestContext& ctx)
    {
        expected<int, std::string> e { 1 };
        expected<int, std::string>& ref = e.emplace(2);
        KAS_TEST_CHECK_IMPL(ctx, &ref == &e);
        KAS_TEST_CHECK_IMPL(ctx, e.value() == 2);
    }

    // --- 26. reset - transitions to monostate ------------------------
    inline void resetState(TestContext& ctx)
    {
        expected<int, std::string> e { 1 };
        e.reset();
        KAS_TEST_CHECK_IMPL(ctx, !e.has_value());
        KAS_TEST_CHECK_IMPL(ctx, !e.has_error());
    }

    // --- 27. MoveOnly as T -------------------------------------------
    inline void moveOnlyValue(TestContext& ctx)
    {
        expected<MoveOnly, int> e { MoveOnly { } };
        KAS_TEST_CHECK_IMPL(ctx, e.has_value());
        KAS_TEST_CHECK_IMPL(ctx, e->v == 7);
    }

    // --- 28. Usage in an if statement --------------------------------
    inline void useInIf(TestContext& ctx)
    {
        expected<int, std::string> e { 123 };
        if (e.has_value())
            KAS_TEST_CHECK_IMPL(ctx, *e == 123);
        else
            KAS_TEST_CHECK_IMPL(ctx, false);
    }

    // --- 29. Conversion to bool --------------------------------------
    inline void boolConversion(TestContext& ctx)
    {
        expected<int, std::string> good { 1 };
        expected<int, std::string> bad = unexpected<std::string> { "x" };

        KAS_TEST_CHECK_IMPL(ctx, static_cast<bool>(good));
        KAS_TEST_CHECK_IMPL(ctx, !static_cast<bool>(bad));
    }

    // --- 30. Address of the value via &* -----------------------------
    inline void addressOfDeref(TestContext& ctx)
    {
        expected<int, std::string> e { 42 };
        const int* p = &*e;
        KAS_TEST_CHECK_IMPL(ctx, p != nullptr);
        KAS_TEST_CHECK_IMPL(ctx, *p == 42);
    }

    // --- 31. emplace chain: error -> value -> error -> value ----------
    inline void multipleEmplaceErrorThenValue(TestContext& ctx)
    {
        expected<std::string, int> e = unexpected<int> { 1 };
        KAS_TEST_CHECK_IMPL(ctx, e.has_error());

        e.emplace(std::string("v1"));
        KAS_TEST_CHECK_IMPL(ctx, e.has_value());
        KAS_TEST_CHECK_IMPL(ctx, e.value() == "v1");

        e.emplace(unexpected<int> { 2 });
        KAS_TEST_CHECK_IMPL(ctx, e.has_error());
        KAS_TEST_CHECK_IMPL(ctx, e.error() == 2);

        e.emplace(std::string("v2"));
        KAS_TEST_CHECK_IMPL(ctx, e.value() == "v2");
    }

    // =================================================================
    // value_or
    // =================================================================

    // --- 32. value_or on lvalue with a value: returns the stored value ---
    inline void valueOrLvalueHasValue(TestContext& ctx)
    {
        expected<int, std::string> e { 42 };
        const int v = e.value_or(-1);
        KAS_TEST_CHECK_IMPL(ctx, v == 42);

        // The stored value is unchanged.
        KAS_TEST_CHECK_IMPL(ctx, e.has_value());
        KAS_TEST_CHECK_IMPL(ctx, e.value() == 42);
    }

    // --- 33. value_or on lvalue with an error: returns the fallback ----
    inline void valueOrLvalueHasError(TestContext& ctx)
    {
        expected<int, std::string> e = unexpected<std::string> { "boom" };
        const int v = e.value_or(-1);
        KAS_TEST_CHECK_IMPL(ctx, v == -1);

        // The object is still in the error state.
        KAS_TEST_CHECK_IMPL(ctx, e.has_error());
        KAS_TEST_CHECK_IMPL(ctx, e.error() == "boom");
    }

    // --- 34. value_or on lvalue after reset(): returns the fallback ----
    inline void valueOrLvalueAfterReset(TestContext& ctx)
    {
        expected<int, std::string> e { 7 };
        e.reset();
        KAS_TEST_CHECK_IMPL(ctx, !e.has_value());
        KAS_TEST_CHECK_IMPL(ctx, !e.has_error());

        const int v = e.value_or(99);
        KAS_TEST_CHECK_IMPL(ctx, v == 99);
    }

    // --- 35. value_or on const lvalue --------------------------------
    inline void valueOrConstLvalue(TestContext& ctx)
    {
        const expected<int, std::string> good { 5 };
        KAS_TEST_CHECK_IMPL(ctx, good.value_or(0) == 5);

        const expected<int, std::string> bad = unexpected<std::string> { "e" };
        KAS_TEST_CHECK_IMPL(ctx, bad.value_or(0) == 0);
    }

    // --- 36. value_or on rvalue with a value: moves the stored value ---
    inline void valueOrRvalueMovesValue(TestContext& ctx)
    {
        expected<std::string, int> e { std::string("payload") };
        std::string s = std::move(e).value_or(std::string("fallback"));
        KAS_TEST_CHECK_IMPL(ctx, s == "payload");
    }

    // --- 37. value_or on rvalue with an error: returns the fallback ----
    inline void valueOrRvalueHasError(TestContext& ctx)
    {
        expected<std::string, int> e = unexpected<int> { 1 };
        std::string s = std::move(e).value_or(std::string("fallback"));
        KAS_TEST_CHECK_IMPL(ctx, s == "fallback");
    }

    // --- 38. value_or with a convertible fallback type ----------------
    inline void valueOrConvertibleFallback(TestContext& ctx)
    {
        // U = const char*, T = std::string
        expected<std::string, int> good { std::string("v") };
        KAS_TEST_CHECK_IMPL(ctx, good.value_or("fb") == "v");

        expected<std::string, int> bad = unexpected<int> { 1 };
        KAS_TEST_CHECK_IMPL(ctx, bad.value_or("fb") == "fb");
    }

    // --- 39. value_or does not affect the stored state ---------------
    inline void valueOrDoesNotMutate(TestContext& ctx)
    {
        expected<int, std::string> e { 10 };
        (void)e.value_or(0);
        KAS_TEST_CHECK_IMPL(ctx, e.has_value());
        KAS_TEST_CHECK_IMPL(ctx, e.value() == 10);

        expected<int, std::string> err = unexpected<std::string> { "keep" };
        (void)err.value_or(0);
        KAS_TEST_CHECK_IMPL(ctx, err.has_error());
        KAS_TEST_CHECK_IMPL(ctx, err.error() == "keep");
    }

    // --- 40. value_or is callable on a MoveOnly T --------------------
    inline void valueOrMoveOnly(TestContext& ctx)
    {
        // Lvalue overload requires copy_constructible<T>; MoveOnly is not
        // copyable, so only the rvalue overload is available.
        expected<MoveOnly, int> e { MoveOnly { } };
        MoveOnly m = std::move(e).value_or(MoveOnly { });
        KAS_TEST_CHECK_IMPL(ctx, m.v == 7);
    }

    // --- 41. value_or on a const expected does not move --------------
    inline void valueOrConstDoesNotMove(TestContext& ctx)
    {
        const expected<std::string, int> e { std::string("abc") };
        std::string s = e.value_or(std::string("fb"));
        KAS_TEST_CHECK_IMPL(ctx, s == "abc");
        // The source is intact (const).
        KAS_TEST_CHECK_IMPL(ctx, e.value() == "abc");
    }

    // -----------------------------------------------------------------
    // value_or: compile-time invariants
    // -----------------------------------------------------------------

    /**
     * @brief Compile-time checks of value_or overload availability.
     *
     * Verifies, for the concrete types used in these tests, that:
     *  - the lvalue overload is callable on a const expected<int, std::string>&
     *    and returns int (T must be copy_constructible);
     *  - the rvalue overload is callable on an expected<int, std::string>&&
     *    and returns int;
     *  - for a move-only T (MoveOnly), the rvalue overload is callable and
     *    returns MoveOnly.
     *
     * The "lvalue overload is not available for move-only T" property is
     * not asserted here: a negative `requires`-check on a constrained
     * overload produces a hard error instead of a false result in this
     * context. Instead, the property follows from the copy_constructible<T>
     * constraint on the lvalue overload and from MoveOnly being non-copyable,
     * both of which are checked below.
     *
     * The function has no runtime body effect; it exists only to host the
     * static_asserts and is called from runTests() to make the intent explicit.
     */
    inline void valueOrCompileTimeInvariants()
    {
        // value_or is callable on a const lvalue for copyable T.
        static_assert(requires(const expected<int, std::string>& e) {
            { e.value_or(0) } -> std::same_as<int>;
        });

        // And on an rvalue.
        static_assert(requires(expected<int, std::string>&& e) {
            { std::move(e).value_or(0) } -> std::same_as<int>;
        });

        // For a move-only T, only the rvalue value_or is available.
        static_assert(requires(expected<MoveOnly, int>&& e) {
            { std::move(e).value_or(MoveOnly { }) } -> std::same_as<MoveOnly>;
        });

        // Sanity: MoveOnly really is move-only.
        static_assert(!std::is_copy_constructible_v<MoveOnly>);
        static_assert(std::is_move_constructible_v<MoveOnly>);
    }

} // namespace

// ============================================================
// Entry point
// ============================================================

/**
 * @brief Runs all expected v3 tests and prints a summary.
 */
inline bool runTests()
{
    TestContext ctx;

    ctorDefault(ctx);
    ctorValue(ctx);
    ctorValueConvertible(ctx);
    ctorError(ctx);
    ctorErrorConvertible(ctx);
    copyAndMove(ctx);

    valueLvalueRef(ctx);
    valueConstRef(ctx);
    valueRvalueRef(ctx);
    errorLvalueRef(ctx);
    errorConstRef(ctx);
    errorRvalueRef(ctx);

    derefLvalue(ctx);
    derefConst(ctx);
    derefRvalue(ctx);
    arrowLvalue(ctx);
    arrowConst(ctx);
    arrowUsesAddressof(ctx);
    derefThrowsOnError(ctx);
    arrowThrowsOnError(ctx);
    derefThrowsAfterReset(ctx);

    emplaceValue(ctx);
    emplaceValueConvertible(ctx);
    emplaceError(ctx);
    emplaceReturnsSelf(ctx);
    resetState(ctx);

    moveOnlyValue(ctx);

    useInIf(ctx);
    boolConversion(ctx);
    addressOfDeref(ctx);
    multipleEmplaceErrorThenValue(ctx);

    valueOrLvalueHasValue(ctx);
    valueOrLvalueHasError(ctx);
    valueOrLvalueAfterReset(ctx);
    valueOrConstLvalue(ctx);
    valueOrRvalueMovesValue(ctx);
    valueOrRvalueHasError(ctx);
    valueOrConvertibleFallback(ctx);
    valueOrDoesNotMutate(ctx);
    valueOrMoveOnly(ctx);
    valueOrConstDoesNotMove(ctx);

    valueOrCompileTimeInvariants();

    return printSummary("kas::test_expected::v3", ctx);
}

} // namespace kas::test_expected::v3

#endif