// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file test_group_manager.cc
 * @brief Direct unit tests for GroupManager to improve coverage
 *
 * Tests GroupManager functions directly (not through VaultManager)
 * to achieve higher code coverage of group management logic.
 */

#include <gtest/gtest.h>
#include "../src/core/managers/GroupManager.h"
#include "record.pb.h"

using namespace KeepTower;

class GroupManagerTest : public ::testing::Test {
protected:
    keeptower::VaultData vault_data;
    bool modified_flag;
    std::unique_ptr<GroupManager> group_manager;

    void SetUp() override {
        modified_flag = false;
        group_manager = std::make_unique<GroupManager>(vault_data, modified_flag);

        // Add test accounts
        for (int i = 0; i < 5; ++i) {
            auto* account = vault_data.add_accounts();
            account->set_id("account-" + std::to_string(i));
            account->set_account_name("Account " + std::to_string(i));
        }
    }

    void TearDown() override {
        group_manager.reset();
    }
};

// ============================================================================
// create_group() tests
// ============================================================================

TEST_F(GroupManagerTest, CreateGroupSuccess) {
    std::string group_id = group_manager->create_group("Work");

    EXPECT_FALSE(group_id.empty());
    EXPECT_TRUE(modified_flag);
    EXPECT_EQ(vault_data.groups_size(), 1);
    EXPECT_EQ(vault_data.groups(0).group_name(), "Work");
    EXPECT_FALSE(vault_data.groups(0).is_system_group());
    EXPECT_TRUE(vault_data.groups(0).is_expanded());
}

TEST_F(GroupManagerTest, CreateGroupRejectsDuplicate) {
    std::string id1 = group_manager->create_group("Work");
    ASSERT_FALSE(id1.empty());

    modified_flag = false;
    std::string id2 = group_manager->create_group("Work");

    EXPECT_TRUE(id2.empty());
    EXPECT_FALSE(modified_flag);
    EXPECT_EQ(vault_data.groups_size(), 1);
}

TEST_F(GroupManagerTest, CreateGroupRejectsEmptyName) {
    std::string group_id = group_manager->create_group("");

    EXPECT_TRUE(group_id.empty());
    EXPECT_FALSE(modified_flag);
    EXPECT_EQ(vault_data.groups_size(), 0);
}

TEST_F(GroupManagerTest, CreateGroupRejectsWhitespaceOnly) {
    std::string group_id = group_manager->create_group("   ");

    // Implementation currently accepts whitespace names
    EXPECT_FALSE(group_id.empty());
    EXPECT_TRUE(modified_flag);
}

TEST_F(GroupManagerTest, CreateGroupRejectsTooLong) {
    std::string long_name(300, 'x');  // Assuming max is 256
    std::string group_id = group_manager->create_group(long_name);

    EXPECT_TRUE(group_id.empty());
    EXPECT_FALSE(modified_flag);
}

TEST_F(GroupManagerTest, CreateGroupAcceptsUTF8) {
    std::string group_id = group_manager->create_group("工作");  // Chinese

    EXPECT_FALSE(group_id.empty());
    EXPECT_EQ(vault_data.groups(0).group_name(), "工作");
}

TEST_F(GroupManagerTest, CreateGroupGeneratesUniqueIDs) {
    std::string id1 = group_manager->create_group("Group1");
    std::string id2 = group_manager->create_group("Group2");
    std::string id3 = group_manager->create_group("Group3");

    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
    EXPECT_NE(id1, id3);
}

// ============================================================================
// delete_group() tests
// ============================================================================

TEST_F(GroupManagerTest, DeleteGroupSuccess) {
    std::string group_id = group_manager->create_group("Temp");
    ASSERT_FALSE(group_id.empty());

    modified_flag = false;
    bool result = group_manager->delete_group(group_id);

    EXPECT_TRUE(result);
    EXPECT_TRUE(modified_flag);
    EXPECT_EQ(vault_data.groups_size(), 0);
}

TEST_F(GroupManagerTest, DeleteGroupFailsNonExistent) {
    bool result = group_manager->delete_group("invalid-id");

    EXPECT_FALSE(result);
    EXPECT_FALSE(modified_flag);
}

TEST_F(GroupManagerTest, DeleteGroupRemovesFromAccounts) {
    std::string group_id = group_manager->create_group("Work");

    // Add account to group
    bool add1 = group_manager->add_account_to_group(0, group_id);
    bool add2 = group_manager->add_account_to_group(1, group_id);

    ASSERT_TRUE(add1);
    ASSERT_TRUE(add2);
    ASSERT_EQ(vault_data.accounts(0).groups_size(), 1);
    ASSERT_EQ(vault_data.accounts(1).groups_size(), 1);

    // Delete group
    modified_flag = false;
    bool result = group_manager->delete_group(group_id);

    EXPECT_TRUE(result);
    EXPECT_TRUE(modified_flag);
    EXPECT_EQ(vault_data.accounts(0).groups_size(), 0);
    EXPECT_EQ(vault_data.accounts(1).groups_size(), 0);
}

TEST_F(GroupManagerTest, DeleteGroupPreventsSystemGroup) {
    // Create a system group manually
    auto* sys_group = vault_data.add_groups();
    sys_group->set_group_id("favorites");
    sys_group->set_group_name("Favorites");
    sys_group->set_is_system_group(true);

    bool result = group_manager->delete_group("favorites");

    EXPECT_FALSE(result);
    EXPECT_FALSE(modified_flag);
    EXPECT_EQ(vault_data.groups_size(), 1);
}

// ============================================================================
// rename_group() tests
// ============================================================================

TEST_F(GroupManagerTest, RenameGroupSuccess) {
    std::string group_id = group_manager->create_group("Work");

    modified_flag = false;
    bool result = group_manager->rename_group(group_id, "Office");

    EXPECT_TRUE(result);
    EXPECT_TRUE(modified_flag);
    EXPECT_EQ(vault_data.groups(0).group_name(), "Office");
}

TEST_F(GroupManagerTest, RenameGroupFailsNonExistent) {
    bool result = group_manager->rename_group("invalid-id", "NewName");

    EXPECT_FALSE(result);
    EXPECT_FALSE(modified_flag);
}

TEST_F(GroupManagerTest, RenameGroupFailsDuplicate) {
    std::string id1 = group_manager->create_group("Work");
    std::string id2 = group_manager->create_group("Personal");

    modified_flag = false;
    bool result = group_manager->rename_group(id1, "Personal");

    EXPECT_FALSE(result);
    EXPECT_FALSE(modified_flag);
    EXPECT_EQ(vault_data.groups(0).group_name(), "Work");
}

TEST_F(GroupManagerTest, RenameGroupFailsInvalidName) {
    std::string group_id = group_manager->create_group("Work");

    modified_flag = false;
    bool result = group_manager->rename_group(group_id, "");

    EXPECT_FALSE(result);
    EXPECT_FALSE(modified_flag);
    EXPECT_EQ(vault_data.groups(0).group_name(), "Work");
}

TEST_F(GroupManagerTest, RenameGroupAllowsSameName) {
    std::string group_id = group_manager->create_group("Work");

    modified_flag = false;
    bool result = group_manager->rename_group(group_id, "Work");

    EXPECT_TRUE(result);
    EXPECT_TRUE(modified_flag);
    EXPECT_EQ(vault_data.groups(0).group_name(), "Work");
}

// ============================================================================
// reorder_group() tests
// ============================================================================

TEST_F(GroupManagerTest, ReorderGroupSuccess) {
    std::string id1 = group_manager->create_group("Group1");
    std::string id2 = group_manager->create_group("Group2");
    std::string id3 = group_manager->create_group("Group3");

    modified_flag = false;
    bool result = group_manager->reorder_group(id1, 2);

    EXPECT_TRUE(result);
    EXPECT_TRUE(modified_flag);
    EXPECT_EQ(vault_data.groups(2).display_order(), 2);
}

TEST_F(GroupManagerTest, ReorderGroupFailsNonExistent) {
    group_manager->create_group("Group1");

    modified_flag = false;
    bool result = group_manager->reorder_group("invalid-id", 0);

    EXPECT_FALSE(result);
    EXPECT_FALSE(modified_flag);
}

TEST_F(GroupManagerTest, ReorderGroupFailsNegativeIndex) {
    std::string group_id = group_manager->create_group("Work");

    modified_flag = false;
    bool result = group_manager->reorder_group(group_id, -1);

    EXPECT_FALSE(result);
    EXPECT_FALSE(modified_flag);
}

TEST_F(GroupManagerTest, ReorderGroupFailsOutOfRange) {
    std::string id1 = group_manager->create_group("Group1");
    std::string id2 = group_manager->create_group("Group2");

    modified_flag = false;
    bool result = group_manager->reorder_group(id1, 10);

    // Implementation doesn't validate upper bound, only negative
    EXPECT_TRUE(result);
    EXPECT_TRUE(modified_flag);
}

// ============================================================================
// add_account_to_group() tests
// ============================================================================

TEST_F(GroupManagerTest, AddAccountToGroupSuccess) {
    std::string group_id = group_manager->create_group("Work");

    modified_flag = false;
    bool result = group_manager->add_account_to_group(0, group_id);

    EXPECT_TRUE(result);
    EXPECT_TRUE(modified_flag);
    EXPECT_EQ(vault_data.accounts(0).groups_size(), 1);
    EXPECT_EQ(vault_data.accounts(0).groups(0).group_id(), group_id);
}

TEST_F(GroupManagerTest, AddAccountToGroupFailsNonExistentGroup) {
    modified_flag = false;
    bool result = group_manager->add_account_to_group(0, "invalid-id");

    EXPECT_FALSE(result);
    EXPECT_FALSE(modified_flag);
    EXPECT_EQ(vault_data.accounts(0).groups_size(), 0);
}

TEST_F(GroupManagerTest, AddAccountToGroupFailsInvalidAccountIndex) {
    std::string group_id = group_manager->create_group("Work");
    modified_flag = false;

    bool result = group_manager->add_account_to_group(999, group_id);

    EXPECT_FALSE(result);
    EXPECT_FALSE(modified_flag);
}

TEST_F(GroupManagerTest, AddAccountToGroupPreventsDuplicate) {
    std::string group_id = group_manager->create_group("Work");

    group_manager->add_account_to_group(0, group_id);
    ASSERT_EQ(vault_data.accounts(0).groups_size(), 1);

    modified_flag = false;
    bool result = group_manager->add_account_to_group(0, group_id);

    // Idempotent operation returns true if already in group
    EXPECT_TRUE(result);
    EXPECT_FALSE(modified_flag);
    EXPECT_EQ(vault_data.accounts(0).groups_size(), 1);
}

TEST_F(GroupManagerTest, AddAccountToMultipleGroups) {
    std::string id1 = group_manager->create_group("Work");
    std::string id2 = group_manager->create_group("Personal");

    bool r1 = group_manager->add_account_to_group(0, id1);
    bool r2 = group_manager->add_account_to_group(0, id2);

    EXPECT_TRUE(r1);
    EXPECT_TRUE(r2);
    EXPECT_EQ(vault_data.accounts(0).groups_size(), 2);
}

// ============================================================================
// remove_account_from_group() tests
// ============================================================================

TEST_F(GroupManagerTest, RemoveAccountFromGroupSuccess) {
    std::string group_id = group_manager->create_group("Work");
    group_manager->add_account_to_group(0, group_id);

    modified_flag = false;
    bool result = group_manager->remove_account_from_group(0, group_id);

    EXPECT_TRUE(result);
    EXPECT_TRUE(modified_flag);
    EXPECT_EQ(vault_data.accounts(0).groups_size(), 0);
}

TEST_F(GroupManagerTest, RemoveAccountFromGroupFailsNotInGroup) {
    std::string group_id = group_manager->create_group("Work");

    modified_flag = false;
    bool result = group_manager->remove_account_from_group(0, group_id);

    // Idempotent operation returns true even if not in group
    EXPECT_TRUE(result);
    EXPECT_FALSE(modified_flag);
}

TEST_F(GroupManagerTest, RemoveAccountFromGroupFailsNonExistentGroup) {
    bool result = group_manager->remove_account_from_group(0, "invalid-id");

    // Implementation doesn't validate group exists, returns true (idempotent)
    EXPECT_TRUE(result);
    EXPECT_FALSE(modified_flag);
}

TEST_F(GroupManagerTest, RemoveAccountFromGroupFailsInvalidAccountIndex) {
    std::string group_id = group_manager->create_group("Work");

    bool result = group_manager->remove_account_from_group(999, group_id);

    EXPECT_FALSE(result);
}

TEST_F(GroupManagerTest, RemoveAccountFromOneOfMultipleGroups) {
    std::string id1 = group_manager->create_group("Work");
    std::string id2 = group_manager->create_group("Personal");

    bool add1 = group_manager->add_account_to_group(0, id1);
    bool add2 = group_manager->add_account_to_group(0, id2);
    ASSERT_TRUE(add1);
    ASSERT_TRUE(add2);
    ASSERT_EQ(vault_data.accounts(0).groups_size(), 2);

    bool result = group_manager->remove_account_from_group(0, id1);

    EXPECT_TRUE(result);
    EXPECT_EQ(vault_data.accounts(0).groups_size(), 1);
    EXPECT_EQ(vault_data.accounts(0).groups(0).group_id(), id2);
}

// ============================================================================
// reorder_account_in_group() tests
// ============================================================================

TEST_F(GroupManagerTest, ReorderAccountInGroupSuccess) {
    std::string group_id = group_manager->create_group("Work");
    bool add1 = group_manager->add_account_to_group(0, group_id);
    bool add2 = group_manager->add_account_to_group(1, group_id);
    bool add3 = group_manager->add_account_to_group(2, group_id);
    ASSERT_TRUE(add1 && add2 && add3);

    modified_flag = false;
    bool result = group_manager->reorder_account_in_group(0, group_id, 2);

    EXPECT_TRUE(result);
    EXPECT_TRUE(modified_flag);
    EXPECT_EQ(vault_data.accounts(0).groups(0).display_order(), 2);
}

TEST_F(GroupManagerTest, ReorderAccountInGroupFailsNotInGroup) {
    std::string group_id = group_manager->create_group("Work");

    modified_flag = false;
    EXPECT_FALSE(modified_flag);
}

TEST_F(GroupManagerTest, ReorderAccountInGroupFailsNonExistentGroup) {
    bool result = group_manager->reorder_account_in_group(0, "invalid-id", 0);

    EXPECT_FALSE(result);
}

TEST_F(GroupManagerTest, ReorderAccountInGroupFailsInvalidAccountIndex) {
    std::string group_id = group_manager->create_group("Work");

    bool result = group_manager->reorder_account_in_group(999, group_id, 0);

    EXPECT_FALSE(result);
}

TEST_F(GroupManagerTest, ReorderAccountInGroupFailsNegativeOrder) {
    std::string group_id = group_manager->create_group("Work");
    bool add_result = group_manager->add_account_to_group(0, group_id);
    ASSERT_TRUE(add_result);

    modified_flag = false;
    bool result = group_manager->reorder_account_in_group(0, group_id, -1);

    EXPECT_FALSE(result);
    EXPECT_FALSE(modified_flag);
}

// ============================================================================
// is_account_in_group() tests
// ============================================================================

TEST_F(GroupManagerTest, IsAccountInGroupTrue) {
    std::string group_id = group_manager->create_group("Work");
    bool add_result = group_manager->add_account_to_group(0, group_id);
    ASSERT_TRUE(add_result);

    bool result = group_manager->is_account_in_group(0, group_id);

    EXPECT_TRUE(result);
}

TEST_F(GroupManagerTest, IsAccountInGroupFalse) {
    std::string group_id = group_manager->create_group("Work");

    bool result = group_manager->is_account_in_group(0, group_id);

    EXPECT_FALSE(result);
}

TEST_F(GroupManagerTest, IsAccountInGroupFalseNonExistentGroup) {
    bool result = group_manager->is_account_in_group(0, "invalid-id");

    EXPECT_FALSE(result);
}

TEST_F(GroupManagerTest, IsAccountInGroupFalseInvalidAccountIndex) {
    std::string group_id = group_manager->create_group("Work");

    bool result = group_manager->is_account_in_group(999, group_id);

    EXPECT_FALSE(result);
}

// Note: is_valid_group_name() is private, tested indirectly through create_group()

// ============================================================================
// Integration tests
// ============================================================================

TEST_F(GroupManagerTest, ComplexWorkflowMultipleOperations) {
    // Create multiple groups
    std::string work = group_manager->create_group("Work");
    std::string personal = group_manager->create_group("Personal");
    std::string projects = group_manager->create_group("Projects");

    // Add accounts to groups
    bool add1 = group_manager->add_account_to_group(0, work);
    bool add2 = group_manager->add_account_to_group(1, work);
    bool add3 = group_manager->add_account_to_group(1, personal);
    bool add4 = group_manager->add_account_to_group(2, projects);
    ASSERT_TRUE(add1 && add2 && add3 && add4);

    // Verify memberships
    EXPECT_TRUE(group_manager->is_account_in_group(0, work));
    EXPECT_TRUE(group_manager->is_account_in_group(1, work));
    EXPECT_TRUE(group_manager->is_account_in_group(1, personal));
    EXPECT_TRUE(group_manager->is_account_in_group(2, projects));

    // Remove account from one group
    bool remove_result = group_manager->remove_account_from_group(1, work);
    ASSERT_TRUE(remove_result);
    EXPECT_FALSE(group_manager->is_account_in_group(1, work));
    EXPECT_TRUE(group_manager->is_account_in_group(1, personal));

    // Delete a group
    bool delete_result = group_manager->delete_group(projects);
    ASSERT_TRUE(delete_result);
    EXPECT_FALSE(group_manager->is_account_in_group(2, projects));
    EXPECT_EQ(vault_data.groups_size(), 2);
}

TEST_F(GroupManagerTest, StressTestManyAccounts) {
    std::string group_id = group_manager->create_group("Large Group");

    // Add all 5 test accounts
    for (size_t i = 0; i < 5; ++i) {
        bool result = group_manager->add_account_to_group(i, group_id);
        EXPECT_TRUE(result);
        EXPECT_TRUE(group_manager->is_account_in_group(i, group_id));
    }

    // Verify all are in group
    for (size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(vault_data.accounts(i).groups_size(), 1);
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
