// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file test_vault_reed_solomon.cc
 * @brief Integration tests for Reed-Solomon error correction in VaultManager
 *
 * Tests the end-to-end functionality of RS encoding/decoding when saving
 * and opening vault files, including corruption recovery scenarios.
 */

#include <gtest/gtest.h>
#include "VaultManager.h"
#include "managers/AccountManager.h"
#include "VaultFormatV2.h"
#include "record.pb.h"
#include "MultiUserTypes.h"
#include <filesystem>
#include <fstream>
#include <random>

namespace fs = std::filesystem;

class VaultReedSolomonTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir = "/tmp/keeptower_rs_tests";
        fs::create_directories(test_dir);

        test_vault_path = test_dir + "/test_rs.vault";
        test_password = "TestPassword123!";
        test_username = "admin";

        policy.require_yubikey = false;
        policy.min_password_length = 12;
        policy.pbkdf2_iterations = 100000;
        policy.password_history_depth = 0;
    }

    void TearDown() override {
        if (fs::exists(test_dir)) {
            fs::remove_all(test_dir);
        }
    }

    // Helper to create a simple account
    keeptower::AccountRecord createAccount(const std::string& name, const std::string& username) {
        keeptower::AccountRecord account;
        account.set_account_name(name);
        account.set_user_name(username);
        account.set_password("password123");
        account.set_created_at(std::time(nullptr));
        account.set_modified_at(std::time(nullptr));
        return account;
    }

    std::vector<uint8_t> readFileBytes(const std::string& filepath) {
        std::ifstream infile(filepath, std::ios::binary);
        return std::vector<uint8_t>(std::istreambuf_iterator<char>(infile), std::istreambuf_iterator<char>());
    }

    // Corrupt bytes in a file at specified positions
    void corruptFile(const std::string& filepath, const std::vector<size_t>& positions) {
        std::ifstream infile(filepath, std::ios::binary);
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(infile)),
                                  std::istreambuf_iterator<char>());
        infile.close();

        for (size_t pos : positions) {
            if (pos < data.size()) {
                data[pos] ^= 0xFF;  // Flip all bits
            }
        }

        std::ofstream outfile(filepath, std::ios::binary);
        outfile.write(reinterpret_cast<const char*>(data.data()), data.size());
        outfile.close();
    }

    // Helper to configure VaultManager for testing (disables backups)
    void configure_for_testing(VaultManager& manager) {
        VaultManager::BackupSettings settings = manager.get_backup_settings();
        settings.enabled = false;
        ASSERT_TRUE(manager.apply_backup_settings(settings));
    }

    std::string test_dir;
    std::string test_vault_path;
    Glib::ustring test_username;
    Glib::ustring test_password;
    KeepTower::VaultSecurityPolicy policy;
};

TEST_F(VaultReedSolomonTest, SaveWithRS_CreatesValidVault) {
    VaultManager manager;
    configure_for_testing(manager);

    // Enable RS with 10% redundancy
    manager.set_reed_solomon_enabled(true);
    manager.set_rs_redundancy_percent(10);

    // Create vault with RS enabled
    ASSERT_TRUE(manager.create_vault_v2(test_vault_path, test_username, test_password, policy));

    // Add some data
    auto account1 = createAccount("Example", "user@example.com");
    auto account2 = createAccount("Test", "test@test.com");
    ASSERT_TRUE(manager.account_manager()->add_account(account1));
    ASSERT_TRUE(manager.account_manager()->add_account(account2));

    // Save vault
    ASSERT_TRUE(manager.save_vault());
    ASSERT_TRUE(manager.close_vault());

    // Verify file exists
    ASSERT_TRUE(fs::exists(test_vault_path));

    // Verify RS setting is persisted in the V2 header (file size is not a stable indicator)
    {
        const auto file_data = readFileBytes(test_vault_path);
        auto header_result = KeepTower::VaultFormatV2::read_header(file_data);
        ASSERT_TRUE(header_result.has_value());
        EXPECT_EQ(header_result.value().first.fec_redundancy_percent, 10);
    }

    // Create another vault without RS for comparison
    VaultManager manager2;
    configure_for_testing(manager2);
    std::string test_vault_no_rs = test_dir + "/test_no_rs.vault";
    ASSERT_TRUE(manager2.create_vault_v2(test_vault_no_rs, test_username, test_password, policy));
    auto account3 = createAccount("Example", "user@example.com");
    auto account4 = createAccount("Test", "test@test.com");
    ASSERT_TRUE(manager2.account_manager()->add_account(account3));
    ASSERT_TRUE(manager2.account_manager()->add_account(account4));
    ASSERT_TRUE(manager2.save_vault());
    ASSERT_TRUE(manager2.close_vault());

    // RS disabled should persist as 0 in the V2 header
    {
        const auto file_data = readFileBytes(test_vault_no_rs);
        auto header_result = KeepTower::VaultFormatV2::read_header(file_data);
        ASSERT_TRUE(header_result.has_value());
        EXPECT_EQ(header_result.value().first.fec_redundancy_percent, 0);
    }
}

TEST_F(VaultReedSolomonTest, OpenRSVault_WithNoCorruption_Success) {
    VaultManager manager;
    configure_for_testing(manager);

    // Create and save RS vault
    manager.set_reed_solomon_enabled(true);
    manager.set_rs_redundancy_percent(10);
    ASSERT_TRUE(manager.create_vault_v2(test_vault_path, test_username, test_password, policy));
    auto account = createAccount("Example", "user@example.com");
    ASSERT_TRUE(manager.account_manager()->add_account(account));
    ASSERT_TRUE(manager.save_vault());
    ASSERT_TRUE(manager.close_vault());

    // Open the vault
    VaultManager manager2;
    configure_for_testing(manager2);
    ASSERT_TRUE(manager2.open_vault_v2(test_vault_path, test_username, test_password));

    // Verify data
    EXPECT_EQ(manager2.get_account_count(), 1);
    auto* account_out = manager2.account_manager()->get_account(0);
    ASSERT_NE(account_out, nullptr);
    EXPECT_EQ(account_out->account_name(), "Example");
    EXPECT_EQ(account_out->user_name(), "user@example.com");
}

TEST_F(VaultReedSolomonTest, OpenRSVault_WithMinorCorruption_Recovers) {
    VaultManager manager;
    configure_for_testing(manager);

    // Create RS vault with 20% redundancy (can recover ~10% corruption)
    manager.set_reed_solomon_enabled(true);
    manager.set_rs_redundancy_percent(20);
    ASSERT_TRUE(manager.create_vault_v2(test_vault_path, test_username, test_password, policy));
    auto account1 = createAccount("Example", "user@example.com");
    auto account2 = createAccount("Test", "test@test.com");
    ASSERT_TRUE(manager.account_manager()->add_account(account1));
    ASSERT_TRUE(manager.account_manager()->add_account(account2));
    ASSERT_TRUE(manager.save_vault());
    ASSERT_TRUE(manager.close_vault());

    // Corrupt a few bytes (well within recovery capability)
    std::vector<size_t> corrupt_positions = {100, 200, 300};  // Corrupt 3 bytes
    corruptFile(test_vault_path, corrupt_positions);

    // Should still open successfully with RS recovery
    VaultManager manager2;
    configure_for_testing(manager2);
    ASSERT_TRUE(manager2.open_vault_v2(test_vault_path, test_username, test_password));

    // Verify data is intact
    EXPECT_EQ(manager2.get_account_count(), 2);
    auto* account_1 = manager2.account_manager()->get_account(0);
    ASSERT_NE(account_1, nullptr);
    EXPECT_EQ(account_1->account_name(), "Example");

    auto* account_2 = manager2.account_manager()->get_account(1);
    ASSERT_NE(account_2, nullptr);
    EXPECT_EQ(account_2->account_name(), "Test");
}

TEST_F(VaultReedSolomonTest, OpenRSVault_WithSevereCorruption_Fails) {
    VaultManager manager;
    configure_for_testing(manager);

    // Create RS vault with 10% redundancy (can recover ~5% corruption)
    manager.set_reed_solomon_enabled(true);
    manager.set_rs_redundancy_percent(10);
    ASSERT_TRUE(manager.create_vault_v2(test_vault_path, test_username, test_password, policy));
    auto account = createAccount("Example", "user@example.com");
    ASSERT_TRUE(manager.account_manager()->add_account(account));
    ASSERT_TRUE(manager.save_vault());
    ASSERT_TRUE(manager.close_vault());

    // Corrupt many bytes (beyond recovery capability)
    std::vector<size_t> corrupt_positions;
    for (size_t i = 50; i < 150; i += 2) {  // Corrupt 50 bytes
        corrupt_positions.push_back(i);
    }
    corruptFile(test_vault_path, corrupt_positions);

    // Should fail to open
    VaultManager manager2;
    configure_for_testing(manager2);
    EXPECT_FALSE(manager2.open_vault_v2(test_vault_path, test_username, test_password));
}

TEST_F(VaultReedSolomonTest, DisableRS_SavesWithoutEncoding) {
    VaultManager manager;
    configure_for_testing(manager);

    // Create vault with RS disabled
    manager.set_reed_solomon_enabled(false);
    ASSERT_TRUE(manager.create_vault_v2(test_vault_path, test_username, test_password, policy));
    auto account = createAccount("Example", "user@example.com");
    ASSERT_TRUE(manager.account_manager()->add_account(account));
    ASSERT_TRUE(manager.save_vault());
    ASSERT_TRUE(manager.close_vault());

    // RS disabled should persist as 0 in the V2 header
    {
        const auto file_data = readFileBytes(test_vault_path);
        auto header_result = KeepTower::VaultFormatV2::read_header(file_data);
        ASSERT_TRUE(header_result.has_value());
        EXPECT_EQ(header_result.value().first.fec_redundancy_percent, 0);
    }

    // Enable RS, create new vault
    std::string test_vault_with_rs = test_dir + "/test_with_rs.vault";
    manager.set_reed_solomon_enabled(true);
    manager.set_rs_redundancy_percent(10);
    ASSERT_TRUE(manager.create_vault_v2(test_vault_with_rs, test_username, test_password, policy));
    auto account2 = createAccount("Example", "user@example.com");
    ASSERT_TRUE(manager.account_manager()->add_account(account2));
    ASSERT_TRUE(manager.save_vault());

    // RS enabled should persist as configured in the V2 header
    {
        const auto file_data = readFileBytes(test_vault_with_rs);
        auto header_result = KeepTower::VaultFormatV2::read_header(file_data);
        ASSERT_TRUE(header_result.has_value());
        EXPECT_EQ(header_result.value().first.fec_redundancy_percent, 10);
    }
}

TEST_F(VaultReedSolomonTest, ChangeRedundancyLevel_Works) {
    VaultManager manager;
    configure_for_testing(manager);

    // Test different redundancy levels
    for (uint8_t redundancy : {5, 10, 20, 30, 50}) {
        std::string vault_path = test_dir + "/vault_" + std::to_string(redundancy) + ".vault";

        manager.set_reed_solomon_enabled(true);
        ASSERT_TRUE(manager.set_rs_redundancy_percent(redundancy));
        EXPECT_EQ(manager.get_rs_redundancy_percent(), redundancy);

        ASSERT_TRUE(manager.create_vault_v2(vault_path, test_username, test_password, policy));
        auto account = createAccount("Test", "test@test.com");
        ASSERT_TRUE(manager.account_manager()->add_account(account));
        ASSERT_TRUE(manager.save_vault());
        ASSERT_TRUE(manager.close_vault());

        // Verify can open
        VaultManager manager2;
        configure_for_testing(manager2);
        ASSERT_TRUE(manager2.open_vault_v2(vault_path, test_username, test_password));
        EXPECT_EQ(manager2.get_account_count(), 1);
        ASSERT_TRUE(manager2.close_vault());
    }
}

TEST_F(VaultReedSolomonTest, InvalidRedundancy_Rejected) {
    VaultManager manager;
    configure_for_testing(manager);

    // Too low
    EXPECT_FALSE(manager.set_rs_redundancy_percent(0));
    EXPECT_FALSE(manager.set_rs_redundancy_percent(4));

    // Too high
    EXPECT_FALSE(manager.set_rs_redundancy_percent(51));
    EXPECT_FALSE(manager.set_rs_redundancy_percent(100));

    // Valid values should work
    EXPECT_TRUE(manager.set_rs_redundancy_percent(5));
    EXPECT_TRUE(manager.set_rs_redundancy_percent(50));
}

TEST_F(VaultReedSolomonTest, LegacyVault_OpensWithoutRS) {
    VaultManager manager;
    configure_for_testing(manager);

    // Create legacy vault (no RS)
    manager.set_reed_solomon_enabled(false);
    ASSERT_TRUE(manager.create_vault_v2(test_vault_path, test_username, test_password, policy));
    auto account = createAccount("Legacy", "legacy@test.com");
    ASSERT_TRUE(manager.account_manager()->add_account(account));
    ASSERT_TRUE(manager.save_vault());
    ASSERT_TRUE(manager.close_vault());

    // Open with RS-enabled manager (should auto-detect legacy format)
    VaultManager manager2;
    configure_for_testing(manager2);
    manager2.set_reed_solomon_enabled(true);  // Preference for NEW vaults
    ASSERT_TRUE(manager2.open_vault_v2(test_vault_path, test_username, test_password));

    // Verify data
    EXPECT_EQ(manager2.get_account_count(), 1);
    auto* account_out = manager2.account_manager()->get_account(0);
    ASSERT_NE(account_out, nullptr);
    EXPECT_EQ(account_out->account_name(), "Legacy");
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}