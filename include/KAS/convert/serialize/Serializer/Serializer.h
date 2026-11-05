/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_CONVERT_SERIALIZE_SERIALIZER_H
#define KASLIBCXX_CONVERT_SERIALIZE_SERIALIZER_H

#include <KAS/convert/serialize/RawPacket/RawPacket.h>
#include <KAS/convert/serialize/concepts.h>

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace kas::convert::serialize {
/**
 * @brief Класс предназначен для формирования сырого буфера. Предоставляя понятный и удобный интерфейс. \
 * @brief Сохраняет информацию как о хранимом типе, так и само значение.
 *
 * @tparam Args
 */
template <concepts::as_serialize_type... Args>
struct Serializer final {
    /**
     * @brief Конструктор \
     * @brief Позволяет принимать переменное число параметров.
     *
     * @tparam SerializeType
     * @param prop
     */
    template <concepts::as_serialize_type... SerializeType>
    explicit Serializer(SerializeType&&... prop)
        : m_raw_packet { }
        , m_count_elems { }
    {
        (addSerializeType(std::forward<SerializeType>(prop)), ...);
    }

    /**
     * @brief Добавляет значение в буфер с типом 'typename InfoType::value_type'
     *
     * @tparam InfoType
     * @tparam Value
     * @param value
     * @return Serializer&
     */
    template <concepts::as_type_info InfoType, typename Value>
    Serializer& addSerializeType(Value&& value) &
    {
        addInfoType<InfoType>();
        addValue(std::type_identity<typename InfoType::value_type> { }, std::forward<Value>(value));

        ++m_count_elems;
        return *this;
    }

    /**
     * @brief Добавляет значение в буфер с типом 'typename InfoType::value_type'
     *
     * @tparam InfoType
     * @tparam Value
     * @param value
     * @return Serializer&&
     */
    template <concepts::as_type_info InfoType, typename Value>
    Serializer&& addSerializeType(Value&& value) &&
    {
        addInfoType<InfoType>();
        addValue(std::type_identity<typename InfoType::value_type> { }, std::forward<Value>(value));

        ++m_count_elems;
        return std::move(*this);
    }

    /**
     * @brief Добавляет новый тип в буфер
     *
     * @tparam SerializeType
     * @param prop
     * @return Serializer&
     */
    template <concepts::as_serialize_type SerializeType>
    Serializer& addSerializeType(SerializeType&& prop) &
    {
        addInfoType<typename SerializeType::info_type>();
        addValue(std::forward<SerializeType>(prop).value);

        ++m_count_elems;
        return *this;
    }

    /**
     * @brief Добавляет новый тип в буфер
     *
     * @tparam SerializeType
     * @param prop
     * @return Serializer&&
     */
    template <concepts::as_serialize_type SerializeType>
    Serializer&& addSerializeType(SerializeType&& prop) &&
    {
        addInfoType<typename SerializeType::info_type>();
        addValue(std::forward<SerializeType>(prop).value);

        ++m_count_elems;
        return std::move(*this);
    }

    /**
     * @brief Возвращает количество элементов записанных в буфер
     *
     * @return std::size_t
     */
    [[nodiscard]] std::size_t getCountElems() const
    {
        return m_count_elems;
    }

    /**
     * @brief Возвращает ссылку на буфер
     *
     * @return const std::vector< std::uint8_t >&
     */
    [[nodiscard]] const std::vector<std::uint8_t>& getRaw() const&
    {
        return m_raw_packet.toRawBytes();
    }

    [[nodiscard]] std::vector<std::uint8_t> getRaw() &&
    {
        return std::move(m_raw_packet).toRawBytes();
    }

private:
    template <concepts::as_type_info Info>
    void addInfoType()
    {
        addValue(std::type_identity<typename Info::code_type> { }, Info::code);
    }

    template <typename Value>
    void addValue(Value&& value)
    {
        using decay_t = std::decay_t<Value>;
        addValue(std::type_identity<decay_t> { }, std::forward<Value>(value));
    }

    template <typename StructType, std::convertible_to<std::string_view> V>
    void addValue(std::type_identity<StructType>, V value)
    {
        const std::string_view sv { value };
        addValue(std::uint64_t { sv.size() });
        m_raw_packet.putArr(std::span { reinterpret_cast<const std::uint8_t*>(sv.data()), sv.size() });
    }

    void addValue(std::type_identity<std::uint8_t>, std::uint8_t value)
    {
        m_raw_packet.put8(value);
    }

    void addValue(std::type_identity<std::uint16_t>, std::uint16_t value)
    {
        m_raw_packet.put16_be(value);
    }

    void addValue(std::type_identity<std::uint32_t>, std::uint32_t value)
    {
        m_raw_packet.put32_be(value);
    }

    void addValue(std::type_identity<std::uint64_t>, std::uint64_t value)
    {
        m_raw_packet.put64_be(value);
    }

    void addValue(std::type_identity<std::int8_t>, std::int8_t value)
    {
        m_raw_packet.put8(static_cast<std::uint8_t>(value));
    }

    void addValue(std::type_identity<std::int16_t>, std::int16_t value)
    {
        m_raw_packet.put16_be(static_cast<std::uint16_t>(value));
    }

    void addValue(std::type_identity<std::int32_t>, std::int32_t value)
    {
        m_raw_packet.put32_be(static_cast<std::uint32_t>(value));
    }

    void addValue(std::type_identity<std::int64_t>, std::int64_t value)
    {
        m_raw_packet.put64_be(static_cast<std::uint64_t>(value));
    }

    void addValue(std::type_identity<std::vector<std::uint8_t>>, std::vector<std::uint8_t> value)
    {
        addValue(value.size());
        m_raw_packet.putArr(value);
    }

private:
    RawPacket m_raw_packet;
    std::size_t m_count_elems;
};
} // namespace kas::convert

#endif