/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_CONVERT_SERIALIZE_DESERIALIZEBLE_H
#define KASLIBCXX_CONVERT_SERIALIZE_DESERIALIZEBLE_H

#include "serialize_types.h"

#include <KAS/convert/serialize/RawPacket/RawPacket.h>
#include <KAS/convert/serialize/concepts.h>
#include <KAS/convert/utils.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace kas::convert::serialize {

/**
 * @brief Класс, получает сохранённые классом Serializer данные из буфера.
 *
 * Формат буфера (см. Serializer):
 *   [code: general_size_type_t (BE)][value]
 * где value:
 *   - для скаляров: sizeof(value) байт BE;
 *   - для string / raw_data: [length: uint64_t (BE)][bytes].
 */
class Deserializer final {
public:
    Deserializer() = default;

    /**
     * @brief Возвращает Deserializer, если данные были сохранены Serializer
     *        и успешно распаршены.
     */
    [[nodiscard]] inline static std::optional<Deserializer>
    getDeserializer(const std::vector<std::uint8_t>& data)
    {
        Deserializer ds { };
        return parse(ds, data) ? std::optional { std::move(ds) } : std::nullopt;
    }

    [[nodiscard]] inline std::size_t getCountElems() const noexcept
    {
        return m_data.size();
    }

    /**
     * @brief Возвращает значение элемента с индексом 'index', если там
     *        действительно хранится тип 'Ret'.
     */
    template <typename Ret>
    [[nodiscard]] std::optional<Ret> getElements(std::size_t index) const;

private:
    template <typename SizeType = types::general_size_type_t>
    static bool parse(Deserializer& ds, const std::vector<std::uint8_t>& data);

    template <typename T>
    [[nodiscard]] static bool
    appendTrivialValueFromRawData(Deserializer& ds,
        std::span<const std::uint8_t>& sp)
    {
        using use_type = T;
        const std::size_t type_size = sizeof(use_type);
        if (sp.size() < type_size) [[unlikely]]
            return false;

        ds.m_data.emplace_back(
            std::in_place_type<use_type>,
            getValueFromRawData<use_type>(sp.subspan(0, type_size)));

        sp = sp.subspan(type_size);
        return true;
    }

    template <typename Container>
        requires requires(Container c) {
            c.data();
            c.resize(std::size_t { });
        }
    [[nodiscard]] static bool
    appendContainerValueFromRawData(Deserializer& ds,
        std::span<const std::uint8_t>& sp)
    {
        using length_type = std::uint64_t;

        if (sp.size() < sizeof(length_type)) [[unlikely]]
            return false;

        const auto length_bytes = getValueFromRawData<length_type>(sp.subspan(0, sizeof(length_type)));

        // Сдвигаем span на 8 байт длины ДО проверки - иначе проверяем
        // sp.size(), который ещё включает эти 8 байт, и пропускаем
        // чтение за границей.
        sp = sp.subspan(sizeof(length_type));

        // Сравниваем в uint64_t, без static_cast<size_t> -
        // иначе на 32-бит длина может обрезаться и проверка пройдёт ложно.
        if (length_bytes > sp.size()) [[unlikely]]
            return false;

        const auto length = static_cast<std::size_t>(length_bytes);

        Container buffer { };
        buffer.resize(length);

        std::copy_n(std::cbegin(sp), length,
            reinterpret_cast<std::uint8_t*>(buffer.data()));

        ds.m_data.emplace_back(std::in_place_type<Container>, std::move(buffer));

        sp = sp.subspan(length);
        return true;
    }

    template <std::integral Ret>
    [[nodiscard]] static Ret
    getValueFromRawData(std::span<const std::uint8_t> sp) noexcept;

    [[nodiscard]] inline bool checkIndex(std::size_t index) const noexcept
    {
        return index < m_data.size();
    }

private:
    using var_t = std::variant<
        types::u8::value_type,
        types::i8::value_type,
        types::u16::value_type,
        types::i16::value_type,
        types::u32::value_type,
        types::i32::value_type,
        types::u64::value_type,
        types::i64::value_type,
        types::string::value_type,
        types::raw_data::value_type>;

    std::vector<var_t> m_data;
};

// ============================================================================
//  Реализация
// ============================================================================

template <typename SizeType>
bool Deserializer::parse(Deserializer& ds,
    const std::vector<std::uint8_t>& data)
{
    if (data.empty()) [[unlikely]]
        return false;

    ds.m_data.clear();

    using handler_func = bool (*)(Deserializer&, std::span<const std::uint8_t>&);

    static const std::unordered_map<SizeType, handler_func> supported_types {
        { types::u8::code, appendTrivialValueFromRawData<types::u8::value_type> },
        { types::i8::code, appendTrivialValueFromRawData<types::i8::value_type> },
        { types::u16::code, appendTrivialValueFromRawData<types::u16::value_type> },
        { types::i16::code, appendTrivialValueFromRawData<types::i16::value_type> },
        { types::u32::code, appendTrivialValueFromRawData<types::u32::value_type> },
        { types::i32::code, appendTrivialValueFromRawData<types::i32::value_type> },
        { types::u64::code, appendTrivialValueFromRawData<types::u64::value_type> },
        { types::i64::code, appendTrivialValueFromRawData<types::i64::value_type> },
        { types::string::code, appendContainerValueFromRawData<types::string::value_type> },
        { types::raw_data::code, appendContainerValueFromRawData<types::raw_data::value_type> },
    };

    std::span<const std::uint8_t> sp { data };

    while (!sp.empty()) {
        if (sp.size() < sizeof(SizeType)) [[unlikely]]
            return false;

        const SizeType size_type = getValueFromRawData<SizeType>(sp.subspan(0, sizeof(SizeType)));
        sp = sp.subspan(sizeof(SizeType));

        const auto fn = supported_types.find(size_type);
        if (fn == std::cend(supported_types)) [[unlikely]]
            return false;

        if (!std::invoke(fn->second, ds, sp)) [[unlikely]]
            return false;
    }

    return true;
}

// ----------------------------------------------------------------------------

template <std::integral Ret>
Ret Deserializer::getValueFromRawData(std::span<const std::uint8_t> sp) noexcept
{
    Ret res { };
    std::copy_n(std::cbegin(sp), sizeof(Ret),
        reinterpret_cast<std::uint8_t*>(&res));

    if constexpr (std::is_signed_v<Ret>) {
        return static_cast<Ret>(kas::convert::utils::swap_binary(
            static_cast<std::make_unsigned_t<Ret>>(res)));
    } else {
        return kas::convert::utils::swap_binary(res);
    }
}

// ----------------------------------------------------------------------------

template <typename Ret>
[[nodiscard]] std::optional<Ret>
Deserializer::getElements(std::size_t index) const
{
    static_assert(std::is_constructible_v<var_t, std::in_place_type_t<Ret>, Ret>,
        "Ret must be one of the Deserializer variant types "
        "(u8/i8/u16/i16/u32/i32/u64/i64/string/raw_data)");

    if (!checkIndex(index)) [[unlikely]]
        return std::nullopt;

    const auto& elem = m_data[index];

    if (!std::holds_alternative<Ret>(elem)) [[unlikely]]
        return std::nullopt;

    return std::get<Ret>(elem);
}

} // namespace kas::convert::serialize

#endif