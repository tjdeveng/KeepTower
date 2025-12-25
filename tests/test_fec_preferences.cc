// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file test_fec_preferences.cc
 * @brief Tests for FEC (Forward Error Correction) preferences handling
 *
 * Tests that verify:
 * - FEC settings are preserved when opening existing vaults
 * - Default preferences are applied to new vaults
 * - Creating new vaults after opening existing ones uses defaults, not previous vault settings
 */

#include <gtest/gtest.h>
#include <glibmm/init.h>
#include "VaultManager.h"
#include <filesystem>

namespace fs = std::filesystem;

class FECPreferencesTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize GLib type system (needed for Glib::ustring)
        Glib::init();

        test_dir = fs::temp_directory_path() / "keeptower_fec_tests";
        fs::create_directories(test_dir);
        test_vault1_path = (test_dir / "vault1.vault").string();
        test_vault2_path = (test_dir / "vault2.vault").string();
        test_password = "TestPassword123!";
    }

    void TearDown() override {
        try {
            fs::remove_all(test_dir);
        } catch (...) {}
    }

    fs::path test_dir;
    std::string test_vault1_path;
    std::string test_vault2_path;
    Glib::ustring test_password;
};

// Test that apply_default_fec_preferences sets the expected values
TEST_F(FECPreferencesTest, ApplyDefaultFECPreferences_SetsCorrectly) {
    VaultManager manager;
    manager.set_backup_enabled(false);

    // Apply defaults: FEC enabled with 15% redundancy
    manager.apply_default_fec_preferences(true, 15);

    EXPECT_TRUE(manager.is_reed_solomon_enabled());
    EXPECT_EQ(manager.get_rs_redundancy_percent(), 15);
}

// Test that FEC settings are preserved when opening a vault with FEC enabled
TEST_F(FECPreferencesTest, OpenVault_PreservesFECEnabled) {
    VaultManager manager;
    manager.set_backup_enabled(false);

    // Create vault with FEC enabled at 20%
    manager.set_reed_solomon_enabled(true);
    manager.set_rs_redundancy_percent(20);
    ASSERT_TRUE(manager.create_vault(test_vault1_path, test_password));
    ASSERT_TRUE(manager.save_vault());
    ASSERT_TRUE(manager.close_vault());

    // Change manager's settings to something different
    manager.set_reed_solomon_enabled(false);
    manager.set_rs_redundancy_percent(10);

    // Open the vault - should restore the file's FEC settings (enabled, 20%)
    ASSERT_TRUE(manager.open_vault(test_vault1_path, test_password));

    EXPECT_TRUE(manager.is_reed_solomon_enabled());
    EXPECT_EQ(manager.get_rs_redundancy_percent(), 20);
}

// Test that FEC settings are preserved when opening a vault with FEC disabled
TEST_F(FECPreferencesTest, OpenVault_PreservesFECDisabled) {
    VaultManager manager;
    manager.set_backup_enabled(false);

    // Create vault with FEC disabled
    manager.set_reed_solomon_enabled(false);
    ASSERT_TRUE(manager.create_vault(test_vault1_path, test_password));
    ASSERT_TRUE(manager.save_vault());
    ASSERT_TRUE(manager.close_vault());

    // Change manager's settings to FEC enabled
    manager.set_reed_solomon_enabled(true);
    manager.set_rs_redundancy_percent(25);

    // Open the vault - should restore the file's FEC settings (disabled)
    ASSERT_TRUE(manager.open_vault(test_vault1_path, test_password));

    EXPECT_FALSE(manager.is_reed_solomon_enabled());
}

// Test that creating a new vault after opening one uses defaults, not previous vault's settings
TEST_F(FECPreferencesTest, CreateVaultAfterOpen_UsesDefaults) {
    VaultManager manager;
    manager.set_backup_enabled(false);

    // Set defaults: FEC disabled
    manager.apply_default_fec_preferences(false, 10);

    // Create first vault with FEC enabled at 30%
    manager.set_reed_solomon_enabled(true);
    manager.set_rs_redundancy_percent(30);
    ASSERT_TRUE(manager.create_vault(test_vault1_path, test_password));
    ASSERT_TRUE(manager.save_vault());
    ASSERT_TRUE(manager.close_vault());

    // Reset to defaults before creating second vault
    // (This simulates what MainWindow does)
    manager.apply_default_fec_preferences(false, 10);

    // Create second vault - should use defaults (disabled), not first vault's settings
    ASSERT_TRUE(manager.create_vault(test_vault2_path, test_password));

    EXPECT_FALSE(manager.is_reed_solomon_enabled());
    EXPECT_EQ(manager.get_rs_redundancy_percent(), 10);

    ASSERT_TRUE(manager.save_vault());
    ASSERT_TRUE(manager.close_vault());

    // Verify by reopening the second vault
    ASSERT_TRUE(manager.open_vault(test_vault2_path, test_password));
    EXPECT_FALSE(manager.is_reed_solomon_enabled());
}

// Test that different redundancy levels are preserved correctly
TEST_F(FECPreferencesTest, OpenVault_PreservesRedundancyLevel) {
    VaultManager manager;
    manager.set_backup_enabled(false);

    // Test various redundancy levels
    std::vector<int> redundancy_levels = {5, 10, 25, 40, 50};

    for (int redundancy : redundancy_levels) {
        std::string vault_path = (test_dir / ("vault_" + std::to_string(redundancy) + ".vault")).string();

        // Create vault with specific redundancy
        manager.set_reed_solomon_enabled(true);
        manager.set_rs_redundancy_percent(redundancy);
        ASSERT_TRUE(manager.create_vault(vault_path, test_password));
        ASSERT_TRUE(manager.save_vault());
        ASSERT_TRUE(manager.close_vault());

        // Change to different settings
        manager.set_reed_solomon_enabled(false);
        manager.set_rs_redundancy_percent(15);

        // Open and verify original redundancy is preserved
        ASSERT_TRUE(manager.open_vault(vault_path, test_password));
        EXPECT_TRUE(manager.is_reed_solomon_enabled());
        EXPECT_EQ(manager.get_rs_redundancy_percent(), redundancy)
            << "Failed for redundancy level " << redundancy << "%";

        ASSERT_TRUE(manager.close_vault());
    }
}

// Test that user modifications override loaded file settings
TEST_F(FECPreferencesTest, UserModifications_OverrideFileSettings) {
    VaultManager manager;
    manager.set_backup_enabled(false);

    // Create vault with FEC enabled at 20%
    manager.set_reed_solomon_enabled(true);
    manager.set_rs_redundancy_percent(20);
    ASSERT_TRUE(manager.create_vault(test_vault1_path, test_password));
    ASSERT_TRUE(manager.save_vault());
    ASSERT_TRUE(manager.close_vault());

    // Open vault (loads FEC settings from file)
    ASSERT_TRUE(manager.open_vault(test_vault1_path, test_password));
    EXPECT_TRUE(manager.is_reed_solomon_enabled());
    EXPECT_EQ(manager.get_rs_redundancy_percent(), 20);

    // User modifies settings
    manager.set_reed_solomon_enabled(false);
    manager.set_rs_redundancy_percent(35);

    // Save with new settings
    ASSERT_TRUE(manager.save_vault());
    ASSERT_TRUE(manager.close_vault());

    // Reopen and verify new settings are preserved
    ASSERT_TRUE(manager.open_vault(test_vault1_path, test_password));
    EXPECT_FALSE(manager.is_reed_solomon_enabled());
    EXPECT_EQ(manager.get_rs_redundancy_percent(), 35);
}
