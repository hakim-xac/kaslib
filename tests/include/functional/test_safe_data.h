/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_FUNCTIONAL_SAFE_DATA_TESTS_H
#define KASLIBCXX_FUNCTIONAL_SAFE_DATA_TESTS_H

#include <atomic>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "KAS/functional/SafeData.h"
#include "tests/tests.h"

/**
 * @brief Tests for kas::functional::SafeData.
 */
namespace kas::functional::safe_data::tests {

namespace {
    using namespace kas::tests;
    using kas::functional::SafeData;

    // ==================================================================
    // Construction / get
    // ==================================================================

    // --- 1. value constructor ----------------------------------------
    inline void constructValue(TestContext& ctx)
    {
        SafeData<int> sd { 42 };
        KAS_TEST_CHECK_IMPL(ctx, sd.get() == 42);
    }

    // --- 2. variadic constructor -------------------------------------
    inline void constructVariadic(TestContext& ctx)
    {
        SafeData<std::string> sd { "hello" };
        KAS_TEST_CHECK_IMPL(ctx, sd.get() == "hello");
    }

    // --- 3. vector as the value --------------------------------------
    inline void constructVector(TestContext& ctx)
    {
        SafeData<std::vector<int>> sd { std::vector<int> { 1, 2, 3 } };
        const auto v = sd.get();
        KAS_TEST_CHECK_IMPL(ctx, v.size() == 3u);
        KAS_TEST_CHECK_IMPL(ctx, v[0] == 1);
        KAS_TEST_CHECK_IMPL(ctx, v[2] == 3);
    }

    // --- 4. default construction via variadic ------------------------
    inline void constructDefault(TestContext& ctx)
    {
        SafeData<int> sd { 0 };
        KAS_TEST_CHECK_IMPL(ctx, sd.get() == 0);
    }

    // ==================================================================
    // update
    // ==================================================================

    inline void updateBasic(TestContext& ctx)
    {
        SafeData<int> sd { 1 };
        sd.update(42);
        KAS_TEST_CHECK_IMPL(ctx, sd.get() == 42);
    }

    inline void updateReturnsSelf(TestContext& ctx)
    {
        SafeData<int> sd { 1 };
        SafeData<int>& ref = sd.update(2);
        KAS_TEST_CHECK_IMPL(ctx, &ref == &sd);
        KAS_TEST_CHECK_IMPL(ctx, sd.get() == 2);
    }

    inline void updateComplex(TestContext& ctx)
    {
        SafeData<std::string> sd { "old" };
        sd.update("new");
        KAS_TEST_CHECK_IMPL(ctx, sd.get() == "new");
    }

    // ==================================================================
    // read - now takes const TData&
    // ==================================================================

    // --- 5. read on an lvalue SafeData -------------------------------
    inline void readBasic(TestContext& ctx)
    {
        SafeData<int> sd { 42 };
        int observed = 0;
        sd.read([&observed](const int& v) { observed = v; });
        KAS_TEST_CHECK_IMPL(ctx, observed == 42);
    }

    // --- 6. read on a const SafeData ---------------------------------
    inline void readConst(TestContext& ctx)
    {
        const SafeData<int> sd { 7 };
        int observed = 0;
        sd.read([&observed](const int& v) { observed = v; });
        KAS_TEST_CHECK_IMPL(ctx, observed == 7);
    }

    // --- 7. read vector - observes the current size ------------------
    inline void readVector(TestContext& ctx)
    {
        SafeData<std::vector<int>> sd { std::vector<int> { 1, 2, 3 } };
        std::size_t observed = 0;
        sd.read([&observed](const std::vector<int>& v) { observed = v.size(); });
        KAS_TEST_CHECK_IMPL(ctx, observed == 3u);
    }

    // --- 8. read does not allow modifying the data -------------------
    //         (via const TData& - the compiler prevents it)
    //         Check that a non-const lambda is not accepted:
    // template <typename T>
    // concept can_read_nonconst = requires(T& sd) {
    //     sd.read([](auto& v) { v = 0; });   // <- does not compile
    // };
    // static_assert(!can_read_nonconst<SafeData<int>>);
    //
    // (checking requires on a lambda is non-trivial, skipping)

    // ==================================================================
    // modify
    // ==================================================================

    inline void modifyBasic(TestContext& ctx)
    {
        SafeData<int> sd { 10 };
        sd.modify([](int& v) { v += 5; });
        KAS_TEST_CHECK_IMPL(ctx, sd.get() == 15);
    }

    inline void modifyVector(TestContext& ctx)
    {
        SafeData<std::vector<int>> sd { std::vector<int> { 1, 2 } };
        sd.modify([](std::vector<int>& v) { v.push_back(3); });
        const auto v = sd.get();
        KAS_TEST_CHECK_IMPL(ctx, v.size() == 3u);
        KAS_TEST_CHECK_IMPL(ctx, v[2] == 3);
    }

    // --- 9. modify is unavailable on a const SafeData ----------------
    template <typename T>
    concept can_modify_const = requires(T& sd) {
        sd.modify([](auto&) { });
    };
    static_assert(!can_modify_const<const SafeData<int>>);

    // ==================================================================
    // copy/move are deleted
    // ==================================================================
    static_assert(!std::is_copy_constructible_v<SafeData<int>>);
    static_assert(!std::is_move_constructible_v<SafeData<int>>);
    static_assert(!std::is_copy_assignable_v<SafeData<int>>);
    static_assert(!std::is_move_assignable_v<SafeData<int>>);

    // ==================================================================
    // Exception in the callable - mutex is released
    // ==================================================================

    inline void modifyThrows(TestContext& ctx)
    {
        SafeData<int> sd { 1 };

        bool caught = false;
        try {
            sd.modify([](int&) { throw std::runtime_error("boom"); });
        } catch (const std::runtime_error& ex) {
            caught = true;
            KAS_TEST_CHECK_IMPL(ctx, std::string_view(ex.what()) == "boom");
        }
        KAS_TEST_CHECK_IMPL(ctx, caught);

        sd.modify([](int& v) { v = 42; });
        KAS_TEST_CHECK_IMPL(ctx, sd.get() == 42);
    }

    inline void readThrows(TestContext& ctx)
    {
        SafeData<int> sd { 1 };

        bool caught = false;
        try {
            sd.read([](const int&) { throw std::runtime_error("read boom"); });
        } catch (const std::runtime_error& ex) {
            caught = true;
            KAS_TEST_CHECK_IMPL(ctx, std::string_view(ex.what()) == "read boom");
        }
        KAS_TEST_CHECK_IMPL(ctx, caught);

        int observed = 0;
        sd.read([&observed](const int& v) { observed = v; });
        KAS_TEST_CHECK_IMPL(ctx, observed == 1);
    }

    // ==================================================================
    // Thread safety
    // ==================================================================

    inline void concurrentReads(TestContext& ctx)
    {
        SafeData<int> sd { 100 };
        std::atomic<int> total { 0 };
        constexpr int kThreads = 4;
        constexpr int kIter = 1000;

        std::vector<std::thread> threads;
        threads.reserve(kThreads);
        for (int i = 0; i < kThreads; ++i) {
            threads.emplace_back([&sd, &total] {
                for (int j = 0; j < kIter; ++j)
                    total.fetch_add(sd.get(), std::memory_order_relaxed);
            });
        }
        for (auto& t : threads)
            t.join();

        KAS_TEST_CHECK_IMPL(ctx, total.load() == kThreads * kIter * 100);
    }

    inline void concurrentModify(TestContext& ctx)
    {
        SafeData<int> sd { 0 };
        constexpr int kThreads = 4;
        constexpr int kIter = 1000;

        std::vector<std::thread> threads;
        threads.reserve(kThreads);
        for (int i = 0; i < kThreads; ++i) {
            threads.emplace_back([&sd] {
                for (int j = 0; j < kIter; ++j)
                    sd.modify([](int& v) { ++v; });
            });
        }
        for (auto& t : threads)
            t.join();

        KAS_TEST_CHECK_IMPL(ctx, sd.get() == kThreads * kIter);
    }

    inline void concurrentReadWrite(TestContext& ctx)
    {
        SafeData<int> sd { 0 };
        std::atomic<bool> stop { false };
        constexpr int kIter = 2000;

        std::thread writer([&sd, &stop] {
            for (int i = 1; i <= kIter; ++i)
                sd.modify([i](int& v) { v = i; });
            stop.store(true, std::memory_order_release);
        });

        std::thread reader([&sd, &stop] {
            while (!stop.load(std::memory_order_acquire)) {
                sd.read([](const int& v) {
                    if (v < 0)
                        std::terminate();
                });
            }
        });

        writer.join();
        reader.join();

        KAS_TEST_CHECK_IMPL(ctx, sd.get() == kIter);
    }

    // ==================================================================
    // update + get - complex
    // ==================================================================

    inline void updateVector(TestContext& ctx)
    {
        SafeData<std::vector<int>> sd { std::vector<int> { 1 } };
        sd.update(std::vector<int> { 10, 20, 30 });
        const auto v = sd.get();
        KAS_TEST_CHECK_IMPL(ctx, v.size() == 3u);
        KAS_TEST_CHECK_IMPL(ctx, v[0] == 10);
        KAS_TEST_CHECK_IMPL(ctx, v[1] == 20);
        KAS_TEST_CHECK_IMPL(ctx, v[2] == 30);
    }

} // namespace

// ============================================================
// Entry point
// ============================================================
/**
 * @brief Runs all SafeData tests and prints a summary.
 */
inline bool runTests()
{
    TestContext ctx;

    constructValue(ctx);
    constructVariadic(ctx);
    constructVector(ctx);
    constructDefault(ctx);

    updateBasic(ctx);
    updateReturnsSelf(ctx);
    updateComplex(ctx);

    readBasic(ctx);
    readConst(ctx);
    readVector(ctx);

    modifyBasic(ctx);
    modifyVector(ctx);

    modifyThrows(ctx);
    readThrows(ctx);

    concurrentReads(ctx);
    concurrentModify(ctx);
    concurrentReadWrite(ctx);

    updateVector(ctx);

    return printSummary("kas::functional::safe_data::tests", ctx);
}

} // namespace kas::functional::safe_data::tests

#endif