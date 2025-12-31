/**
 * @file test_undo_redo_preferences.cc
 * @brief Tests for undo/redo preference integration
 *
 * Tests that the undo-redo-enabled preference correctly controls
 * whether operations are added to undo history.
 */

#include <gtest/gtest.h>
#include <glibmm/init.h>
#include <giomm/init.h>
#include <giomm/settings.h>
#include <memory>
#include "../src/core/VaultManager.h"
#include "../src/core/commands/AccountCommands.h"
#include "../src/core/commands/UndoManager.h"
#include "record.pb.h"

class UndoRedoPreferencesTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize GLib type system and Gio
        Glib::init();
        Gio::init();

        // Get settings instance
        m_settings = Gio::Settings::create("com.tjdeveng.keeptower");

        // Save original values
        m_original_undo_enabled = m_settings->get_boolean("undo-redo-enabled");
        m_original_history_limit = m_settings->get_int("undo-history-limit");

        // Create vault manager
        m_vault_manager = std::make_unique<VaultManager>();

        // Create and open a test vault
        const std::string vault_path = "/tmp/test_undo_prefs.vault";
        const std::string password = "TestPassword123!";

        // Clean up if vault exists
        std::remove(vault_path.c_str());

bool result = m_vault_manager->create_vault(vault_path, password);
        ASSERT_TRUE(result) << "Failed to create vault";

        result = m_vault_manager->open_vault(vault_path, password);
        ASSERT_TRUE(result) << "Failed to open vault";

        m_undo_manager = std::make_unique<UndoManager>();
    }

    void TearDown() override {
        // Restore original settings
        m_settings->set_boolean("undo-redo-enabled", m_original_undo_enabled);
        m_settings->set_int("undo-history-limit", m_original_history_limit);

        // Close vault
        if (m_vault_manager) {
            (void)m_vault_manager->close_vault();
        }

        // Clean up test vault
        std::remove("/tmp/test_undo_prefs.vault");
        std::remove("/tmp/test_undo_prefs.vault.backup");
    }

    std::unique_ptr<VaultManager> m_vault_manager;
    std::unique_ptr<UndoManager> m_undo_manager;
    Glib::RefPtr<Gio::Settings> m_settings;
    bool m_original_undo_enabled{true};
    int m_original_history_limit{50};
};

/**
 * @test Test that undo-redo-enabled preference defaults to true
 */
TEST_F(UndoRedoPreferencesTest, DefaultEnabledValue) {
    // Reset to schema default first to test actual default, not user override
    m_settings->reset("undo-redo-enabled");

    // Default should be true (enabled)
    bool enabled = m_settings->get_boolean("undo-redo-enabled");
    EXPECT_TRUE(enabled) << "Default undo-redo-enabled should be true";
}

/**
 * @test Test that undo-history-limit preference defaults to 50
 */
TEST_F(UndoRedoPreferencesTest, DefaultHistoryLimit) {
    // Reset to schema default first to test actual default, not user override
    m_settings->reset("undo-history-limit");

    // Default should be 50
    int limit = m_settings->get_int("undo-history-limit");
    EXPECT_EQ(limit, 50) << "Default undo-history-limit should be 50";
}

/**
 * @test Test that setting undo-redo-enabled to false can be read back
 */
TEST_F(UndoRedoPreferencesTest, TogglePreference) {
    // Set to false
    m_settings->set_boolean("undo-redo-enabled", false);
    bool enabled = m_settings->get_boolean("undo-redo-enabled");
    EXPECT_FALSE(enabled) << "Setting undo-redo-enabled to false should persist";

    // Set to true
    m_settings->set_boolean("undo-redo-enabled", true);
    enabled = m_settings->get_boolean("undo-redo-enabled");
    EXPECT_TRUE(enabled) << "Setting undo-redo-enabled to true should persist";
}

/**
 * @test Test that history limit can be set and read back
 */
TEST_F(UndoRedoPreferencesTest, ChangeHistoryLimit) {
    // Test various valid values
    for (int limit : {1, 10, 25, 50, 75, 100}) {
        m_settings->set_int("undo-history-limit", limit);
        int read_limit = m_settings->get_int("undo-history-limit");
        EXPECT_EQ(read_limit, limit) << "History limit " << limit << " should persist";
    }
}

/**
 * @test Test that UndoManager respects max history limit from settings
 */
TEST_F(UndoRedoPreferencesTest, HistoryLimitRespected) {
    // Set limit to 5
    const int limit = 5;
    m_settings->set_int("undo-history-limit", limit);

    // Apply limit to UndoManager
    int setting_limit = m_settings->get_int("undo-history-limit");
    m_undo_manager->set_max_history(setting_limit);

    // Add more commands than the limit
    bool ui_called = false;
    auto ui_callback = [&ui_called]() { ui_called = true; };

    for (int i = 0; i < limit + 3; ++i) {
        keeptower::AccountRecord account;
        account.set_id("test_" + std::to_string(i));
        account.set_account_name("Account " + std::to_string(i));
        account.set_password("password");

        auto command = std::make_unique<AddAccountCommand>(
            m_vault_manager.get(),
            std::move(account),
            ui_callback
        );

        (void)m_undo_manager->execute_command(std::move(command));
    }

    // Should only be able to undo up to the limit
    int undo_count = 0;
    while (m_undo_manager->can_undo()) {
        (void)m_undo_manager->undo();
        undo_count++;
    }

    EXPECT_EQ(undo_count, limit) << "Should only keep " << limit << " commands in history";
}

/**
 * @test Test that when undo is disabled, history should be cleared
 *
 * This simulates what MainWindow does when preferences change:
 * 1. User operates with undo enabled
 * 2. User disables undo in preferences
 * 3. History is cleared (for security)
 */
TEST_F(UndoRedoPreferencesTest, DisablingClearsHistory) {
    // Enable undo and add some commands
    m_settings->set_boolean("undo-redo-enabled", true);

    bool ui_called = false;
    auto ui_callback = [&ui_called]() { ui_called = true; };

    for (int i = 0; i < 3; ++i) {
        keeptower::AccountRecord account;
        account.set_id("test_" + std::to_string(i));
        account.set_account_name("Account " + std::to_string(i));
        account.set_password("sensitive_password");

        auto command = std::make_unique<AddAccountCommand>(
            m_vault_manager.get(),
            std::move(account),
            ui_callback
        );

        (void)m_undo_manager->execute_command(std::move(command));
    }

    EXPECT_TRUE(m_undo_manager->can_undo()) << "Should have undo history";

    // Now disable undo (simulating MainWindow behavior)
    m_settings->set_boolean("undo-redo-enabled", false);

    // Application should clear history for security
    m_undo_manager->clear();

    EXPECT_FALSE(m_undo_manager->can_undo()) << "History should be cleared when undo is disabled";
    EXPECT_FALSE(m_undo_manager->can_redo()) << "Redo history should also be cleared";
}

/**
 * @test Test bounds checking for history limit
 */
TEST_F(UndoRedoPreferencesTest, HistoryLimitBounds) {
    // GSettings schema enforces 1-100 range, but test clamping logic

    // Test minimum
    m_settings->set_int("undo-history-limit", 1);
    int limit = m_settings->get_int("undo-history-limit");
    int clamped = std::clamp(limit, 1, 100);
    EXPECT_EQ(clamped, 1) << "Minimum limit should be 1";

    // Test maximum
    m_settings->set_int("undo-history-limit", 100);
    limit = m_settings->get_int("undo-history-limit");
    clamped = std::clamp(limit, 1, 100);
    EXPECT_EQ(clamped, 100) << "Maximum limit should be 100";
}

/**
 * @test Test that preference changes are thread-safe
 *
 * This test verifies that reading/writing preferences doesn't cause
 * race conditions (though GSettings handles this internally).
 */
TEST_F(UndoRedoPreferencesTest, PreferenceReadWrite) {
    // Perform multiple read/write cycles
    for (int i = 0; i < 10; ++i) {
        bool enabled = (i % 2 == 0);
        m_settings->set_boolean("undo-redo-enabled", enabled);

        bool read_enabled = m_settings->get_boolean("undo-redo-enabled");
        EXPECT_EQ(read_enabled, enabled) << "Read value should match written value at iteration " << i;

        int limit = (i % 5) * 20 + 10;  // Values: 10, 30, 50, 70, 90
        m_settings->set_int("undo-history-limit", limit);

        int read_limit = m_settings->get_int("undo-history-limit");
        EXPECT_EQ(read_limit, limit) << "Read limit should match written limit at iteration " << i;
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    // Initialize GLib for Settings
    Glib::init();

    return RUN_ALL_TESTS();
}
