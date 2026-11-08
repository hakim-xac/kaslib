// SPDX-FileCopyrightText: 2026 KAS
//
// SPDX-License-Identifier: MPL-2.0

#include "KAS/net/http/HttpRoute.h"

#include <utility>

namespace kas::net::http {

//------------------
HttpRoute::HttpRoute(end_point_handler_t end_point_handler)
    : m_handler_opt { std::move(end_point_handler) }
{
}

//------------------

HttpRoute& HttpRoute::setHandler(end_point_handler_t end_point_handler) &
{
    m_handler_opt = std::move(end_point_handler);
    return *this;
}

//------------------

HttpRoute&& HttpRoute::setHandler(end_point_handler_t end_point_handler) &&
{
    m_handler_opt = std::move(end_point_handler);
    return std::move(*this);
}

//------------------

bool HttpRoute::allows(std::string_view method) const noexcept
{
    for (auto m : m_allowed_methods)
        if (m == method)
            return true;
    return false;
}

//------------------

const std::vector<std::string_view>& HttpRoute::getAllowedMethods() const noexcept
{
    return m_allowed_methods;
}

//------------------

bool HttpRoute::hasHandler() const noexcept
{
    return m_handler_opt.has_value() && static_cast<bool>(*m_handler_opt);
}

//------------------

expected_string<void> HttpRoute::call(const Request& request, Response& response) const
{
    if (!hasHandler())
        return unexpected_string { std::string { ERROR_NO_HANDLER } };

    if (!m_allowed_methods.empty() && !allows(request.getMethod()))
        return unexpected_string { std::string { ERROR_METHOD_NOT_ALLOWED } };

    return (*m_handler_opt)(request, response);
}

//------------------

} // namespace kas::net::http