/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_NET_HTTP_ROUTE_H
#define KASLIBCXX_NET_HTTP_ROUTE_H

#include "Methods.h"
#include "Request.h"
#include "Response.h"

#include <KAS/expected/expected.h>
#include <KAS/functional/MoveOnlyFunction.h>

#include <optional>
#include <string_view>
#include <vector>

namespace kas::net::http {

/**
 * @brief A single HTTP route: an optional handler plus the set of allowed methods.
 *
 * An HttpRoute pairs an endpoint handler (a move-only callable taking the
 * Request and Response) with the list of HTTP methods it accepts. The route
 * can be configured either with a variadic method list via
 * @ref setAllowedMethods or with a single method tag via @ref setMethod, and
 * its handler can be installed with @ref setHandler.
 *
 * Configuration methods are provided in both lvalue (&) and rvalue (&&)
 * overloads to support fluent chaining on temporaries, for example:
 * @code
 * HttpRoute{}
 *     .setMethod<Methods::GET>()
 *     .setHandler([](const Request&, Response&) { ... });
 * @endcode
 *
 * @note The route does not own the Request or Response; both are passed by
 *       reference into @ref call and must outlive the call.
 */
class HttpRoute final {
public:
    /// Error returned by @ref call when no handler is installed.
    inline static constexpr std::string_view ERROR_NO_HANDLER { "no handler" };
    /// Error returned by @ref call when the request method is not allowed.
    inline static constexpr std::string_view ERROR_METHOD_NOT_ALLOWED { "method not allowed" };

    /// Endpoint handler type: move-only callable returning an expected result.
    using end_point_handler_t = functional::MoveOnlyFunction<expected_string<void>(const Request&, Response&)>;

    /// Constructs an empty route with no handler and no allowed methods.
    HttpRoute() = default;

    /**
     * @brief Constructs a route with the given handler.
     *
     * @param end_point_handler Handler to install.
     */
    explicit HttpRoute(end_point_handler_t end_point_handler);

    /**
     * @brief Sets the allowed methods from a variadic list of method tags.
     *
     * Replaces any previously configured allowed methods.
     *
     * @tparam TMethods Method tags (see @c Methods).
     * @return Reference to this route (lvalue overload).
     */
    template <Methods... TMethods>
    HttpRoute& setAllowedMethods() &
    {
        m_allowed_methods = { Method<TMethods>::method... };
        return *this;
    }

    /**
     * @brief Sets the allowed methods from a variadic list of method tags.
     *
     * @tparam TMethods Method tags (see @c Methods).
     * @return Moved reference to this route (rvalue overload).
     */
    template <Methods... TMethods>
    HttpRoute&& setAllowedMethods() &&
    {
        m_allowed_methods = { Method<TMethods>::method... };
        return std::move(*this);
    }

    /**
     * @brief Sets the allowed methods from a single method tag.
     *
     * The tag may map either to a single HTTP method or to a group of
     * methods (for example, "all methods"). The appropriate mapping is
     * selected at compile time via @c Method<TMethod>.
     *
     * @tparam TMethod Method tag (see @c Methods).
     * @return Reference to this route (lvalue overload).
     */
    template <Methods TMethod>
    HttpRoute& setMethod() &
    {
        if constexpr (requires { typename Method<TMethod>::method_t; })
            m_allowed_methods = { Method<TMethod>::method };
        else if constexpr (requires { typename Method<TMethod>::all_method_t; })
            m_allowed_methods = Method<TMethod>::allowed_methods;
        else
            static_assert(
                requires { typename Method<TMethod>::method_t; }
                    || requires { typename Method<TMethod>::all_method_t; },
                "No Method<> specialization for this Methods value");
        return *this;
    }

    /**
     * @brief Sets the allowed methods from a single method tag.
     *
     * @tparam TMethod Method tag (see @c Methods).
     * @return Moved reference to this route (rvalue overload).
     */
    template <Methods TMethod>
    HttpRoute&& setMethod() &&
    {
        setMethod<TMethod>();
        return std::move(*this);
    }

    /**
     * @brief Installs the endpoint handler.
     *
     * @param end_point_handler Handler to install.
     * @return Reference to this route (lvalue overload).
     */
    HttpRoute& setHandler(end_point_handler_t end_point_handler) &;

    /**
     * @brief Installs the endpoint handler.
     *
     * @param end_point_handler Handler to install.
     * @return Moved reference to this route (rvalue overload).
     */
    HttpRoute&& setHandler(end_point_handler_t end_point_handler) &&;

    /**
     * @brief Checks whether the given HTTP method is allowed by this route.
     *
     * @param method HTTP method name (for example, "GET").
     * @return true if the method is in the allowed set, false otherwise.
     */
    [[nodiscard]] bool allows(std::string_view method) const noexcept;

    /**
     * @brief Returns the list of allowed HTTP methods.
     *
     * @return Reference to the internal vector of allowed method names.
     */
    [[nodiscard]] const std::vector<std::string_view>& getAllowedMethods() const noexcept;

    /**
     * @brief Checks whether a handler has been installed.
     *
     * @return true if a handler is present, false otherwise.
     */
    [[nodiscard]] bool hasHandler() const noexcept;

    /**
     * @brief Invokes the route's handler.
     *
     * If no handler is installed, returns @ref ERROR_NO_HANDLER. If the
     * request method is not allowed, returns @ref ERROR_METHOD_NOT_ALLOWED.
     * Otherwise, forwards the call to the installed handler.
     *
     * @param request  Incoming request.
     * @param response Response to populate.
     * @return An expected result produced by the handler, or one of the
     *         route errors described above.
     */
    [[nodiscard]] expected_string<void> call(const Request& request, Response& response) const;

private:
    std::optional<end_point_handler_t> m_handler_opt; ///< Installed handler, if any.
    std::vector<std::string_view> m_allowed_methods; ///< Allowed HTTP method names.
};

} // namespace kas::net::http

#endif // KASLIBCXX_NET_HTTP_ROUTE