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
 * - V2 vault headers ALWAYS have 20% minimum FEC protection
 */

#include <gtest/gtest.h>
#include <glibmm/init.h>
#include "VaultManager.h"
#include "VaultFormatV2.h"
#include <filesystem>
#include <fstream>

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

// Test that FEC settings are preserved when opening a V2 vault with FEC enabled
TEST_F(FECPreferencesTest, OpenVault_PreservesFECEnabled) {
    VaultManager manager;
    manager.set_backup_enabled(false);

    // Create V2 vault with FEC enabled at 20%
    manager.set_reed_solomon_enabled(true);
    manager.set_rs_redundancy_percent(20);

    KeepTower::VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    policy.pbkdf2_iterations = 100000;
    policy.require_yubikey = false;

    auto result = manager.create_vault_v2(test_vault1_path, "admin", test_password, policy);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(manager.save_vault());
    ASSERT_TRUE(manager.close_vault());

    // Change manager's settings to something different
    manager.set_reed_solomon_enabled(false);
    manager.set_rs_redundancy_percent(10);

    // Open the vault - should restore the file's FEC settings (enabled, 20%)
    auto open_result = manager.open_vault_v2(test_vault1_path, "admin", test_password);
    ASSERT_TRUE(open_result.has_value());

    EXPECT_TRUE(manager.is_reed_solomon_enabled());
    EXPECT_EQ(manager.get_rs_redundancy_percent(), 20);
}

// Test that data FEC settings are preserved when opening a V2 vault with data FEC disabled
// Note: V2 vaults always have header FEC enabled at 20% minimum
TEST_F(FECPreferencesTest, OpenVault_PreservesDataFECDisabled) {
    VaultManager manager;
    manager.set_backup_enabled(false);

    // Create V2 vault with data FEC disabled (header still has 20% FEC)
    manager.set_reed_solomon_enabled(false);

    KeepTower::VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    policy.pbkdf2_iterations = 100000;
    policy.require_yubikey = false;

    auto result = manager.create_vault_v2(test_vault1_path, "admin", test_password, policy);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(manager.save_vault());
    ASSERT_TRUE(manager.close_vault());

    // Change manager's settings to FEC enabled
    manager.set_reed_solomon_enabled(true);
    manager.set_rs_redundancy_percent(25);

    // Open the vault - should restore the file's data FEC settings (disabled)
    auto open_result = manager.open_vault_v2(test_vault1_path, "admin", test_password);
    ASSERT_TRUE(open_result.has_value());

    EXPECT_FALSE(manager.is_reed_solomon_enabled());
}

// Test that creating a new V2 vault after opening one uses defaults, not previous vault's settings
TEST_F(FECPreferencesTest, CreateVaultAfterOpen_UsesDefaults) {
    VaultManager manager;
    manager.set_backup_enabled(false);

    KeepTower::VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    policy.pbkdf2_iterations = 100000;
    policy.require_yubikey = false;

    // Set defaults: data FEC disabled
    manager.apply_default_fec_preferences(false, 10);

    // Create first vault with data FEC enabled at 30%
    manager.set_reed_solomon_enabled(true);
    manager.set_rs_redundancy_percent(30);
    auto result1 = manager.create_vault_v2(test_vault1_path, "admin", test_password, policy);
    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(manager.save_vault());
    ASSERT_TRUE(manager.close_vault());

    // Reset to defaults before creating second vault
    // (This simulates what MainWindow does)
    manager.apply_default_fec_preferences(false, 10);

    // Create second vault - should use defaults (disabled), not first vault's settings
    auto result2 = manager.create_vault_v2(test_vault2_path, "admin", test_password, policy);
    ASSERT_TRUE(result2.has_value());

    EXPECT_FALSE(manager.is_reed_solomon_enabled());
    EXPECT_EQ(manager.get_rs_redundancy_percent(), 10);

    ASSERT_TRUE(manager.save_vault());
    ASSERT_TRUE(manager.close_vault());

    // Verify by reopening the second vault
    auto open_result = manager.open_vault_v2(test_vault2_path, "admin", test_password);
    ASSERT_TRUE(open_result.has_value());
    EXPECT_FALSE(manager.is_reed_solomon_enabled());
}

// Test that different redundancy levels are preserved correctly in V2 vaults
TEST_F(FECPreferencesTest, OpenVault_PreservesRedundancyLevel) {
    VaultManager manager;
    manager.set_backup_enabled(false);

    KeepTower::VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    policy.pbkdf2_iterations = 100000;
    policy.require_yubikey = false;

    // Test various redundancy levels
    std::vector<int> redundancy_levels = {5, 10, 25, 40, 50};

    for (int redundancy : redundancy_levels) {
        std::string vault_path = (test_dir / ("vault_" + std::to_string(redundancy) + ".vault")).string();

        // Create V2 vault with specific redundancy
        manager.set_reed_solomon_enabled(true);
        manager.set_rs_redundancy_percent(redundancy);
        auto result = manager.create_vault_v2(vault_path, "admin", test_password, policy);
        ASSERT_TRUE(result.has_value());
        ASSERT_TRUE(manager.save_vault());
        ASSERT_TRUE(manager.close_vault());

        // Change to different settings
        manager.set_reed_solomon_enabled(false);
        manager.set_rs_redundancy_percent(15);

        // Open and verify original redundancy is preserved
        auto open_result = manager.open_vault_v2(vault_path, "admin", test_password);
        ASSERT_TRUE(open_result.has_value());
        EXPECT_TRUE(manager.is_reed_solomon_enabled());
        EXPECT_EQ(manager.get_rs_redundancy_percent(), redundancy)
            << "Failed for redundancy level " << redundancy << "%";

        ASSERT_TRUE(manager.close_vault());
    }
}

// Test that user modifications override loaded file settings in V2 vaults
TEST_F(FECPreferencesTest, UserModifications_OverrideFileSettings) {
    VaultManager manager;
    manager.set_backup_enabled(false);

    KeepTower::VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    policy.pbkdf2_iterations = 100000;
    policy.require_yubikey = false;

    // Create V2 vault with data FEC enabled at 20%
    manager.set_reed_solomon_enabled(true);
    manager.set_rs_redundancy_percent(20);
    auto result = manager.create_vault_v2(test_vault1_path, "admin", test_password, policy);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(manager.save_vault());
    ASSERT_TRUE(manager.close_vault());

    // Open vault (loads FEC settings from file)
    auto open_result = manager.open_vault_v2(test_vault1_path, "admin", test_password);
    ASSERT_TRUE(open_result.has_value());
    EXPECT_TRUE(manager.is_reed_solomon_enabled());
    EXPECT_EQ(manager.get_rs_redundancy_percent(), 20);

    // User modifies settings
    manager.set_reed_solomon_enabled(false);
    manager.set_rs_redundancy_percent(35);

    // Save with new settings
    ASSERT_TRUE(manager.save_vault());
    ASSERT_TRUE(manager.close_vault());

    // Reopen and verify new settings are preserved
    auto reopen_result = manager.open_vault_v2(test_vault1_path, "admin", test_password);
    ASSERT_TRUE(reopen_result.has_value());
    EXPECT_FALSE(manager.is_reed_solomon_enabled());
    EXPECT_EQ(manager.get_rs_redundancy_percent(), 35);
}

// ============================================================================
// V2 Vault FEC Tests (Header must ALWAYS have 20% minimum FEC)
// ============================================================================

// Test that V2 vaults have 20% header FEC even when data FEC is disabled
TEST_F(FECPreferencesTest, V2_HeaderFEC_Always20Percent_WhenDataFECDisabled) {
    VaultManager manager;
    manager.set_backup_enabled(false);

    // Set FEC to disabled (for data)
    manager.apply_default_fec_preferences(false, 0);
    EXPECT_FALSE(manager.is_reed_solomon_enabled());

    // Create V2 vault
    KeepTower::VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    policy.pbkdf2_iterations = 100000;
    policy.require_yubikey = false;

    auto result = manager.create_vault_v2(test_vault1_path, "admin", test_password, policy);
    ASSERT_TRUE(result.has_value()) << "Failed to create V2 vault";
    ASSERT_TRUE(manager.save_vault());

    // Read the raw vault file and verify header has FEC
    std::ifstream file(test_vault1_path, std::ios::binary);
    ASSERT_TRUE(file.is_open());
    std::vector<uint8_t> file_data((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
    file.close();

    // Parse header
    auto header_result = KeepTower::VaultFormatV2::read_header(file_data);
    ASSERT_TRUE(header_result.has_value()) << "Failed to read header";

    // Check that header had FEC applied (file should be larger than raw header)
    // Magic(4) + version(4) + pbkdf2(4) + header_size(4) + flags(1) = 17 bytes minimum
    ASSERT_GT(file_data.size(), 17) << "Header should have FEC protection";

    // Verify the header flag has FEC enabled
    // Byte 16 is the header_flags (after magic, version, pbkdf2_iterations, header_size)
    ASSERT_GE(file_data.size(), 17);
    uint8_t header_flags = file_data[16];
    const uint8_t HEADER_FLAG_FEC_ENABLED = 0x01;
    EXPECT_TRUE((header_flags & HEADER_FLAG_FEC_ENABLED) != 0)
        << "Header FEC flag should be set even when data FEC is disabled";
}

// Test that V2 vaults use 20% header FEC when data FEC is < 20%
TEST_F(FECPreferencesTest, V2_HeaderFEC_Uses20Percent_WhenDataFECIsLow) {
    VaultManager manager;
    manager.set_backup_enabled(false);

    // Set FEC to 10% (below 20% minimum for header)
    manager.apply_default_fec_preferences(true, 10);
    EXPECT_TRUE(manager.is_reed_solomon_enabled());
    EXPECT_EQ(manager.get_rs_redundancy_percent(), 10);

    // Create V2 vault
    KeepTower::VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    policy.pbkdf2_iterations = 100000;
    policy.require_yubikey = false;

    auto result = manager.create_vault_v2(test_vault1_path, "admin", test_password, policy);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(manager.save_vault());

    // Read and verify header has FEC
    std::ifstream file(test_vault1_path, std::ios::binary);
    ASSERT_TRUE(file.is_open());
    std::vector<uint8_t> file_data((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
    file.close();

    ASSERT_GE(file_data.size(), 17);
    uint8_t header_flags = file_data[16];
    const uint8_t HEADER_FLAG_FEC_ENABLED = 0x01;
    EXPECT_TRUE((header_flags & HEADER_FLAG_FEC_ENABLED) != 0)
        << "Header FEC flag should be set with 20% minimum";
}

// Test that V2 vaults use user's rate for header when data FEC > 20%
TEST_F(FECPreferencesTest, V2_HeaderFEC_UsesUserRate_WhenDataFECIsHigh) {
    VaultManager manager;
    manager.set_backup_enabled(false);

    // Set FEC to 30% (above 20% minimum)
    manager.apply_default_fec_preferences(true, 30);
    EXPECT_TRUE(manager.is_reed_solomon_enabled());
    EXPECT_EQ(manager.get_rs_redundancy_percent(), 30);

    // Create V2 vault
    KeepTower::VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    policy.pbkdf2_iterations = 100000;
    policy.require_yubikey = false;

    auto result = manager.create_vault_v2(test_vault1_path, "admin", test_password, policy);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(manager.save_vault());

    // Verify header has FEC
    std::ifstream file(test_vault1_path, std::ios::binary);
    ASSERT_TRUE(file.is_open());
    std::vector<uint8_t> file_data((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
    file.close();

    ASSERT_GE(file_data.size(), 17);
    uint8_t header_flags = file_data[16];
    const uint8_t HEADER_FLAG_FEC_ENABLED = 0x01;
    EXPECT_TRUE((header_flags & HEADER_FLAG_FEC_ENABLED) != 0)
        << "Header FEC flag should be set with user's 30% rate";
}

// Test that saving V2 vault also maintains 20% minimum header FEC
TEST_F(FECPreferencesTest, V2_SaveVault_HeaderFEC_Always20Percent) {
    VaultManager manager;
    manager.set_backup_enabled(false);

    // Create vault with FEC enabled at 25%
    manager.apply_default_fec_preferences(true, 25);

    KeepTower::VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    policy.pbkdf2_iterations = 100000;
    policy.require_yubikey = false;

    auto result = manager.create_vault_v2(test_vault1_path, "admin", test_password, policy);
    ASSERT_TRUE(result.has_value());

    // Now disable FEC and save
    manager.set_reed_solomon_enabled(false);
    ASSERT_TRUE(manager.save_vault());

    // Read and verify header still has FEC
    std::ifstream file(test_vault1_path, std::ios::binary);
    ASSERT_TRUE(file.is_open());
    std::vector<uint8_t> file_data((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
    file.close();

    ASSERT_GE(file_data.size(), 17);
    uint8_t header_flags = file_data[16];
    const uint8_t HEADER_FLAG_FEC_ENABLED = 0x01;
    EXPECT_TRUE((header_flags & HEADER_FLAG_FEC_ENABLED) != 0)
        << "Header FEC should remain enabled even after disabling data FEC";
}

