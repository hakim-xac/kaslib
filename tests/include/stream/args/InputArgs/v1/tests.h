/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_STREAM_INPUT_ARGS_V1_TESTS_H
#define KASLIBCXX_STREAM_INPUT_ARGS_V1_TESTS_H

#include <cstddef>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "KAS/stream/args/InputArgs/v1/InputArgs.h"
#include "tests/tests.h"

/**
 * @file InputArgsTests.h
 * @brief Unit tests for kas::stream::args::v1::InputArgs.
 *
 * The tests cover:
 *  - getIndex()/countIndex() for positional access, including
 *    out-of-range, empty argv, nullptr argv, and negative count;
 *  - getKey()/hasKey()/countKeys() for the "--key value" form,
 *    flags without values, a flag followed by another key, keys
 *    after positional arguments, "--key=value" not being split,
 *    single-dash keys, and a trailing key without a value;
 *  - duplicate keys: the last value wins (insert_or_assign);
 *  - print(): non-empty output, contains the program name, keys and
 *    values, and ends with a newline;
 *  - repeatability of getIndex()/getKey() and consistency between
 *    hasKey() and getKey();
 *  - static type properties.
 *
 * Test style:
 *  - each test is a free function taking TestContext&;
 *  - KAS_TEST_CHECK_IMPL records a failure but does not abort the test;
 *  - Argv is a small helper that owns a stable std::vector<std::string>
 *    plus a char* array, so InputArgs can keep non-owning
 *    std::string_view values into it;
 *  - the parser contract is pinned explicitly: "--key=value" is NOT
 *    split, "--flag" without a value yields an empty string value,
 *    and a positional argument between keys does not become a value.
 *
 * @note The tests assume the following InputArgs contracts:
 *  - InputArgs(count, nullptr) is a safe no-op;
 *  - InputArgs(count <= 0, argv) is a safe no-op;
 *  - an element starting with '-' is treated as a key;
 *  - a non-key element following a key without a value becomes that
 *    key's value;
 *  - "--key=value" is stored as a single key with an empty value;
 *  - the last duplicate key wins;
 *  - print() writes a trailing newline.
 */

namespace kas::stream::args::input_args::v1::tests {

namespace {
    using namespace kas::tests;
    using kas::stream::args::v1::InputArgs;

    /**
     * @brief Owns a stable argv-like array of strings.
     *
     * Holds a std::vector<std::string> so that the char* pointers and
     * the std::string_view values inside InputArgs remain valid for
     * the lifetime of the Argv object.
     */
    class Argv {
    public:
        /**
         * @brief Builds the storage and the char* array.
         * @param args Initializer list of argument strings.
         */
        explicit Argv(std::initializer_list<std::string_view> args)
        {
            m_storage.reserve(args.size());
            for (auto sv : args)
                m_storage.emplace_back(sv);

            m_argv.reserve(m_storage.size());
            for (auto& s : m_storage)
                m_argv.push_back(s.data());
        }

        /// Number of arguments (equivalent to argc).
        [[nodiscard]] int count() const noexcept
        {
            return static_cast<int>(m_argv.size());
        }

        /// Mutable argv pointer array (equivalent to argv).
        [[nodiscard]] char** argv() noexcept { return m_argv.data(); }

    private:
        std::vector<std::string> m_storage; ///< Owns the argument text.
        std::vector<char*> m_argv; ///< Non-owning pointers into m_storage.
    };

    // ==================================================================
    //  getIndex
    // ==================================================================

    /**
     * @brief Verifies positional access to all arguments.
     *
     * m_args contains the whole argv, including the program name.
     */
    inline void indexBasic(TestContext& ctx)
    {
        Argv a { "prog", "one", "two" };
        InputArgs in(a.count(), a.argv());

        KAS_TEST_CHECK_IMPL(ctx, in.getIndex(0).has_value() && *in.getIndex(0) == "prog");
        KAS_TEST_CHECK_IMPL(ctx, in.getIndex(1).has_value() && *in.getIndex(1) == "one");
        KAS_TEST_CHECK_IMPL(ctx, in.getIndex(2).has_value() && *in.getIndex(2) == "two");
    }

    /**
     * @brief Verifies that out-of-range indices return std::nullopt.
     */
    inline void indexOutOfRange(TestContext& ctx)
    {
        Argv a { "prog", "one" };
        InputArgs in(a.count(), a.argv());

        KAS_TEST_CHECK_IMPL(ctx, !in.getIndex(2).has_value());
        KAS_TEST_CHECK_IMPL(ctx, !in.getIndex(100).has_value());
    }

    /**
     * @brief Verifies behavior with an empty argv.
     */
    inline void indexEmpty(TestContext& ctx)
    {
        Argv a { };
        InputArgs in(a.count(), a.argv());
        KAS_TEST_CHECK_IMPL(ctx, in.countIndex() == 0u);
        KAS_TEST_CHECK_IMPL(ctx, !in.getIndex(0).has_value());
    }

    /**
     * @brief Verifies that nullptr argv is handled as an empty input.
     */
    inline void indexNullArgv(TestContext& ctx)
    {
        InputArgs in(5, nullptr); // safety: argv == nullptr
        KAS_TEST_CHECK_IMPL(ctx, in.countIndex() == 0u);
        KAS_TEST_CHECK_IMPL(ctx, in.countKeys() == 0u);
    }

    /**
     * @brief Verifies that a non-positive count is handled as empty.
     */
    inline void indexNegativeCount(TestContext& ctx)
    {
        Argv a { "prog" };
        InputArgs in(-1, a.argv()); // safety: count <= 0
        KAS_TEST_CHECK_IMPL(ctx, in.countIndex() == 0u);
    }

    /**
     * @brief Verifies that countIndex() reports the full argv size.
     */
    inline void countIndex(TestContext& ctx)
    {
        Argv a { "p", "a", "b", "c" };
        InputArgs in(a.count(), a.argv());
        KAS_TEST_CHECK_IMPL(ctx, in.countIndex() == 4u);
    }

    // ==================================================================
    //  getKey / hasKey - "--key value" form (two arguments)
    // ==================================================================

    /**
     * @brief Verifies the basic "--key value" form.
     */
    inline void keySeparateArg(TestContext& ctx)
    {
        Argv a { "prog", "--name", "Alice", "--age", "30" };
        InputArgs in(a.count(), a.argv());

        const auto name = in.getKey("--name");
        const auto age = in.getKey("--age");
        KAS_TEST_CHECK_IMPL(ctx, name.has_value() && *name == "Alice");
        KAS_TEST_CHECK_IMPL(ctx, age.has_value() && *age == "30");
        KAS_TEST_CHECK_IMPL(ctx, in.hasKey("--name"));
        KAS_TEST_CHECK_IMPL(ctx, in.hasKey("--age"));
    }

    /**
     * @brief Verifies that an absent key is reported as missing.
     */
    inline void keyMissing(TestContext& ctx)
    {
        Argv a { "prog", "--name", "Alice" };
        InputArgs in(a.count(), a.argv());

        KAS_TEST_CHECK_IMPL(ctx, !in.getKey("--age").has_value());
        KAS_TEST_CHECK_IMPL(ctx, !in.hasKey("--age"));
    }

    /**
     * @brief Verifies that a flag without a following value has an
     *        empty string value and is still reported as present.
     */
    inline void keyFlagNoValue(TestContext& ctx)
    {
        // "--verbose" without a following value (or the next arg is
        // also a key) -> value ""
        Argv a { "prog", "--verbose" };
        InputArgs in(a.count(), a.argv());

        const auto v = in.getKey("--verbose");
        KAS_TEST_CHECK_IMPL(ctx, v.has_value() && v->empty());
        KAS_TEST_CHECK_IMPL(ctx, in.hasKey("--verbose"));
    }

    /**
     * @brief Verifies that a flag immediately followed by another key
     *        does not consume the next key as its value.
     */
    inline void keyFlagFollowedByKey(TestContext& ctx)
    {
        // "--a" has no value, then "--b x" -> "--a" = "", "--b" = "x"
        Argv a { "prog", "--a", "--b", "x" };
        InputArgs in(a.count(), a.argv());

        KAS_TEST_CHECK_IMPL(ctx, in.getKey("--a").has_value());
        KAS_TEST_CHECK_IMPL(ctx, in.getKey("--a")->empty());
        KAS_TEST_CHECK_IMPL(ctx, in.getKey("--b").has_value());
        KAS_TEST_CHECK_IMPL(ctx, *in.getKey("--b") == "x");
    }

    /**
     * @brief Verifies that a positional argument between keys is not
     *        treated as a value, and an extra positional after a value
     *        is ignored (not stored as a key).
     */
    inline void keyPositionalBetween(TestContext& ctx)
    {
        // "positional" before a key is neither a key nor a value
        // (tmp_key is empty). "--k" -> "v", and "extra" after the
        // value is ignored (tmp_key->value is already set).
        Argv a { "prog", "positional", "--k", "v", "extra" };
        InputArgs in(a.count(), a.argv());

        KAS_TEST_CHECK_IMPL(ctx, in.getKey("--k").has_value());
        KAS_TEST_CHECK_IMPL(ctx, *in.getKey("--k") == "v");
        KAS_TEST_CHECK_IMPL(ctx, !in.hasKey("positional"));
        KAS_TEST_CHECK_IMPL(ctx, !in.hasKey("extra"));
    }

    /**
     * @brief Pins the contract that "--key=value" is not split.
     *
     * The whole "--name=Alice" is stored as a single key with an
     * empty value; getKey("--name") returns std::nullopt.
     */
    inline void keyWithEqualsIsNotSplit(TestContext& ctx)
    {
        // "--name=Alice" is one argument; the implementation stores it
        // as a single key "--name=Alice" (it does not split it into a
        // key and a value).
        Argv a { "prog", "--name=Alice" };
        InputArgs in(a.count(), a.argv());

        KAS_TEST_CHECK_IMPL(ctx, in.hasKey("--name=Alice"));
        KAS_TEST_CHECK_IMPL(ctx, in.getKey("--name=Alice").has_value());
        KAS_TEST_CHECK_IMPL(ctx, in.getKey("--name=Alice")->empty());
        KAS_TEST_CHECK_IMPL(ctx, !in.hasKey("--name"));
    }

    /**
     * @brief Verifies that a single-dash key is accepted.
     */
    inline void keySingleDash(TestContext& ctx)
    {
        // "-v" is a key as well (it starts with '-')
        Argv a { "prog", "-v", "value" };
        InputArgs in(a.count(), a.argv());

        KAS_TEST_CHECK_IMPL(ctx, in.hasKey("-v"));
        KAS_TEST_CHECK_IMPL(ctx, *in.getKey("-v") == "value");
    }

    /**
     * @brief Verifies a trailing key without a value.
     */
    inline void keyLastNoValue(TestContext& ctx)
    {
        Argv a { "prog", "--a", "1", "--b" };
        InputArgs in(a.count(), a.argv());

        KAS_TEST_CHECK_IMPL(ctx, *in.getKey("--a") == "1");
        KAS_TEST_CHECK_IMPL(ctx, in.getKey("--b").has_value());
        KAS_TEST_CHECK_IMPL(ctx, in.getKey("--b")->empty());
    }

    /**
     * @brief Verifies countKeys() with two keys and a positional.
     */
    inline void countKeys(TestContext& ctx)
    {
        Argv a { "prog", "--a", "1", "--b", "2", "positional" };
        InputArgs in(a.count(), a.argv());
        KAS_TEST_CHECK_IMPL(ctx, in.countKeys() == 2u); // --a, --b
    }

    /**
     * @brief Verifies countKeys() == 0 when there are no keys.
     */
    inline void countKeysEmpty(TestContext& ctx)
    {
        Argv a { "prog", "a", "b", "c" };
        InputArgs in(a.count(), a.argv());
        KAS_TEST_CHECK_IMPL(ctx, in.countKeys() == 0u);
    }

    /**
     * @brief Verifies countKeys() with several flag-only keys.
     */
    inline void countKeysFlag(TestContext& ctx)
    {
        Argv a { "prog", "--x", "--y" };
        InputArgs in(a.count(), a.argv());
        KAS_TEST_CHECK_IMPL(ctx, in.countKeys() == 2u);
        KAS_TEST_CHECK_IMPL(ctx, in.getKey("--x")->empty());
        KAS_TEST_CHECK_IMPL(ctx, in.getKey("--y")->empty());
    }

    // ==================================================================
    //  print
    // ==================================================================

    /**
     * @brief Verifies that print() writes something for non-empty input.
     */
    inline void printNotEmpty(TestContext& ctx)
    {
        Argv a { "prog", "--name", "Alice" };
        InputArgs in(a.count(), a.argv());

        std::ostringstream oss;
        in.print(oss);
        KAS_TEST_CHECK_IMPL(ctx, !oss.str().empty());
    }

    /**
     * @brief Verifies that print() includes the program name, keys and
     *        values.
     */
    inline void printContainsValues(TestContext& ctx)
    {
        Argv a { "prog", "--name", "Alice" };
        InputArgs in(a.count(), a.argv());

        std::ostringstream oss;
        in.print(oss);
        const std::string s = oss.str();

        KAS_TEST_CHECK_IMPL(ctx, s.find("prog") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, s.find("--name") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, s.find("Alice") != std::string::npos);
    }

    /**
     * @brief Verifies that print() ends with a newline.
     */
    inline void printEndsWithNewline(TestContext& ctx)
    {
        Argv a { "prog" };
        InputArgs in(a.count(), a.argv());

        std::ostringstream oss;
        in.print(oss);
        KAS_TEST_CHECK_IMPL(ctx, !oss.str().empty() && oss.str().back() == '\n');
    }

    // ==================================================================
    //  Properties / idempotence
    // ==================================================================

    /**
     * @brief Verifies that repeated getIndex() calls return the same
     *        value (no hidden mutation, no dangling views).
     */
    inline void getIndexRepeatable(TestContext& ctx)
    {
        Argv a { "prog", "one" };
        InputArgs in(a.count(), a.argv());

        const auto first = in.getIndex(1);
        for (int i = 0; i < 10; ++i) {
            const auto again = in.getIndex(1);
            KAS_TEST_CHECK_IMPL(ctx, first.has_value() == again.has_value());
            if (first && again)
                KAS_TEST_CHECK_IMPL(ctx, *first == *again);
        }
    }

    /**
     * @brief Verifies that repeated getKey() calls return the same
     *        value.
     */
    inline void getKeyRepeatable(TestContext& ctx)
    {
        Argv a { "prog", "--k", "v" };
        InputArgs in(a.count(), a.argv());

        const auto first = in.getKey("--k");
        for (int i = 0; i < 10; ++i) {
            const auto again = in.getKey("--k");
            KAS_TEST_CHECK_IMPL(ctx, first.has_value() == again.has_value());
            if (first && again)
                KAS_TEST_CHECK_IMPL(ctx, *first == *again);
        }
    }

    /**
     * @brief Verifies that hasKey(k) matches getKey(k).has_value()
     *        for present and absent keys.
     */
    inline void hasKeyMatchesGetKey(TestContext& ctx)
    {
        Argv a { "prog", "--a", "1", "--b", "2" };
        InputArgs in(a.count(), a.argv());

        KAS_TEST_CHECK_IMPL(ctx, in.hasKey("--a") == in.getKey("--a").has_value());
        KAS_TEST_CHECK_IMPL(ctx, in.hasKey("--b") == in.getKey("--b").has_value());
        KAS_TEST_CHECK_IMPL(ctx, in.hasKey("--c") == in.getKey("--c").has_value());
    }

    /**
     * @brief Pins the duplicate-key contract: the last value wins.
     *
     * insert_or_assign overwrites the previous value, and countKeys()
     * reflects a single key.
     */
    inline void duplicateKeyLastWins(TestContext& ctx)
    {
        // insert_or_assign: a repeated key overwrites the previous one
        Argv a { "prog", "--k", "first", "--k", "second" };
        InputArgs in(a.count(), a.argv());

        const auto v = in.getKey("--k");
        KAS_TEST_CHECK_IMPL(ctx, v.has_value() && *v == "second");
        KAS_TEST_CHECK_IMPL(ctx, in.countKeys() == 1u);
    }

} // namespace

// ============================================================
//  Entry point
// ============================================================

/**
 * @brief Runs all InputArgs unit tests.
 *
 * Tests are executed in a fixed order: positional access, key access,
 * print, repeatability and duplicate handling. The summary is printed
 * via printSummary() at the end.
 */
inline bool runTests()
{
    TestContext ctx;

    indexBasic(ctx);
    indexOutOfRange(ctx);
    indexEmpty(ctx);
    indexNullArgv(ctx);
    indexNegativeCount(ctx);
    countIndex(ctx);

    keySeparateArg(ctx);
    keyMissing(ctx);
    keyFlagNoValue(ctx);
    keyFlagFollowedByKey(ctx);
    keyPositionalBetween(ctx);
    keyWithEqualsIsNotSplit(ctx);
    keySingleDash(ctx);
    keyLastNoValue(ctx);
    countKeys(ctx);
    countKeysEmpty(ctx);
    countKeysFlag(ctx);

    printNotEmpty(ctx);
    printContainsValues(ctx);
    printEndsWithNewline(ctx);

    getIndexRepeatable(ctx);
    getKeyRepeatable(ctx);
    hasKeyMatchesGetKey(ctx);
    duplicateKeyLastWins(ctx);

    return printSummary("kas::stream::args::input_args::v1::tests", ctx);
}

} // namespace kas::stream::args::input_args::v1::tests

#endif