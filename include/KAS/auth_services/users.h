/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_AUTH_SERVICES_USERS_H
#define KASLIBCXX_AUTH_SERVICES_USERS_H

#include <cstdint>
#include <string_view>
#include <tuple>

namespace kas::auth_services {
/**
 * @brief Describes a single user entry used by the authentication subsystem.
 *
 * Holds the numeric identifier and the human-readable name of a user. The ID
 * is treated as a privilege level: larger values correspond to higher
 * privileges (see SupportedUsers::checkAccessAllowed).
 */
struct UserProperties {
    std::size_t id; ///< Numeric user identifier / privilege level.
    std::string_view name; ///< Human-readable user name.
};
} // namespace kas::auth_services

namespace kas::auth_services::users {
/**
 * @brief Built-in user types with predefined privilege levels.
 *
 * Each type models a role and satisfies concepts::as_authentification
 * (it defines auth_t and a static user_properties member). The IDs are
 * powers of two, so they can also be used as bit flags:
 *
 *   Type    | ID | Name
 *   --------|----|--------
 *   User    | 1  | "user"
 *   Admin   | 2  | "admin"
 *   Seven   | 4  | "seven"
 *   Develop | 8  | "dev"
 *   Root    | 16 | "root"
 */

/**
 * @brief Basic user role (lowest privilege level).
 */
struct User final {
    using auth_t = void;
    static constexpr UserProperties user_properties {
        .id { 1 },
        .name { "user" }
    };
};

/**
 * @brief Administrator role.
 */
struct Admin final {
    using auth_t = void;
    static constexpr UserProperties user_properties {
        .id { 2 },
        .name { "admin" }
    };
};

/**
 * @brief Role with ID 4, name "seven".
 */
struct Seven final {
    using auth_t = void;
    static constexpr UserProperties user_properties {
        .id { 4 },
        .name { "seven" }
    };
};

/**
 * @brief Developer role.
 */
struct Develop final {
    using auth_t = void;
    static constexpr UserProperties user_properties {
        .id { 8 },
        .name { "dev" }
    };
};

/**
 * @brief Root role (highest privilege level).
 */
struct Root final {
    using auth_t = void;
    static constexpr UserProperties user_properties {
        .id { 16 },
        .name { "root" }
    };
};

} // namespace kas::auth_services::users

#endif