// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng
//
// Test program to verify UI security features:
// - GSettings security configuration
// - Clipboard timeout settings
// - Auto-lock settings
// - Password history settings

#include <gtest/gtest.h>
#include <glibmm/init.h>
#include <giomm/init.h>
#include <giomm/settings.h>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

class UISecurityTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize GLib type system and Gio
        Glib::init();
        Gio::init();

        // Use GSETTINGS_SCHEMA_DIR from environment if set (for meson test),
        // otherwise fallback to source directory
        const char* env_schema_dir = std::getenv("GSETTINGS_SCHEMA_DIR");
        if (env_schema_dir) {
            schema_dir = env_schema_dir;
        } else {
            schema_dir = fs::current_path() / ".." / "data";
            g_setenv("GSETTINGS_SCHEMA_DIR", schema_dir.c_str(), TRUE);
        }

        try {
            settings = Gio::Settings::create("com.tjdeveng.keeptower");
        } catch (const Glib::Error& e) {
            FAIL() << "Failed to create settings: " << e.what();
        }
    }

    void TearDown() override {
        // Reset to defaults
        if (settings) {
            settings->reset("clipboard-clear-timeout");
            settings->reset("auto-lock-enabled");
            settings->reset("auto-lock-timeout");
            settings->reset("password-history-enabled");
            settings->reset("password-history-limit");
        }
    }

    Glib::RefPtr<Gio::Settings> settings;
    fs::path schema_dir;
};

// Test clipboard timeout settings
TEST_F(UISecurityTest, ClipboardTimeoutDefaults) {
    ASSERT_TRUE(settings);

    // Check default value (schema default is 30 seconds)
    const int default_timeout = settings->get_int("clipboard-clear-timeout");
    EXPECT_EQ(default_timeout, 30) << "Default clipboard timeout should be 30 seconds";
}

TEST_F(UISecurityTest, ClipboardTimeoutRange) {
    ASSERT_TRUE(settings);

    // Test minimum boundary
    settings->set_int("clipboard-clear-timeout", 5);
    EXPECT_EQ(settings->get_int("clipboard-clear-timeout"), 5);

    // Test maximum boundary
    settings->set_int("clipboard-clear-timeout", 300);
    EXPECT_EQ(settings->get_int("clipboard-clear-timeout"), 300);

    // Test mid-range value
    settings->set_int("clipboard-clear-timeout", 60);
    EXPECT_EQ(settings->get_int("clipboard-clear-timeout"), 60);
}

TEST_F(UISecurityTest, ClipboardTimeoutBoundaryValidation) {
    ASSERT_TRUE(settings);

    // GSettings should enforce schema bounds
    // Values outside 5-300 range should be rejected or clamped

    // This test verifies the schema defines proper ranges
    // The key should exist and be readable
    EXPECT_NO_THROW({
        const int value = settings->get_int("clipboard-clear-timeout");
        EXPECT_GE(value, 5);
        EXPECT_LE(value, 300);
    });
}// Test auto-lock settings
TEST_F(UISecurityTest, AutoLockDefaults) {
    ASSERT_TRUE(settings);

    // Check default enabled state
    const bool auto_lock_enabled = settings->get_boolean("auto-lock-enabled");
    EXPECT_TRUE(auto_lock_enabled) << "Auto-lock should be enabled by default for security";

    // Check default timeout (5 minutes = 300 seconds)
    const int auto_lock_timeout = settings->get_int("auto-lock-timeout");
    EXPECT_EQ(auto_lock_timeout, 300) << "Default auto-lock timeout should be 300 seconds (5 minutes)";
}

TEST_F(UISecurityTest, AutoLockEnableDisable) {
    ASSERT_TRUE(settings);

    // Test toggling auto-lock
    settings->set_boolean("auto-lock-enabled", false);
    EXPECT_FALSE(settings->get_boolean("auto-lock-enabled"));

    settings->set_boolean("auto-lock-enabled", true);
    EXPECT_TRUE(settings->get_boolean("auto-lock-enabled"));
}

TEST_F(UISecurityTest, AutoLockTimeoutRange) {
    ASSERT_TRUE(settings);

    // Test minimum boundary (1 minute = 60 seconds)
    settings->set_int("auto-lock-timeout", 60);
    EXPECT_EQ(settings->get_int("auto-lock-timeout"), 60);

    // Test maximum boundary (1 hour = 3600 seconds)
    settings->set_int("auto-lock-timeout", 3600);
    EXPECT_EQ(settings->get_int("auto-lock-timeout"), 3600);

    // Test reasonable mid-range value (10 minutes = 600 seconds)
    settings->set_int("auto-lock-timeout", 600);
    EXPECT_EQ(settings->get_int("auto-lock-timeout"), 600);
}

// Test password history settings
TEST_F(UISecurityTest, PasswordHistoryDefaults) {
    ASSERT_TRUE(settings);

    // Check default enabled state
    const bool history_enabled = settings->get_boolean("password-history-enabled");
    EXPECT_TRUE(history_enabled) << "Password history should be enabled by default";

    // Check default limit
    const int history_limit = settings->get_int("password-history-limit");
    EXPECT_EQ(history_limit, 5) << "Default password history limit should be 5";
}

TEST_F(UISecurityTest, PasswordHistoryRange) {
    ASSERT_TRUE(settings);

    // Test minimum boundary
    settings->set_int("password-history-limit", 1);
    EXPECT_EQ(settings->get_int("password-history-limit"), 1);

    // Test maximum boundary
    settings->set_int("password-history-limit", 20);
    EXPECT_EQ(settings->get_int("password-history-limit"), 20);

    // Test mid-range value
    settings->set_int("password-history-limit", 10);
    EXPECT_EQ(settings->get_int("password-history-limit"), 10);
}

// Test schema completeness
TEST_F(UISecurityTest, SchemaCompleteness) {
    ASSERT_TRUE(settings);

    // Verify all security keys exist by attempting to read them
    EXPECT_NO_THROW({
        settings->get_int("clipboard-clear-timeout");
    }) << "Schema missing clipboard-clear-timeout key";

    EXPECT_NO_THROW({
        settings->get_boolean("auto-lock-enabled");
    }) << "Schema missing auto-lock-enabled key";

    EXPECT_NO_THROW({
        settings->get_int("auto-lock-timeout");
    }) << "Schema missing auto-lock-timeout key";

    EXPECT_NO_THROW({
        settings->get_boolean("password-history-enabled");
    }) << "Schema missing password-history-enabled key";

    EXPECT_NO_THROW({
        settings->get_int("password-history-limit");
    }) << "Schema missing password-history-limit key";
}// Test security defaults are appropriate
TEST_F(UISecurityTest, SecurityDefaultsAppropriate) {
    ASSERT_TRUE(settings);

    // Security features should be enabled by default (secure by default principle)
    EXPECT_TRUE(settings->get_boolean("auto-lock-enabled"))
        << "Auto-lock should be enabled by default";
    EXPECT_TRUE(settings->get_boolean("password-history-enabled"))
        << "Password history should be enabled by default";

    // Timeouts should be reasonable (not too short, not too long)
    const int clipboard_timeout = settings->get_int("clipboard-clear-timeout");
    EXPECT_GE(clipboard_timeout, 15) << "Clipboard timeout too short for usability";
    EXPECT_LE(clipboard_timeout, 120) << "Clipboard timeout too long for security";

    const int auto_lock_timeout = settings->get_int("auto-lock-timeout");
    EXPECT_GE(auto_lock_timeout, 120) << "Auto-lock timeout too short for usability";
    EXPECT_LE(auto_lock_timeout, 900) << "Auto-lock timeout too long for security";
}

// Test settings persistence (simulated)
TEST_F(UISecurityTest, SettingsPersistence) {
    ASSERT_TRUE(settings);

    // Set custom values
    const int custom_clipboard_timeout = 45;
    const int custom_auto_lock_timeout = 480;

    settings->set_int("clipboard-clear-timeout", custom_clipboard_timeout);
    settings->set_int("auto-lock-timeout", custom_auto_lock_timeout);
    settings->set_boolean("auto-lock-enabled", false);

    // Verify values persist in same session
    EXPECT_EQ(settings->get_int("clipboard-clear-timeout"), custom_clipboard_timeout);
    EXPECT_EQ(settings->get_int("auto-lock-timeout"), custom_auto_lock_timeout);
    EXPECT_FALSE(settings->get_boolean("auto-lock-enabled"));
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
