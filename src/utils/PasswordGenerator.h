// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#ifndef PASSWORD_GENERATOR_H
#define PASSWORD_GENERATOR_H

/**
 * @file PasswordGenerator.h
 * @brief Password generation utilities
 */

#include <expected>
#include <string>
#include <string_view>

namespace KeepTower {

/**
 * @brief Errors that can occur during password generation.
 */
enum class PasswordGeneratorError : int {
    INVALID_LENGTH,
    RNG_FAILURE,
};

/**
 * @brief Convert PasswordGeneratorError to a human-readable string.
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
     */
    [[nodiscard]] static std::expected<std::string, PasswordGeneratorError>
    generate_temporary_password(size_t length);

    [[nodiscard]] static constexpr std::string_view uppercase_charset() noexcept {
        return "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    }

    [[nodiscard]] static constexpr std::string_view lowercase_charset() noexcept {
        return "abcdefghijklmnopqrstuvwxyz";
    }

    [[nodiscard]] static constexpr std::string_view digits_charset() noexcept {
        return "0123456789";
    }

    [[nodiscard]] static constexpr std::string_view symbols_charset() noexcept {
        return "!@#$%^&*-_=+";
    }

    [[nodiscard]] static constexpr std::string_view all_charset() noexcept {
        return "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*-_=+";
    }
};

} // namespace KeepTower

#endif // PASSWORD_GENERATOR_H
