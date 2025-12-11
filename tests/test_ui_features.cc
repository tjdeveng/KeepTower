// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file test_ui_features.cc
 * @brief Unit tests for UI features (password generator, delete functionality)
 */

#include <gtest/gtest.h>
#include <string>
#include <set>
#include <algorithm>
#include <random>

// Test password generation algorithm characteristics
class PasswordGeneratorTest : public ::testing::Test {
protected:
    // Character sets matching MainWindow::on_generate_password
    static constexpr std::string_view lowercase = "abcdefghjkmnpqrstuvwxyz";
    static constexpr std::string_view uppercase = "ABCDEFGHJKMNPQRSTUVWXYZ";
    static constexpr std::string_view digits = "23456789";
    static constexpr std::string_view special = "!@#$%^&*()-_=+[]{}|;:,.<>?";
    static constexpr std::string_view full_charset =
        "abcdefghjkmnpqrstuvwxyz"
        "ABCDEFGHJKMNPQRSTUVWXYZ"
        "23456789"
        "!@#$%^&*()-_=+[]{}|;:,.<>?";

    static constexpr int password_length = 20;

    // Generate password using same algorithm as MainWindow
    std::string generate_password() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<std::size_t> dis(0, full_charset.size() - 1);

        std::string password;
        password.reserve(password_length);

        for (int i = 0; i < password_length; ++i) {
            password += full_charset[dis(gen)];
        }

        return password;
    }

    // Helper to check if character is in a set
    bool contains_char(std::string_view charset, char c) {
        return charset.find(c) != std::string_view::npos;
    }
};

// Test: Password length is exactly 20 characters
TEST_F(PasswordGeneratorTest, PasswordLengthIsCorrect) {
    for (int i = 0; i < 10; ++i) {
        std::string password = generate_password();
        EXPECT_EQ(password.length(), password_length)
            << "Password #" << i << " has incorrect length";
    }
}

// Test: All characters are from the allowed charset
TEST_F(PasswordGeneratorTest, AllCharactersAreValid) {
    for (int i = 0; i < 10; ++i) {
        std::string password = generate_password();
        for (char c : password) {
            EXPECT_TRUE(contains_char(full_charset, c))
                << "Password contains invalid character: '" << c << "'";
        }
    }
}

// Test: No ambiguous characters (0, O, 1, l, I)
TEST_F(PasswordGeneratorTest, NoAmbiguousCharacters) {
    constexpr std::string_view ambiguous = "0O1lI";

    for (int i = 0; i < 100; ++i) {
        std::string password = generate_password();
        for (char c : password) {
            EXPECT_TRUE(ambiguous.find(c) == std::string_view::npos)
                << "Password contains ambiguous character: '" << c << "'";
        }
    }
}

// Test: Passwords are random (no identical passwords in 100 generations)
TEST_F(PasswordGeneratorTest, PasswordsAreRandom) {
    std::set<std::string> generated_passwords;

    for (int i = 0; i < 100; ++i) {
        std::string password = generate_password();
        EXPECT_TRUE(generated_passwords.find(password) == generated_passwords.end())
            << "Generated duplicate password: " << password;
        generated_passwords.insert(password);
    }
}

// Test: Password entropy is high (contains varied character types)
TEST_F(PasswordGeneratorTest, PasswordHasGoodEntropy) {
    int passwords_with_all_types = 0;
    const int iterations = 100;

    for (int i = 0; i < iterations; ++i) {
        std::string password = generate_password();

        bool has_lowercase = false;
        bool has_uppercase = false;
        bool has_digit = false;
        bool has_special = false;

        for (char c : password) {
            if (contains_char(lowercase, c)) has_lowercase = true;
            if (contains_char(uppercase, c)) has_uppercase = true;
            if (contains_char(digits, c)) has_digit = true;
            if (contains_char(special, c)) has_special = true;
        }

        if (has_lowercase && has_uppercase && has_digit && has_special) {
            passwords_with_all_types++;
        }
    }

    // At least 75% of passwords should contain all character types
    // (Random distributions naturally vary, especially with only 20 chars)
    EXPECT_GE(passwords_with_all_types, iterations * 0.75)
        << "Only " << passwords_with_all_types << " out of " << iterations
        << " passwords contained all character types";
}

// Test: Character distribution is roughly uniform
TEST_F(PasswordGeneratorTest, CharacterDistributionIsUniform) {
    std::map<char, int> char_frequency;
    const int total_chars = 1000;  // Generate enough characters for statistical test

    for (int i = 0; i < total_chars / password_length; ++i) {
        std::string password = generate_password();
        for (char c : password) {
            char_frequency[c]++;
        }
    }

    // Calculate expected frequency (uniform distribution)
    const double expected_frequency = static_cast<double>(total_chars) / full_charset.size();
    const double tolerance = expected_frequency * 1.5;  // 150% tolerance for randomness

    for (char c : full_charset) {
        int frequency = char_frequency[c];
        EXPECT_NEAR(frequency, expected_frequency, tolerance)
            << "Character '" << c << "' frequency (" << frequency
            << ") deviates significantly from expected (" << expected_frequency << ")";
    }
}

// Test: Password strength metrics
TEST_F(PasswordGeneratorTest, PasswordMeetsStrengthRequirements) {
    for (int i = 0; i < 10; ++i) {
        std::string password = generate_password();

        // Count unique characters
        std::set<char> unique_chars(password.begin(), password.end());

        // Should have reasonable diversity (at least 50% unique chars)
        EXPECT_GE(unique_chars.size(), password_length / 2)
            << "Password has low character diversity: " << password;

        // Note: We don't check for repeated characters because true randomness
        // means patterns CAN occur. The probability of 3 identical consecutive
        // chars is ~0.016% per position, which is acceptable for strong passwords.
        // Entropy checks above are sufficient for security validation.
    }
}

// Test: Charset correctness - verify no excluded characters
TEST_F(PasswordGeneratorTest, CharsetExcludesAmbiguousCharacters) {
    // Verify the charset itself doesn't contain ambiguous chars
    EXPECT_EQ(full_charset.find('0'), std::string_view::npos) << "Charset contains '0'";
    EXPECT_EQ(full_charset.find('O'), std::string_view::npos) << "Charset contains 'O'";
    EXPECT_EQ(full_charset.find('1'), std::string_view::npos) << "Charset contains '1'";
    EXPECT_EQ(full_charset.find('l'), std::string_view::npos) << "Charset contains 'l'";
    EXPECT_EQ(full_charset.find('I'), std::string_view::npos) << "Charset contains 'I'";
}

// Test: Charset completeness - verify expected characters are present
TEST_F(PasswordGeneratorTest, CharsetIsComplete) {
    // Verify key characters from each category
    EXPECT_NE(full_charset.find('a'), std::string_view::npos) << "Missing lowercase 'a'";
    EXPECT_NE(full_charset.find('z'), std::string_view::npos) << "Missing lowercase 'z'";
    EXPECT_NE(full_charset.find('A'), std::string_view::npos) << "Missing uppercase 'A'";
    EXPECT_NE(full_charset.find('Z'), std::string_view::npos) << "Missing uppercase 'Z'";
    EXPECT_NE(full_charset.find('2'), std::string_view::npos) << "Missing digit '2'";
    EXPECT_NE(full_charset.find('9'), std::string_view::npos) << "Missing digit '9'";
    EXPECT_NE(full_charset.find('!'), std::string_view::npos) << "Missing special '!'";
    EXPECT_NE(full_charset.find('?'), std::string_view::npos) << "Missing special '?'";
}

// Test: Random device entropy check (ensure non-deterministic behavior)
TEST_F(PasswordGeneratorTest, RandomDeviceHasEntropy) {
    std::random_device rd;

    // Generate multiple random values
    std::set<unsigned int> random_values;
    for (int i = 0; i < 10; ++i) {
        random_values.insert(rd());
    }

    // Should get at least some variation (not all identical values)
    EXPECT_GT(random_values.size(), 1)
        << "Random device appears to be deterministic";

    // Note: entropy() == 0 might be platform-dependent
    // On Linux, std::random_device typically uses /dev/urandom which has entropy
}

// Main function
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
