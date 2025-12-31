// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file test_account_repository.cc
 * @brief Unit tests for AccountRepository
 */

#include <gtest/gtest.h>
#include "../src/core/repositories/AccountRepository.h"
#include "../src/core/VaultManager.h"
#include <memory>
#include <cstdio>

namespace KeepTower {

/**
 * @class AccountRepositoryTest
 * @brief Test fixture for AccountRepository
 */
class AccountRepositoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary vault for testing
        vault_manager = std::make_unique<VaultManager>();
        temp_vault_path = "/tmp/test_account_repository_vault.ktv";

        // Create and open a test vault
        ASSERT_TRUE(vault_manager->create_vault(temp_vault_path, "test_password"));
        ASSERT_TRUE(vault_manager->open_vault(temp_vault_path, "test_password"));

        // Create repository
        repository = std::make_unique<AccountRepository>(vault_manager.get());

        // Add test accounts
        keeptower::AccountRecord account1;
        account1.set_id("account1");
        account1.set_account_name("Gmail Personal");
        account1.set_user_name("john@gmail.com");
        account1.set_email("john@gmail.com");
        account1.set_password("password123");
        account1.set_is_favorite(true);
        vault_manager->add_account(account1);

        keeptower::AccountRecord account2;
        account2.set_id("account2");
        account2.set_account_name("GitHub Work");
        account2.set_user_name("jdoe");
        account2.set_email("john@company.com");
        account2.set_password("ghp_token");
        account2.set_is_favorite(false);
        vault_manager->add_account(account2);

        keeptower::AccountRecord account3;
        account3.set_id("account3");
        account3.set_account_name("AWS Console");
        account3.set_user_name("admin");
        account3.set_email("admin@company.com");
        account3.set_password("aws_secret");
        account3.set_is_favorite(false);
        vault_manager->add_account(account3);
    }

    void TearDown() override {
        if (vault_manager && vault_manager->is_vault_open()) {
            vault_manager->close_vault();
        }
        // Clean up test file
        std::remove(temp_vault_path.c_str());
    }

    std::unique_ptr<VaultManager> vault_manager;
    std::unique_ptr<AccountRepository> repository;
    std::string temp_vault_path;
};

// =============================================================================
// Construction Tests
// =============================================================================

TEST_F(AccountRepositoryTest, ConstructorThrowsOnNull) {
    EXPECT_THROW(AccountRepository(nullptr), std::invalid_argument);
}

TEST_F(AccountRepositoryTest, IsVaultOpen) {
    EXPECT_TRUE(repository->is_vault_open());

    vault_manager->close_vault();
    EXPECT_FALSE(repository->is_vault_open());
}

// =============================================================================
// Add Account Tests
// =============================================================================

TEST_F(AccountRepositoryTest, AddAccount) {
    keeptower::AccountRecord new_account;
    new_account.set_id("account4");
    new_account.set_account_name("Netflix");
    new_account.set_user_name("john@gmail.com");
    new_account.set_password("netflix_pass");

    auto result = repository->add(new_account);
    ASSERT_TRUE(result.has_value()) << "Add should succeed";

    // Verify account was added
    auto count = repository->count();
    ASSERT_TRUE(count.has_value());
    EXPECT_EQ(*count, 4);

    // Verify we can retrieve it
    auto retrieved = repository->get_by_id("account4");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->account_name(), "Netflix");
}

TEST_F(AccountRepositoryTest, AddAccountWhenVaultClosed) {
    vault_manager->close_vault();

    keeptower::AccountRecord new_account;
    new_account.set_id("account4");
    new_account.set_account_name("Netflix");

    auto result = repository->add(new_account);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), RepositoryError::VAULT_CLOSED);
}

// =============================================================================
// Get Account Tests
// =============================================================================

TEST_F(AccountRepositoryTest, GetAccountByIndex) {
    auto result = repository->get(0);
    ASSERT_TRUE(result.has_value()) << "Get should succeed";
    EXPECT_EQ(result->id(), "account1");
    EXPECT_EQ(result->account_name(), "Gmail Personal");
}

TEST_F(AccountRepositoryTest, GetAccountByInvalidIndex) {
    auto result = repository->get(999);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), RepositoryError::INVALID_INDEX);
}

TEST_F(AccountRepositoryTest, GetAccountById) {
    auto result = repository->get_by_id("account2");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->account_name(), "GitHub Work");
    EXPECT_EQ(result->user_name(), "jdoe");
}

TEST_F(AccountRepositoryTest, GetAccountByNonexistentId) {
    auto result = repository->get_by_id("nonexistent");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), RepositoryError::ACCOUNT_NOT_FOUND);
}

TEST_F(AccountRepositoryTest, GetAccountWhenVaultClosed) {
    vault_manager->close_vault();

    auto result = repository->get(0);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), RepositoryError::VAULT_CLOSED);
}

// =============================================================================
// Get All Accounts Tests
// =============================================================================

TEST_F(AccountRepositoryTest, GetAllAccounts) {
    auto result = repository->get_all();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 3);

    // Verify order and content
    EXPECT_EQ((*result)[0].id(), "account1");
    EXPECT_EQ((*result)[1].id(), "account2");
    EXPECT_EQ((*result)[2].id(), "account3");
}

TEST_F(AccountRepositoryTest, GetAllAccountsWhenVaultClosed) {
    vault_manager->close_vault();

    auto result = repository->get_all();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), RepositoryError::VAULT_CLOSED);
}

// =============================================================================
// Update Account Tests
// =============================================================================

TEST_F(AccountRepositoryTest, UpdateAccount) {
    // Get existing account
    auto account = repository->get(1);
    ASSERT_TRUE(account.has_value());

    // Modify it
    account->set_account_name("GitHub Personal");
    account->set_is_favorite(true);

    // Update via repository
    auto result = repository->update(1, *account);
    ASSERT_TRUE(result.has_value()) << "Update should succeed";

    // Verify update
    auto updated = repository->get(1);
    ASSERT_TRUE(updated.has_value());
    EXPECT_EQ(updated->account_name(), "GitHub Personal");
    EXPECT_TRUE(updated->is_favorite());
}

TEST_F(AccountRepositoryTest, UpdateAccountInvalidIndex) {
    keeptower::AccountRecord account;
    account.set_id("test");
    account.set_account_name("Test");

    auto result = repository->update(999, account);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), RepositoryError::INVALID_INDEX);
}

TEST_F(AccountRepositoryTest, UpdateAccountWhenVaultClosed) {
    vault_manager->close_vault();

    keeptower::AccountRecord account;
    auto result = repository->update(0, account);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), RepositoryError::VAULT_CLOSED);
}

// =============================================================================
// Remove Account Tests
// =============================================================================

TEST_F(AccountRepositoryTest, RemoveAccount) {
    // Verify initial count
    auto count_before = repository->count();
    ASSERT_TRUE(count_before.has_value());
    EXPECT_EQ(*count_before, 3);

    // Remove account at index 1
    auto result = repository->remove(1);
    ASSERT_TRUE(result.has_value()) << "Remove should succeed";

    // Verify count decreased
    auto count_after = repository->count();
    ASSERT_TRUE(count_after.has_value());
    EXPECT_EQ(*count_after, 2);

    // Verify the account is gone
    auto all_accounts = repository->get_all();
    ASSERT_TRUE(all_accounts.has_value());
    EXPECT_EQ(all_accounts->size(), 2);
    // Account at index 1 (GitHub Work) should be gone
    EXPECT_EQ((*all_accounts)[0].id(), "account1");
    EXPECT_EQ((*all_accounts)[1].id(), "account3");
}

TEST_F(AccountRepositoryTest, RemoveAccountInvalidIndex) {
    auto result = repository->remove(999);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), RepositoryError::INVALID_INDEX);
}

TEST_F(AccountRepositoryTest, RemoveAccountWhenVaultClosed) {
    vault_manager->close_vault();

    auto result = repository->remove(0);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), RepositoryError::VAULT_CLOSED);
}

// =============================================================================
// Count Tests
// =============================================================================

TEST_F(AccountRepositoryTest, Count) {
    auto result = repository->count();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 3);
}

TEST_F(AccountRepositoryTest, CountWhenVaultClosed) {
    vault_manager->close_vault();

    auto result = repository->count();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), RepositoryError::VAULT_CLOSED);
}

// =============================================================================
// Permission Tests
// =============================================================================

TEST_F(AccountRepositoryTest, CanView) {
    // For V1 vaults, all valid accounts are viewable
    EXPECT_TRUE(repository->can_view(0));
    EXPECT_TRUE(repository->can_view(1));
    EXPECT_TRUE(repository->can_view(2));
    EXPECT_FALSE(repository->can_view(999));  // Out of bounds
}

TEST_F(AccountRepositoryTest, CanModify) {
    // For V1 vaults, all valid accounts are modifiable
    EXPECT_TRUE(repository->can_modify(0));
    EXPECT_TRUE(repository->can_modify(1));
    EXPECT_TRUE(repository->can_modify(2));
    EXPECT_FALSE(repository->can_modify(999));  // Out of bounds
}

TEST_F(AccountRepositoryTest, CanViewWhenVaultClosed) {
    vault_manager->close_vault();
    EXPECT_FALSE(repository->can_view(0));
}

TEST_F(AccountRepositoryTest, CanModifyWhenVaultClosed) {
    vault_manager->close_vault();
    EXPECT_FALSE(repository->can_modify(0));
}

// =============================================================================
// Find Index Tests
// =============================================================================

TEST_F(AccountRepositoryTest, FindIndexById) {
    auto index = repository->find_index_by_id("account1");
    ASSERT_TRUE(index.has_value());
    EXPECT_EQ(*index, 0);

    index = repository->find_index_by_id("account2");
    ASSERT_TRUE(index.has_value());
    EXPECT_EQ(*index, 1);

    index = repository->find_index_by_id("account3");
    ASSERT_TRUE(index.has_value());
    EXPECT_EQ(*index, 2);
}

TEST_F(AccountRepositoryTest, FindIndexByNonexistentId) {
    auto index = repository->find_index_by_id("nonexistent");
    EXPECT_FALSE(index.has_value());
}

TEST_F(AccountRepositoryTest, FindIndexWhenVaultClosed) {
    vault_manager->close_vault();

    auto index = repository->find_index_by_id("account1");
    EXPECT_FALSE(index.has_value());
}

// =============================================================================
// Error String Conversion Tests
// =============================================================================

TEST_F(AccountRepositoryTest, ErrorToString) {
    EXPECT_EQ(to_string(RepositoryError::VAULT_CLOSED), "Vault is not open");
    EXPECT_EQ(to_string(RepositoryError::ACCOUNT_NOT_FOUND), "Account not found");
    EXPECT_EQ(to_string(RepositoryError::INVALID_INDEX), "Invalid index");
    EXPECT_EQ(to_string(RepositoryError::PERMISSION_DENIED), "Permission denied");
    EXPECT_EQ(to_string(RepositoryError::DUPLICATE_ID), "Duplicate account ID");
    EXPECT_EQ(to_string(RepositoryError::SAVE_FAILED), "Failed to save");
    EXPECT_EQ(to_string(RepositoryError::UNKNOWN_ERROR), "Unknown error");
}

}  // namespace KeepTower

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
