// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include "KAS/stream/args/InputArgs/v1/InputArgs.h"

namespace kas::stream::args::v1 {

namespace {
    struct Key {
        std::string_view key;
        std::optional<std::string_view> value;
    };
}

//------------------

InputArgs::InputArgs(int count, char* argv[])
{
    if (count <= 0 || argv == nullptr)
        return;

    m_args.reserve(static_cast<std::size_t>(count));
    std::optional<Key> tmp_key { };
    for (int i { }; i < count; ++i) {
        std::string_view elem { argv[i] };
        m_args.emplace_back(elem);

        if (!elem.empty() && elem.front() == '-') {
            if (tmp_key)
                m_keys.insert_or_assign(tmp_key->key, tmp_key->value.value_or(std::string_view { }));

            tmp_key.emplace(Key { elem, std::nullopt });
        } else if (tmp_key && !tmp_key->value) {
            tmp_key->value.emplace(elem);
        }
    }

    if (tmp_key)
        m_keys.insert_or_assign(tmp_key->key, tmp_key->value.value_or(std::string_view { }));
}

//------------------

std::optional<std::string_view>
InputArgs::getIndex(std::size_t index) const
{
    return (index < m_args.size()) ? std::optional { m_args[index] } : std::nullopt;
}

//------------------

std::optional<std::string_view>
InputArgs::getKey(std::string_view key) const
{
    const auto it { m_keys.find(key) };
    return (it != m_keys.end()) ? std::optional { it->second } : std::nullopt;
}

//------------------

bool InputArgs::hasKey(std::string_view key) const
{
    return m_keys.contains(key);
}

//------------------

std::size_t
InputArgs::countIndex() const noexcept
{
    return m_args.size();
}

//------------------

std::size_t
InputArgs::countKeys() const noexcept
{
    return m_keys.size();
}

//------------------

void InputArgs::print(std::ostream& out) const
{
    for (std::size_t i { }, ie { m_args.size() }; i < ie; ++i)
        out << m_args[i] << (((i + 1) % 8) ? " " : "\n");
    out << "\n";
}

//------------------

}