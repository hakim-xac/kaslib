/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_STREAM_LOGGER_V1_TAGS_H
#define KASLIBCXX_STREAM_LOGGER_V1_TAGS_H

#include <KAS/stream/cots/cots.h>

namespace kas::stream::logger::v1::tags {
struct Default;

struct Trace {
    constexpr static auto color_time { kas::stream::cots::ColorTypes::Cyan };
    constexpr static auto color_header { kas::stream::cots::ColorTypes::Blue };
    constexpr static auto color_text { kas::stream::cots::ColorTypes::Green };

    constexpr static auto propery_time { kas::stream::cots::ColorProperties::Faint };
    constexpr static auto propery_header { kas::stream::cots::ColorProperties::Faint };
    constexpr static auto propery_text { kas::stream::cots::ColorProperties::Default };

    constexpr static size_t code { 1 };
    constexpr static std::string_view header { "Trace" };
};

struct Debug {
    constexpr static auto color_time { kas::stream::cots::ColorTypes::Cyan };
    constexpr static auto color_header { kas::stream::cots::ColorTypes::Yellow };
    constexpr static auto color_text { kas::stream::cots::ColorTypes::Green };

    constexpr static auto propery_time { kas::stream::cots::ColorProperties::Faint };
    constexpr static auto propery_header { kas::stream::cots::ColorProperties::Faint };
    constexpr static auto propery_text { kas::stream::cots::ColorProperties::Default };

    constexpr static size_t code { 2 };
    constexpr static std::string_view header { "Debug" };
};
struct Log {
    constexpr static auto color_time { kas::stream::cots::ColorTypes::Cyan };
    constexpr static auto color_header { kas::stream::cots::ColorTypes::Green };
    constexpr static auto color_text { kas::stream::cots::ColorTypes::Green };

    constexpr static auto propery_time { kas::stream::cots::ColorProperties::Faint };
    constexpr static auto propery_header { kas::stream::cots::ColorProperties::Default };
    constexpr static auto propery_text { kas::stream::cots::ColorProperties::Default };

    constexpr static size_t code { 4 };
    constexpr static std::string_view header { "Log" };
};
struct Info {
    constexpr static auto color_time { kas::stream::cots::ColorTypes::Cyan };
    constexpr static auto color_header { kas::stream::cots::ColorTypes::Yellow };
    constexpr static auto color_text { kas::stream::cots::ColorTypes::Green };

    constexpr static auto propery_time { kas::stream::cots::ColorProperties::Faint };
    constexpr static auto propery_header { kas::stream::cots::ColorProperties::Faint };
    constexpr static auto propery_text { kas::stream::cots::ColorProperties::Default };

    constexpr static size_t code { 8 };
    constexpr static std::string_view header { "Info" };
};
struct Warning {
    constexpr static auto color_time { kas::stream::cots::ColorTypes::Cyan };
    constexpr static auto color_header { kas::stream::cots::ColorTypes::Red };
    constexpr static auto color_text { kas::stream::cots::ColorTypes::Yellow };

    constexpr static auto propery_time { kas::stream::cots::ColorProperties::Faint };
    constexpr static auto propery_header { kas::stream::cots::ColorProperties::Default };
    constexpr static auto propery_text { kas::stream::cots::ColorProperties::Default };

    constexpr static size_t code { 16 };
    constexpr static std::string_view header { "Warning" };
};
struct Error {
    constexpr static auto color_time { kas::stream::cots::ColorTypes::Cyan };
    constexpr static auto color_header { kas::stream::cots::ColorTypes::Red };
    constexpr static auto color_text { kas::stream::cots::ColorTypes::Yellow };
    constexpr static auto color_location { kas::stream::cots::ColorTypes::Yellow };
    constexpr static auto color_function { kas::stream::cots::ColorTypes::Cyan };

    constexpr static auto propery_time { kas::stream::cots::ColorProperties::Faint };
    constexpr static auto propery_header { kas::stream::cots::ColorProperties::Bold };
    constexpr static auto propery_text { kas::stream::cots::ColorProperties::Default };
    constexpr static auto propery_location { kas::stream::cots::ColorProperties::Faint };
    constexpr static auto propery_function { kas::stream::cots::ColorProperties::Default };

    constexpr static size_t code { 32 };
    constexpr static std::string_view header { "Error" };
};
struct Fatal {
    constexpr static auto color_time { kas::stream::cots::ColorTypes::Cyan };
    constexpr static auto color_header { kas::stream::cots::ColorTypes::Red };
    constexpr static auto color_text { kas::stream::cots::ColorTypes::Yellow };
    constexpr static auto color_location { kas::stream::cots::ColorTypes::Yellow };
    constexpr static auto color_function { kas::stream::cots::ColorTypes::Cyan };

    constexpr static auto propery_time { kas::stream::cots::ColorProperties::Faint };
    constexpr static auto propery_header { kas::stream::cots::ColorProperties::Bold };
    constexpr static auto propery_text { kas::stream::cots::ColorProperties::Default };
    constexpr static auto propery_location { kas::stream::cots::ColorProperties::Faint };
    constexpr static auto propery_function { kas::stream::cots::ColorProperties::Default };

    constexpr static size_t code { 64 };
    constexpr static std::string_view header { "Fatal" };
};
struct Exception {
    constexpr static auto color_time { kas::stream::cots::ColorTypes::Cyan };
    constexpr static auto color_header { kas::stream::cots::ColorTypes::Red };
    constexpr static auto color_text { kas::stream::cots::ColorTypes::Yellow };
    constexpr static auto color_location { kas::stream::cots::ColorTypes::Yellow };
    constexpr static auto color_function { kas::stream::cots::ColorTypes::Cyan };

    constexpr static auto propery_time { kas::stream::cots::ColorProperties::Faint };
    constexpr static auto propery_header { kas::stream::cots::ColorProperties::Bold };
    constexpr static auto propery_text { kas::stream::cots::ColorProperties::Default };
    constexpr static auto propery_location { kas::stream::cots::ColorProperties::Faint };
    constexpr static auto propery_function { kas::stream::cots::ColorProperties::Default };

    constexpr static size_t code { 128 };
    constexpr static std::string_view header { "Exception" };
};

} // namespace kas::stream::logger::tags

namespace kas::concepts {
template <typename T>
concept as_simple_color_t = std::same_as<T, kas::stream::logger::v1::tags::Default>
    || requires {
           T::color_time;
           T::color_header;
           T::color_text;
           T::propery_time;
           T::propery_header;
           T::propery_text;
           T::code;
           T::header;
       };

template <typename T>
concept as_verbose_color_t = as_simple_color_t<T>
    && requires {
           T::color_location;
           T::color_function;
           T::propery_location;
           T::propery_function;
       };
} // namespace kas::concepts

#endif