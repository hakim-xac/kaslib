/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_EXPECTED_H
#define KASLIBCXX_EXPECTED_H

#include "v3/expected.h"

namespace kas {

template <typename T>
using expected = v3::expected<T, std::string_view>;

template <typename T>
using expected_string = v3::expected<T, std::string>;

template <typename T, typename U>
using expected_type = v3::expected<T, U>;

template <typename T>
using unexpected = v3::unexpected<T>;

// необходим для работы ADL, чтобы подхватил специализации
using v3::unexpected;
using unexpected_string = unexpected<std::string>;

} // namespace kas

#endif