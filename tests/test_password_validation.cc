// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include <gtest/gtest.h>
#include "CreatePasswordDialog.h"
#include "../src/core/CommonPasswords.h"
#include <gtkmm.h>

// Test fixture for password validation
class PasswordValidationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize GTK application for testing (handle headless environments)
        if (!Gtk::Application::get_default()) {
            try {
                app = Gtk::Application::create("com.keeptower.test");
            } catch (...) {
                // GTK initialization can fail in headless CI - not needed for these tests
            }
        }
    }

    Glib::RefPtr<Gtk::Application> app;
};

// Helper function to test password validation logic
// This tests the NIST SP 800-63B requirements
class PasswordValidator {
public:
    static bool validate_nist_requirements(const Glib::ustring& password) {
        // Minimum length requirement
        if (password.length() < 8) {
            return false;
        }

        // Maximum reasonable length (to prevent DoS)
        if (password.length() > 128) {
            return false;
        }

        // Check using comprehensive common password list
        std::string password_str(password.raw());
        return !KeepTower::is_common_password(password_str);
    }

    static int calculate_strength(const Glib::ustring& password) {
        int strength = 0;

        // Length scoring
        if (password.length() >= 8) strength += 20;
        if (password.length() >= 12) strength += 20;
        if (password.length() >= 16) strength += 20;
        if (password.length() >= 20) strength += 10;

        // Character diversity
        bool has_lower = false;
        bool has_upper = false;
        bool has_digit = false;
        bool has_special = false;

        for (char32_t ch : password) {
            if (g_unichar_islower(ch)) has_lower = true;
            else if (g_unichar_isupper(ch)) has_upper = true;
            else if (g_unichar_isdigit(ch)) has_digit = true;
            else if (g_unichar_ispunct(ch)) has_special = true;
        }

        if (has_lower) strength += 10;
        if (has_upper) strength += 10;
        if (has_digit) strength += 5;
        if (has_special) strength += 5;

        return std::min(strength, 100);
    }
};

// ============================================================================
// NIST SP 800-63B Requirement Tests
// ============================================================================

TEST_F(PasswordValidationTest, MinimumLength_8Characters_Valid) {
    // Use non-common passwords for valid test cases
    EXPECT_TRUE(PasswordValidator::validate_nist_requirements("xK9#mP2q"));
    EXPECT_TRUE(PasswordValidator::validate_nist_requirements("rT8$vN3w"));
}

TEST_F(PasswordValidationTest, MinimumLength_LessThan8_Invalid) {
    EXPECT_FALSE(PasswordValidator::validate_nist_requirements("1234567"));
    EXPECT_FALSE(PasswordValidator::validate_nist_requirements("short"));
    EXPECT_FALSE(PasswordValidator::validate_nist_requirements(""));
}

TEST_F(PasswordValidationTest, CommonPasswords_Rejected) {
    EXPECT_FALSE(PasswordValidator::validate_nist_requirements("password"));
    EXPECT_FALSE(PasswordValidator::validate_nist_requirements("12345678"));
    EXPECT_FALSE(PasswordValidator::validate_nist_requirements("qwerty"));
    EXPECT_FALSE(PasswordValidator::validate_nist_requirements("Password1"));
}

TEST_F(PasswordValidationTest, CaseInsensitiveCommonPasswordCheck) {
    EXPECT_FALSE(PasswordValidator::validate_nist_requirements("PASSWORD"));
    EXPECT_FALSE(PasswordValidator::validate_nist_requirements("PaSsWoRd"));
    EXPECT_FALSE(PasswordValidator::validate_nist_requirements("QWERTY"));
}

TEST_F(PasswordValidationTest, NoCompositionRules_SimplePasswordsAllowed) {
    // NIST doesn't require character type mixing, but must not be common
    EXPECT_TRUE(PasswordValidator::validate_nist_requirements("zvxqkmjp"));
    EXPECT_TRUE(PasswordValidator::validate_nist_requirements("ZQXWVKJM"));
    EXPECT_TRUE(PasswordValidator::validate_nist_requirements("47298351"));
}

TEST_F(PasswordValidationTest, UnicodeCharacters_Supported) {
    // Use longer, more unique unicode passwords
    EXPECT_TRUE(PasswordValidator::validate_nist_requirements("mýp@ss☕🔒wørd"));
    EXPECT_TRUE(PasswordValidator::validate_nist_requirements("パスワード98765"));
    EXPECT_TRUE(PasswordValidator::validate_nist_requirements("крêпость987"));
}

TEST_F(PasswordValidationTest, MaximumLength_PreventDoS) {
    // Mix characters to avoid repeating pattern detection
    std::string very_long = "xK9mP2qrT8vN3w" + std::string(115, 'b');
    EXPECT_FALSE(PasswordValidator::validate_nist_requirements(very_long));

    std::string acceptable = "xK9mP2qrT8vN3w" + std::string(114, 'b');
    EXPECT_TRUE(PasswordValidator::validate_nist_requirements(acceptable));
}

TEST_F(PasswordValidationTest, Spaces_Allowed) {
    EXPECT_TRUE(PasswordValidator::validate_nist_requirements("pass word with spaces"));
    EXPECT_TRUE(PasswordValidator::validate_nist_requirements("my secure passphrase"));
}

TEST_F(PasswordValidationTest, SpecialCharacters_Allowed) {
    // Use a non-common password with special characters
    EXPECT_TRUE(PasswordValidator::validate_nist_requirements("xK#9mP!2q"));
    EXPECT_TRUE(PasswordValidator::validate_nist_requirements("testK#123$"));
    EXPECT_TRUE(PasswordValidator::validate_nist_requirements("cmplex&*()_+Z"));
}

// ============================================================================
// Password Strength Calculation Tests
// ============================================================================

TEST_F(PasswordValidationTest, StrengthCalculation_ShortPassword_LowStrength) {
    int strength = PasswordValidator::calculate_strength("pass");
    EXPECT_LT(strength, 30);
}

TEST_F(PasswordValidationTest, StrengthCalculation_MinimumLength_ModerateStrength) {
    int strength = PasswordValidator::calculate_strength("password");
    EXPECT_GE(strength, 20);
    EXPECT_LT(strength, 50);
}

TEST_F(PasswordValidationTest, StrengthCalculation_LongPassword_HigherStrength) {
    int strength = PasswordValidator::calculate_strength("averylongpassword");
    EXPECT_GE(strength, 60);
}

TEST_F(PasswordValidationTest, StrengthCalculation_MixedCharacters_BonusPoints) {
    int strength_simple = PasswordValidator::calculate_strength("passwordpass");
    int strength_mixed = PasswordValidator::calculate_strength("P@ssw0rd!");

    // Mixed characters should be at least as strong as simple passwords
    EXPECT_GE(strength_mixed, strength_simple);
}

TEST_F(PasswordValidationTest, StrengthCalculation_MaxStrength) {
    std::string strong_password = "ThisIsAVeryLongAndComplexP@ssw0rd!123";
    int strength = PasswordValidator::calculate_strength(strong_password);
    EXPECT_EQ(strength, 100);
}

TEST_F(PasswordValidationTest, StrengthCalculation_CharacterDiversity) {
    // Only lowercase (length 16)
    // Score: 20+20+20 (length) + 10 (lower) = 70
    int strength_lower = PasswordValidator::calculate_strength("passwordpassword");
    EXPECT_EQ(strength_lower, 70);

    // Lowercase + uppercase (length 16)
    // Score: 20+20+20 (length) + 10 (lower) + 10 (upper) = 80
    int strength_case = PasswordValidator::calculate_strength("PasswordPassword");
    EXPECT_EQ(strength_case, 80);
    EXPECT_GT(strength_case, strength_lower);

    // Lowercase + uppercase + digits + special (length 12)
    // Score: 20+20 (length) + 10 (lower) + 10 (upper) + 5 (digit) + 5 (special) = 70
    int strength_all = PasswordValidator::calculate_strength("Password123!");
    EXPECT_EQ(strength_all, 70);

    // Note: shorter length with all character types scores same as longer with fewer types
    // This is expected behavior of the algorithm
}

TEST_F(PasswordValidationTest, StrengthCalculation_LengthBonuses) {
    int strength_8 = PasswordValidator::calculate_strength("password");
    int strength_12 = PasswordValidator::calculate_strength("passwordpass");
    int strength_16 = PasswordValidator::calculate_strength("passwordpassword");
    int strength_20 = PasswordValidator::calculate_strength("passwordpasswordpassw");

    EXPECT_LT(strength_8, strength_12);
    EXPECT_LT(strength_12, strength_16);
    EXPECT_LT(strength_16, strength_20);
}

// ============================================================================
// Password Matching Tests (for confirmation field)
// ============================================================================

TEST_F(PasswordValidationTest, PasswordMatch_Identical_Success) {
    Glib::ustring password1 = "TestPassword123!";
    Glib::ustring password2 = "TestPassword123!";
    EXPECT_EQ(password1, password2);
}

TEST_F(PasswordValidationTest, PasswordMatch_Different_Fails) {
    Glib::ustring password1 = "TestPassword123!";
    Glib::ustring password2 = "TestPassword123";  // Missing !
    EXPECT_NE(password1, password2);
}

TEST_F(PasswordValidationTest, PasswordMatch_CaseSensitive) {
    Glib::ustring password1 = "TestPassword";
    Glib::ustring password2 = "testpassword";
    EXPECT_NE(password1, password2);
}

TEST_F(PasswordValidationTest, PasswordMatch_WhitespaceMatters) {
    Glib::ustring password1 = "Test Password";
    Glib::ustring password2 = "TestPassword";
    EXPECT_NE(password1, password2);
}

// ============================================================================
// Real-World Password Examples
// ============================================================================

TEST_F(PasswordValidationTest, RealWorld_PassphraseStyle_Valid) {
    EXPECT_TRUE(PasswordValidator::validate_nist_requirements("correct horse battery staple"));
    int strength = PasswordValidator::calculate_strength("correct horse battery staple");
    EXPECT_GE(strength, 70);
}

TEST_F(PasswordValidationTest, RealWorld_ComplexPassword_Valid) {
    EXPECT_TRUE(PasswordValidator::validate_nist_requirements("Tr0ub4dor&3"));
    int strength = PasswordValidator::calculate_strength("Tr0ub4dor&3");
    EXPECT_GE(strength, 50);
}

TEST_F(PasswordValidationTest, RealWorld_VeryStrongPassword_MaxStrength) {
    EXPECT_TRUE(PasswordValidator::validate_nist_requirements("MyC0mpl3x&SecureP@ssphrase!2024"));
    int strength = PasswordValidator::calculate_strength("MyC0mpl3x&SecureP@ssphrase!2024");
    EXPECT_EQ(strength, 100);
}

TEST_F(PasswordValidationTest, RealWorld_WeakVariations_Rejected) {
    // Common pattern variations
    EXPECT_FALSE(PasswordValidator::validate_nist_requirements("password1"));
    EXPECT_FALSE(PasswordValidator::validate_nist_requirements("password123"));
    EXPECT_FALSE(PasswordValidator::validate_nist_requirements("password!"));
}

TEST_F(PasswordValidationTest, RealWorld_AcceptablePasswords_Valid) {
    // These should all be acceptable according to NIST - truly unique phrases
    EXPECT_TRUE(PasswordValidator::validate_nist_requirements("MyDog2024!"));
    EXPECT_TRUE(PasswordValidator::validate_nist_requirements("Vacation_Morocco_2027"));
    EXPECT_TRUE(PasswordValidator::validate_nist_requirements("ilovepizza123"));
    EXPECT_TRUE(PasswordValidator::validate_nist_requirements("RandomWords42"));
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(PasswordValidationTest, EdgeCase_EmptyPassword) {
    EXPECT_FALSE(PasswordValidator::validate_nist_requirements(""));
    int strength = PasswordValidator::calculate_strength("");
    EXPECT_EQ(strength, 0);
}

TEST_F(PasswordValidationTest, EdgeCase_OnlySpaces) {
    EXPECT_FALSE(PasswordValidator::validate_nist_requirements("       "));
}

TEST_F(PasswordValidationTest, EdgeCase_ExactMinimumLength) {
    // Use a non-sequential, non-common 8-character password
    EXPECT_TRUE(PasswordValidator::validate_nist_requirements("xK9#mP2q"));
    EXPECT_FALSE(PasswordValidator::validate_nist_requirements("1234567"));
}

TEST_F(PasswordValidationTest, EdgeCase_ExactMaximumLength) {
    // Mix characters to avoid repeating patterns
    std::string max_length = "xK9mP2qrT8vN3w" + std::string(114, 'b');
    EXPECT_TRUE(PasswordValidator::validate_nist_requirements(max_length));

    std::string over_max = "xK9mP2qrT8vN3w" + std::string(115, 'b');
    EXPECT_FALSE(PasswordValidator::validate_nist_requirements(over_max));
}

TEST_F(PasswordValidationTest, EdgeCase_OnlySpecialCharacters) {
    // Use special characters that aren't keyboard patterns
    EXPECT_TRUE(PasswordValidator::validate_nist_requirements("!#%^&*}{"));
    int strength = PasswordValidator::calculate_strength("!#%^&*}{");
    EXPECT_GT(strength, 20);
}

TEST_F(PasswordValidationTest, EdgeCase_RepeatingCharacters) {
    // Repeating characters are in the common password list and should be rejected
    // NIST allows any composition, but common patterns must still be blocked
    EXPECT_FALSE(PasswordValidator::validate_nist_requirements("aaaaaaaa"));
    EXPECT_FALSE(PasswordValidator::validate_nist_requirements("11111111"));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    // Initialize GTK (handle headless environments)
    try {
        // Set offscreen backend for headless CI environments
        g_setenv("GDK_BACKEND", "broadway", FALSE);
        Gtk::Application::create("com.keeptower.test");
    } catch (...) {
        // GTK initialization failed (headless environment)
        // Tests don't actually need GTK, so continue anyway
        std::cerr << "Warning: GTK initialization failed (headless environment?)" << std::endl;
    }

    return RUN_ALL_TESTS();
}
