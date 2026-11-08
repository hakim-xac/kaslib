/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_TESTS_H
#define KASLIBCXX_TESTS_H

/*
 * kaslibcxx/tests.h - minimal dependency-free unit-test support.
 *
 * This header provides a small, self-contained test harness used by the
 * kaslibcxx test suite. It has no external dependencies (no Catch2,
 * GoogleTest, etc.) and everything here is header-only.
 *
 * Contents:
 *
 *   TestContext
 *     Tracks the number of executed and failed checks. report() records
 *     the result of a single assertion and, on failure, prints the file,
 *     line and expression text to std::cerr. ok() returns true when no
 *     check has failed so far.
 *
 *   CerrMute
 *     RAII helper that temporarily redirects std::cerr into an internal
 *     std::ostringstream, so diagnostic output produced by code under
 *     test can be captured and inspected. The original stream buffer is
 *     restored in the destructor; str() returns the captured text.
 *
 *   Utility helpers for inspecting program output:
 *     hasAnsi        - true if the string contains an ANSI escape intro.
 *     stripAnsi      - removes ANSI SGR color sequences ("\x1b[...m").
 *     endsWithReset  - true if the string ends with the reset code
 *                      "\033[0m".
 *     splitLines     - splits a string into lines, dropping '\n'.
 *     countSubstr    - counts non-overlapping occurrences of a substring.
 *     allDigits      - true if the string is non-empty and contains only
 *                      decimal digits.
 *     printSummary   - prints a summary line of the form
 *                      "[group] checks: N, failed: M".
 *
 *   Macros:
 *     KAS_TEST_CHECK_IMPL(ctx, expr)
 *       Records the result of expr without aborting the current test.
 *     KAS_TEST_REQUIRE_IMPL(ctx, expr)
 *       Same as above, but performs an immediate return from the
 *       enclosing function if expr is false. Useful for preconditions
 *       without which the remaining checks would be meaningless.
 *
 * Notes:
 *   - All non-macro entities live in namespace kas::tests and are
 *     marked inline, so the header can safely be included in multiple
 *     translation units.
 *   - CerrMute is non-copyable in spirit (it owns a stream and swaps
 *     rdbuf); its lifetime must be limited to a single scope.
 *   - stripAnsi uses a function-local static std::regex, compiled once.
 *   - splitLines does not emit a trailing empty line when the input ends
 *     with '\n'.
 *   - No test entry point is provided; test registration, grouping and
 *     main() are the responsibility of the calling code.
 */

#include <algorithm>
#include <cstdio>
#include <exception>
#include <iostream>
#include <mutex>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unistd.h>
#include <vector>

namespace kas::tests {

/**
 * Aggregates the results of a group of checks.
 *
 * total  - number of recorded checks.
 * failed - number of checks that evaluated to false.
 *
 * report() is normally invoked through the KAS_TEST_CHECK_IMPL /
 * KAS_TEST_REQUIRE_IMPL macros so that expr, file and line are captured
 * automatically at the call site.
 */
struct TestContext {
    int total = 0;
    int failed = 0;

    /** Records one check; on failure prints a diagnostic to std::cerr. */
    void report(bool ok, const char* expr, const char* file, int line)
    {
        ++total;
        if (!ok) {
            ++failed;
            std::cerr << "    FAILED " << file << ":" << line
                      << "  " << expr << "\n";
        }
    }

    /** True when no recorded check has failed. */
    bool ok() const { return failed == 0; }
};

/**
 * RAII guard that redirects std::cerr into an internal string stream
 * for the duration of its lifetime.
 *
 * Typical use: construct in a scope, run code under test, call str() to
 * inspect the captured diagnostics. The original std::cerr buffer is
 * restored automatically in the destructor, including on exceptions.
 */
class CerrMute {
public:
    CerrMute()
        : m_old { std::cerr.rdbuf(m_sink.rdbuf()) }
    {
    }
    ~CerrMute() { std::cerr.rdbuf(m_old); }

    /** Returns everything written to std::cerr while the guard was alive. */
    [[nodiscard]] std::string str() const { return m_sink.str(); }

private:
    std::ostringstream m_sink;
    std::streambuf* m_old;
};

/**
 * @brief Thread-safe replacement for CerrMute.
 *
 * Redirects std::cerr into an internal buffer that records, per write,
 * the id of the thread that produced it. Writes from the same thread
 * are coalesced into a single entry until another thread writes, so
 * "std::cerr << a << b << c" from one thread yields one entry with
 * "abc" and that thread's id.
 *
 * The guard is non-copyable and non-movable; it restores the original
 * std::cerr buffer in the destructor.
 *
 * str() returns the concatenation of all captured text (without thread
 * ids), in the order the entries were appended. entries() returns the
 * per-thread records.
 */
class CerrMuteThreaded {
public:
    struct Entry {
        std::thread::id tid;
        std::string text;
    };

    CerrMuteThreaded()
        : m_old { std::cerr.rdbuf(&m_buf) }
    {
    }
    ~CerrMuteThreaded() { std::cerr.rdbuf(m_old); }

    CerrMuteThreaded(const CerrMuteThreaded&) = delete;
    CerrMuteThreaded& operator=(const CerrMuteThreaded&) = delete;

    /** Concatenation of all captured text, no thread ids. */
    [[nodiscard]] std::string str() const
    {
        std::lock_guard lk { m_mx };
        std::string out;
        for (const auto& e : m_entries)
            out += e.text;
        return out;
    }

    /** Per-thread records in append order. */
    [[nodiscard]] std::vector<Entry> entries() const
    {
        std::lock_guard lk { m_mx };
        return m_entries;
    }

private:
    class SyncBuf final : public std::streambuf {
    public:
        explicit SyncBuf(std::mutex& mx, std::vector<Entry>& entries)
            : m_mx { mx }
            , m_entries { entries }
        {
        }

    protected:
        int_type overflow(int_type ch) override
        {
            if (traits_type::eq_int_type(ch, traits_type::eof()))
                return traits_type::not_eof(ch);
            append(traits_type::to_char_type(ch));
            return ch;
        }

        std::streamsize xsputn(const char* s, std::streamsize n) override
        {
            append(s, static_cast<std::size_t>(n));
            return n;
        }

    private:
        void append(char c)
        {
            const auto tid = std::this_thread::get_id();
            std::lock_guard lk { m_mx };
            if (!m_entries.empty() && m_entries.back().tid == tid)
                m_entries.back().text.push_back(c);
            else
                m_entries.push_back(Entry { tid, std::string(1, c) });
        }

        void append(const char* s, std::size_t n)
        {
            if (n == 0)
                return;

            const auto tid = std::this_thread::get_id();
            std::lock_guard lk { m_mx };
            if (!m_entries.empty() && m_entries.back().tid == tid)
                m_entries.back().text.append(s, n);
            else
                m_entries.push_back(Entry { tid, std::string { s, n } });
        }

        std::mutex& m_mx;
        std::vector<Entry>& m_entries;
    };

    mutable std::mutex m_mx;
    std::vector<Entry> m_entries;
    SyncBuf m_buf { m_mx, m_entries };
    std::streambuf* m_old;
};

/** True if s contains an ANSI escape sequence introducer ("\x1b["). */
inline bool hasAnsi(const std::string& s)
{
    return s.find("\x1b[") != std::string::npos;
}

/** Removes ANSI SGR color sequences ("\x1b[...m") from s. */
inline std::string stripAnsi(const std::string& s)
{
    static const std::regex re { R"(\x1b\[[0-9;]*m)" };
    return std::regex_replace(s, re, "");
}

/** True if s ends with the ANSI reset code "\033[0m". */
inline bool endsWithReset(const std::string& s)
{
    return s.size() >= 4 && s.compare(s.size() - 4, 4, "\033[0m") == 0;
}

/**
 * Splits s into lines on '\n'.
 *
 * The newline characters are not included in the result, and no trailing
 * empty element is produced when s ends with '\n'.
 */
inline std::vector<std::string> splitLines(const std::string& s)
{
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == '\n') {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty())
        out.push_back(cur);
    return out;
}

/**
 * Counts non-overlapping occurrences of needle in hay.
 *
 * Returns 0 when needle is empty.
 */
inline std::size_t countSubstr(const std::string& hay, const std::string& needle)
{
    if (needle.empty())
        return 0;
    std::size_t n = 0;
    for (std::size_t pos = hay.find(needle); pos != std::string::npos;
        pos = hay.find(needle, pos + needle.size())) {
        ++n;
    }
    return n;
}

/** True if s is non-empty and consists only of decimal digits. */
inline bool allDigits(const std::string& s)
{
    return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c); });
}

/**
 * Prints a one-line summary for a named test group:
 *     [group] checks: N, failed: M
 */
inline bool printSummary(const char* group, const TestContext& ctx)
{
    std::cout << "[" << group << "] checks: " << ctx.total
              << ", failed: " << ctx.failed << "\n";

    return ctx.ok();
}

} // namespace kas::tests

// ============================================================
//  Common check macros
// ============================================================

/*
 * Records the result of expr in ctx without aborting the test.
 * Use this for ordinary assertions that may fail independently.
 */
#define KAS_TEST_CHECK_IMPL(ctx, expr) \
    (ctx).report(static_cast<bool>(expr), #expr, __FILE__, __LINE__)

/*
 * Like KAS_TEST_CHECK_IMPL, but performs an immediate return from the
 * enclosing function when expr is false. Intended for preconditions
 * without which subsequent checks would be meaningless.
 */
#define KAS_TEST_REQUIRE_IMPL(ctx, expr) \
    do {                                 \
        KAS_TEST_CHECK_IMPL(ctx, expr);  \
        if (!(expr))                     \
            return;                      \
    } while (0)

#endif