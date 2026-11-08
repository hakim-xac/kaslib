/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_STREAM_ARGS_ARGS_HANDLER_V1_H
#define KASLIBCXX_STREAM_ARGS_ARGS_HANDLER_V1_H

#include "CommandInfo.h"

#include <KAS/functional/MoveOnlyFunction.h>
#include <KAS/stream/args/InputArgs/InputArgs.h>

#include <iosfwd>
#include <string>
#include <vector>

namespace kas::stream::args::v1 {

/**
 * @brief Command registry and dispatcher for command-line arguments.
 *
 * ArgsHandler owns a parsed InputArgs (built either from argc/argv or
 * from a caller-supplied InputArgs), stores a registry of named
 * commands, can print help for them, and dispatches the command
 * selected on the command line.
 *
 * Typical usage:
 *  - construct with argc/argv (or a pre-parsed InputArgs) and an
 *    output stream;
 *  - register commands with insert();
 *  - call print() to show help (e.g. in response to --help);
 *  - call call() to dispatch the command selected on the command line.
 *
 * All diagnostic and help output is written to the stream supplied at
 * construction. That stream is not owned by ArgsHandler and must
 * outlive it.
 *
 * @par Thread safety
 * ArgsHandler is not thread-safe. All calls (insert, call, print) and
 * destruction must be serialized by the caller.
 *
 * @par Example
 * @code
 * kas::stream::args::v1::ArgsHandler handler(argc, argv, std::cout);
 * handler.insert("build",
 *     []() -> bool { do_build(); return true; },
 *     kas::stream::args::v1::CommandInfo::ShowLevelType::User)
 *     .setDescription("Build the project")
 *     .appendPrototype("app build [--release]")
 *     .appendOption("--release", "Build in release mode")
 *     .appendExample("Release build", { "app", "build", "--release" });
 * handler.call();
 * @endcode
 *
 * @note The callable passed to insert() must satisfy
 *       CommandInfo::function_t (MoveOnlyFunction<bool()>): it must be
 *       move-only and return bool. A capture-less lambda is copyable
 *       and will not bind; wrap it or capture something by move.
 */
class ArgsHandler final {
public:
    /**
     * @brief Constructs the handler, parses argc/argv into InputArgs,
     *        and stores the output stream.
     *
     * @param argc Number of command-line arguments.
     * @param argv Array of command-line arguments; must be non-null
     *             and contain at least argc entries.
     * @param out  Output stream used by print() and, where applicable,
     *             by call() to report errors. Must outlive *this; the
     *             handler stores a non-owning pointer.
     *
     * @throws Whatever InputArgs throws while parsing (see InputArgs).
     */
    explicit ArgsHandler(int argc, char** argv, std::ostream& out);

    /**
     * @brief Constructs the handler from an already-parsed InputArgs
     *        and stores the output stream.
     *
     * @param input_args Parsed command-line arguments, moved into the
     *                   handler.
     * @param out        Output stream used by print() and, where
     *                   applicable, by call() to report errors. Must
     *                   outlive *this; the handler stores a non-owning
     *                   pointer.
     */
    explicit ArgsHandler(InputArgs input_args, std::ostream& out);

    /**
     * @brief Registers a command.
     *
     * Appends a CommandInfo to the registry and returns a reference to
     * it, so the caller can further configure the stored entry
     * (description, options, prototypes, examples). The callable and
     * visibility level are supplied here and stored as-is.
     *
     * The callable is moved into the registry and is executed by
     * call() when the command is selected on the command line. It must
     * satisfy CommandInfo::function_t (MoveOnlyFunction<bool()>).
     *
     * This overload is ref-qualified `&`: it cannot be called on a
     * temporary ArgsHandler.
     *
     * @param command_name    Name of the command as it appears on the
     *                        command line.
     * @param func            Callable executed when the command is
     *                        selected. Must be move-only and return
     *                        bool.
     * @param show_level_type Visibility of the command in the printed
     *                        help (see CommandInfo::ShowLevelType).
     *
     * @return Reference to the stored CommandInfo. The reference is
     *         valid until the next insert() (the underlying vector may
     *         reallocate) or until the handler is destroyed.
     */
    CommandInfo& insert(
        std::string command_name,
        CommandInfo::function_t func,
        CommandInfo::ShowLevelType show_level_type) &;

    /**
     * @brief Dispatches the command selected on the command line.
     *
     * Looks up the command chosen in the parsed arguments and invokes
     * its callable. If no command was selected, behavior is defined by
     * the implementation (for example, printing help to the output
     * stream).
     *
     * A command that is not registered is not executed; the
     * implementation may report an error via the output stream.
     *
     * @return true if a registered command was found and executed,
     *         false otherwise (no command selected, unknown command,
     *         or an error was reported).
     *
     * @note call() is `const` with respect to the registry, but it
     *       executes user-supplied code and may write to the output
     *       stream. It must not be invoked concurrently with insert()
     *       or print().
     */
    bool call() const;

    /**
     * @brief Prints help for the registered commands.
     *
     * Output is written to the stream supplied at construction.
     *
     * @param pac Controls whether all commands are printed or only
     *            those whose ShowLevelType allows it (see
     *            CommandInfo::PrintAllCommands).
     */
    void print(CommandInfo::PrintAllCommands pac = CommandInfo::PrintAllCommands::NO) const;

private:
    InputArgs m_input_args; ///< Parsed command-line arguments.
    std::ostream* m_out_ptr; ///< Non-owning output stream for print() and error reporting.
    std::vector<CommandInfo> m_commands; ///< Registered commands (FIFO order of insertion).
};

}

#endif