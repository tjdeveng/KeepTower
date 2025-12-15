// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include <gtest/gtest.h>
#include "../src/core/VaultManager.h"
#include <filesystem>
#include <chrono>
#include <thread>

using namespace KeepTower;
namespace fs = std::filesystem;

class AccountGroupsTest : public ::testing::Test {
protected:
    std::unique_ptr<VaultManager> vault_manager;
    std::string test_vault_path;
    const std::string test_password = "TestPassword123!";

    void SetUp() override {
        vault_manager = std::make_unique<VaultManager>();
        test_vault_path = "test_groups_vault_" +
                         std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) +
                         ".vault";

        // Create a new vault
        ASSERT_TRUE(vault_manager->create_vault(test_vault_path, test_password))
            << "Failed to create test vault";

        // Add some test accounts
        for (int i = 0; i < 5; ++i) {
            keeptower::AccountRecord account;
            account.set_id("account-" + std::to_string(i));
            account.set_account_name("Test Account " + std::to_string(i));
            account.set_user_name("user" + std::to_string(i));
            account.set_password("pass" + std::to_string(i));
            ASSERT_TRUE(vault_manager->add_account(account));
        }
    }

    void TearDown() override {
        if (vault_manager->is_vault_open()) {
            (void)vault_manager->close_vault();
        }

        // Clean up test vault files
        try {
            if (fs::exists(test_vault_path)) {
                fs::remove(test_vault_path);
            }
            if (fs::exists(test_vault_path + ".backup")) {
                fs::remove(test_vault_path + ".backup");
            }
        } catch (const std::exception& e) {
            std::cerr << "Cleanup error: " << e.what() << std::endl;
        }
    }
};

// Test: Create a new group
TEST_F(AccountGroupsTest, CreateGroup) {
    std::string group_id = vault_manager->create_group("Work");
    EXPECT_FALSE(group_id.empty()) << "Group ID should not be empty";

    // Group ID should be a valid UUID format (basic check)
    EXPECT_GT(group_id.length(), 30) << "UUID should be reasonably long";
    EXPECT_NE(group_id.find('-'), std::string::npos) << "UUID should contain dashes";
}

// Test: Create group with duplicate name fails
TEST_F(AccountGroupsTest, CreateDuplicateGroupFails) {
    std::string group_id1 = vault_manager->create_group("Work");
    EXPECT_FALSE(group_id1.empty());

    std::string group_id2 = vault_manager->create_group("Work");
    EXPECT_TRUE(group_id2.empty()) << "Duplicate group name should be rejected";
}

// Test: Create group with invalid name fails
TEST_F(AccountGroupsTest, CreateGroupInvalidName) {
    // Empty name
    EXPECT_TRUE(vault_manager->create_group("").empty());

    // Name too long
    std::string long_name(101, 'x');
    EXPECT_TRUE(vault_manager->create_group(long_name).empty());

    // Control characters
    EXPECT_TRUE(vault_manager->create_group("Test\nGroup").empty());

    // Path traversal attempts
    EXPECT_TRUE(vault_manager->create_group("../Work").empty());
    EXPECT_TRUE(vault_manager->create_group("..").empty());
}

// Test: Get or create Favorites group
TEST_F(AccountGroupsTest, GetFavoritesGroup) {
    std::string favorites_id = vault_manager->get_favorites_group_id();
    EXPECT_FALSE(favorites_id.empty()) << "Favorites group should be created automatically";

    // Calling again should return same ID
    std::string favorites_id2 = vault_manager->get_favorites_group_id();
    EXPECT_EQ(favorites_id, favorites_id2) << "Favorites group ID should be consistent";
}

// Test: Add account to group
TEST_F(AccountGroupsTest, AddAccountToGroup) {
    std::string group_id = vault_manager->create_group("Personal");
    ASSERT_FALSE(group_id.empty());

    // Add first account to the group
    EXPECT_TRUE(vault_manager->add_account_to_group(0, group_id));

    // Verify membership
    EXPECT_TRUE(vault_manager->is_account_in_group(0, group_id));
    EXPECT_FALSE(vault_manager->is_account_in_group(1, group_id));
}

// Test: Add account to group is idempotent
TEST_F(AccountGroupsTest, AddAccountToGroupIdempotent) {
    std::string group_id = vault_manager->create_group("Banking");
    ASSERT_FALSE(group_id.empty());

    // Add account twice
    EXPECT_TRUE(vault_manager->add_account_to_group(0, group_id));
    EXPECT_TRUE(vault_manager->add_account_to_group(0, group_id));

    // Should still be in group
    EXPECT_TRUE(vault_manager->is_account_in_group(0, group_id));
}

// Test: Add account to non-existent group fails
TEST_F(AccountGroupsTest, AddAccountToNonExistentGroup) {
    EXPECT_FALSE(vault_manager->add_account_to_group(0, "fake-group-id"));
}

// Test: Add invalid account index fails
TEST_F(AccountGroupsTest, AddInvalidAccountToGroup) {
    std::string group_id = vault_manager->create_group("Test");
    ASSERT_FALSE(group_id.empty());

    // Invalid index (we only have 5 accounts: 0-4)
    EXPECT_FALSE(vault_manager->add_account_to_group(999, group_id));
}

// Test: Remove account from group
TEST_F(AccountGroupsTest, RemoveAccountFromGroup) {
    std::string group_id = vault_manager->create_group("Shopping");
    ASSERT_FALSE(group_id.empty());

    // Add then remove
    EXPECT_TRUE(vault_manager->add_account_to_group(0, group_id));
    EXPECT_TRUE(vault_manager->is_account_in_group(0, group_id));

    EXPECT_TRUE(vault_manager->remove_account_from_group(0, group_id));
    EXPECT_FALSE(vault_manager->is_account_in_group(0, group_id));
}

// Test: Remove account from group is idempotent
TEST_F(AccountGroupsTest, RemoveAccountFromGroupIdempotent) {
    std::string group_id = vault_manager->create_group("Travel");
    ASSERT_FALSE(group_id.empty());

    ASSERT_TRUE(vault_manager->add_account_to_group(0, group_id));

    // Remove twice
    EXPECT_TRUE(vault_manager->remove_account_from_group(0, group_id));
    EXPECT_TRUE(vault_manager->remove_account_from_group(0, group_id));

    EXPECT_FALSE(vault_manager->is_account_in_group(0, group_id));
}

// Test: Delete group
TEST_F(AccountGroupsTest, DeleteGroup) {
    std::string group_id = vault_manager->create_group("Temporary");
    ASSERT_FALSE(group_id.empty());

    // Add account to group
    ASSERT_TRUE(vault_manager->add_account_to_group(0, group_id));
    EXPECT_TRUE(vault_manager->is_account_in_group(0, group_id));

    // Delete group
    EXPECT_TRUE(vault_manager->delete_group(group_id));

    // Account should no longer be in group
    EXPECT_FALSE(vault_manager->is_account_in_group(0, group_id));
}

// Test: Cannot delete system groups (Favorites)
TEST_F(AccountGroupsTest, CannotDeleteFavoritesGroup) {
    std::string favorites_id = vault_manager->get_favorites_group_id();
    ASSERT_FALSE(favorites_id.empty());

    // Attempt to delete Favorites should fail
    EXPECT_FALSE(vault_manager->delete_group(favorites_id))
        << "System groups should not be deletable";
}

// Test: Multi-group membership
TEST_F(AccountGroupsTest, MultiGroupMembership) {
    std::string work_id = vault_manager->create_group("Work");
    std::string personal_id = vault_manager->create_group("Personal");
    std::string urgent_id = vault_manager->create_group("Urgent");

    ASSERT_FALSE(work_id.empty());
    ASSERT_FALSE(personal_id.empty());
    ASSERT_FALSE(urgent_id.empty());

    // Add account to multiple groups
    EXPECT_TRUE(vault_manager->add_account_to_group(0, work_id));
    EXPECT_TRUE(vault_manager->add_account_to_group(0, personal_id));
    EXPECT_TRUE(vault_manager->add_account_to_group(0, urgent_id));

    // Verify membership in all groups
    EXPECT_TRUE(vault_manager->is_account_in_group(0, work_id));
    EXPECT_TRUE(vault_manager->is_account_in_group(0, personal_id));
    EXPECT_TRUE(vault_manager->is_account_in_group(0, urgent_id));

    // Other accounts should not be in these groups
    EXPECT_FALSE(vault_manager->is_account_in_group(1, work_id));
}

// Test: Persistence - groups survive vault close/reopen
TEST_F(AccountGroupsTest, GroupsPersistAcrossVaultReopen) {
    std::string group_id = vault_manager->create_group("Persistent");
    ASSERT_FALSE(group_id.empty());

    ASSERT_TRUE(vault_manager->add_account_to_group(0, group_id));
    ASSERT_TRUE(vault_manager->add_account_to_group(2, group_id));

    // Close and reopen vault
    ASSERT_TRUE(vault_manager->close_vault());
    ASSERT_TRUE(vault_manager->open_vault(test_vault_path, test_password));

    // Verify accounts are still in group
    EXPECT_TRUE(vault_manager->is_account_in_group(0, group_id));
    EXPECT_TRUE(vault_manager->is_account_in_group(2, group_id));
    EXPECT_FALSE(vault_manager->is_account_in_group(1, group_id));
}

// Test: Favorites group persists
TEST_F(AccountGroupsTest, FavoritesGroupPersists) {
    std::string favorites_id1 = vault_manager->get_favorites_group_id();
    ASSERT_FALSE(favorites_id1.empty());

    // Close and reopen
    ASSERT_TRUE(vault_manager->close_vault());
    ASSERT_TRUE(vault_manager->open_vault(test_vault_path, test_password));

    // Should get same Favorites group
    std::string favorites_id2 = vault_manager->get_favorites_group_id();
    EXPECT_EQ(favorites_id1, favorites_id2);
}

// Test: Delete group removes all account memberships
TEST_F(AccountGroupsTest, DeleteGroupRemovesAllMemberships) {
    std::string group_id = vault_manager->create_group("ToDelete");
    ASSERT_FALSE(group_id.empty());

    // Add multiple accounts
    ASSERT_TRUE(vault_manager->add_account_to_group(0, group_id));
    ASSERT_TRUE(vault_manager->add_account_to_group(1, group_id));
    ASSERT_TRUE(vault_manager->add_account_to_group(2, group_id));

    // Verify memberships
    EXPECT_TRUE(vault_manager->is_account_in_group(0, group_id));
    EXPECT_TRUE(vault_manager->is_account_in_group(1, group_id));
    EXPECT_TRUE(vault_manager->is_account_in_group(2, group_id));

    // Delete group
    EXPECT_TRUE(vault_manager->delete_group(group_id));

    // All memberships should be removed
    EXPECT_FALSE(vault_manager->is_account_in_group(0, group_id));
    EXPECT_FALSE(vault_manager->is_account_in_group(1, group_id));
    EXPECT_FALSE(vault_manager->is_account_in_group(2, group_id));
}

// Test: Create group when vault is not open fails
TEST_F(AccountGroupsTest, OperationsFailWhenVaultClosed) {
    // Close vault
    ASSERT_TRUE(vault_manager->close_vault());

    // All operations should fail gracefully
    EXPECT_TRUE(vault_manager->create_group("Fail").empty());
    EXPECT_TRUE(vault_manager->get_favorites_group_id().empty());
    EXPECT_FALSE(vault_manager->add_account_to_group(0, "any-id"));
    EXPECT_FALSE(vault_manager->remove_account_from_group(0, "any-id"));
    EXPECT_FALSE(vault_manager->delete_group("any-id"));
    EXPECT_FALSE(vault_manager->is_account_in_group(0, "any-id"));
}

// Test: Group names can use various characters
TEST_F(AccountGroupsTest, GroupNamesWithSpecialCharacters) {
    // Valid special characters
    EXPECT_FALSE(vault_manager->create_group("Work & Personal").empty());
    EXPECT_FALSE(vault_manager->create_group("High Priority!!!").empty());
    EXPECT_FALSE(vault_manager->create_group("Banking (2024)").empty());
    EXPECT_FALSE(vault_manager->create_group("Team: DevOps").empty());

    // Unicode characters (if supported by protobuf)
    EXPECT_FALSE(vault_manager->create_group("日本語").empty());
    EXPECT_FALSE(vault_manager->create_group("Работа").empty());
}

// ============================================================================
// PHASE 5 TESTS: Group Rename, Reorder, and Account Ordering Within Groups
// ============================================================================

// Test: Rename group succeeds with valid name
TEST_F(AccountGroupsTest, RenameGroupSuccess) {
    std::string group_id = vault_manager->create_group("Old Name");
    ASSERT_FALSE(group_id.empty());

    // Rename should succeed
    EXPECT_TRUE(vault_manager->rename_group(group_id, "New Name"));

    // Verify name changed
    auto groups = vault_manager->get_all_groups();
    auto it = std::find_if(groups.begin(), groups.end(),
                          [&](const auto& g) { return g.group_id() == group_id; });
    ASSERT_NE(it, groups.end());
    EXPECT_EQ(it->group_name(), "New Name");
}

// Test: Cannot rename system groups
TEST_F(AccountGroupsTest, CannotRenameSystemGroups) {
    std::string fav_id = vault_manager->get_favorites_group_id();
    ASSERT_FALSE(fav_id.empty());

    // Attempt to rename Favorites should fail
    EXPECT_FALSE(vault_manager->rename_group(fav_id, "Not Favorites"));

    // Verify name unchanged
    auto groups = vault_manager->get_all_groups();
    auto it = std::find_if(groups.begin(), groups.end(),
                          [&](const auto& g) { return g.group_id() == fav_id; });
    ASSERT_NE(it, groups.end());
    EXPECT_EQ(it->group_name(), "Favorites");
}

// Test: Cannot rename to duplicate name
TEST_F(AccountGroupsTest, RenameGroupDuplicateName) {
    std::string group1_id = vault_manager->create_group("Work");
    std::string group2_id = vault_manager->create_group("Personal");
    ASSERT_FALSE(group1_id.empty());
    ASSERT_FALSE(group2_id.empty());

    // Attempt to rename group2 to "Work" should fail
    EXPECT_FALSE(vault_manager->rename_group(group2_id, "Work"));

    // Verify group2 name unchanged
    auto groups = vault_manager->get_all_groups();
    auto it = std::find_if(groups.begin(), groups.end(),
                          [&](const auto& g) { return g.group_id() == group2_id; });
    ASSERT_NE(it, groups.end());
    EXPECT_EQ(it->group_name(), "Personal");
}

// Test: Rename with invalid name fails
TEST_F(AccountGroupsTest, RenameGroupInvalidName) {
    std::string group_id = vault_manager->create_group("Valid");
    ASSERT_FALSE(group_id.empty());

    // Empty name
    EXPECT_FALSE(vault_manager->rename_group(group_id, ""));

    // Too long name
    std::string long_name(101, 'x');
    EXPECT_FALSE(vault_manager->rename_group(group_id, long_name));

    // Path traversal
    EXPECT_FALSE(vault_manager->rename_group(group_id, "../evil"));
}

// Test: Rename persists across vault close/reopen
TEST_F(AccountGroupsTest, RenameGroupPersistence) {
    std::string group_id = vault_manager->create_group("Original");
    ASSERT_FALSE(group_id.empty());
    ASSERT_TRUE(vault_manager->rename_group(group_id, "Renamed"));

    // Close and reopen vault
    ASSERT_TRUE(vault_manager->close_vault());
    ASSERT_TRUE(vault_manager->open_vault(test_vault_path, test_password));

    // Verify renamed group persisted
    auto groups = vault_manager->get_all_groups();
    auto it = std::find_if(groups.begin(), groups.end(),
                          [&](const auto& g) { return g.group_id() == group_id; });
    ASSERT_NE(it, groups.end());
    EXPECT_EQ(it->group_name(), "Renamed");
}

// Test: Reorder group succeeds
TEST_F(AccountGroupsTest, ReorderGroupSuccess) {
    std::string group1_id = vault_manager->create_group("Group 1");
    std::string group2_id = vault_manager->create_group("Group 2");
    ASSERT_FALSE(group1_id.empty());
    ASSERT_FALSE(group2_id.empty());

    // Reorder group1 to position 5
    EXPECT_TRUE(vault_manager->reorder_group(group1_id, 5));

    // Verify order changed
    auto groups = vault_manager->get_all_groups();
    auto it = std::find_if(groups.begin(), groups.end(),
                          [&](const auto& g) { return g.group_id() == group1_id; });
    ASSERT_NE(it, groups.end());
    EXPECT_EQ(it->display_order(), 5);
}

// Test: Cannot reorder system groups
TEST_F(AccountGroupsTest, CannotReorderSystemGroups) {
    std::string fav_id = vault_manager->get_favorites_group_id();
    ASSERT_FALSE(fav_id.empty());

    // Attempt to reorder Favorites should fail
    EXPECT_FALSE(vault_manager->reorder_group(fav_id, 10));

    // Verify order unchanged (should be 0)
    auto groups = vault_manager->get_all_groups();
    auto it = std::find_if(groups.begin(), groups.end(),
                          [&](const auto& g) { return g.group_id() == fav_id; });
    ASSERT_NE(it, groups.end());
    EXPECT_EQ(it->display_order(), 0);
}

// Test: Reorder with negative order fails
TEST_F(AccountGroupsTest, ReorderGroupInvalidOrder) {
    std::string group_id = vault_manager->create_group("Test");
    ASSERT_FALSE(group_id.empty());

    // Negative order should fail
    EXPECT_FALSE(vault_manager->reorder_group(group_id, -1));
}

// Test: Reorder persists across vault close/reopen
TEST_F(AccountGroupsTest, ReorderGroupPersistence) {
    std::string group_id = vault_manager->create_group("Test");
    ASSERT_FALSE(group_id.empty());
    ASSERT_TRUE(vault_manager->reorder_group(group_id, 42));

    // Close and reopen vault
    ASSERT_TRUE(vault_manager->close_vault());
    ASSERT_TRUE(vault_manager->open_vault(test_vault_path, test_password));

    // Verify reorder persisted
    auto groups = vault_manager->get_all_groups();
    auto it = std::find_if(groups.begin(), groups.end(),
                          [&](const auto& g) { return g.group_id() == group_id; });
    ASSERT_NE(it, groups.end());
    EXPECT_EQ(it->display_order(), 42);
}

// Test: Reorder account within group
TEST_F(AccountGroupsTest, ReorderAccountInGroup) {
    std::string group_id = vault_manager->create_group("Work");
    ASSERT_FALSE(group_id.empty());

    // Add accounts to group
    ASSERT_TRUE(vault_manager->add_account_to_group(0, group_id));
    ASSERT_TRUE(vault_manager->add_account_to_group(1, group_id));

    // Reorder first account to position 3
    EXPECT_TRUE(vault_manager->reorder_account_in_group(0, group_id, 3));

    // Verify by reading account data directly
    const auto* account = vault_manager->get_account(0);
    ASSERT_NE(account, nullptr);

    bool found = false;
    for (const auto& membership : account->groups()) {
        if (membership.group_id() == group_id) {
            EXPECT_EQ(membership.display_order(), 3);
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

// Test: Cannot reorder account not in group
TEST_F(AccountGroupsTest, ReorderAccountNotInGroup) {
    std::string group_id = vault_manager->create_group("Work");
    ASSERT_FALSE(group_id.empty());

    // Account 0 is not in the group
    EXPECT_FALSE(vault_manager->reorder_account_in_group(0, group_id, 5));
}

// Test: Reorder account with invalid parameters
TEST_F(AccountGroupsTest, ReorderAccountInGroupInvalidParams) {
    std::string group_id = vault_manager->create_group("Work");
    ASSERT_FALSE(group_id.empty());
    ASSERT_TRUE(vault_manager->add_account_to_group(0, group_id));

    // Invalid account index
    EXPECT_FALSE(vault_manager->reorder_account_in_group(999, group_id, 0));

    // Invalid group ID
    EXPECT_FALSE(vault_manager->reorder_account_in_group(0, "nonexistent-id", 0));

    // Negative order
    EXPECT_FALSE(vault_manager->reorder_account_in_group(0, group_id, -1));
}

// Test: Reorder account within group persists
TEST_F(AccountGroupsTest, ReorderAccountInGroupPersistence) {
    std::string group_id = vault_manager->create_group("Work");
    ASSERT_FALSE(group_id.empty());
    ASSERT_TRUE(vault_manager->add_account_to_group(0, group_id));
    ASSERT_TRUE(vault_manager->reorder_account_in_group(0, group_id, 7));

    // Close and reopen vault
    ASSERT_TRUE(vault_manager->close_vault());
    ASSERT_TRUE(vault_manager->open_vault(test_vault_path, test_password));

    // Verify reorder persisted
    const auto* account = vault_manager->get_account(0);
    ASSERT_NE(account, nullptr);

    bool found = false;
    for (const auto& membership : account->groups()) {
        if (membership.group_id() == group_id) {
            EXPECT_EQ(membership.display_order(), 7);
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
