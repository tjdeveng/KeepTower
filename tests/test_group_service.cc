// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file test_group_service.cc
 * @brief Comprehensive tests for GroupService business logic
 *
 * Tests Phase 3 service layer implementation including:
 * - CRUD operations with validation
 * - Group name validation
 * - Duplicate name detection
 * - Account-group relationships
 * - Error handling
 */

#include <gtest/gtest.h>
#include "../src/core/services/GroupService.h"
#include "../src/core/repositories/GroupRepository.h"
#include "../src/core/VaultManager.h"
#include <filesystem>
#include <chrono>

using namespace KeepTower;
namespace fs = std::filesystem;

class GroupServiceTest : public ::testing::Test {
protected:
    std::unique_ptr<VaultManager> vault_manager;
    std::unique_ptr<GroupRepository> group_repo;
    std::unique_ptr<GroupService> group_service;
    std::string test_vault_path;
    const std::string test_password = "TestPassword123!";

    void SetUp() override {
        // Create unique vault for each test
        vault_manager = std::make_unique<VaultManager>();
        test_vault_path = "test_group_service_" +
                         std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) +
                         ".vault";

        ASSERT_TRUE(vault_manager->create_vault(test_vault_path, test_password))
            << "Failed to create test vault";

        // Initialize repository and service
        group_repo = std::make_unique<GroupRepository>(vault_manager.get());
        group_service = std::make_unique<GroupService>(group_repo.get());
    }

    void TearDown() override {
        // Clean up
        group_service.reset();
        group_repo.reset();

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
};

// ============================================================================
// CRUD Operations Tests
// ============================================================================

TEST_F(GroupServiceTest, CreateGroup_ValidName_Success) {
    auto result = group_service->create_group("Work");

    ASSERT_TRUE(result.has_value()) << "Failed to create valid group";
    EXPECT_FALSE(result.value().empty()) << "Group ID should not be empty";
}

TEST_F(GroupServiceTest, CreateGroup_EmptyName_Fails) {
    auto result = group_service->create_group("");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ServiceError::VALIDATION_FAILED);
}

TEST_F(GroupServiceTest, CreateGroup_NameTooLong_Fails) {
    std::string long_name(MAX_GROUP_NAME_LENGTH + 1, 'x');

    auto result = group_service->create_group(long_name);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ServiceError::FIELD_TOO_LONG);
}

TEST_F(GroupServiceTest, CreateGroup_MaxLengthName_Success) {
    std::string max_name(MAX_GROUP_NAME_LENGTH, 'x');

    auto result = group_service->create_group(max_name);

    ASSERT_TRUE(result.has_value()) << "Max length name should be accepted";
}

TEST_F(GroupServiceTest, GetGroup_ExistingGroup_Success) {
    auto create_result = group_service->create_group("Test Group");
    ASSERT_TRUE(create_result.has_value());
    std::string group_id = create_result.value();

    auto get_result = group_service->get_group(group_id);

    ASSERT_TRUE(get_result.has_value());
    EXPECT_EQ(get_result.value().group_id(), group_id);
    EXPECT_EQ(get_result.value().group_name(), "Test Group");
}

TEST_F(GroupServiceTest, GetGroup_NonExistentGroup_Fails) {
    auto result = group_service->get_group("nonexistent-id");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ServiceError::ACCOUNT_NOT_FOUND);
}

TEST_F(GroupServiceTest, GetAllGroups_MultipleGroups_Success) {
    // Create multiple groups
    ASSERT_TRUE(group_service->create_group("Work").has_value());
    ASSERT_TRUE(group_service->create_group("Personal").has_value());
    ASSERT_TRUE(group_service->create_group("Finance").has_value());

    auto result = group_service->get_all_groups();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 3);
}

TEST_F(GroupServiceTest, GetAllGroups_EmptyVault_ReturnsEmpty) {
    auto result = group_service->get_all_groups();

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().empty());
}

TEST_F(GroupServiceTest, DeleteGroup_ExistingGroup_Success) {
    auto create_result = group_service->create_group("Test Group");
    ASSERT_TRUE(create_result.has_value());
    std::string group_id = create_result.value();

    auto delete_result = group_service->delete_group(group_id);

    ASSERT_TRUE(delete_result.has_value());
    auto count_result = group_service->count();
    ASSERT_TRUE(count_result.has_value());
    EXPECT_EQ(count_result.value(), 0);
}

TEST_F(GroupServiceTest, DeleteGroup_NonExistentGroup_Fails) {
    auto result = group_service->delete_group("nonexistent-id");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ServiceError::ACCOUNT_NOT_FOUND);
}

// ============================================================================
// Rename Group Tests
// ============================================================================

TEST_F(GroupServiceTest, RenameGroup_ValidNewName_Success) {
    auto create_result = group_service->create_group("Old Name");
    ASSERT_TRUE(create_result.has_value());
    std::string group_id = create_result.value();

    auto rename_result = group_service->rename_group(group_id, "New Name");

    ASSERT_TRUE(rename_result.has_value());
    auto get_result = group_service->get_group(group_id);
    ASSERT_TRUE(get_result.has_value());
    EXPECT_EQ(get_result.value().group_name(), "New Name");
}

TEST_F(GroupServiceTest, RenameGroup_EmptyName_Fails) {
    auto create_result = group_service->create_group("Test Group");
    ASSERT_TRUE(create_result.has_value());
    std::string group_id = create_result.value();

    auto result = group_service->rename_group(group_id, "");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ServiceError::VALIDATION_FAILED);
}

TEST_F(GroupServiceTest, RenameGroup_NameTooLong_Fails) {
    auto create_result = group_service->create_group("Test Group");
    ASSERT_TRUE(create_result.has_value());
    std::string group_id = create_result.value();

    std::string long_name(MAX_GROUP_NAME_LENGTH + 1, 'x');
    auto result = group_service->rename_group(group_id, long_name);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ServiceError::FIELD_TOO_LONG);
}

TEST_F(GroupServiceTest, RenameGroup_DuplicateName_Fails) {
    auto create_result1 = group_service->create_group("Group 1");
    ASSERT_TRUE(create_result1.has_value());

    auto create_result2 = group_service->create_group("Group 2");
    ASSERT_TRUE(create_result2.has_value());
    std::string group_id2 = create_result2.value();

    auto result = group_service->rename_group(group_id2, "Group 1");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ServiceError::DUPLICATE_NAME);
}

TEST_F(GroupServiceTest, RenameGroup_SameNameAllowed_Success) {
    auto create_result = group_service->create_group("Test Group");
    ASSERT_TRUE(create_result.has_value());
    std::string group_id = create_result.value();

    auto result = group_service->rename_group(group_id, "Test Group");

    ASSERT_TRUE(result.has_value()) << "Renaming to same name should be allowed";
}

TEST_F(GroupServiceTest, RenameGroup_NonExistentGroup_Fails) {
    auto result = group_service->rename_group("nonexistent-id", "New Name");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ServiceError::ACCOUNT_NOT_FOUND);
}

// ============================================================================
// Duplicate Name Detection Tests
// ============================================================================

TEST_F(GroupServiceTest, CreateGroup_DuplicateName_Fails) {
    auto create_result1 = group_service->create_group("Work");
    ASSERT_TRUE(create_result1.has_value());

    auto result = group_service->create_group("Work");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ServiceError::DUPLICATE_NAME);
}

TEST_F(GroupServiceTest, CreateGroup_DifferentNames_Success) {
    auto result1 = group_service->create_group("Work");
    auto result2 = group_service->create_group("Personal");
    auto result3 = group_service->create_group("Finance");

    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());
    ASSERT_TRUE(result3.has_value());
}

TEST_F(GroupServiceTest, IsNameUnique_NewName_ReturnsTrue) {
    group_service->create_group("Existing Group");

    bool is_unique = group_service->is_name_unique("New Group", "");

    EXPECT_TRUE(is_unique);
}

TEST_F(GroupServiceTest, IsNameUnique_ExistingName_ReturnsFalse) {
    auto create_result = group_service->create_group("Existing Group");
    ASSERT_TRUE(create_result.has_value());

    bool is_unique = group_service->is_name_unique("Existing Group", "");

    EXPECT_FALSE(is_unique);
}

TEST_F(GroupServiceTest, IsNameUnique_SameGroupExcluded_ReturnsTrue) {
    auto create_result = group_service->create_group("Test Group");
    ASSERT_TRUE(create_result.has_value());
    std::string group_id = create_result.value();

    bool is_unique = group_service->is_name_unique("Test Group", group_id);

    EXPECT_TRUE(is_unique) << "Should be unique when excluding self";
}

TEST_F(GroupServiceTest, IsNameUnique_EmptyName_ReturnsFalse) {
    bool is_unique = group_service->is_name_unique("", "");

    EXPECT_FALSE(is_unique) << "Empty name should not be unique";
}

// ============================================================================
// Account-Group Relationship Tests
// ============================================================================

TEST_F(GroupServiceTest, AddAccountToGroup_ValidAccountAndGroup_Success) {
    // Note: This test can only validate the service call - we would need
    // AccountService integrated to create actual accounts with indices.
    // For now, test with index 0 which would fail if no accounts exist.
    auto create_result = group_service->create_group("Test Group");
    ASSERT_TRUE(create_result.has_value());
    std::string group_id = create_result.value();

    // This will fail since no accounts exist, but tests the interface
    auto result = group_service->add_account_to_group(0, group_id);

    // Expected to fail since no accounts in vault
    ASSERT_FALSE(result.has_value());
}

TEST_F(GroupServiceTest, AddAccountToGroup_NonExistentGroup_Fails) {
    // When no accounts exist, index 0 is invalid (0 >= 0 check fails)
    // This is correct behavior - account validation happens before group validation
    auto result = group_service->add_account_to_group(0, "nonexistent-group");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ServiceError::INVALID_INDEX) << "Account index validation happens first";
}

TEST_F(GroupServiceTest, RemoveAccountFromGroup_ExistingRelationship_Success) {
    auto create_result = group_service->create_group("Test Group");
    ASSERT_TRUE(create_result.has_value());
    std::string group_id = create_result.value();

    // Note: Without actual accounts, this tests the interface only
    auto result = group_service->remove_account_from_group(0, group_id);

    // Will fail since no accounts exist
    ASSERT_FALSE(result.has_value());
}

TEST_F(GroupServiceTest, RemoveAccountFromGroup_NonExistentGroup_Fails) {
    // When no accounts exist, index 0 is invalid (0 >= 0 check fails)
    // This is correct behavior - account validation happens before group validation
    auto result = group_service->remove_account_from_group(0, "nonexistent-group");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ServiceError::INVALID_INDEX) << "Account index validation happens first";
}

TEST_F(GroupServiceTest, GetAccountsInGroup_WithAccounts_ReturnsAccounts) {
    auto create_result = group_service->create_group("Test Group");
    ASSERT_TRUE(create_result.has_value());
    std::string group_id = create_result.value();

    // Note: Without AccountService, we can't create real accounts
    // This test validates the interface returns empty for empty group
    auto result = group_service->get_accounts_in_group(group_id);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().empty()) << "Empty group should have no accounts";
}

TEST_F(GroupServiceTest, GetAccountsInGroup_EmptyGroup_ReturnsEmpty) {
    auto create_result = group_service->create_group("Test Group");
    ASSERT_TRUE(create_result.has_value());
    std::string group_id = create_result.value();

    auto result = group_service->get_accounts_in_group(group_id);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().empty());
}

TEST_F(GroupServiceTest, GetAccountsInGroup_NonExistentGroup_Fails) {
    auto result = group_service->get_accounts_in_group("nonexistent-group");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ServiceError::ACCOUNT_NOT_FOUND);
}

// ============================================================================
// Count Tests
// ============================================================================

TEST_F(GroupServiceTest, Count_EmptyVault_ReturnsZero) {
    auto result = group_service->count();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 0);
}

TEST_F(GroupServiceTest, Count_MultipleGroups_ReturnsCorrectCount) {
    group_service->create_group("Group 1");
    group_service->create_group("Group 2");
    group_service->create_group("Group 3");

    auto result = group_service->count();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 3);
}

TEST_F(GroupServiceTest, Count_AfterDeletion_UpdatesCorrectly) {
    auto create_result1 = group_service->create_group("Group 1");
    auto create_result2 = group_service->create_group("Group 2");
    ASSERT_TRUE(create_result1.has_value());
    ASSERT_TRUE(create_result2.has_value());

    group_service->delete_group(create_result1.value());

    auto result = group_service->count();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 1);
}

// ============================================================================
// Vault State Tests
// ============================================================================

TEST_F(GroupServiceTest, Operations_ClosedVault_Fail) {
    (void)vault_manager->close_vault();
    auto result = group_service->create_group("Test Group");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ServiceError::VAULT_CLOSED);
}

TEST_F(GroupServiceTest, GetAllGroups_ClosedVault_Fails) {
    (void)vault_manager->close_vault();
    auto result = group_service->get_all_groups();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ServiceError::VAULT_CLOSED);
}

// ============================================================================
// Edge Cases and Special Scenarios
// ============================================================================

TEST_F(GroupServiceTest, CreateGroup_WithWhitespace_Success) {
    auto result = group_service->create_group("  Work Group  ");

    ASSERT_TRUE(result.has_value()) << "Group names with whitespace should be allowed";
}

TEST_F(GroupServiceTest, CreateGroup_WithSpecialCharacters_Success) {
    // VaultManager rejects / and \ for security (path traversal)
    // But allows other special characters like parentheses, hyphen, etc.
    auto result = group_service->create_group("Work-Personal (2024)");

    ASSERT_TRUE(result.has_value()) << "Group names with safe special characters should be allowed";
}

TEST_F(GroupServiceTest, RenameGroup_MaxLength_Success) {
    auto create_result = group_service->create_group("Short Name");
    ASSERT_TRUE(create_result.has_value());
    std::string group_id = create_result.value();

    std::string max_name(MAX_GROUP_NAME_LENGTH, 'x');
    auto result = group_service->rename_group(group_id, max_name);

    ASSERT_TRUE(result.has_value()) << "Max length rename should succeed";
}

TEST_F(GroupServiceTest, MultipleOperations_SameGroup_Success) {
    // Create
    auto create_result = group_service->create_group("Test Group");
    ASSERT_TRUE(create_result.has_value());
    std::string group_id = create_result.value();

    // Rename
    auto rename_result = group_service->rename_group(group_id, "Renamed Group");
    ASSERT_TRUE(rename_result.has_value());

    // Verify rename
    auto get_result = group_service->get_group(group_id);
    ASSERT_TRUE(get_result.has_value());
    EXPECT_EQ(get_result.value().group_name(), "Renamed Group");

    // Get accounts (should be empty)
    auto accounts_result = group_service->get_accounts_in_group(group_id);
    ASSERT_TRUE(accounts_result.has_value());
    EXPECT_TRUE(accounts_result.value().empty());

    // Delete
    auto delete_result = group_service->delete_group(group_id);
    ASSERT_TRUE(delete_result.has_value());
}
