/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_CONVERT_JWT_V2_H
#define KASLIBCXX_CONVERT_JWT_V2_H

#include <KAS/expected/expected.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>

/**
 * @brief JSON Web Token (JWT) creation and verification, version 2.
 *
 * Tokens are signed with a shared secret. The payload carries a subject,
 * a display name, an issued-at timestamp, and an optional expiration time.
 */
namespace kas::convert::jwt::v2 {

// ==========================================================================
// JwtPayload - DTO, no behavior
// ==========================================================================

/**
 * @brief Decoded JWT payload.
 *
 * A plain data holder with no behavior. Field names in comments map to the
 * corresponding JWT claim names.
 */
struct JwtPayload {
    std::string subject; // "sub"
    std::string name; // "name"
    std::int64_t issued_at = 0; // "iat", unix seconds
    std::int64_t expires_at = 0; // "exp", unix seconds; 0 = no expiration

    friend bool operator==(const JwtPayload&, const JwtPayload&) = default;
};

// ==========================================================================
// Payload serialization (free functions)
// ==========================================================================

/**
 * @brief Serializes a JwtPayload into a compact JSON object.
 *
 * Produces a JSON object with the "sub" and "name" string fields, and
 * optionally "iat" and "exp" numeric fields when the corresponding
 * timestamps are non-zero. String fields are escaped with escapeJson,
 * numeric fields are formatted with intToString (which uses std::to_chars
 * and does not allocate beyond the final string).
 *
 * On a numeric conversion error the field is omitted; this cannot happen
 * for int64_t with intToString's buffer, but the error path is kept for
 * robustness if the payload type changes.
 *
 * @param payload Payload to serialize.
 * @return JSON object as a string.
 */
[[nodiscard]] std::string toJson(const JwtPayload& payload);

/**
 * @brief Parses a payload from a JSON string.
 *
 * @param json JSON text to parse.
 * @return On success, the parsed payload; on failure, an error value.
 */
[[nodiscard]] kas::expected<JwtPayload> fromJson(std::string_view json);

// ==========================================================================
// Expiration check (free function, no parsing)
// ==========================================================================

/**
 * @brief Checks whether a payload has expired.
 *
 * Compares the payload's expiration time against the current time.
 * A payload with expires_at == 0 is treated as having no expiration.
 *
 * @param payload Payload to check.
 * @return true if the payload has expired, false otherwise.
 *
 * @note This function does not throw.
 */
[[nodiscard]] bool isExpired(const JwtPayload& payload) noexcept;

// ==========================================================================
// Jwt - class encapsulating the secret
// ==========================================================================

/**
 * @brief Creates and verifies signed JWTs using a shared secret.
 */
class Jwt final {
public:
    /**
     * @brief Constructs a JWT helper with the given signing secret.
     *
     * @param secret Secret used to sign and verify tokens.
     */
    explicit Jwt(std::string secret);

    // --- Creation ---
    /**
     * @brief Creates a signed token from a payload.
     *
     * @param payload Payload to embed in the token.
     * @return Signed token string.
     */
    [[nodiscard]] std::string createToken(const JwtPayload& payload) const;

    /**
     * @brief Creates a signed token from individual claims.
     *
     * Sets issued_at to the current time and expires_at to now + ttl.
     *
     * @param subject Subject claim ("sub").
     * @param name Display name claim ("name").
     * @param ttl Token time-to-live; defaults to one hour.
     * @return Signed token string.
     */
    [[nodiscard]] std::string createToken(
        std::string_view subject,
        std::string_view name,
        std::chrono::seconds ttl = std::chrono::hours { 1 }) const;

    // --- Verification ---
    // verify:            signature + payload structure (no exp check)
    // verifyAndValidate: signature + payload structure + exp check
    /**
     * @brief Verifies the token signature and payload structure.
     *
     * Does not check the expiration claim.
     *
     * @param token Token to verify.
     * @return On success, the decoded payload; on failure, an error value.
     */
    [[nodiscard]] kas::expected<JwtPayload> verify(std::string_view token) const;

    /**
     * @brief Verifies the token signature, payload structure, and expiration.
     *
     * @param token Token to verify.
     * @return On success, the decoded payload; on failure, an error value
     *         (including when the token has expired).
     */
    [[nodiscard]] kas::expected<JwtPayload> verifyAndValidate(std::string_view token) const;

private:
    std::string m_secret;
};

} // namespace kas::convert::jwt::v2

#endif