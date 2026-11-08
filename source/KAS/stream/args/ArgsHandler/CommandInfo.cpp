// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include "KAS/stream/args/ArgsHandler/v1/CommandInfo.h"
#include "KAS/stream/cots/cots.h"

namespace kas::stream::args::v1 {

//------------------

CommandInfo::CommandInfo(
    std::string command_name,
    CommandInfo::function_t func,
    ShowLevelType show_level_type)
    : m_command_name { std::move(command_name) }
    , m_func { std::move(func) }
    , m_show_level_type { show_level_type }
    , m_description { }
{
}

//------------------

[[nodiscard]]
CommandInfo::ShowLevelType
CommandInfo::getShowLevelType() const noexcept
{
    return m_show_level_type;
}

//------------------

bool CommandInfo::call() const
{
    return std::invoke(m_func);
}

//------------------

[[nodiscard]]
const std::string&
CommandInfo::getCommandName() const& noexcept
{
    return m_command_name;
}

//------------------

CommandInfo& CommandInfo::setDescription(std::string description_text) &
{
    std::swap(m_description, description_text);
    return *this;
}

//------------------

CommandInfo& CommandInfo::appendExample(std::string description, std::vector<std::string> options) &
{
    m_examples.emplace_back(std::move(description), std::move(options));
    return *this;
}

//------------------

CommandInfo& CommandInfo::appendPrototype(std::string prototype) &
{
    m_prototypes.emplace_back(std::move(prototype));
    return *this;
}

//------------------

CommandInfo& CommandInfo::appendOption(std::string name, std::optional<std::string> description_opt) &
{
    m_option_list.emplace_back(std::move(name), std::move(description_opt));
    return *this;
}

//------------------

void CommandInfo::print(std::ostream& out, std::string_view appname) const
{
    using kas::stream::cots::v3::ColorOutToStream;
    using kas::stream::cots::v3::ColorProperties;
    using kas::stream::cots::v3::ColorTypes;

    ColorOutToStream cots { out };

    cots
        .setColor<ColorTypes::Magenta>()
        .print(m_command_name)
        .print(" - ")
        .setColor<ColorTypes::Green>()
        .print(!m_description.empty() ? m_description : "-------")
        .colorReset()
        .ln();

    if (!m_prototypes.empty()) {
        cots.tab().setColor<ColorTypes::Cyan>().print("Prototype:").ln();

        for (const auto& elem : m_prototypes) {
            cots.tab(2).setColor<ColorTypes::Magenta>().print(appname, m_command_name);
            cots.setColor<ColorTypes::Yellow>().print(elem).ln();
        }
        cots.colorReset();
    }

    if (!m_option_list.empty()) {
        cots.tab().setColor<ColorTypes::Cyan>().print("Options:").ln();

        for (const auto& option : m_option_list) {
            cots.tab(2).setColor<ColorTypes::Yellow>().print(option.name);
            if (option.description_opt)
                cots.print(" - ").setColor<ColorTypes::Green>().print(option.description_opt.value());
            cots.ln();
        }
        cots.colorReset();
    }

    if (!m_examples.empty()) {
        cots.tab().setColor<ColorTypes::Cyan>().print("Example:").ln();

        for (const auto& elem : m_examples) {
            cots.tab(2).setColor<ColorTypes::Magenta>().print(appname, m_command_name);

            if (!elem.options.empty()) {
                cots.space().setColor<ColorTypes::Yellow>();

                for (bool first { true }; const auto& option : elem.options) {
                    if (!first)
                        cots.print(" ");
                    first = false;
                    cots.print(option);
                }
            }
            cots.ln();

            if (!elem.description.empty())
                cots.tab(3).setColor<ColorTypes::Green>().print(elem.description).ln();
        }
        cots.reset().ln();
    }
}

//------------------
}