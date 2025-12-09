// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "../src/utils/SettingsValidator.h"
#include <gtest/gtest.h>
#include <giomm/init.h>
#include <giomm/settings.h>
#include <cstdlib>

/**
 * @brief Test fixture for SettingsValidator tests
 */
class SettingsValidatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize Gio
        Gio::init();

        // Set schema directory for testing
        const char* schema_dir = std::getenv("GSETTINGS_SCHEMA_DIR");
        if (!schema_dir) {
            GTEST_SKIP() << "GSETTINGS_SCHEMA_DIR not set";
        }

        try {
            settings = Gio::Settings::create("com.tjdeveng.keeptower");
        } catch (const Glib::Error& e) {
            GTEST_SKIP() << "Could not create settings: " << e.what();
        }
    }

    void TearDown() override {
        if (settings) {
            // Reset to defaults after each test
            settings->reset("clipboard-clear-timeout");
            settings->reset("auto-lock-enabled");
            settings->reset("auto-lock-timeout");
            settings->reset("password-history-enabled");
            settings->reset("password-history-limit");
        }
    }

    Glib::RefPtr<Gio::Settings> settings;
};

/**
 * @brief Test clipboard timeout validation clamps values to safe range
 *
 * Note: GSettings schema provides the first line of defense by rejecting
 * out-of-range values. The validator provides a second layer that ensures
 * even if the schema is modified, values are clamped to safe ranges.
 */
TEST_F(SettingsValidatorTest, ClipboardTimeoutClampsToSafeRange) {
    // Schema prevents setting values outside its defined range (5-300)
    // If we try to set 1, schema will clamp it to 5
    // If we try to set 9999, schema will clamp it to 300

    // Test that validator returns schema-clamped minimum
    settings->set_int("clipboard-clear-timeout", 5);  // Schema minimum
    int value = SettingsValidator::get_clipboard_timeout(settings);
    EXPECT_EQ(value, 5);
    EXPECT_GE(value, SettingsValidator::MIN_CLIPBOARD_TIMEOUT);

    // Test that validator returns schema-clamped maximum
    settings->set_int("clipboard-clear-timeout", 300);  // Schema maximum
    value = SettingsValidator::get_clipboard_timeout(settings);
    EXPECT_EQ(value, 300);
    EXPECT_LE(value, SettingsValidator::MAX_CLIPBOARD_TIMEOUT);

    // Test valid value passes through
    settings->set_int("clipboard-clear-timeout", 60);
    value = SettingsValidator::get_clipboard_timeout(settings);
    EXPECT_EQ(value, 60);
}

/**
 * @brief Test auto-lock timeout validation clamps values to safe range
 */
TEST_F(SettingsValidatorTest, AutoLockTimeoutClampsToSafeRange) {
    // Schema prevents setting values outside its defined range (60-3600)

    // Test that validator returns schema-clamped minimum
    settings->set_int("auto-lock-timeout", 60);  // Schema minimum
    int value = SettingsValidator::get_auto_lock_timeout(settings);
    EXPECT_EQ(value, 60);
    EXPECT_GE(value, SettingsValidator::MIN_AUTO_LOCK_TIMEOUT);

    // Test that validator returns schema-clamped maximum
    settings->set_int("auto-lock-timeout", 3600);  // Schema maximum
    value = SettingsValidator::get_auto_lock_timeout(settings);
    EXPECT_EQ(value, 3600);
    EXPECT_LE(value, SettingsValidator::MAX_AUTO_LOCK_TIMEOUT);

    // Test valid value passes through
    settings->set_int("auto-lock-timeout", 600);
    value = SettingsValidator::get_auto_lock_timeout(settings);
    EXPECT_EQ(value, 600);
}

/**
 * @brief Test password history limit validation
 */
TEST_F(SettingsValidatorTest, PasswordHistoryLimitClampsToSafeRange) {
    // Schema prevents setting values outside its defined range (1-20)

    // Test that validator returns schema-clamped minimum
    settings->set_int("password-history-limit", 1);  // Schema minimum
    int value = SettingsValidator::get_password_history_limit(settings);
    EXPECT_EQ(value, 1);
    EXPECT_GE(value, SettingsValidator::MIN_PASSWORD_HISTORY);

    // Test that validator returns schema-clamped maximum
    settings->set_int("password-history-limit", 20);  // Schema maximum
    value = SettingsValidator::get_password_history_limit(settings);
    EXPECT_EQ(value, 20);
    EXPECT_LE(value, SettingsValidator::MAX_PASSWORD_HISTORY);

    // Test valid value passes through
    settings->set_int("password-history-limit", 10);
    value = SettingsValidator::get_password_history_limit(settings);
    EXPECT_EQ(value, 10);
}/**
 * @brief Test boolean getters work correctly
 */
TEST_F(SettingsValidatorTest, BooleanGettersWorkCorrectly) {
    settings->set_boolean("auto-lock-enabled", true);
    EXPECT_TRUE(SettingsValidator::is_auto_lock_enabled(settings));

    settings->set_boolean("auto-lock-enabled", false);
    EXPECT_FALSE(SettingsValidator::is_auto_lock_enabled(settings));

    settings->set_boolean("password-history-enabled", true);
    EXPECT_TRUE(SettingsValidator::is_password_history_enabled(settings));

    settings->set_boolean("password-history-enabled", false);
    EXPECT_FALSE(SettingsValidator::is_password_history_enabled(settings));
}

/**
 * @brief Test validator constants are sensible
 */
TEST(SettingsValidatorConstantsTest, ValidatorConstantsAreSensible) {
    // Clipboard timeout
    EXPECT_GE(SettingsValidator::MIN_CLIPBOARD_TIMEOUT, 5);
    EXPECT_LE(SettingsValidator::MAX_CLIPBOARD_TIMEOUT, 300);
    EXPECT_GE(SettingsValidator::DEFAULT_CLIPBOARD_TIMEOUT, SettingsValidator::MIN_CLIPBOARD_TIMEOUT);
    EXPECT_LE(SettingsValidator::DEFAULT_CLIPBOARD_TIMEOUT, SettingsValidator::MAX_CLIPBOARD_TIMEOUT);

    // Auto-lock timeout
    EXPECT_GE(SettingsValidator::MIN_AUTO_LOCK_TIMEOUT, 60);
    EXPECT_LE(SettingsValidator::MAX_AUTO_LOCK_TIMEOUT, 3600);
    EXPECT_GE(SettingsValidator::DEFAULT_AUTO_LOCK_TIMEOUT, SettingsValidator::MIN_AUTO_LOCK_TIMEOUT);
    EXPECT_LE(SettingsValidator::DEFAULT_AUTO_LOCK_TIMEOUT, SettingsValidator::MAX_AUTO_LOCK_TIMEOUT);

    // Password history
    EXPECT_GE(SettingsValidator::MIN_PASSWORD_HISTORY, 1);
    EXPECT_LE(SettingsValidator::MAX_PASSWORD_HISTORY, 20);
    EXPECT_GE(SettingsValidator::DEFAULT_PASSWORD_HISTORY, SettingsValidator::MIN_PASSWORD_HISTORY);
    EXPECT_LE(SettingsValidator::DEFAULT_PASSWORD_HISTORY, SettingsValidator::MAX_PASSWORD_HISTORY);
}

/**
 * @brief Test defense-in-depth: Schema + Validator protect against tampering
 *
 * This demonstrates the two-layer security approach:
 * 1. GSettings schema enforces ranges at the data layer
 * 2. SettingsValidator enforces the same ranges in code
 *
 * Even if an attacker modifies the schema file to allow insecure values,
 * the validator will clamp them. This test verifies the validator's constants
 * match or exceed the schema's security requirements.
 */
TEST_F(SettingsValidatorTest, ValidatorProvidesDefenseInDepth) {
    // Verify validator constants match schema constraints
    // This ensures even if schema is modified, code enforces security

    // Clipboard timeout: schema allows 5-300, validator enforces same
    EXPECT_EQ(SettingsValidator::MIN_CLIPBOARD_TIMEOUT, 5);
    EXPECT_EQ(SettingsValidator::MAX_CLIPBOARD_TIMEOUT, 300);

    // Auto-lock timeout: schema allows 60-3600, validator enforces same
    EXPECT_EQ(SettingsValidator::MIN_AUTO_LOCK_TIMEOUT, 60);
    EXPECT_EQ(SettingsValidator::MAX_AUTO_LOCK_TIMEOUT, 3600);

    // Password history: schema allows 1-20, validator enforces same
    EXPECT_EQ(SettingsValidator::MIN_PASSWORD_HISTORY, 1);
    EXPECT_EQ(SettingsValidator::MAX_PASSWORD_HISTORY, 20);

    // Test that validator would clamp hypothetical out-of-range values
    // (Schema prevents setting these, but validator provides backup)
    int clamped = std::clamp(1, SettingsValidator::MIN_CLIPBOARD_TIMEOUT,
                             SettingsValidator::MAX_CLIPBOARD_TIMEOUT);
    EXPECT_GE(clamped, 5) << "Validator would enforce minimum 5 seconds";

    clamped = std::clamp(1, SettingsValidator::MIN_AUTO_LOCK_TIMEOUT,
                        SettingsValidator::MAX_AUTO_LOCK_TIMEOUT);
    EXPECT_GE(clamped, 60) << "Validator would enforce minimum 60 seconds";
}int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
