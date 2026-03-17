// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#include "PasswordGenerator.h"

#include <openssl/rand.h>

#include <array>

namespace KeepTower {

namespace {

[[nodiscard]] std::expected<size_t, PasswordGeneratorError>
random_index(std::string_view charset) {
    if (charset.empty()) {
        return std::unexpected(PasswordGeneratorError::INVALID_LENGTH);
    }

    std::array<unsigned char, 1> random_byte{};
    if (RAND_bytes(random_byte.data(), 1) != 1) {
        return std::unexpected(PasswordGeneratorError::RNG_FAILURE);
    }

    return static_cast<size_t>(random_byte[0]) % charset.size();
}

} // namespace

std::expected<std::string, PasswordGeneratorError>
PasswordGenerator::generate_temporary_password(size_t length) {
    if (length < 4) {
        return std::unexpected(PasswordGeneratorError::INVALID_LENGTH);
    }

    std::string password;
    password.reserve(length);

    const std::array<std::string_view, 4> required_sets = {
        uppercase_charset(),
        lowercase_charset(),
        digits_charset(),
        symbols_charset(),
    };

    for (const auto charset : required_sets) {
        auto idx = random_index(charset);
        if (!idx) {
            return std::unexpected(idx.error());
        }
        password += charset[*idx];
    }

    const std::string_view all_chars = all_charset();
    for (size_t i = password.size(); i < length; ++i) {
        auto idx = random_index(all_chars);
        if (!idx) {
            return std::unexpected(idx.error());
        }
        password += all_chars[*idx];
    }

    for (size_t i = password.size() - 1; i > 0; --i) {
        std::array<unsigned char, 1> random_byte{};
        if (RAND_bytes(random_byte.data(), 1) != 1) {
            return std::unexpected(PasswordGeneratorError::RNG_FAILURE);
        }

        const size_t j = static_cast<size_t>(random_byte[0]) % (i + 1);
        std::swap(password[i], password[j]);
    }

    return password;
}

} // namespace KeepTower
