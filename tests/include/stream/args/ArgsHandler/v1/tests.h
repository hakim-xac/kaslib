/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_STREAM_ARGS_ARGS_HANDLER_V1_TESTS_H
#define KASLIBCXX_STREAM_ARGS_ARGS_HANDLER_V1_TESTS_H

#include <cstddef>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "KAS/stream/args/ArgsHandler/v1/ArgsHandler.h"
#include "KAS/stream/args/ArgsHandler/v1/CommandInfo.h"
#include "tests/tests.h"

namespace kas::stream::args::args_handler::v1::tests {

namespace {
    using namespace kas::tests;
    using kas::stream::args::v1::ArgsHandler;
    using kas::stream::args::v1::CommandInfo;

    // ------------------------------------------------------------------
    // Helper: argv with stable addresses
    // ------------------------------------------------------------------

    /// Owns the argument strings and builds a stable char* argv view.
    class Argv {
    public:
        /// Copies [begin, end) into internal storage and builds argv.
        template <std::input_iterator It>
        explicit Argv(It begin, It end)
        {
            for (auto it = begin; it != end; ++it)
                m_storage.emplace_back(*it);
            m_argv.reserve(m_storage.size());
            for (auto& s : m_storage)
                m_argv.push_back(s.data());
        }

        /// Returns argc (number of stored arguments).
        [[nodiscard]] int count() const noexcept
        {
            return static_cast<int>(m_argv.size());
        }
        /// Returns argv (array of stable char* pointers).
        [[nodiscard]] char** argv() noexcept { return m_argv.data(); }

    private:
        std::vector<std::string> m_storage; ///< Owned argument strings.
        std::vector<char*> m_argv; ///< Pointers into m_storage.
    };

    // ------------------------------------------------------------------
    // Fixture: ArgsHandler + ostringstream (shared for a test)
    // ------------------------------------------------------------------

    /// Bundles an output stream, an Argv, and a factory for ArgsHandler.
    struct Fixture {
        std::ostringstream out; ///< Captures handler output.
        Argv args; ///< Owned argument strings.

        /// Builds the fixture from a list of argument views.
        explicit Fixture(std::initializer_list<std::string_view> a)
            : out { }
            , args { a.begin(), a.end() }
        {
        }

        /// Creates an ArgsHandler wired to the fixture's argv and out.
        [[nodiscard]] ArgsHandler make()
        {
            return ArgsHandler(args.count(), args.argv(), out);
        }
    };

    // ==================================================================
    // CommandInfo - basics
    // ==================================================================

    // --- 1. ctor + getCommandName + getShowLevelType -------

    /// Construction stores the name and level and does not invoke the callback.
    inline void commandInfoCtor(TestContext& ctx)
    {
        int calls = 0;
        CommandInfo ci {
            "cmd",
            [&calls] { ++calls; },
            CommandInfo::ShowLevelType::User
        };

        KAS_TEST_CHECK_IMPL(ctx, ci.getCommandName() == "cmd");
        KAS_TEST_CHECK_IMPL(ctx,
            ci.getShowLevelType() == CommandInfo::ShowLevelType::User);
        KAS_TEST_CHECK_IMPL(ctx, calls == 0);
    }

    // --- 2. Developer level --------------------------------------

    /// The Developer level is stored and reported by getShowLevelType.
    inline void commandInfoDeveloperLevel(TestContext& ctx)
    {
        CommandInfo ci {
            "cmd",
            [] { },
            CommandInfo::ShowLevelType::Developer
        };
        KAS_TEST_CHECK_IMPL(ctx,
            ci.getShowLevelType() == CommandInfo::ShowLevelType::Developer);
    }

    // --- 3. call() invokes func exactly once per call --------------

    /// Each call() invocation invokes the stored callback exactly once.
    inline void commandInfoCall(TestContext& ctx)
    {
        int calls = 0;
        CommandInfo ci {
            "cmd",
            [&calls] { ++calls; },
            CommandInfo::ShowLevelType::User
        };

        ci.call();
        KAS_TEST_CHECK_IMPL(ctx, calls == 1);

        ci.call();
        KAS_TEST_CHECK_IMPL(ctx, calls == 2);
    }

    // --- 4. call() propagates the exception -----------------------

    /// Exceptions thrown by the callback propagate out of call().
    inline void commandInfoCallThrows(TestContext& ctx)
    {
        CommandInfo ci {
            "cmd",
            [] { throw std::runtime_error("boom"); },
            CommandInfo::ShowLevelType::User
        };

        bool caught = false;
        try {
            ci.call();
        } catch (const std::runtime_error& ex) {
            caught = true;
            KAS_TEST_CHECK_IMPL(ctx, std::string_view(ex.what()) == "boom");
        }
        KAS_TEST_CHECK_IMPL(ctx, caught);
    }

    // ==================================================================
    // CommandInfo::print - description
    // ==================================================================

    // --- 5. printing with a description ------------------------

    /// print() includes both the command name and the configured description.
    inline void printWithDescription(TestContext& ctx)
    {
        CommandInfo ci { "cmd", [] { }, CommandInfo::ShowLevelType::User };
        ci.setDescription("My command");

        std::ostringstream oss;
        ci.print(oss, "app");
        const std::string s = oss.str();

        KAS_TEST_CHECK_IMPL(ctx, s.find("cmd") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, s.find("My command") != std::string::npos);
    }

    // --- 6. printing with an empty description - placeholder ------

    /// With no description, print() emits a placeholder "-------".
    inline void printDefaultDescription(TestContext& ctx)
    {
        CommandInfo ci { "cmd", [] { }, CommandInfo::ShowLevelType::User };

        std::ostringstream oss;
        ci.print(oss, "app");
        const std::string s = oss.str();

        KAS_TEST_CHECK_IMPL(ctx, s.find("-------") != std::string::npos);
    }

    // ==================================================================
    // CommandInfo::print - options
    // ==================================================================

    // --- 7. options with and without description ------------------

    /// print() lists options under "Options:" including their descriptions.
    inline void printOptions(TestContext& ctx)
    {
        CommandInfo ci { "cmd", [] { }, CommandInfo::ShowLevelType::User };
        ci.appendOption("--a", "first option")
            .appendOption("--b"); // without description

        std::ostringstream oss;
        ci.print(oss, "app");
        const std::string s = oss.str();

        KAS_TEST_CHECK_IMPL(ctx, s.find("Options:") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, s.find("--a") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, s.find("first option") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, s.find("--b") != std::string::npos);
    }

    // ==================================================================
    // CommandInfo::print - prototype
    // ==================================================================

    // --- 8. prototype with non-empty options ----------------------

    /// Both "Prototype:" and "Options:" sections are printed when present.
    inline void printPrototypeWithOptions(TestContext& ctx)
    {
        CommandInfo ci { "cmd", [] { }, CommandInfo::ShowLevelType::User };
        ci.appendOption("--a", "opt")
            .appendPrototype(" <file> ");

        std::ostringstream oss;
        ci.print(oss, "app");
        const std::string s = oss.str();

        KAS_TEST_CHECK_IMPL(ctx, s.find("Prototype:") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, s.find("<file>") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, s.find("Options:") != std::string::npos);
    }

    // --- 9. prototype without options must still be printed -------

    /// A prototype is printed even when no options are configured.
    inline void printPrototypeWithoutOptions(TestContext& ctx)
    {
        CommandInfo ci { "cmd", [] { }, CommandInfo::ShowLevelType::User };
        ci.appendPrototype(" <file> ");

        std::ostringstream oss;
        ci.print(oss, "app");
        const std::string s = oss.str();

        KAS_TEST_CHECK_IMPL(ctx, s.find("Prototype:") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, s.find("<file>") != std::string::npos);
    }

    // --- 10. multiple prototypes ----------------------------------

    /// Multiple prototypes are all printed.
    inline void printMultiplePrototypes(TestContext& ctx)
    {
        CommandInfo ci { "cmd", [] { }, CommandInfo::ShowLevelType::User };
        ci.appendOption("--a")
            .appendPrototype(" <file> ")
            .appendPrototype(" <dir> ");

        std::ostringstream oss;
        ci.print(oss, "app");
        const std::string s = oss.str();

        KAS_TEST_CHECK_IMPL(ctx, s.find("<file>") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, s.find("<dir>") != std::string::npos);
    }

    // ==================================================================
    // CommandInfo::print - examples
    // ==================================================================

    // --- 11. example without options ------------------------------

    /// An example with only a description is printed.
    inline void printExampleNoOptions(TestContext& ctx)
    {
        CommandInfo ci { "cmd", [] { }, CommandInfo::ShowLevelType::User };
        ci.appendExample("only description");

        std::ostringstream oss;
        ci.print(oss, "app");
        const std::string s = oss.str();

        KAS_TEST_CHECK_IMPL(ctx, s.find("Example:") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, s.find("only description") != std::string::npos);
    }

    // --- 12. multiple options in an example - space-separated ------

    /// Multiple example options are space-separated and prefixed with a color code.
    inline void printExampleMultipleOptions(TestContext& ctx)
    {
        CommandInfo ci { "cmd", [] { }, CommandInfo::ShowLevelType::User };
        ci.appendExample("example description",
            std::vector<std::string> { "--a", "--b", "--c" });

        std::ostringstream oss;
        ci.print(oss, "app");
        const std::string s = oss.str();

        KAS_TEST_CHECK_IMPL(ctx, s.find("Example:") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, s.find("example description") != std::string::npos);

        KAS_TEST_CHECK_IMPL(ctx, s.find("app cmd \033[33m--a --b --c") != std::string::npos);
    }

    // --- 13. multiple examples ------------------------------------

    /// Multiple examples are all printed.
    inline void printMultipleExamples(TestContext& ctx)
    {
        CommandInfo ci { "cmd", [] { }, CommandInfo::ShowLevelType::User };
        ci.appendExample("first")
            .appendExample("second", std::vector<std::string> { "--x" });

        std::ostringstream oss;
        ci.print(oss, "app");
        const std::string s = oss.str();

        KAS_TEST_CHECK_IMPL(ctx, s.find("first") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, s.find("second") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, s.find("--x") != std::string::npos);
    }

    // ==================================================================
    // ArgsHandler - call()
    // ==================================================================

    // --- 14. no arguments -> print + 0 invocations -----------------

    /// With no command argument, call() prints the command list and invokes nothing.
    inline void handlerNoArgs(TestContext& ctx)
    {
        Fixture f { "prog" };
        auto h = f.make();

        int calls = 0;
        h.insert("cmd", [&calls] { ++calls; }, CommandInfo::ShowLevelType::User);

        h.call();
        KAS_TEST_CHECK_IMPL(ctx, calls == 0);

        const std::string s = f.out.str();
        KAS_TEST_CHECK_IMPL(ctx, s.find("Commands:") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, s.find("cmd") != std::string::npos);
    }

    // --- 15. unknown command -> "Error: ...not found!" -------------

    /// An unknown command prints an error followed by the command list.
    inline void handlerUnknownCommand(TestContext& ctx)
    {
        Fixture f { "prog", "no_such" };
        auto h = f.make();

        int calls = 0;
        h.insert("cmd", [&calls] { ++calls; }, CommandInfo::ShowLevelType::User);

        h.call();
        KAS_TEST_CHECK_IMPL(ctx, calls == 0);

        const std::string s = f.out.str();
        KAS_TEST_CHECK_IMPL(ctx, s.find("Error:") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx,
            s.find("The command was not found!") != std::string::npos);
        // After the error - the command list.
        KAS_TEST_CHECK_IMPL(ctx, s.find("Commands:") != std::string::npos);
    }

    // --- 16. known command -> invocation, no output ----------------

    /// A known command is invoked and produces no output.
    inline void handlerKnownCommand(TestContext& ctx)
    {
        Fixture f { "prog", "cmd" };
        auto h = f.make();

        int calls = 0;
        h.insert("cmd", [&calls] { ++calls; }, CommandInfo::ShowLevelType::User);

        h.call();
        KAS_TEST_CHECK_IMPL(ctx, calls == 1);
        KAS_TEST_CHECK_IMPL(ctx, f.out.str().empty());
    }

    // --- 17. known command throws -> "Error: <what()>" ------------

    /// An exception from the command is reported as "Error: <what()>".
    inline void handlerCommandThrows(TestContext& ctx)
    {
        Fixture f { "prog", "boom" };
        auto h = f.make();

        h.insert("boom", [] { throw std::runtime_error("custom failure"); }, CommandInfo::ShowLevelType::User);

        h.call();

        const std::string s = f.out.str();
        KAS_TEST_CHECK_IMPL(ctx, s.find("Error:") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, s.find("custom failure") != std::string::npos);
        // There must be no "not found" - the command was found.
        KAS_TEST_CHECK_IMPL(ctx,
            s.find("The command was not found!") == std::string::npos);
    }

    // --- 18. Developer command is callable regardless of PrintAll --

    /// ShowLevelType only affects printing; call() still invokes the command.
    inline void handlerDeveloperCommandHidden(TestContext& ctx)
    {
        Fixture f { "prog", "dev" };
        auto h = f.make();

        int calls = 0;
        h.insert("dev", [&calls] { ++calls; }, CommandInfo::ShowLevelType::Developer);

        // call() still finds and invokes it (show_level only affects print).
        h.call();
        KAS_TEST_CHECK_IMPL(ctx, calls == 1);
    }

    // ==================================================================
    // ArgsHandler - insert
    // ==================================================================

    // --- 19. insert returns a reference to CommandInfo ------------

    /// insert() returns a CommandInfo& that can be configured further.
    inline void handlerInsertReturnsRef(TestContext& ctx)
    {
        Fixture f { "prog" };
        auto h = f.make();

        auto& ci = h.insert("cmd", [] { }, CommandInfo::ShowLevelType::User);
        KAS_TEST_CHECK_IMPL(ctx, ci.getCommandName() == "cmd");

        ci.setDescription("desc");

        h.print(CommandInfo::PrintAllCommands::YES);
        KAS_TEST_CHECK_IMPL(ctx,
            f.out.str().find("desc") != std::string::npos);
    }

    // ==================================================================
    // ArgsHandler - print
    // ==================================================================

    // --- 20. print(NO) - User only --------------------------------

    /// print(NO) lists only User-level commands.
    inline void handlerPrintUserOnly(TestContext& ctx)
    {
        Fixture f { "prog" };
        auto h = f.make();

        h.insert("user_cmd", [] { }, CommandInfo::ShowLevelType::User);
        h.insert("dev_cmd", [] { }, CommandInfo::ShowLevelType::Developer);

        h.print(CommandInfo::PrintAllCommands::NO);
        const std::string s = f.out.str();

        KAS_TEST_CHECK_IMPL(ctx, s.find("user_cmd") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, s.find("dev_cmd") == std::string::npos);
    }

    // --- 21. print(YES) - all -------------------------------------

    /// print(YES) lists all commands, including Developer-level ones.
    inline void handlerPrintAll(TestContext& ctx)
    {
        Fixture f { "prog" };
        auto h = f.make();

        h.insert("user_cmd", [] { }, CommandInfo::ShowLevelType::User);
        h.insert("dev_cmd", [] { }, CommandInfo::ShowLevelType::Developer);

        h.print(CommandInfo::PrintAllCommands::YES);
        const std::string s = f.out.str();

        KAS_TEST_CHECK_IMPL(ctx, s.find("user_cmd") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, s.find("dev_cmd") != std::string::npos);
    }

    // --- 22. appname in the output --------------------------------

    /// The application name from argv[0] appears in the printed listing.
    inline void handlerPrintContainsAppName(TestContext& ctx)
    {
        Fixture f { "myapp" };
        auto h = f.make();
        h.insert("cmd", [] { }, CommandInfo::ShowLevelType::User);

        h.print(CommandInfo::PrintAllCommands::NO);
        const std::string s = f.out.str();

        KAS_TEST_CHECK_IMPL(ctx, s.find("myapp") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, s.find("Commands:") != std::string::npos);
    }

    // --- 23. default appname when argv[0] is missing --------------
    //         (InputArgs with count == 0)

    /// When there is no argv[0], print() falls back to the "appname" placeholder.
    inline void handlerPrintNoAppName(TestContext& ctx)
    {
        std::ostringstream out;
        // count = 0 -> m_input_args empty -> getIndex(0) = nullopt -> "appname".
        ArgsHandler h(0, nullptr, out);
        h.insert("cmd", [] { }, CommandInfo::ShowLevelType::User);

        h.print(CommandInfo::PrintAllCommands::NO);
        KAS_TEST_CHECK_IMPL(ctx,
            out.str().find("appname") != std::string::npos);
    }

    // --- 24. empty m_commands -> "No commands available" ----------

    /// With no registered commands, print() reports "No commands available".
    inline void handlerPrintEmptyCommands(TestContext& ctx)
    {
        Fixture f { "prog" };
        auto h = f.make();

        h.print(CommandInfo::PrintAllCommands::NO);
        const std::string s = f.out.str();

        KAS_TEST_CHECK_IMPL(ctx,
            s.find("No commands available") != std::string::npos);
    }

    // --- 25. command with description and option in print ---------

    /// A command's description and options are printed in the listing.
    inline void handlerPrintCommandWithDescription(TestContext& ctx)
    {
        Fixture f { "prog" };
        auto h = f.make();

        h.insert("cmd", [] { }, CommandInfo::ShowLevelType::User).setDescription("my desc").appendOption("--o", "opt desc");

        h.print(CommandInfo::PrintAllCommands::YES);
        const std::string s = f.out.str();

        KAS_TEST_CHECK_IMPL(ctx, s.find("cmd") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, s.find("my desc") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, s.find("--o") != std::string::npos);
        KAS_TEST_CHECK_IMPL(ctx, s.find("opt desc") != std::string::npos);
    }

} // namespace

// ============================================================
// Entry point
// ============================================================

/**
 * @brief Runs all ArgsHandler v1 tests.
 *
 * Executes every test case defined in this header against a shared
 * TestContext and prints a summary for the
 * "kas::stream::args::v1::tests" suite.
 */
inline bool runTests()
{
    TestContext ctx;

    commandInfoCtor(ctx);
    commandInfoDeveloperLevel(ctx);
    commandInfoCall(ctx);
    commandInfoCallThrows(ctx);

    printWithDescription(ctx);
    printDefaultDescription(ctx);
    printOptions(ctx);
    printPrototypeWithOptions(ctx);
    printPrototypeWithoutOptions(ctx);
    printMultiplePrototypes(ctx);
    printExampleNoOptions(ctx);
    printExampleMultipleOptions(ctx);
    printMultipleExamples(ctx);

    handlerNoArgs(ctx);
    handlerUnknownCommand(ctx);
    handlerKnownCommand(ctx);
    handlerCommandThrows(ctx);
    handlerDeveloperCommandHidden(ctx);

    handlerInsertReturnsRef(ctx);

    handlerPrintUserOnly(ctx);
    handlerPrintAll(ctx);
    handlerPrintContainsAppName(ctx);
    handlerPrintNoAppName(ctx);
    handlerPrintEmptyCommands(ctx);
    handlerPrintCommandWithDescription(ctx);

    return printSummary("kas::stream::args::v1::tests", ctx);
}

} // namespace kas::stream::args::v1::tests

#endif