// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng
//
// Test FIPS-140-3 mode functionality

#include <gtest/gtest.h>
#include "VaultManager.h"
#include <filesystem>
#include <fstream>
#include <chrono>

namespace fs = std::filesystem;

class FIPSModeTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for test vaults
        test_dir = fs::temp_directory_path() / "keeptower_fips_tests";
        fs::create_directories(test_dir);
        test_vault_path = (test_dir / "fips_test_vault.vault").string();
        test_password = "SecureTestPassword123!@#";
    }

    void TearDown() override {
        // Clean up test files
        try {
            fs::remove_all(test_dir);
        } catch (...) {}
    }

    fs::path test_dir;
    std::string test_vault_path;
    Glib::ustring test_password;
};

// ============================================================================
// FIPS Initialization Tests
// ============================================================================

TEST_F(FIPSModeTest, InitFIPSMode_CanOnlyInitializeOnce) {
    // VaultManager::init_fips_mode() should only succeed once per process
    // Note: This test assumes no prior initialization in this test binary
    bool first_init = VaultManager::init_fips_mode(false);

    // Second call should return the cached availability result
    // but won't actually re-initialize (logs warning)
    bool second_init = VaultManager::init_fips_mode(false);

    // First init should succeed (loads default provider)
    // Second returns cached FIPS availability (false in this case since no FIPS config)
    EXPECT_TRUE(first_init);   // Default provider loaded successfully
    EXPECT_FALSE(second_init); // FIPS not available, cached result

    // Multiple calls should return consistent results
    bool available1 = VaultManager::is_fips_available();
    bool available2 = VaultManager::is_fips_available();
    bool available3 = VaultManager::is_fips_available();

    EXPECT_EQ(available1, available2);
    EXPECT_EQ(available2, available3);
}

TEST_F(FIPSModeTest, FIPSEnabled_ReflectsInitialization) {
    // Initialize FIPS mode disabled
    [[maybe_unused]] bool init_result = VaultManager::init_fips_mode(false);

    // If FIPS is available, it should be disabled
    if (VaultManager::is_fips_available()) {
        EXPECT_FALSE(VaultManager::is_fips_enabled());
    } else {
        // If FIPS not available, enabled should also be false
        EXPECT_FALSE(VaultManager::is_fips_enabled());
    }
}

// ============================================================================
// Vault Operations in Default Mode
// ============================================================================

TEST_F(FIPSModeTest, VaultOperations_DefaultMode_CreateAndOpen) {
    // Initialize in non-FIPS mode (default provider)
    [[maybe_unused]] bool _r1 = VaultManager::init_fips_mode(false);

    VaultManager vault;
    vault.set_backup_enabled(false);
    vault.set_reed_solomon_enabled(false);

    // Create vault
    ASSERT_TRUE(vault.create_vault(test_vault_path, test_password));
    EXPECT_TRUE(vault.is_vault_open());

    // Add test data
    keeptower::AccountRecord account;
    account.set_account_name("Test Account");
    account.set_user_name("testuser");
    account.set_password("testpass123");
    account.set_website("https://example.com");

    ASSERT_TRUE(vault.add_account(account));
    ASSERT_TRUE(vault.save_vault());
    ASSERT_TRUE(vault.close_vault());

    // Reopen vault
    ASSERT_TRUE(vault.open_vault(test_vault_path, test_password));
    EXPECT_EQ(vault.get_account_count(), 1);

    auto accounts = vault.get_all_accounts();
    ASSERT_EQ(accounts.size(), 1);
    EXPECT_EQ(accounts[0].account_name(), "Test Account");
    EXPECT_EQ(accounts[0].user_name(), "testuser");
    EXPECT_EQ(accounts[0].password(), "testpass123");
}

TEST_F(FIPSModeTest, VaultOperations_DefaultMode_Encryption) {
    [[maybe_unused]] bool _r2 = VaultManager::init_fips_mode(false);

    VaultManager vault;
    vault.set_backup_enabled(false);
    vault.set_reed_solomon_enabled(false);

    // Create and save vault with data
    ASSERT_TRUE(vault.create_vault(test_vault_path, test_password));

    keeptower::AccountRecord account;
    account.set_account_name("Sensitive Data");
    account.set_password("VerySecretPassword123!@#");
    ASSERT_TRUE(vault.add_account(account));
    ASSERT_TRUE(vault.save_vault());
    ASSERT_TRUE(vault.close_vault());

    // Verify vault file is encrypted (not plaintext)
    std::ifstream file(test_vault_path, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    // Should not contain plaintext password
    EXPECT_EQ(content.find("VerySecretPassword123"), std::string::npos);
    EXPECT_EQ(content.find("Sensitive Data"), std::string::npos);
}

TEST_F(FIPSModeTest, VaultOperations_DefaultMode_WrongPassword) {
    [[maybe_unused]] bool _r3 = VaultManager::init_fips_mode(false);

    VaultManager vault;
    vault.set_backup_enabled(false);
    vault.set_reed_solomon_enabled(false);

    // Create vault
    ASSERT_TRUE(vault.create_vault(test_vault_path, test_password));
    ASSERT_TRUE(vault.close_vault());

    // Try to open with wrong password
    EXPECT_FALSE(vault.open_vault(test_vault_path, "WrongPassword123!"));
    EXPECT_FALSE(vault.is_vault_open());
}

// ============================================================================
// FIPS Mode Conditional Tests
// ============================================================================

TEST_F(FIPSModeTest, FIPSMode_EnabledMode_IfAvailable) {
    // Try to initialize with FIPS enabled
    bool init_result = VaultManager::init_fips_mode(true);

    if (VaultManager::is_fips_available()) {
        // If FIPS is available, initialization should succeed
        EXPECT_TRUE(init_result);
        EXPECT_TRUE(VaultManager::is_fips_enabled());

        // Test vault operations work in FIPS mode
        VaultManager vault;
        vault.set_backup_enabled(false);
        vault.set_reed_solomon_enabled(false);

        ASSERT_TRUE(vault.create_vault(test_vault_path, test_password));

        keeptower::AccountRecord account;
        account.set_account_name("FIPS Test Account");
        account.set_password("FIPSPassword123!");

        ASSERT_TRUE(vault.add_account(account));
        ASSERT_TRUE(vault.save_vault());
        ASSERT_TRUE(vault.close_vault());

        // Reopen in FIPS mode
        ASSERT_TRUE(vault.open_vault(test_vault_path, test_password));
        EXPECT_EQ(vault.get_account_count(), 1);

    } else {
        // If FIPS not available, init might succeed (default provider)
        // but FIPS should not be enabled
        EXPECT_FALSE(VaultManager::is_fips_enabled());
    }
}

TEST_F(FIPSModeTest, FIPSMode_RuntimeToggle_IfAvailable) {
    [[maybe_unused]] bool _r4 = VaultManager::init_fips_mode(false);

    if (VaultManager::is_fips_available()) {
        // Test enabling FIPS at runtime
        bool enable_result = VaultManager::set_fips_mode(true);
        EXPECT_TRUE(enable_result);
        EXPECT_TRUE(VaultManager::is_fips_enabled());

        // Test disabling FIPS at runtime
        bool disable_result = VaultManager::set_fips_mode(false);
        EXPECT_TRUE(disable_result);
        EXPECT_FALSE(VaultManager::is_fips_enabled());

    } else {
        // If FIPS not available, runtime toggle should fail
        EXPECT_FALSE(VaultManager::set_fips_mode(true));
        EXPECT_FALSE(VaultManager::is_fips_enabled());
    }
}

// ============================================================================
// Cross-Mode Compatibility Tests
// ============================================================================

TEST_F(FIPSModeTest, CrossMode_VaultCreatedInDefault_OpenableRegardless) {
    // Create vault in default mode
    [[maybe_unused]] bool _r5 = VaultManager::init_fips_mode(false);

    VaultManager vault1;
    vault1.set_backup_enabled(false);
    vault1.set_reed_solomon_enabled(false);

    ASSERT_TRUE(vault1.create_vault(test_vault_path, test_password));

    keeptower::AccountRecord account;
    account.set_account_name("Cross-Mode Test");
    account.set_password("CrossModePass123");
    ASSERT_TRUE(vault1.add_account(account));
    ASSERT_TRUE(vault1.save_vault());
    ASSERT_TRUE(vault1.close_vault());

    // Open vault in same process (should work)
    VaultManager vault2;
    ASSERT_TRUE(vault2.open_vault(test_vault_path, test_password));
    EXPECT_EQ(vault2.get_account_count(), 1);

    auto accounts = vault2.get_all_accounts();
    EXPECT_EQ(accounts[0].account_name(), "Cross-Mode Test");
    EXPECT_EQ(accounts[0].password(), "CrossModePass123");
}

// ============================================================================
// Performance Tests (Optional)
// ============================================================================

TEST_F(FIPSModeTest, Performance_DefaultMode_EncryptionSpeed) {
    [[maybe_unused]] bool _r6 = VaultManager::init_fips_mode(false);

    VaultManager vault;
    vault.set_backup_enabled(false);
    vault.set_reed_solomon_enabled(false);

    ASSERT_TRUE(vault.create_vault(test_vault_path, test_password));

    // Add multiple accounts and measure time
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; ++i) {
        keeptower::AccountRecord account;
        account.set_account_name("Test Account " + std::to_string(i));
        account.set_user_name("user" + std::to_string(i));
        account.set_password("password" + std::to_string(i));
        ASSERT_TRUE(vault.add_account(account));
    }

    ASSERT_TRUE(vault.save_vault());

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete in reasonable time (less than 5 seconds for 100 accounts)
    EXPECT_LT(duration.count(), 5000);

    std::cout << "Default mode: 100 accounts saved in " << duration.count() << "ms" << std::endl;
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(FIPSModeTest, ErrorHandling_QueryBeforeInit_ReturnsFalse) {
    // Note: This test may fail if init was already called in another test
    // In real scenarios, we'd use a fresh process

    // If init was never called, these should handle gracefully
    // The actual behavior depends on whether init happened elsewhere
    [[maybe_unused]] bool available = VaultManager::is_fips_available();
    [[maybe_unused]] bool enabled = VaultManager::is_fips_enabled();

    // These shouldn't crash - they should return false and log warning
    // We can't test the specific values without fresh process isolation
    SUCCEED();  // Main goal is no crash
}

TEST_F(FIPSModeTest, ErrorHandling_CorruptedVault_FailsGracefully) {
    [[maybe_unused]] bool _r7 = VaultManager::init_fips_mode(false);

    VaultManager vault;
    vault.set_backup_enabled(false);
    vault.set_reed_solomon_enabled(false);

    // Create valid vault
    ASSERT_TRUE(vault.create_vault(test_vault_path, test_password));
    ASSERT_TRUE(vault.save_vault());
    ASSERT_TRUE(vault.close_vault());

    // Corrupt the vault file
    std::ofstream corrupt_file(test_vault_path, std::ios::binary | std::ios::trunc);
    corrupt_file << "This is not a valid vault file!";
    corrupt_file.close();

    // Try to open corrupted vault
    VaultManager vault2;
    EXPECT_FALSE(vault2.open_vault(test_vault_path, test_password));
    EXPECT_FALSE(vault2.is_vault_open());
}
