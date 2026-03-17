// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#ifndef PASSWORD_GENERATOR_H
#define PASSWORD_GENERATOR_H

/**
 * @file PasswordGenerator.h
 * @brief Password generation utilities.
 *
 * This header currently provides utilities for generating administrator-issued
 * temporary passwords used during user provisioning and password reset.
 */

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

namespace KeepTower {

/**
 * @brief Errors that can occur during password generation.
 */
enum class PasswordGeneratorError : int {
    /** The requested length violates generator constraints (e.g., too short). */
    INVALID_LENGTH,
    /** Cryptographic RNG failed (e.g., OpenSSL RAND_bytes returned failure). */
    RNG_FAILURE,
};

/**
 * @brief Convert PasswordGeneratorError to a human-readable string.
 *
 * Intended for logs, diagnostics, and tests. The returned strings are stable
 * identifiers (not localized UI text).
 */
[[nodiscard]] constexpr std::string_view to_string(PasswordGeneratorError error) noexcept {
    switch (error) {
        case PasswordGeneratorError::INVALID_LENGTH:
            return "INVALID_LENGTH";
        case PasswordGeneratorError::RNG_FAILURE:
            return "RNG_FAILURE";
    }
    return "UNKNOWN";
}

/**
 * @brief Temporary password generation helper.
 *
 * Uses OpenSSL's RNG (`RAND_bytes`) to generate passwords suitable for
 * temporary account bootstrap.
 *
 * @note The generated passwords are intended for short-lived bootstrap flows
 * (first login / forced change). They are not meant to replace interactive
 * user-chosen password UX.
 */
class PasswordGenerator final {
public:
    /**
     * @brief Generate a temporary password.
     *
     * The generated password will contain at least one character from each
     * required character set: uppercase, lowercase, digits, and symbols.
     *
     * @param length Desired password length (must be >= 4).
     * @return Password string, or an error.
     * @retval PasswordGeneratorError::INVALID_LENGTH if @p length is too small.
     * @retval PasswordGeneratorError::RNG_FAILURE if the RNG fails.
     */
    [[nodiscard]] static std::expected<std::string, PasswordGeneratorError>
    generate_temporary_password(size_t length);

    /** @brief Uppercase letters used by the generator. */
    [[nodiscard]] static constexpr std::string_view uppercase_charset() noexcept {
        return "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    }

    /** @brief Lowercase letters used by the generator. */
    [[nodiscard]] static constexpr std::string_view lowercase_charset() noexcept {
        return "abcdefghijklmnopqrstuvwxyz";
    }

    /** @brief Digits used by the generator. */
    [[nodiscard]] static constexpr std::string_view digits_charset() noexcept {
        return "0123456789";
    }

    /** @brief Symbols used by the generator. */
    [[nodiscard]] static constexpr std::string_view symbols_charset() noexcept {
        return "!@#$%^&*-_=+";
    }

    /** @brief Convenience union of all characters used by the generator. */
    [[nodiscard]] static constexpr std::string_view all_charset() noexcept {
        return "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*-_=+";
    }
};

} // namespace KeepTower

#endif // PASSWORD_GENERATOR_H
