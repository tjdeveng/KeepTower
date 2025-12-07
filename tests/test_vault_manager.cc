// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include <gtest/gtest.h>
#include "VaultManager.h"
#include "VaultError.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

#define EXPECT_RESULT_FAILED(expr) EXPECT_FALSE(expr)
// Helper macros for std::expected testing

class VaultManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for test vaults
        test_dir = fs::temp_directory_path() / "keeptower_tests";
        fs::create_directories(test_dir);
        test_vault_path = (test_dir / "test_vault.vault").string();
        vault_manager = std::make_unique<VaultManager>();
    }

    void TearDown() override {
        // Clean up test files
        vault_manager.reset();
        try {
            fs::remove_all(test_dir);
        } catch (...) {}
    }

    fs::path test_dir;
    std::string test_vault_path;
    std::unique_ptr<VaultManager> vault_manager;
    const Glib::ustring test_password = "TestPassword123!";
};

// ============================================================================
// Vault Creation and Opening Tests
// ============================================================================

TEST_F(VaultManagerTest, CreateVault_Success) {
    ASSERT_TRUE(vault_manager->create_vault(test_vault_path, test_password));
    EXPECT_TRUE(vault_manager->is_vault_open());
    EXPECT_EQ(vault_manager->get_current_vault_path(), test_vault_path);
    EXPECT_TRUE(fs::exists(test_vault_path));
}

TEST_F(VaultManagerTest, CreateVault_FileHasRestrictivePermissions) {
    ASSERT_TRUE(vault_manager->create_vault(test_vault_path, test_password));

    auto perms = fs::status(test_vault_path).permissions();
    // Check owner read/write only (0600)
    EXPECT_TRUE((perms & fs::perms::owner_read) != fs::perms::none);
    EXPECT_TRUE((perms & fs::perms::owner_write) != fs::perms::none);
    EXPECT_TRUE((perms & fs::perms::group_read) == fs::perms::none);
    EXPECT_TRUE((perms & fs::perms::others_read) == fs::perms::none);
}

TEST_F(VaultManagerTest, OpenVault_WithCorrectPassword_Success) {
    // Create vault first
    ASSERT_TRUE(vault_manager->create_vault(test_vault_path, test_password));
    ASSERT_TRUE(vault_manager->close_vault());

    // Open with correct password
    ASSERT_TRUE(vault_manager->open_vault(test_vault_path, test_password));
    EXPECT_TRUE(vault_manager->is_vault_open());
}

TEST_F(VaultManagerTest, OpenVault_WithWrongPassword_Fails) {
    // Create vault
    ASSERT_TRUE(vault_manager->create_vault(test_vault_path, test_password));
    ASSERT_TRUE(vault_manager->close_vault());

    // Try to open with wrong password
    EXPECT_RESULT_FAILED(vault_manager->open_vault(test_vault_path, "WrongPassword"));
    EXPECT_FALSE(vault_manager->is_vault_open());
}

TEST_F(VaultManagerTest, OpenVault_NonExistentFile_Fails) {
    EXPECT_RESULT_FAILED(vault_manager->open_vault("/nonexistent/vault.vault", test_password));
    EXPECT_FALSE(vault_manager->is_vault_open());
}

TEST_F(VaultManagerTest, OpenVault_CorruptedFile_Fails) {
    // Create a corrupted vault file
    std::ofstream file(test_vault_path, std::ios::binary);
    file << "This is not a valid vault file";
    file.close();

    EXPECT_RESULT_FAILED(vault_manager->open_vault(test_vault_path, test_password));
    EXPECT_FALSE(vault_manager->is_vault_open());
}

TEST_F(VaultManagerTest, CloseVault_ClearsState) {
    ASSERT_TRUE(vault_manager->create_vault(test_vault_path, test_password));
    ASSERT_TRUE(vault_manager->is_vault_open());

    ASSERT_TRUE(vault_manager->close_vault());
    EXPECT_FALSE(vault_manager->is_vault_open());
    EXPECT_EQ(vault_manager->get_current_vault_path(), "");
}

// ============================================================================
// Encryption/Decryption Round-Trip Tests
// ============================================================================

TEST_F(VaultManagerTest, EncryptionDecryption_RoundTrip) {
    ASSERT_TRUE(vault_manager->create_vault(test_vault_path, test_password));

    // Add test account
    keeptower::AccountRecord account;
    account.set_id("test-id-001");
    account.set_account_name("Test Account");
    account.set_user_name("testuser");
    account.set_password("SecretPassword123!");
    account.set_email("test@example.com");
    account.set_website("https://example.com");
    account.set_notes("Test notes");
    account.set_created_at(std::time(nullptr));
    account.set_modified_at(std::time(nullptr));

    ASSERT_TRUE(vault_manager->add_account(account));
    ASSERT_TRUE(vault_manager->save_vault());
    ASSERT_TRUE(vault_manager->close_vault());

    // Reopen and verify
    ASSERT_TRUE(vault_manager->open_vault(test_vault_path, test_password));
    auto accounts = vault_manager->get_all_accounts();

    ASSERT_EQ(accounts.size(), 1);
    EXPECT_EQ(accounts[0].id(), "test-id-001");
    EXPECT_EQ(accounts[0].account_name(), "Test Account");
    EXPECT_EQ(accounts[0].user_name(), "testuser");
    EXPECT_EQ(accounts[0].password(), "SecretPassword123!");
    EXPECT_EQ(accounts[0].email(), "test@example.com");
    EXPECT_EQ(accounts[0].website(), "https://example.com");
    EXPECT_EQ(accounts[0].notes(), "Test notes");
}

TEST_F(VaultManagerTest, EncryptionDecryption_MultipleAccounts) {
    ASSERT_TRUE(vault_manager->create_vault(test_vault_path, test_password));

    // Add multiple accounts
    for (int i = 0; i < 10; i++) {
        keeptower::AccountRecord account;
        account.set_id("id-" + std::to_string(i));
        account.set_account_name("Account " + std::to_string(i));
        account.set_user_name("user" + std::to_string(i));
        account.set_password("pass" + std::to_string(i));
        account.set_created_at(std::time(nullptr));
        account.set_modified_at(std::time(nullptr));

        ASSERT_TRUE(vault_manager->add_account(account));
    }

    ASSERT_TRUE(vault_manager->save_vault());
    ASSERT_TRUE(vault_manager->close_vault());

    // Reopen and verify all accounts
    ASSERT_TRUE(vault_manager->open_vault(test_vault_path, test_password));
    auto accounts = vault_manager->get_all_accounts();

    ASSERT_EQ(accounts.size(), 10);
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(accounts[i].id(), "id-" + std::to_string(i));
        EXPECT_EQ(accounts[i].account_name(), "Account " + std::to_string(i));
    }
}

TEST_F(VaultManagerTest, EncryptionDecryption_EmptyVault) {
    ASSERT_TRUE(vault_manager->create_vault(test_vault_path, test_password));
    ASSERT_TRUE(vault_manager->save_vault());
    ASSERT_TRUE(vault_manager->close_vault());

    // Reopen empty vault
    ASSERT_TRUE(vault_manager->open_vault(test_vault_path, test_password));
    auto accounts = vault_manager->get_all_accounts();
    EXPECT_EQ(accounts.size(), 0);
}

// ============================================================================
// Account CRUD Operations Tests
// ============================================================================

TEST_F(VaultManagerTest, AddAccount_Success) {
    ASSERT_TRUE(vault_manager->create_vault(test_vault_path, test_password));

    keeptower::AccountRecord account;
    account.set_id("test-id");
    account.set_account_name("Test");

    ASSERT_TRUE(vault_manager->add_account(account));
    EXPECT_EQ(vault_manager->get_account_count(), 1);
}

TEST_F(VaultManagerTest, AddAccount_WithoutOpenVault_Fails) {
    keeptower::AccountRecord account;
    account.set_id("test-id");

    EXPECT_FALSE(vault_manager->add_account(account));
}

TEST_F(VaultManagerTest, GetAccount_ValidIndex_Success) {
    ASSERT_TRUE(vault_manager->create_vault(test_vault_path, test_password));

    keeptower::AccountRecord account;
    account.set_id("test-id");
    account.set_account_name("Test Account");
    ASSERT_TRUE(vault_manager->add_account(account));

    const auto* retrieved = vault_manager->get_account(0);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->id(), "test-id");
    EXPECT_EQ(retrieved->account_name(), "Test Account");
}

TEST_F(VaultManagerTest, GetAccount_InvalidIndex_ReturnsNull) {
    ASSERT_TRUE(vault_manager->create_vault(test_vault_path, test_password));

    EXPECT_EQ(vault_manager->get_account(0), nullptr);
    EXPECT_EQ(vault_manager->get_account(999), nullptr);
}

TEST_F(VaultManagerTest, UpdateAccount_ValidIndex_Success) {
    ASSERT_TRUE(vault_manager->create_vault(test_vault_path, test_password));

    keeptower::AccountRecord account;
    account.set_id("test-id");
    account.set_account_name("Original Name");
    ASSERT_TRUE(vault_manager->add_account(account));

    // Update account
    keeptower::AccountRecord updated;
    updated.set_id("test-id");
    updated.set_account_name("Updated Name");
    updated.set_user_name("newuser");

    ASSERT_TRUE(vault_manager->update_account(0, updated));

    const auto* retrieved = vault_manager->get_account(0);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->account_name(), "Updated Name");
    EXPECT_EQ(retrieved->user_name(), "newuser");
}

TEST_F(VaultManagerTest, UpdateAccount_InvalidIndex_Fails) {
    ASSERT_TRUE(vault_manager->create_vault(test_vault_path, test_password));

    keeptower::AccountRecord account;
    account.set_id("test-id");

    EXPECT_FALSE(vault_manager->update_account(0, account));
    EXPECT_FALSE(vault_manager->update_account(999, account));
}

TEST_F(VaultManagerTest, DeleteAccount_ValidIndex_Success) {
    ASSERT_TRUE(vault_manager->create_vault(test_vault_path, test_password));

    // Add two accounts
    keeptower::AccountRecord account1;
    account1.set_id("id-1");
    account1.set_account_name("Account 1");
    ASSERT_TRUE(vault_manager->add_account(account1));

    keeptower::AccountRecord account2;
    account2.set_id("id-2");
    account2.set_account_name("Account 2");
    ASSERT_TRUE(vault_manager->add_account(account2));

    EXPECT_EQ(vault_manager->get_account_count(), 2);

    // Delete first account
    ASSERT_TRUE(vault_manager->delete_account(0));
    EXPECT_EQ(vault_manager->get_account_count(), 1);

    // Verify remaining account
    const auto* remaining = vault_manager->get_account(0);
    ASSERT_NE(remaining, nullptr);
    EXPECT_EQ(remaining->id(), "id-2");
}

TEST_F(VaultManagerTest, DeleteAccount_InvalidIndex_Fails) {
    ASSERT_TRUE(vault_manager->create_vault(test_vault_path, test_password));

    EXPECT_FALSE(vault_manager->delete_account(0));
    EXPECT_FALSE(vault_manager->delete_account(999));
}

TEST_F(VaultManagerTest, GetAllAccounts_ReturnsCorrectCount) {
    ASSERT_TRUE(vault_manager->create_vault(test_vault_path, test_password));

    auto accounts = vault_manager->get_all_accounts();
    EXPECT_EQ(accounts.size(), 0);

    keeptower::AccountRecord account;
    account.set_id("test-id");
    ASSERT_TRUE(vault_manager->add_account(account));

    accounts = vault_manager->get_all_accounts();
    EXPECT_EQ(accounts.size(), 1);
}

// ============================================================================
// Modification Tracking Tests
// ============================================================================

TEST_F(VaultManagerTest, ModificationTracking_AfterAddAccount) {
    ASSERT_TRUE(vault_manager->create_vault(test_vault_path, test_password));
    ASSERT_TRUE(vault_manager->save_vault());
    EXPECT_FALSE(vault_manager->is_modified());

    keeptower::AccountRecord account;
    account.set_id("test-id");
    ASSERT_TRUE(vault_manager->add_account(account));

    EXPECT_TRUE(vault_manager->is_modified());
}

TEST_F(VaultManagerTest, ModificationTracking_AfterUpdate) {
    ASSERT_TRUE(vault_manager->create_vault(test_vault_path, test_password));

    keeptower::AccountRecord account;
    account.set_id("test-id");
    ASSERT_TRUE(vault_manager->add_account(account));
    ASSERT_TRUE(vault_manager->save_vault());
    EXPECT_FALSE(vault_manager->is_modified());

    keeptower::AccountRecord updated;
    updated.set_id("test-id");
    updated.set_account_name("Updated");
    ASSERT_TRUE(vault_manager->update_account(0, updated));

    EXPECT_TRUE(vault_manager->is_modified());
}

TEST_F(VaultManagerTest, ModificationTracking_AfterDelete) {
    ASSERT_TRUE(vault_manager->create_vault(test_vault_path, test_password));

    keeptower::AccountRecord account;
    account.set_id("test-id");
    ASSERT_TRUE(vault_manager->add_account(account));
    ASSERT_TRUE(vault_manager->save_vault());
    EXPECT_FALSE(vault_manager->is_modified());

    ASSERT_TRUE(vault_manager->delete_account(0));
    EXPECT_TRUE(vault_manager->is_modified());
}

TEST_F(VaultManagerTest, ModificationTracking_ClearedAfterSave) {
    ASSERT_TRUE(vault_manager->create_vault(test_vault_path, test_password));

    keeptower::AccountRecord account;
    account.set_id("test-id");
    ASSERT_TRUE(vault_manager->add_account(account));
    EXPECT_TRUE(vault_manager->is_modified());

    ASSERT_TRUE(vault_manager->save_vault());
    EXPECT_FALSE(vault_manager->is_modified());
}

// ============================================================================
// Security Tests
// ============================================================================

TEST_F(VaultManagerTest, Security_DifferentPasswordsProduceDifferentCiphertext) {
    // Create two vaults with same data but different passwords
    std::string vault1_path = (test_dir / "vault1.vault").string();
    std::string vault2_path = (test_dir / "vault2.vault").string();

    // Vault 1
    ASSERT_TRUE(vault_manager->create_vault(vault1_path, "password1"));
    keeptower::AccountRecord account;
    account.set_id("same-id");
    account.set_account_name("Same Data");
    ASSERT_TRUE(vault_manager->add_account(account));
    ASSERT_TRUE(vault_manager->save_vault());
    ASSERT_TRUE(vault_manager->close_vault());

    // Vault 2
    ASSERT_TRUE(vault_manager->create_vault(vault2_path, "password2"));
    ASSERT_TRUE(vault_manager->add_account(account));
    ASSERT_TRUE(vault_manager->save_vault());
    ASSERT_TRUE(vault_manager->close_vault());

    // Compare file contents - they should be different
    std::ifstream f1(vault1_path, std::ios::binary);
    std::ifstream f2(vault2_path, std::ios::binary);

    std::vector<uint8_t> data1((std::istreambuf_iterator<char>(f1)),
                                std::istreambuf_iterator<char>());
    std::vector<uint8_t> data2((std::istreambuf_iterator<char>(f2)),
                                std::istreambuf_iterator<char>());

    EXPECT_NE(data1, data2);
}

TEST_F(VaultManagerTest, Security_SaltIsRandomEachTime) {
    std::string vault1_path = (test_dir / "vault1.vault").string();
    std::string vault2_path = (test_dir / "vault2.vault").string();

    // Create two vaults with same password
    ASSERT_TRUE(vault_manager->create_vault(vault1_path, test_password));
    ASSERT_TRUE(vault_manager->close_vault());

    ASSERT_TRUE(vault_manager->create_vault(vault2_path, test_password));
    ASSERT_TRUE(vault_manager->close_vault());

    // Read and compare salts (first 32 bytes)
    std::ifstream f1(vault1_path, std::ios::binary);
    std::ifstream f2(vault2_path, std::ios::binary);

    std::vector<uint8_t> salt1(32);
    std::vector<uint8_t> salt2(32);

    f1.read(reinterpret_cast<char*>(salt1.data()), 32);
    f2.read(reinterpret_cast<char*>(salt2.data()), 32);

    EXPECT_NE(salt1, salt2);
}

// ============================================================================
// Atomic Save Tests
// ============================================================================

TEST_F(VaultManagerTest, AtomicSave_TempFileCleanedUpOnSuccess) {
    ASSERT_TRUE(vault_manager->create_vault(test_vault_path, test_password));
    ASSERT_TRUE(vault_manager->save_vault());

    std::string temp_path = test_vault_path + ".tmp";
    EXPECT_FALSE(fs::exists(temp_path));
}

TEST_F(VaultManagerTest, AtomicSave_PreservesDataOnMultipleSaves) {
    ASSERT_TRUE(vault_manager->create_vault(test_vault_path, test_password));

    keeptower::AccountRecord account;
    account.set_id("test-id");
    account.set_account_name("Original");
    ASSERT_TRUE(vault_manager->add_account(account));
    ASSERT_TRUE(vault_manager->save_vault());

    // Modify and save again
    auto* acc = vault_manager->get_account_mutable(0);
    ASSERT_NE(acc, nullptr);
    acc->set_account_name("Modified");
    ASSERT_TRUE(vault_manager->save_vault());
    ASSERT_TRUE(vault_manager->close_vault());

    // Verify final state
    ASSERT_TRUE(vault_manager->open_vault(test_vault_path, test_password));
    const auto* retrieved = vault_manager->get_account(0);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->account_name(), "Modified");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
