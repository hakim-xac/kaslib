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

#include <string>
#include <vector>

namespace kas::stream::args::v1 {

/**
 * @brief Command registry and dispatcher for command-line arguments.
 *
 * ArgsHandler parses the process command line (argc/argv) into an
 * InputArgs, stores a registry of named commands, prints help, and
 * dispatches the selected command.
 *
 * Typical usage:
 *  - construct with argc/argv and an output stream;
 *  - register commands with insert();
 *  - call print() to show help (e.g. for --help);
 *  - call call() to dispatch the command selected on the command line.
 *
 * The output stream is not owned by ArgsHandler and must outlive it.
 *
 * @par Thread safety
 * ArgsHandler is not thread-safe. All calls (insert, call, print)
 * must be serialized by the caller.
 *
 * @par Example
 * @code
 * kas::stream::args::v1::ArgsHandler handler(argc, argv, std::cout);
 * handler.insert("build",
 *     [] { do_build(); },
 *     kas::stream::args::v1::CommandInfo::ShowLevelType::ALWAYS);
 * handler.call();
 * @endcode
 */
class ArgsHandler final {
public:
    /**
     * @brief Constructs the handler and parses the command line.
     *
     * @param argc Number of command-line arguments.
     * @param argv Array of command-line arguments; must be non-null
     *             and contain at least argc entries.
     * @param out  Output stream used by print(). Must outlive *this;
     *             the handler stores a non-owning pointer.
     *
     * @throws Depends on InputArgs parsing; see InputArgs.
     */
    explicit ArgsHandler(int argc, char** argv, std::ostream& out);

    /**
     * @brief Registers a command.
     *
     * Inserts a command into the registry and returns a reference to
     * the stored CommandInfo so the caller can further configure it
     * (description, aliases, etc.).
     *
     * The callable is moved into the registry; it is executed by
     * call() when the command is selected on the command line.
     *
     * This overload is ref-qualified `&`: it cannot be called on a
     * temporary ArgsHandler.
     *
     * @param command_name   Name of the command as it appears on the
     *                       command line.
     * @param func           Callable executed when the command is
     *                       selected. Must be move-only.
     * @param show_level_type Visibility of the command in the printed
     *                        help (see CommandInfo::ShowLevelType).
     * @return Reference to the stored CommandInfo; valid until the
     *         next insert() (the vector may reallocate) or until the
     *         handler is destroyed.
     *
     * @warning The returned reference may be invalidated by a
     *          subsequent insert() due to vector reallocation.
     */
    CommandInfo& insert(
        std::string command_name,
        kas::functional::MoveOnlyFunction<void()> func,
        CommandInfo::ShowLevelType show_level_type) &;

    /**
     * @brief Dispatches the command selected on the command line.
     *
     * Looks up the command chosen in the parsed arguments and invokes
     * its callable. If no command was selected, behavior is defined by
     * the implementation (for example, printing help).
     *
     * @note A command that is not registered is not executed; the
     *       implementation may report an error via the output stream.
     */
    void call() const;

    /**
     * @brief Prints help for the registered commands.
     *
     * @param pac Controls whether all commands are printed or only
     *            those whose ShowLevelType allows it (see
     *            CommandInfo::PrintAllCommands).
     */
    void print(CommandInfo::PrintAllCommands pac = CommandInfo::PrintAllCommands::NO) const;

private:
    InputArgs m_input_args; ///< Parsed command-line arguments.
    std::ostream* m_out_ptr; ///< Non-owning output stream for print().
    std::vector<CommandInfo> m_commands; ///< Registered commands (FIFO order of insertion).
};
}

#endif