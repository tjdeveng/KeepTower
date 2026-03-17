// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

/**
 * @file test_password_generator.cc
 * @brief Unit tests for KeepTower::PasswordGenerator
 */

#include <gtest/gtest.h>

#include "utils/PasswordGenerator.h"

#include <string_view>

namespace {

[[nodiscard]] bool contains_any(std::string_view charset, std::string_view text) {
    for (const char c : text) {
        if (charset.find(c) != std::string_view::npos) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool all_in_charset(std::string_view charset, std::string_view text) {
    for (const char c : text) {
        if (charset.find(c) == std::string_view::npos) {
            return false;
        }
    }
    return true;
}

} // namespace

TEST(PasswordGenerator, GenerateTemporaryPassword_InvalidLength_ReturnsError) {
    using KeepTower::PasswordGenerator;
    using KeepTower::PasswordGeneratorError;

    {
        auto pw = PasswordGenerator::generate_temporary_password(0);
        ASSERT_FALSE(pw);
        EXPECT_EQ(pw.error(), PasswordGeneratorError::INVALID_LENGTH);
    }

    {
        auto pw = PasswordGenerator::generate_temporary_password(3);
        ASSERT_FALSE(pw);
        EXPECT_EQ(pw.error(), PasswordGeneratorError::INVALID_LENGTH);
    }
}

TEST(PasswordGenerator, GenerateTemporaryPassword_LengthAndCharset_AreValid) {
    using KeepTower::PasswordGenerator;

    constexpr size_t length = 16;

    auto pw = PasswordGenerator::generate_temporary_password(length);
    ASSERT_TRUE(pw);

    EXPECT_EQ(pw->size(), length);
    EXPECT_TRUE(all_in_charset(PasswordGenerator::all_charset(), *pw));
}

TEST(PasswordGenerator, GenerateTemporaryPassword_ContainsAllRequiredCharacterClasses) {
    using KeepTower::PasswordGenerator;

    constexpr size_t length = 16;

    auto pw = PasswordGenerator::generate_temporary_password(length);
    ASSERT_TRUE(pw);

    EXPECT_TRUE(contains_any(PasswordGenerator::uppercase_charset(), *pw));
    EXPECT_TRUE(contains_any(PasswordGenerator::lowercase_charset(), *pw));
    EXPECT_TRUE(contains_any(PasswordGenerator::digits_charset(), *pw));
    EXPECT_TRUE(contains_any(PasswordGenerator::symbols_charset(), *pw));
}
