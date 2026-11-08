/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_STREAM_ARGS_COMMAND_INFO_V1_H
#define KASLIBCXX_STREAM_ARGS_COMMAND_INFO_V1_H

#include <KAS/functional/MoveOnlyFunction.h>

#include <cstdint>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace kas::stream::args::v1 {

/**
 * @brief Metadata and callable for a single command-line command.
 *
 * CommandInfo holds everything ArgsHandler needs to know about one
 * command: its name, the callable to run when the command is selected,
 * visibility in the printed help, an optional description, option
 * entries, usage prototypes, and examples.
 *
 * CommandInfo is move-only because the stored callable is move-only.
 * Setters are ref-qualified `&` and return *this to allow chaining:
 *
 * @code
 * handler.insert("build", []() -> bool { do_build(); return true; },
 *                ShowLevelType::User)
 *     .setDescription("Build the project")
 *     .appendPrototype("app build [--release]")
 *     .appendOption("--release", "Build in release mode")
 *     .appendExample("Release build", { "app", "build", "--release" });
 * @endcode
 *
 * @par Thread safety
 * CommandInfo is not thread-safe. All calls must be serialized by the
 * caller. In particular, do not call call() concurrently with any
 * setter or with print() on the same object.
 */
class CommandInfo final {
public:
    using function_t = kas::functional::MoveOnlyFunction<bool()>;

    /// Visibility of a command in the printed help.
    enum class ShowLevelType : uint8_t {
        User, ///< Always shown in help.
        Developer ///< Shown only when print() is called with
                  ///< PrintAllCommands::YES (developer-level help).
    };

    /// Whether print() should include commands hidden by ShowLevelType.
    enum class PrintAllCommands : uint8_t {
        YES, ///< Print every registered command, including Developer ones.
        NO ///< Print only commands whose ShowLevelType is User.
    };

    /// One named option with an optional description.
    struct OptionCommandInfo {
        std::string name; ///< Option spelling, e.g. "--release".
        std::optional<std::string> description_opt; ///< Optional human-readable description.
    };

    /// One usage example: a short description plus an argv-like line.
    struct Examples {
        std::string description; ///< What this example demonstrates.
        std::vector<std::string> options; ///< argv-like words for the example.
    };

    /**
     * @brief Constructs a CommandInfo.
     *
     * @param command_name    Name of the command as it appears on the
     *                        command line.
     * @param func            Callable executed by call(). Must be
     *                        move-only and return bool.
     * @param show_level_type Visibility of the command in printed help.
     */
    explicit CommandInfo(
        std::string command_name,
        function_t func,
        ShowLevelType show_level_type);

    CommandInfo(const CommandInfo&) = delete;
    CommandInfo(CommandInfo&&) noexcept = default;
    CommandInfo& operator=(const CommandInfo&) = delete;
    CommandInfo& operator=(CommandInfo&&) noexcept = default;
    ~CommandInfo() = default;

    /**
     * @brief Invokes the stored callable.
     *
     * The call is `const` with respect to CommandInfo: it does not
     * modify the metadata. The callable is invoked through the stored
     * MoveOnlyFunction and may have arbitrary side effects; to mutate
     * its own captured state it must use `mutable` captures.
     *
     * @return The bool returned by the stored callable. A `false` means
     *         "the command ran but reported failure"; it does not mean
     *         "the command was not found" (that is ArgsHandler::call()'s
     *         concern).
     *
     * @throws Whatever the stored callable throws.
     */
    [[nodiscard]] bool call() const;

    /**
     * @brief Returns the command name.
     *
     * Ref-qualified `&`: not callable on an rvalue CommandInfo. After
     * the object has been moved from, the returned reference refers to
     * the moved-from string and must not be relied upon.
     *
     * @return Const reference to the stored command name. Valid for
     *         the lifetime of *this or until the object is moved from.
     */
    [[nodiscard]] const std::string& getCommandName() const& noexcept;

    /**
     * @brief Returns the visibility level of the command.
     */
    [[nodiscard]] ShowLevelType getShowLevelType() const noexcept;

    /**
     * @brief Sets the command description.
     * @param description Human-readable description shown in help.
     * @return *this, to allow chaining.
     */
    CommandInfo& setDescription(std::string description) &;

    /**
     * @brief Appends a usage example.
     * @param description Short description of the example.
     * @param options     argv-like words for the example.
     * @return *this, to allow chaining.
     */
    CommandInfo& appendExample(std::string description,
        std::vector<std::string> options = { }) &;

    /**
     * @brief Appends a usage prototype line.
     * @param prototype Prototype string, e.g. "app build [--release]".
     * @return *this, to allow chaining.
     */
    CommandInfo& appendPrototype(std::string prototype) &;

    /**
     * @brief Appends a named option entry.
     * @param name            Option spelling, e.g. "--release".
     * @param description_opt Optional description of the option.
     * @return *this, to allow chaining.
     */
    CommandInfo& appendOption(
        std::string name,
        std::optional<std::string> description_opt = std::nullopt) &;

    /**
     * @brief Prints the command help to the given stream.
     *
     * Order of output: command name and description, then prototypes,
     * then options, then examples, each section prefixed with
     * `appname` where meaningful. Exact layout is implementation-
     * defined.
     *
     * @param out     Output stream to write to. Must be valid.
     * @param appname Application name used in the printed header and
     *                prototypes. May be empty; the implementation then
     *                omits the name prefix.
     */
    void print(std::ostream& out, std::string_view appname) const;

private:
    std::string m_command_name; ///< Command name as typed on the command line.
    function_t m_func; ///< Callable executed by call().
    ShowLevelType m_show_level_type; ///< Visibility in printed help.
    std::string m_description; ///< Optional command description.
    std::vector<OptionCommandInfo> m_option_list; ///< Declared options.
    std::vector<Examples> m_examples; ///< Usage examples.
    std::vector<std::string> m_prototypes; ///< Usage prototype lines.
};

}

#endif