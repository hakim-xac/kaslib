// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include "KAS/stream/args/ArgsHandler/v1/ArgsHandler.h"
#include "KAS/stream/cots/cots.h"

#include <algorithm>
#include <ranges>

namespace kas::stream::args::v1 {

namespace {

    //------------------

    void printError(std::ostream& out, std::string_view msg)
    {
        using kas::stream::cots::v3::ColorOutToStream;
        using kas::stream::cots::v3::ColorTypes;

        std::stringstream ss;
        ColorOutToStream { ss }
            .setColor<ColorTypes::Red>()
            .print("Error: ")
            .setColor<ColorTypes::Yellow>()
            .print(msg)
            .reset()
            .ln();

        out << ss.rdbuf();
    }

    //------------------
}

//------------------

ArgsHandler::ArgsHandler(int argc, char** argv, std::ostream& out)
    : m_input_args { argc, argv }
    , m_out_ptr { std::addressof(out) }
    , m_commands { }
{
}

//------------------

ArgsHandler::ArgsHandler(InputArgs input_args, std::ostream& out)
    : m_input_args { std::move(input_args) }
    , m_out_ptr { std::addressof(out) }
    , m_commands { }
{
}

//------------------

bool ArgsHandler::call() const
{
    if (m_input_args.countIndex() == 1) [[unlikely]] {
        print(CommandInfo::PrintAllCommands::NO);
        return false;
    }

    if (m_commands.empty()) [[unlikely]] {
        print(CommandInfo::PrintAllCommands::NO);
        return false;
    }

    const std::optional<std::string_view> input_command_name_opt { m_input_args.getIndex(1) };
    if (!input_command_name_opt) [[unlikely]] {
        print(CommandInfo::PrintAllCommands::NO);
        return false;
    }

    const std::string_view command_name { input_command_name_opt.value() };

    const auto command_fn {
        std::find_if(
            std::cbegin(m_commands), std::cend(m_commands),
            [command_name](const CommandInfo& cinfo) {
                return command_name == cinfo.getCommandName();
            })
    };

    using kas::stream::cots::v3::ColorOutToStream;
    using kas::stream::cots::v3::ColorTypes;

    if (command_fn == std::cend(m_commands)) [[unlikely]] {
        printError(*m_out_ptr, "The command was not found!");
        print(CommandInfo::PrintAllCommands::NO);
        return false;
    }

    try {
        return command_fn->call();
    } catch (const std::exception& ex) {
        printError(*m_out_ptr, ex.what());
        return false;
    }
}

//------------------

void ArgsHandler::print(CommandInfo::PrintAllCommands pac) const
{
    using kas::stream::cots::v3::ColorOutToStream;
    using kas::stream::cots::v3::ColorTypes;

    std::stringstream ss { };

    const std::string_view appname { m_input_args.getIndex(0).value_or("appname") };

    ColorOutToStream out { ss };
    out.setColor<ColorTypes::Cyan>().print(appname, "< ");
    out.setColor<ColorTypes::Magenta>().print("command");
    out.setColor<ColorTypes::Cyan>().print(" > ");
    out
        .setColor<ColorTypes::Cyan>()
        .print(" [ ")
        .setColor<ColorTypes::Yellow>()
        .print("options")
        .setColor<ColorTypes::Cyan>()
        .print(" ]")
        .ln();

    if (m_commands.empty()) {
        out
            .setColor<ColorTypes::Yellow>()
            .print("No commands available")
            .colorReset()
            .ln();

        *m_out_ptr << ss.rdbuf();
        return;
    }

    out.setColor<ColorTypes::Cyan>().print("Commands:").reset().ln();

    size_t command_id { };

    std::ranges::for_each(
        m_commands, [&](const CommandInfo& command) {
            if (!(command.getShowLevelType() == CommandInfo::ShowLevelType::User || pac == CommandInfo::PrintAllCommands::YES))
                return;

            out.tab().print(++command_id, ") ");
            command.print(ss, appname);
        });

    *m_out_ptr << ss.rdbuf();
}

//------------------

CommandInfo& ArgsHandler::insert(
    std::string command_name,
    CommandInfo::function_t func,
    CommandInfo::ShowLevelType show_level_type) &
{
    return m_commands.emplace_back(std::move(command_name), std::move(func), show_level_type);
}

//------------------
}