// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file test_account_view_controller.cc
 * @brief Unit tests for AccountViewController
 */

#include <gtest/gtest.h>
#include "../src/ui/controllers/AccountViewController.h"
#include "../src/core/VaultManager.h"
#include <memory>

/**
 * @class AccountViewControllerTest
 * @brief Test fixture for AccountViewController
 */
class AccountViewControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary vault for testing
        vault_manager = std::make_unique<VaultManager>();
        temp_vault_path = "/tmp/test_account_view_controller_vault.ktv";

        // Create a test vault
        ASSERT_TRUE(vault_manager->create_vault(temp_vault_path, "test_password"));
        ASSERT_TRUE(vault_manager->open_vault(temp_vault_path, "test_password"));

        // Add test accounts
        keeptower::AccountRecord account1;
        account1.set_id("account1");
        account1.set_account_name("Gmail Account");
        account1.set_user_name("user1@gmail.com");
        account1.set_is_favorite(true);
        vault_manager->add_account(account1);

        keeptower::AccountRecord account2;
        account2.set_id("account2");
        account2.set_account_name("GitHub Account");
        account2.set_user_name("user2");
        account2.set_is_favorite(false);
        vault_manager->add_account(account2);

        keeptower::AccountRecord account3;
        account3.set_id("account3");
        account3.set_account_name("AWS Account");
        account3.set_user_name("user3");
        account3.set_is_favorite(false);
        vault_manager->add_account(account3);

        // Add test group
        test_group_id = vault_manager->create_group("Work Accounts");

        // Create controller
        controller = std::make_unique<AccountViewController>(vault_manager.get());
    }

    void TearDown() override {
        if (vault_manager && vault_manager->is_vault_open()) {
            vault_manager->close_vault();
        }
        // Clean up test file
        std::remove(temp_vault_path.c_str());
    }

    std::unique_ptr<VaultManager> vault_manager;
    std::unique_ptr<AccountViewController> controller;
    std::string temp_vault_path;
    std::string test_group_id;
};

/**
 * @test Constructor throws on null VaultManager
 */
TEST_F(AccountViewControllerTest, ConstructorThrowsOnNull) {
    EXPECT_THROW(AccountViewController(nullptr), std::invalid_argument);
}

/**
 * @test Refresh account list populates viewable accounts
 */
TEST_F(AccountViewControllerTest, RefreshAccountList) {
    // Setup signal spy
    bool signal_received = false;
    size_t accounts_count = 0;
    size_t groups_count = 0;

    controller->signal_list_updated().connect(
        [&](const auto& accounts, const auto& groups, size_t total) {
            signal_received = true;
            accounts_count = accounts.size();
            groups_count = groups.size();
        });

    // Refresh
    controller->refresh_account_list();

    // Verify signal emitted
    EXPECT_TRUE(signal_received);
    EXPECT_EQ(accounts_count, 3);  // All 3 accounts
    EXPECT_EQ(groups_count, 1);    // 1 group

    // Verify cached data
    EXPECT_EQ(controller->get_viewable_accounts().size(), 3);
    EXPECT_EQ(controller->get_groups().size(), 1);
    EXPECT_EQ(controller->get_viewable_account_count(), 3);
}

/**
 * @test Get viewable accounts returns correct data
 */
TEST_F(AccountViewControllerTest, GetViewableAccounts) {
    controller->refresh_account_list();

    const auto& accounts = controller->get_viewable_accounts();
    ASSERT_EQ(accounts.size(), 3);

    // Check account IDs
    EXPECT_EQ(accounts[0].id(), "account1");
    EXPECT_EQ(accounts[1].id(), "account2");
    EXPECT_EQ(accounts[2].id(), "account3");
}

/**
 * @test Find account by ID
 */
TEST_F(AccountViewControllerTest, FindAccountIndexById) {
    controller->refresh_account_list();

    EXPECT_EQ(controller->find_account_index_by_id("account1"), 0);
    EXPECT_EQ(controller->find_account_index_by_id("account2"), 1);
    EXPECT_EQ(controller->find_account_index_by_id("account3"), 2);
    EXPECT_EQ(controller->find_account_index_by_id("nonexistent"), -1);
}

/**
 * @test Toggle favorite updates account
 */
TEST_F(AccountViewControllerTest, ToggleFavorite) {
    controller->refresh_account_list();

    // Setup signal spy
    bool signal_received = false;
    size_t toggled_index = 0;
    bool is_favorite = false;

    controller->signal_favorite_toggled().connect(
        [&](size_t index, bool favorite) {
            signal_received = true;
            toggled_index = index;
            is_favorite = favorite;
        });

    // Toggle account2 (index 1) favorite status
    EXPECT_TRUE(controller->toggle_favorite(1));

    // Verify signal
    EXPECT_TRUE(signal_received);
    EXPECT_EQ(toggled_index, 1);
    EXPECT_TRUE(is_favorite);  // Was false, now true

    // Verify change in vault
    auto accounts = vault_manager->get_all_accounts();
    EXPECT_TRUE(accounts[1].is_favorite());
}

/**
 * @test Toggle favorite on invalid index fails
 */
TEST_F(AccountViewControllerTest, ToggleFavoriteInvalidIndex) {
    controller->refresh_account_list();

    // Setup error signal spy
    bool error_received = false;
    controller->signal_error().connect([&](const std::string& msg) {
        error_received = true;
    });

    // Try invalid index
    EXPECT_FALSE(controller->toggle_favorite(999));
    EXPECT_TRUE(error_received);
}

/**
 * @test Vault open status
 */
TEST_F(AccountViewControllerTest, VaultOpenStatus) {
    EXPECT_TRUE(controller->is_vault_open());

    vault_manager->close_vault();
    EXPECT_FALSE(controller->is_vault_open());
}

/**
 * @test Refresh with closed vault clears data
 */
TEST_F(AccountViewControllerTest, RefreshWithClosedVault) {
    // First populate data
    controller->refresh_account_list();
    EXPECT_EQ(controller->get_viewable_account_count(), 3);

    // Close vault
    vault_manager->close_vault();

    // Setup signal spy
    bool signal_received = false;
    size_t accounts_count = 999;

    controller->signal_list_updated().connect(
        [&](const auto& accounts, const auto& groups, size_t total) {
            signal_received = true;
            accounts_count = accounts.size();
        });

    // Refresh again
    controller->refresh_account_list();

    // Should emit signal with empty data
    EXPECT_TRUE(signal_received);
    EXPECT_EQ(accounts_count, 0);
    EXPECT_EQ(controller->get_viewable_account_count(), 0);
}

/**
 * @test Can view account checks permissions
 */
TEST_F(AccountViewControllerTest, CanViewAccount) {
    controller->refresh_account_list();

    // For V1 vault (non-multi-user), all accounts are viewable
    EXPECT_TRUE(controller->can_view_account(0));
    EXPECT_TRUE(controller->can_view_account(1));
    EXPECT_TRUE(controller->can_view_account(2));
    EXPECT_FALSE(controller->can_view_account(999));  // Out of bounds
}

/**
 * @test Get groups returns correct data
 */
TEST_F(AccountViewControllerTest, GetGroups) {
    controller->refresh_account_list();

    const auto& groups = controller->get_groups();
    ASSERT_EQ(groups.size(), 1);
    EXPECT_EQ(groups[0].group_id(), test_group_id);
    EXPECT_EQ(groups[0].group_name(), "Work Accounts");
}

/**
 * @test Multiple refreshes update data correctly
/**
 * @test Multiple refreshes update data correctly
 */
TEST_F(AccountViewControllerTest, MultipleRefreshes) {
    // First refresh
    controller->refresh_account_list();
    EXPECT_EQ(controller->get_viewable_account_count(), 3);

    // Add another account
    keeptower::AccountRecord account4;
    account4.set_id("account4");
    account4.set_account_name("New Account");
    vault_manager->add_account(account4);

    // Second refresh
    controller->refresh_account_list();
    EXPECT_EQ(controller->get_viewable_account_count(), 4);

    const auto& accounts = controller->get_viewable_accounts();
    EXPECT_EQ(accounts[3].id(), "account4");
}

/**
 * @test Signal connections are independent
 */
TEST_F(AccountViewControllerTest, MultipleSignalConnections) {
    int signal1_count = 0;
    int signal2_count = 0;

    controller->signal_list_updated().connect(
        [&](const auto&, const auto&, size_t) { ++signal1_count; });

    controller->signal_list_updated().connect(
        [&](const auto&, const auto&, size_t) { ++signal2_count; });

    controller->refresh_account_list();

    // Both connections should fire
    EXPECT_EQ(signal1_count, 1);
    EXPECT_EQ(signal2_count, 1);
}
