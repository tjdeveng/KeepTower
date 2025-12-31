// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file test_account_service.cc
 * @brief Comprehensive tests for AccountService business logic
 *
 * Tests Phase 3 service layer implementation including:
 * - CRUD operations with validation
 * - Field length limits
 * - Email validation
 * - Duplicate name detection
 * - Search functionality
 * - Tag filtering
 * - Error handling
 */

#include <gtest/gtest.h>
#include "../src/core/services/AccountService.h"
#include "../src/core/repositories/AccountRepository.h"
#include "../src/core/VaultManager.h"
#include <filesystem>
#include <chrono>

using namespace KeepTower;
namespace fs = std::filesystem;

class AccountServiceTest : public ::testing::Test {
protected:
    std::unique_ptr<VaultManager> vault_manager;
    std::unique_ptr<AccountRepository> account_repo;
    std::unique_ptr<AccountService> account_service;
    std::string test_vault_path;
    const std::string test_password = "TestPassword123!";

    void SetUp() override {
        // Create unique vault for each test
        vault_manager = std::make_unique<VaultManager>();
        test_vault_path = "test_account_service_" +
                         std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) +
                         ".vault";

        ASSERT_TRUE(vault_manager->create_vault(test_vault_path, test_password))
            << "Failed to create test vault";

        // Initialize repository and service
        account_repo = std::make_unique<AccountRepository>(vault_manager.get());
        account_service = std::make_unique<AccountService>(account_repo.get());
    }

    void TearDown() override {
        // Clean up
        account_service.reset();
        account_repo.reset();

        if (vault_manager->is_vault_open()) {
            (void)vault_manager->close_vault();
        }
        vault_manager.reset();

        // Remove test files
        try {
            if (fs::exists(test_vault_path)) {
                fs::remove(test_vault_path);
            }
            if (fs::exists(test_vault_path + ".backup")) {
                fs::remove(test_vault_path + ".backup");
            }
        } catch (...) {
            // Ignore cleanup errors
        }
    }

    // Helper to create valid test account
    keeptower::AccountRecord create_test_account(const std::string& suffix = "") {
        keeptower::AccountRecord account;
        account.set_id("test-id" + suffix);
        account.set_account_name("Test Account" + suffix);
        account.set_user_name("testuser" + suffix);
        account.set_password("testpass123");
        account.set_email("test" + suffix + "@example.com");
        account.set_website("https://example.com");
        account.set_notes("Test notes");
        return account;
    }
};

// ============================================================================
// CRUD Operations Tests
// ============================================================================

TEST_F(AccountServiceTest, CreateAccount_ValidAccount_Success) {
    auto account = create_test_account();

    auto result = account_service->create_account(account);

    ASSERT_TRUE(result.has_value()) << "Failed to create valid account";
    EXPECT_FALSE(result.value().empty()) << "Account ID should not be empty";
}

TEST_F(AccountServiceTest, CreateAccount_EmptyName_Fails) {
    auto account = create_test_account();
    account.set_account_name("");

    auto result = account_service->create_account(account);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ServiceError::VALIDATION_FAILED);
}

TEST_F(AccountServiceTest, GetAccount_ValidIndex_Success) {
    auto account = create_test_account();
    auto create_result = account_service->create_account(account);
    ASSERT_TRUE(create_result.has_value());

    auto get_result = account_service->get_account(0);

    ASSERT_TRUE(get_result.has_value());
    EXPECT_EQ(get_result.value().account_name(), account.account_name());
}

TEST_F(AccountServiceTest, GetAccount_InvalidIndex_Fails) {
    auto result = account_service->get_account(999);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ServiceError::INVALID_INDEX);
}

TEST_F(AccountServiceTest, GetAccountById_ExistingId_Success) {
    auto account = create_test_account();
    auto create_result = account_service->create_account(account);
    ASSERT_TRUE(create_result.has_value());
    std::string account_id = create_result.value();

    auto get_result = account_service->get_account_by_id(account_id);

    ASSERT_TRUE(get_result.has_value());
    EXPECT_EQ(get_result.value().id(), account_id);
}

TEST_F(AccountServiceTest, GetAccountById_NonExistentId_Fails) {
    auto result = account_service->get_account_by_id("nonexistent-id");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ServiceError::ACCOUNT_NOT_FOUND);
}

TEST_F(AccountServiceTest, GetAllAccounts_MultipleAccounts_Success) {
    // Create multiple accounts
    for (int i = 0; i < 3; ++i) {
        auto account = create_test_account(std::to_string(i));
        ASSERT_TRUE(account_service->create_account(account).has_value());
    }

    auto result = account_service->get_all_accounts();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 3);
}

TEST_F(AccountServiceTest, UpdateAccount_ValidChanges_Success) {
    auto account = create_test_account();
    auto create_result = account_service->create_account(account);
    ASSERT_TRUE(create_result.has_value());

    auto get_result = account_service->get_account(0);
    ASSERT_TRUE(get_result.has_value());
    auto updated_account = get_result.value();
    updated_account.set_user_name("newusername");

    auto update_result = account_service->update_account(0, updated_account);
    ASSERT_TRUE(create_result.has_value());

    auto delete_result = account_service->delete_account(0);

    ASSERT_TRUE(delete_result.has_value());
    auto count_result = account_service->count();
    ASSERT_TRUE(count_result.has_value());
    EXPECT_EQ(count_result.value(), 0);
}

TEST_F(AccountServiceTest, DeleteAccount_InvalidIndex_Fails) {
    auto result = account_service->delete_account(999);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ServiceError::INVALID_INDEX);
}

// ============================================================================
// Field Length Validation Tests
// ============================================================================

TEST_F(AccountServiceTest, CreateAccount_AccountNameTooLong_Fails) {
    auto account = create_test_account();
    account.set_account_name(std::string(MAX_ACCOUNT_NAME_LENGTH + 1, 'x'));

    auto result = account_service->create_account(account);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ServiceError::FIELD_TOO_LONG);
}

TEST_F(AccountServiceTest, CreateAccount_UserNameTooLong_Fails) {
    auto account = create_test_account();
    account.set_user_name(std::string(MAX_USERNAME_LENGTH + 1, 'x'));

    auto result = account_service->create_account(account);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ServiceError::FIELD_TOO_LONG);
}

TEST_F(AccountServiceTest, CreateAccount_PasswordTooLong_Fails) {
    auto account = create_test_account();
    account.set_password(std::string(MAX_PASSWORD_LENGTH + 1, 'x'));

    auto result = account_service->create_account(account);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ServiceError::FIELD_TOO_LONG);
}

TEST_F(AccountServiceTest, CreateAccount_EmailTooLong_Fails) {
    auto account = create_test_account();
    account.set_email(std::string(MAX_EMAIL_LENGTH + 1, 'x'));

    auto result = account_service->create_account(account);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ServiceError::FIELD_TOO_LONG);
}

TEST_F(AccountServiceTest, CreateAccount_WebsiteTooLong_Fails) {
    auto account = create_test_account();
    account.set_website(std::string(MAX_WEBSITE_LENGTH + 1, 'x'));

    auto result = account_service->create_account(account);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ServiceError::FIELD_TOO_LONG);
}

TEST_F(AccountServiceTest, CreateAccount_NotesTooLong_Fails) {
    auto account = create_test_account();
    account.set_notes(std::string(MAX_NOTES_LENGTH + 1, 'x'));

    auto result = account_service->create_account(account);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ServiceError::FIELD_TOO_LONG);
}

TEST_F(AccountServiceTest, CreateAccount_MaxLengthFields_Success) {
    auto account = create_test_account();
    account.set_account_name(std::string(MAX_ACCOUNT_NAME_LENGTH, 'a'));
    account.set_user_name(std::string(MAX_USERNAME_LENGTH, 'b'));
    account.set_password(std::string(MAX_PASSWORD_LENGTH, 'c'));
    account.set_email("test@" + std::string(MAX_EMAIL_LENGTH - 15, 'd') + ".com"); // Valid email at max
    account.set_website(std::string(MAX_WEBSITE_LENGTH, 'e'));
    account.set_notes(std::string(MAX_NOTES_LENGTH, 'f'));

    auto result = account_service->create_account(account);

    ASSERT_TRUE(result.has_value()) << "Max length fields should be accepted";
}

// ============================================================================
// Email Validation Tests
// ============================================================================

TEST_F(AccountServiceTest, CreateAccount_ValidEmail_Success) {
    auto account = create_test_account();
    account.set_email("user@example.com");

    auto result = account_service->create_account(account);

    ASSERT_TRUE(result.has_value());
}

TEST_F(AccountServiceTest, CreateAccount_EmptyEmail_Success) {
    auto account = create_test_account();
    account.set_email("");

    auto result = account_service->create_account(account);

    ASSERT_TRUE(result.has_value()) << "Empty email should be allowed";
}

TEST_F(AccountServiceTest, CreateAccount_InvalidEmailNoAt_Fails) {
    auto account = create_test_account();
    account.set_email("userexample.com");

    auto result = account_service->create_account(account);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ServiceError::INVALID_EMAIL);
}

TEST_F(AccountServiceTest, CreateAccount_InvalidEmailNoDomain_Fails) {
    auto account = create_test_account();
    account.set_email("user@");

    auto result = account_service->create_account(account);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ServiceError::INVALID_EMAIL);
}

TEST_F(AccountServiceTest, CreateAccount_InvalidEmailNoTLD_Fails) {
    auto account = create_test_account();
    account.set_email("user@example");

    auto result = account_service->create_account(account);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ServiceError::INVALID_EMAIL);
}

TEST_F(AccountServiceTest, CreateAccount_ComplexValidEmail_Success) {
    auto account = create_test_account();
    account.set_email("user.name+tag@sub.example.co.uk");

    auto result = account_service->create_account(account);

    ASSERT_TRUE(result.has_value());
}

// ============================================================================
// Duplicate Name Detection Tests
// ============================================================================

TEST_F(AccountServiceTest, CreateAccount_DuplicateName_Fails) {
    auto account1 = create_test_account();
    auto create_result1 = account_service->create_account(account1);
    ASSERT_TRUE(create_result1.has_value());

    auto account2 = create_test_account("2");
    account2.set_account_name(account1.account_name()); // Same name

    auto result = account_service->create_account(account2);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ServiceError::DUPLICATE_NAME);
}

TEST_F(AccountServiceTest, CreateAccount_DifferentNames_Success) {
    auto account1 = create_test_account("1");
    auto account2 = create_test_account("2");

    auto result1 = account_service->create_account(account1);
    auto result2 = account_service->create_account(account2);

    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());
}

TEST_F(AccountServiceTest, IsNameUnique_NewName_ReturnsTrue) {
    auto account = create_test_account();
    account_service->create_account(account);

    bool result = account_service->is_name_unique("Unique Name", "");

    EXPECT_TRUE(result);
}

TEST_F(AccountServiceTest, IsNameUnique_ExistingName_ReturnsFalse) {
    auto account = create_test_account();
    auto create_result = account_service->create_account(account);
    ASSERT_TRUE(create_result.has_value());

    bool result = account_service->is_name_unique(account.account_name(), "");

    EXPECT_FALSE(result);
}

TEST_F(AccountServiceTest, IsNameUnique_SameAccountExcluded_ReturnsTrue) {
    auto account = create_test_account();
    auto create_result = account_service->create_account(account);
    ASSERT_TRUE(create_result.has_value());
    std::string account_id = create_result.value();

    bool result = account_service->is_name_unique(account.account_name(), account_id);

    EXPECT_TRUE(result) << "Should be unique when excluding self";
}

// ============================================================================
// Search Functionality Tests
// ============================================================================

TEST_F(AccountServiceTest, SearchAccounts_MatchInName_Success) {
    auto account = create_test_account();
    account.set_account_name("GitHub Account");
    account_service->create_account(account);

    auto result = account_service->search_accounts("github");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 1);
}

TEST_F(AccountServiceTest, SearchAccounts_CaseInsensitive_Success) {
    auto account = create_test_account();
    account.set_account_name("GitHub Account");
    account_service->create_account(account);

    auto result = account_service->search_accounts("GITHUB");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 1);
}

TEST_F(AccountServiceTest, SearchAccounts_NoMatch_ReturnsEmpty) {
    auto account = create_test_account();
    account_service->create_account(account);

    auto result = account_service->search_accounts("nonexistent");

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().empty());
}

TEST_F(AccountServiceTest, SearchAccounts_FilterByName_Success) {
    auto account = create_test_account();
    account.set_account_name("GitHub");
    account.set_user_name("github_user");
    account_service->create_account(account);

    auto result = account_service->search_accounts("github", "name");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 1);
}

TEST_F(AccountServiceTest, SearchAccounts_FilterByUsername_Success) {
    auto account = create_test_account();
    account.set_account_name("GitHub");
    account.set_user_name("github_user");
    account_service->create_account(account);

    auto result = account_service->search_accounts("github", "username");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 1);
}

TEST_F(AccountServiceTest, SearchAccounts_FilterByEmail_Success) {
    auto account = create_test_account();
    account.set_email("user@github.com");
    account_service->create_account(account);

    auto result = account_service->search_accounts("github", "email");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 1);
}

TEST_F(AccountServiceTest, SearchAccounts_FilterByWebsite_Success) {
    auto account = create_test_account();
    account.set_website("https://github.com");
    account_service->create_account(account);

    auto result = account_service->search_accounts("github", "website");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 1);
}

TEST_F(AccountServiceTest, SearchAccounts_FilterByNotes_Success) {
    auto account = create_test_account();
    account.set_notes("GitHub repository account");
    account_service->create_account(account);

    auto result = account_service->search_accounts("repository", "notes");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 1);
}

TEST_F(AccountServiceTest, SearchAccounts_MultipleMatches_ReturnsAll) {
    for (int i = 0; i < 3; ++i) {
        auto account = create_test_account(std::to_string(i));
        account.set_account_name("Test " + std::to_string(i));
        account_service->create_account(account);
    }

    auto result = account_service->search_accounts("test");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 3);
}

// ============================================================================
// Tag Filtering Tests
// ============================================================================

TEST_F(AccountServiceTest, FilterByTag_MatchingTag_Success) {
    auto account = create_test_account();
    account.add_tags("work");
    account.add_tags("important");
    account_service->create_account(account);

    auto result = account_service->filter_by_tag("work");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 1);
}

TEST_F(AccountServiceTest, FilterByTag_NoMatch_ReturnsEmpty) {
    auto account = create_test_account();
    account.add_tags("personal");
    account_service->create_account(account);

    auto result = account_service->filter_by_tag("work");

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().empty());
}

TEST_F(AccountServiceTest, FilterByTag_MultipleAccountsWithTag_ReturnsAll) {
    for (int i = 0; i < 3; ++i) {
        auto account = create_test_account(std::to_string(i));
        account.add_tags("work");
        account_service->create_account(account);
    }

    auto result = account_service->filter_by_tag("work");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 3);
}

// ============================================================================
// Toggle Favorite Tests
// ============================================================================

TEST_F(AccountServiceTest, ToggleFavorite_SetToTrue_Success) {
    auto account = create_test_account();
    account.set_is_favorite(false);
    account_service->create_account(account);

    auto result = account_service->toggle_favorite(0);

    ASSERT_TRUE(result.has_value());
    auto get_result = account_service->get_account(0);
    ASSERT_TRUE(get_result.has_value());
    EXPECT_TRUE(get_result.value().is_favorite());
}

TEST_F(AccountServiceTest, ToggleFavorite_SetToFalse_Success) {
    auto account = create_test_account();
    account.set_is_favorite(true);
    account_service->create_account(account);

    auto result = account_service->toggle_favorite(0);

    ASSERT_TRUE(result.has_value());
    auto get_result = account_service->get_account(0);
    ASSERT_TRUE(get_result.has_value());
    EXPECT_FALSE(get_result.value().is_favorite());
}

TEST_F(AccountServiceTest, ToggleFavorite_InvalidIndex_Fails) {
    auto result = account_service->toggle_favorite(999);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ServiceError::INVALID_INDEX);
}

// ============================================================================
// Count Tests
// ============================================================================

TEST_F(AccountServiceTest, Count_EmptyVault_ReturnsZero) {
    auto result = account_service->count();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 0);
}

TEST_F(AccountServiceTest, Count_MultipleAccounts_ReturnsCorrectCount) {
    for (int i = 0; i < 5; ++i) {
        auto account = create_test_account(std::to_string(i));
        account_service->create_account(account);
    }

    auto result = account_service->count();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 5);
}

// ============================================================================
// Vault State Tests
// ============================================================================

TEST_F(AccountServiceTest, Operations_ClosedVault_Fail) {
    (void)vault_manager->close_vault();
    auto account = create_test_account();
    auto result = account_service->create_account(account);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ServiceError::VAULT_CLOSED);
}
