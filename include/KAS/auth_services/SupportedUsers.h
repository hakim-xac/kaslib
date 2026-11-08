/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_AUTH_SERVICES_CLASS_H
#define KASLIBCXX_AUTH_SERVICES_CLASS_H

#include "users.h"

#include <KAS/expected/expected.h>

#include <algorithm>
#include <array>
#include <concepts>
#include <cstdint>
#include <string>
#include <string_view>

namespace kas::auth_services::concepts {
/**
 * @brief Concept describing a type that can be used as an authentication user.
 *
 * A conforming type must expose a nested type alias 'auth_t' and a static
 * member 'user_properties' (typically of type 'UserProperties').
 */
template <typename U>
concept as_authentification = requires {
    typename U::auth_t;
    U::user_properties;
};
} // namespace kas::auth_services::concepts

namespace kas::auth_services {
/**
 * @brief Compile-time registry of supported users.
 *
 * 'SupportedUsers' is a container that holds the properties of a fixed set of
 * user types supplied as template parameters. All users are validated at
 * compile time through the 'concepts::as_authentification' concept, and their
 * properties are stored in a 'std::array' constructed during object
 * initialization.
 *
 * The class is intended for access-control checks: given a username, it can
 * determine whether access is allowed for a particular user type and can
 * resolve the username to its numeric user ID.
 *
 * @tparam Users User types satisfying 'concepts::as_authentification'.
 *
 * Example:
 * @code
 * using Users = SupportedUsers<Admin, Operator, Guest>;
 * Users users;
 * if (users.checkAccessAllowed<Operator>("alice")) { ... }
 * @endcode
 */
template <concepts::as_authentification... Users>
class SupportedUsers final {
public:
    /**
     * @brief Constructs the registry and initializes the properties array
     *        from the 'user_properties' of each user type.
     */
    SupportedUsers()
        : m_supported_users { Users::user_properties... }
    {
    }

    /**
     * @brief Checks whether access is allowed for the given username.
     *
     * Access is granted if a user with a matching name exists and its ID is
     * greater than or equal to the ID of 'U''s properties. This effectively
     * enforces a minimum required privilege level defined by 'U'.
     *
     * @tparam U User type whose privilege level is used as the threshold.
     * @param username Name of the user to check.
     * @return 'true' if access is allowed, 'false' otherwise.
     *
     * @note This function does not throw.
     */
    template <concepts::as_authentification U>
    [[nodiscard]]
    bool
    checkAccessAllowed(const std::string& username) const noexcept
    {
        return std::ranges::any_of(
            m_supported_users,
            [&username](const auto& user_properties) {
                return user_properties.name == username
                    && user_properties.id >= U::user_properties.id;
            });
    }

    /**
     * @brief Returns the number of supported users.
     *
     * @return Count of user types registered in this container.
     *
     * @note This function does not throw.
     */
    [[nodiscard]]
    size_t
    getCountUsers() const noexcept
    {
        return sizeof...(Users);
    }

    /**
     * @brief Resolves a username to its numeric user ID.
     *
     * @param username Name of the user to look up.
     * @return The user's ID wrapped in 'std::optional', or 'std::nullopt'
     *         if no matching user is found.
     */
    [[nodiscard]]
    std::optional<std::size_t>
    getUserID(const std::string& username) const
    {
        const auto fn {
            std::ranges::find_if(
                m_supported_users, [&username](const auto& user_properties) {
                    return user_properties.name == username;
                })
        };

        return fn == std::ranges::cend(m_supported_users)
            ? std::nullopt
            : std::optional { fn->id };
    }

private:
    std::array<auth_services::UserProperties, sizeof...(Users)> m_supported_users;
};
} // namespace kas::auth_services

#endif