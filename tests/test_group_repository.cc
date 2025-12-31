// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file test_group_repository.cc
 * @brief Unit tests for GroupRepository
 */

#include <gtest/gtest.h>
#include "../src/core/repositories/GroupRepository.h"
#include "../src/core/VaultManager.h"
#include <memory>
#include <cstdio>

namespace KeepTower {

/**
 * @class GroupRepositoryTest
 * @brief Test fixture for GroupRepository
 */
class GroupRepositoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary vault for testing
        vault_manager = std::make_unique<VaultManager>();
        temp_vault_path = "/tmp/test_group_repository_vault.ktv";

        // Create and open a test vault
        ASSERT_TRUE(vault_manager->create_vault(temp_vault_path, "test_password"));
        ASSERT_TRUE(vault_manager->open_vault(temp_vault_path, "test_password"));

        // Create repository
        repository = std::make_unique<GroupRepository>(vault_manager.get());

        // Create test groups
        group1_id = vault_manager->create_group("Personal");
        group2_id = vault_manager->create_group("Work");
        group3_id = vault_manager->create_group("Finance");

        // Add test accounts
        keeptower::AccountRecord account1;
        account1.set_id("account1");
        account1.set_account_name("Gmail");
        vault_manager->add_account(account1);

        keeptower::AccountRecord account2;
        account2.set_id("account2");
        account2.set_account_name("GitHub");
        vault_manager->add_account(account2);

        keeptower::AccountRecord account3;
        account3.set_id("account3");
        account3.set_account_name("AWS");
        vault_manager->add_account(account3);

        // Add accounts to groups
        vault_manager->add_account_to_group(0, group1_id);  // Gmail -> Personal
        vault_manager->add_account_to_group(1, group2_id);  // GitHub -> Work
        vault_manager->add_account_to_group(2, group2_id);  // AWS -> Work
    }

    void TearDown() override {
        if (vault_manager && vault_manager->is_vault_open()) {
            vault_manager->close_vault();
        }
        // Clean up test file
        std::remove(temp_vault_path.c_str());
    }

    std::unique_ptr<VaultManager> vault_manager;
    std::unique_ptr<GroupRepository> repository;
    std::string temp_vault_path;
    std::string group1_id;
    std::string group2_id;
    std::string group3_id;
};

// =============================================================================
// Construction Tests
// =============================================================================

TEST_F(GroupRepositoryTest, ConstructorThrowsOnNull) {
    EXPECT_THROW(GroupRepository(nullptr), std::invalid_argument);
}

TEST_F(GroupRepositoryTest, IsVaultOpen) {
    EXPECT_TRUE(repository->is_vault_open());

    vault_manager->close_vault();
    EXPECT_FALSE(repository->is_vault_open());
}

// =============================================================================
// Create Group Tests
// =============================================================================

TEST_F(GroupRepositoryTest, CreateGroup) {
    auto result = repository->create("Development");
    ASSERT_TRUE(result.has_value()) << "Create should succeed";
    EXPECT_FALSE(result->empty()) << "Should return non-empty group ID";

    // Verify group was created
    auto count = repository->count();
    ASSERT_TRUE(count.has_value());
    EXPECT_EQ(*count, 4);  // 3 existing + 1 new

    // Verify we can retrieve it
    auto retrieved = repository->get(*result);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->group_name(), "Development");
}

TEST_F(GroupRepositoryTest, CreateGroupEmptyName) {
    auto result = repository->create("");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), RepositoryError::INVALID_INDEX);  // Reusing for invalid input
}

TEST_F(GroupRepositoryTest, CreateGroupWhenVaultClosed) {
    vault_manager->close_vault();

    auto result = repository->create("TestGroup");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), RepositoryError::VAULT_CLOSED);
}

// =============================================================================
// Get Group Tests
// =============================================================================

TEST_F(GroupRepositoryTest, GetGroupById) {
    auto result = repository->get(group1_id);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->group_id(), group1_id);
    EXPECT_EQ(result->group_name(), "Personal");
}

TEST_F(GroupRepositoryTest, GetGroupByNonexistentId) {
    auto result = repository->get("nonexistent-uuid");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), RepositoryError::ACCOUNT_NOT_FOUND);  // Reusing for group
}

TEST_F(GroupRepositoryTest, GetGroupWhenVaultClosed) {
    vault_manager->close_vault();

    auto result = repository->get(group1_id);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), RepositoryError::VAULT_CLOSED);
}

// =============================================================================
// Get All Groups Tests
// =============================================================================

TEST_F(GroupRepositoryTest, GetAllGroups) {
    auto result = repository->get_all();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 3);

    // Verify groups exist (order may vary)
    bool found_personal = false;
    bool found_work = false;
    bool found_finance = false;

    for (const auto& group : *result) {
        if (group.group_name() == "Personal") found_personal = true;
        if (group.group_name() == "Work") found_work = true;
        if (group.group_name() == "Finance") found_finance = true;
    }

    EXPECT_TRUE(found_personal);
    EXPECT_TRUE(found_work);
    EXPECT_TRUE(found_finance);
}

TEST_F(GroupRepositoryTest, GetAllGroupsWhenVaultClosed) {
    vault_manager->close_vault();

    auto result = repository->get_all();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), RepositoryError::VAULT_CLOSED);
}

// =============================================================================
// Remove Group Tests
// =============================================================================

TEST_F(GroupRepositoryTest, RemoveGroup) {
    // Verify initial count
    auto count_before = repository->count();
    ASSERT_TRUE(count_before.has_value());
    EXPECT_EQ(*count_before, 3);

    // Remove group
    auto result = repository->remove(group3_id);
    ASSERT_TRUE(result.has_value()) << "Remove should succeed";

    // Verify count decreased
    auto count_after = repository->count();
    ASSERT_TRUE(count_after.has_value());
    EXPECT_EQ(*count_after, 2);

    // Verify group is gone
    EXPECT_FALSE(repository->exists(group3_id));
}

TEST_F(GroupRepositoryTest, RemoveNonexistentGroup) {
    auto result = repository->remove("nonexistent-uuid");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), RepositoryError::ACCOUNT_NOT_FOUND);
}

TEST_F(GroupRepositoryTest, RemoveGroupWhenVaultClosed) {
    vault_manager->close_vault();

    auto result = repository->remove(group1_id);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), RepositoryError::VAULT_CLOSED);
}

// =============================================================================
// Count Tests
// =============================================================================

TEST_F(GroupRepositoryTest, Count) {
    auto result = repository->count();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 3);
}

TEST_F(GroupRepositoryTest, CountWhenVaultClosed) {
    vault_manager->close_vault();

    auto result = repository->count();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), RepositoryError::VAULT_CLOSED);
}

// =============================================================================
// Add Account to Group Tests
// =============================================================================

TEST_F(GroupRepositoryTest, AddAccountToGroup) {
    // Add account3 (AWS) to Finance group
    auto result = repository->add_account_to_group(2, group3_id);
    ASSERT_TRUE(result.has_value()) << "Add account to group should succeed";

    // Verify account is in group
    auto accounts = repository->get_accounts_in_group(group3_id);
    ASSERT_TRUE(accounts.has_value());
    EXPECT_EQ(accounts->size(), 1);
    EXPECT_EQ((*accounts)[0], 2);
}

TEST_F(GroupRepositoryTest, AddAccountToGroupInvalidIndex) {
    auto result = repository->add_account_to_group(999, group1_id);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), RepositoryError::INVALID_INDEX);
}

TEST_F(GroupRepositoryTest, AddAccountToNonexistentGroup) {
    auto result = repository->add_account_to_group(0, "nonexistent-uuid");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), RepositoryError::ACCOUNT_NOT_FOUND);
}

TEST_F(GroupRepositoryTest, AddAccountToGroupWhenVaultClosed) {
    vault_manager->close_vault();

    auto result = repository->add_account_to_group(0, group1_id);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), RepositoryError::VAULT_CLOSED);
}

// =============================================================================
// Remove Account from Group Tests
// =============================================================================

TEST_F(GroupRepositoryTest, RemoveAccountFromGroup) {
    // Remove account1 (Gmail) from Personal group
    auto result = repository->remove_account_from_group(0, group1_id);
    ASSERT_TRUE(result.has_value()) << "Remove account from group should succeed";

    // Verify account is no longer in group
    auto accounts = repository->get_accounts_in_group(group1_id);
    ASSERT_TRUE(accounts.has_value());
    EXPECT_EQ(accounts->size(), 0);
}

TEST_F(GroupRepositoryTest, RemoveAccountFromGroupInvalidIndex) {
    auto result = repository->remove_account_from_group(999, group1_id);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), RepositoryError::INVALID_INDEX);
}

TEST_F(GroupRepositoryTest, RemoveAccountFromNonexistentGroup) {
    auto result = repository->remove_account_from_group(0, "nonexistent-uuid");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), RepositoryError::ACCOUNT_NOT_FOUND);
}

TEST_F(GroupRepositoryTest, RemoveAccountFromGroupWhenVaultClosed) {
    vault_manager->close_vault();

    auto result = repository->remove_account_from_group(0, group1_id);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), RepositoryError::VAULT_CLOSED);
}

// =============================================================================
// Get Accounts in Group Tests
// =============================================================================

TEST_F(GroupRepositoryTest, GetAccountsInGroup) {
    // Work group should have 2 accounts (GitHub and AWS)
    auto result = repository->get_accounts_in_group(group2_id);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 2);

    // Should contain indices 1 and 2
    EXPECT_TRUE(std::find(result->begin(), result->end(), 1) != result->end());
    EXPECT_TRUE(std::find(result->begin(), result->end(), 2) != result->end());
}

TEST_F(GroupRepositoryTest, GetAccountsInEmptyGroup) {
    // Finance group has no accounts
    auto result = repository->get_accounts_in_group(group3_id);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 0);
}

TEST_F(GroupRepositoryTest, GetAccountsInNonexistentGroup) {
    auto result = repository->get_accounts_in_group("nonexistent-uuid");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), RepositoryError::ACCOUNT_NOT_FOUND);
}

TEST_F(GroupRepositoryTest, GetAccountsInGroupWhenVaultClosed) {
    vault_manager->close_vault();

    auto result = repository->get_accounts_in_group(group1_id);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), RepositoryError::VAULT_CLOSED);
}

// =============================================================================
// Exists Tests
// =============================================================================

TEST_F(GroupRepositoryTest, GroupExists) {
    EXPECT_TRUE(repository->exists(group1_id));
    EXPECT_TRUE(repository->exists(group2_id));
    EXPECT_TRUE(repository->exists(group3_id));
    EXPECT_FALSE(repository->exists("nonexistent-uuid"));
}

TEST_F(GroupRepositoryTest, ExistsWhenVaultClosed) {
    vault_manager->close_vault();
    EXPECT_FALSE(repository->exists(group1_id));
}

// =============================================================================
// Complex Scenario Tests
// =============================================================================

TEST_F(GroupRepositoryTest, AddMultipleAccountsToSameGroup) {
    // Start with empty Finance group
    auto initial = repository->get_accounts_in_group(group3_id);
    ASSERT_TRUE(initial.has_value());
    EXPECT_EQ(initial->size(), 0);

    // Add all 3 accounts to Finance
    ASSERT_TRUE(repository->add_account_to_group(0, group3_id).has_value());
    ASSERT_TRUE(repository->add_account_to_group(1, group3_id).has_value());
    ASSERT_TRUE(repository->add_account_to_group(2, group3_id).has_value());

    // Verify all accounts in group
    auto final = repository->get_accounts_in_group(group3_id);
    ASSERT_TRUE(final.has_value());
    EXPECT_EQ(final->size(), 3);
}

TEST_F(GroupRepositoryTest, CreateAndDeleteGroupRoundtrip) {
    // Create new group
    auto create_result = repository->create("Temporary");
    ASSERT_TRUE(create_result.has_value());
    std::string temp_group_id = *create_result;

    // Verify it exists
    EXPECT_TRUE(repository->exists(temp_group_id));

    // Delete it
    auto delete_result = repository->remove(temp_group_id);
    ASSERT_TRUE(delete_result.has_value());

    // Verify it's gone
    EXPECT_FALSE(repository->exists(temp_group_id));
}

}  // namespace KeepTower

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
