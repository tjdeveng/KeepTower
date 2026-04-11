// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file test_vault_manager_v2.cc
 * @brief Integration tests for V2 vault authentication and user management
 *
 * Tests Phase 2 implementation:
 * - V2 vault creation with security policy
 * - User authentication (open_vault_v2)
 * - User management (add/remove/change password)
 * - Permission enforcement
 * - Password change enforcement workflow
 */

#include <gtest/gtest.h>
#include <glibmm/main.h>
#include "../src/core/VaultManager.h"
#include "../src/core/MultiUserTypes.h"
#include "../src/core/services/VaultFileService.h"
#include <chrono>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <thread>

using namespace KeepTower;

class VaultManagerV2Test : public ::testing::Test {
protected:
    void SetUp() override {
        test_vault_path = std::filesystem::temp_directory_path() / "test_v2_vault.vault";
        cleanup_test_vault();
    }

    void TearDown() override {
        cleanup_test_vault();
    }

    void cleanup_test_vault() {
        if (std::filesystem::exists(test_vault_path)) {
            std::filesystem::remove(test_vault_path);
        }
    }

    std::filesystem::path test_vault_path;
    VaultManager vault_manager;
};

namespace {
bool pump_main_context_until(
    const std::function<bool()>& predicate,
    std::chrono::milliseconds timeout = std::chrono::seconds(5)) {
    auto context = Glib::MainContext::get_default();
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            while (context->iteration(false)) {
            }
            return true;
        }

        while (context->iteration(false)) {
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    while (context->iteration(false)) {
    }
    return predicate();
}

AccountDetail make_v2_account_detail(
    const std::string& id,
    const std::string& account_name,
    bool admin_only_viewable = false,
    bool admin_only_deletable = false) {
    AccountDetail detail;
    detail.id = id;
    detail.account_name = account_name;
    detail.user_name = id + "-user";
    detail.password = "StoredSecret123!";
    detail.email = id + "@example.com";
    detail.website = "https://" + id + ".example.com";
    detail.notes = "notes";
    detail.is_admin_only_viewable = admin_only_viewable;
    detail.is_admin_only_deletable = admin_only_deletable;
    return detail;
}

std::vector<uint8_t> read_file_bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(input),
                                std::istreambuf_iterator<char>());
}

void write_file_bytes(const std::filesystem::path& path, const std::vector<uint8_t>& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}
}

// ============================================================================
// V2 Vault Creation Tests
// ============================================================================

TEST_F(VaultManagerV2Test, CreateV2VaultBasic) {
    VaultSecurityPolicy policy;
    policy.require_yubikey = false;
    policy.min_password_length = 12;
    policy.pbkdf2_iterations = 100000;

    auto result = vault_manager.create_vault_v2(
        test_vault_path.string(),
        "admin",
        "adminpass123",
        policy);

    ASSERT_TRUE(result) << "Failed to create V2 vault";
    EXPECT_TRUE(std::filesystem::exists(test_vault_path));

    // Verify vault can be closed
    EXPECT_TRUE(vault_manager.close_vault());
}

TEST_F(VaultManagerV2Test, CreateV2VaultRejectsShortPassword) {
    VaultSecurityPolicy policy;
    policy.min_password_length = 12;

    auto result = vault_manager.create_vault_v2(
        test_vault_path.string(),
        "admin",
        "short",  // Only 5 chars
        policy);

    EXPECT_FALSE(result);
    EXPECT_EQ(result.error(), VaultError::WeakPassword);
}

TEST_F(VaultManagerV2Test, CreateV2VaultRejectsEmptyUsername) {
    VaultSecurityPolicy policy;

    auto result = vault_manager.create_vault_v2(
        test_vault_path.string(),
        "",  // Empty username
        "validpassword123",
        policy);

    EXPECT_FALSE(result);
    EXPECT_EQ(result.error(), VaultError::InvalidUsername);
}

TEST_F(VaultManagerV2Test, CreateV2VaultAsyncInitializesStateAndInvokesCompletion) {
    VaultSecurityPolicy policy;
    policy.min_password_length = 10;
    policy.pbkdf2_iterations = 100000;

    std::atomic<bool> completion_called{false};
    KeepTower::VaultResult<> completion_result = {};

    vault_manager.create_vault_v2_async(
        test_vault_path.string(),
        "admin",
        "adminpass123",
        policy,
        nullptr,
        [&](KeepTower::VaultResult<> result) {
            completion_result = result;
            completion_called.store(true);
        });

    ASSERT_TRUE(pump_main_context_until([&completion_called]() {
        return completion_called.load();
    }));

    ASSERT_TRUE(completion_result);
    EXPECT_TRUE(std::filesystem::exists(test_vault_path));

    auto session = vault_manager.get_current_user_session();
    ASSERT_TRUE(session);
    EXPECT_EQ(session->username, "admin");
    EXPECT_EQ(session->role, UserRole::ADMINISTRATOR);
    EXPECT_FALSE(session->password_change_required);

    auto loaded_policy = vault_manager.get_vault_security_policy();
    ASSERT_TRUE(loaded_policy);
    EXPECT_EQ(loaded_policy->min_password_length, 10);
}

TEST_F(VaultManagerV2Test, CreateV2VaultReplacesExistingOpenVault) {
    const auto second_vault_path = std::filesystem::temp_directory_path() / "test_v2_vault_second.vault";
    if (std::filesystem::exists(second_vault_path)) {
        std::filesystem::remove(second_vault_path);
    }

    VaultSecurityPolicy first_policy;
    first_policy.min_password_length = 8;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "alice", "alicepass123", first_policy));

    VaultSecurityPolicy second_policy;
    second_policy.min_password_length = 14;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        second_vault_path.string(), "bob", "bobpassword12345", second_policy));

    auto session = vault_manager.get_current_user_session();
    ASSERT_TRUE(session);
    EXPECT_EQ(session->username, "bob");

    auto loaded_policy = vault_manager.get_vault_security_policy();
    ASSERT_TRUE(loaded_policy);
    EXPECT_EQ(loaded_policy->min_password_length, 14);

    EXPECT_TRUE(std::filesystem::exists(second_vault_path));
    EXPECT_TRUE(vault_manager.close_vault());
    std::filesystem::remove(second_vault_path);
}

// ============================================================================
// V2 Authentication Tests
// ============================================================================

TEST_F(VaultManagerV2Test, OpenV2VaultSuccessful) {
    // Create vault
    VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "alice", "validpass123", policy));
    ASSERT_TRUE(vault_manager.close_vault());

    // Open vault
    auto session = vault_manager.open_vault_v2(
        test_vault_path.string(), "alice", "validpass123");

    ASSERT_TRUE(session) << "Failed to open V2 vault";
    EXPECT_EQ(session->username, "alice");
    EXPECT_EQ(session->role, UserRole::ADMINISTRATOR);
    EXPECT_FALSE(session->password_change_required);
}

TEST_F(VaultManagerV2Test, ClosedVaultSessionAndPolicyAccessorsReturnEmpty) {
    EXPECT_FALSE(vault_manager.get_current_user_session().has_value());
    EXPECT_FALSE(vault_manager.get_vault_security_policy().has_value());
    EXPECT_TRUE(vault_manager.list_users().empty());
}

TEST_F(VaultManagerV2Test, OpenV2VaultWrongPassword) {
    // Create vault
    VaultSecurityPolicy policy;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "alice", "validpass123", policy));
    ASSERT_TRUE(vault_manager.close_vault());

    // Try wrong password
    auto session = vault_manager.open_vault_v2(
        test_vault_path.string(), "alice", "wrongpassword");

    EXPECT_FALSE(session);
    EXPECT_EQ(session.error(), VaultError::AuthenticationFailed);
}

TEST_F(VaultManagerV2Test, OpenV2VaultFlagsEnrollmentWhenPolicyRequiresYubiKey) {
    VaultSecurityPolicy policy;
    policy.min_password_length = 8;

    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "validpass123", policy));
    ASSERT_TRUE(vault_manager.add_user("bob", "bobpassword123", UserRole::STANDARD_USER, false));
    policy.require_yubikey = true;
    ASSERT_TRUE(vault_manager.update_security_policy(policy));
    ASSERT_TRUE(vault_manager.save_vault());
    ASSERT_TRUE(vault_manager.close_vault());

    auto session = vault_manager.open_vault_v2(
        test_vault_path.string(), "bob", "bobpassword123");

    ASSERT_TRUE(session);
    EXPECT_TRUE(session->requires_yubikey_enrollment);
    EXPECT_FALSE(session->password_change_required);
}

TEST_F(VaultManagerV2Test, OpenV2VaultRejectsTruncatedEncryptedPayload) {
    VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "alice", "validpass123", policy));
    ASSERT_TRUE(vault_manager.close_vault());

    auto file_data = read_file_bytes(test_vault_path);
    auto metadata = KeepTower::VaultFileService::read_v2_metadata(file_data);
    ASSERT_TRUE(metadata);
    ASSERT_LT(0u, metadata->data_offset);

    file_data.resize(metadata->data_offset);
    write_file_bytes(test_vault_path, file_data);

    auto session = vault_manager.open_vault_v2(
        test_vault_path.string(), "alice", "validpass123");

    EXPECT_FALSE(session);
    EXPECT_EQ(session.error(), VaultError::CorruptedFile);
}

TEST_F(VaultManagerV2Test, OpenV2VaultRejectsTamperedCiphertext) {
    VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "alice", "validpass123", policy));
    ASSERT_TRUE(vault_manager.close_vault());

    auto file_data = read_file_bytes(test_vault_path);
    auto metadata = KeepTower::VaultFileService::read_v2_metadata(file_data);
    ASSERT_TRUE(metadata);
    ASSERT_LT(metadata->data_offset, file_data.size());

    file_data[metadata->data_offset] ^= 0x01;
    write_file_bytes(test_vault_path, file_data);

    auto session = vault_manager.open_vault_v2(
        test_vault_path.string(), "alice", "validpass123");

    EXPECT_FALSE(session);
    EXPECT_EQ(session.error(), VaultError::DecryptionFailed);
}

TEST_F(VaultManagerV2Test, BackupEnabledPersistsAcrossReopen) {
    // Create vault
    VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "alice", "validpass123", policy));
    ASSERT_TRUE(vault_manager.close_vault());

    // Open vault
    ASSERT_TRUE(vault_manager.open_vault_v2(
        test_vault_path.string(), "alice", "validpass123"));

    // Disable backups and save
    const VaultManager::BackupSettings backup_settings_before_save{false, 5, ""};
    ASSERT_TRUE(vault_manager.apply_backup_settings(backup_settings_before_save));
    EXPECT_FALSE(vault_manager.get_backup_settings().enabled);
    ASSERT_TRUE(vault_manager.save_vault());
    ASSERT_TRUE(vault_manager.close_vault());

    // Change in-memory defaults before reopen so open_vault_v2 must reload from disk.
    const VaultManager::BackupSettings stale_in_memory_settings{true, 3, ""};
    ASSERT_TRUE(vault_manager.apply_backup_settings(stale_in_memory_settings));

    // Reopen and verify setting persisted
    ASSERT_TRUE(vault_manager.open_vault_v2(
        test_vault_path.string(), "alice", "validpass123"));
    const VaultManager::BackupSettings loaded_settings = vault_manager.get_backup_settings();
    EXPECT_FALSE(loaded_settings.enabled);
    EXPECT_EQ(loaded_settings.count, 5);
}

TEST_F(VaultManagerV2Test, BackupPathRemainsRuntimeLocalAcrossReopen) {
    VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "alice", "validpass123", policy));
    ASSERT_TRUE(vault_manager.close_vault());

    ASSERT_TRUE(vault_manager.open_vault_v2(
        test_vault_path.string(), "alice", "validpass123"));

    VaultManager::BackupSettings settings = vault_manager.get_backup_settings();
    settings.path = "/tmp/keeptower-backup-path-a";
    settings.enabled = true;
    settings.count = 5;
    ASSERT_TRUE(vault_manager.apply_backup_settings(settings));
    ASSERT_TRUE(vault_manager.save_vault());
    ASSERT_TRUE(vault_manager.close_vault());

    // Simulate app/runtime preference change before reopen.
    VaultManager::BackupSettings runtime_settings = vault_manager.get_backup_settings();
    runtime_settings.path = "/tmp/keeptower-backup-path-b";
    ASSERT_TRUE(vault_manager.apply_backup_settings(runtime_settings));

    ASSERT_TRUE(vault_manager.open_vault_v2(
        test_vault_path.string(), "alice", "validpass123"));

    const VaultManager::BackupSettings reopened = vault_manager.get_backup_settings();
    EXPECT_EQ(reopened.path, "/tmp/keeptower-backup-path-b");
    EXPECT_TRUE(reopened.enabled);
    EXPECT_EQ(reopened.count, 5);
}

TEST_F(VaultManagerV2Test, OpenV2VaultNonExistentUser) {
    // Create vault
    VaultSecurityPolicy policy;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "alice", "validpass123", policy));
    ASSERT_TRUE(vault_manager.close_vault());

    // Try non-existent user
    auto session = vault_manager.open_vault_v2(
        test_vault_path.string(), "bob", "validpass123");

    EXPECT_FALSE(session);
    EXPECT_EQ(session.error(), VaultError::AuthenticationFailed);
}

TEST_F(VaultManagerV2Test, OpenV2VaultReplacesExistingOpenVault) {
    const auto second_vault_path = std::filesystem::temp_directory_path() / "test_v2_vault_second.vault";
    if (std::filesystem::exists(second_vault_path)) {
        std::filesystem::remove(second_vault_path);
    }

    VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "alice", "alicepass123", policy));
    ASSERT_TRUE(vault_manager.close_vault());

    ASSERT_TRUE(vault_manager.create_vault_v2(
        second_vault_path.string(), "bob", "bobpassword12345", policy));
    ASSERT_TRUE(vault_manager.close_vault());

    ASSERT_TRUE(vault_manager.open_vault_v2(
        test_vault_path.string(), "alice", "alicepass123"));

    auto second_session = vault_manager.open_vault_v2(
        second_vault_path.string(), "bob", "bobpassword12345");
    ASSERT_TRUE(second_session);
    EXPECT_EQ(second_session->username, "bob");

    auto current_session = vault_manager.get_current_user_session();
    ASSERT_TRUE(current_session);
    EXPECT_EQ(current_session->username, "bob");

    EXPECT_TRUE(vault_manager.close_vault());
    std::filesystem::remove(second_vault_path);
}

// ============================================================================
// User Management Tests
// ============================================================================

TEST_F(VaultManagerV2Test, AddUserSuccessful) {
    // Create vault with admin
    VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpass123", policy));

    // Add new user
    auto result = vault_manager.add_user("bob", "temppass1234", UserRole::STANDARD_USER, true);
    ASSERT_TRUE(result) << "Failed to add user";

    // Save and verify
    EXPECT_TRUE(vault_manager.save_vault());

    // List users
    auto users = vault_manager.list_users();
    ASSERT_EQ(users.size(), 2);

    // Find bob
    auto bob_it = std::find_if(users.begin(), users.end(),
        [](const KeySlot& slot) { return slot.username == "bob"; });
    ASSERT_NE(bob_it, users.end());
    EXPECT_EQ(bob_it->role, UserRole::STANDARD_USER);
    EXPECT_TRUE(bob_it->must_change_password);
}

TEST_F(VaultManagerV2Test, AddUserRequiresOpenVault) {
    auto result = vault_manager.add_user("bob", "temppass1234", UserRole::STANDARD_USER);
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error(), VaultError::VaultNotOpen);
}

TEST_F(VaultManagerV2Test, AddUserRejectsEmptyUsername) {
    VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpass123", policy));

    auto result = vault_manager.add_user("", "temppass1234", UserRole::STANDARD_USER);
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error(), VaultError::InvalidUsername);
}

TEST_F(VaultManagerV2Test, AddUserSeedsInitialPasswordHistoryWhenEnabled) {
    VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    policy.password_history_depth = 3;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpass123", policy));

    ASSERT_TRUE(vault_manager.add_user("bob", "bobpass12345", UserRole::STANDARD_USER, false));

    const auto users = vault_manager.list_users();
    const auto bob_it = std::find_if(users.begin(), users.end(),
        [](const KeySlot& slot) { return slot.username == "bob"; });
    ASSERT_NE(bob_it, users.end());
    EXPECT_FALSE(bob_it->must_change_password);
    ASSERT_EQ(bob_it->password_history.size(), 1u);

    ASSERT_TRUE(vault_manager.save_vault());
    ASSERT_TRUE(vault_manager.close_vault());
    ASSERT_TRUE(vault_manager.open_vault_v2(
        test_vault_path.string(), "bob", "bobpass12345"));

    auto validate_result = vault_manager.validate_new_password("bob", "bobpass12345");
    EXPECT_FALSE(validate_result);
    EXPECT_EQ(validate_result.error(), VaultError::PasswordReused);
}

TEST_F(VaultManagerV2Test, AddUserRequiresAdminPermission) {
    // Create vault
    VaultSecurityPolicy policy;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpass123", policy));

    // Add standard user
    ASSERT_TRUE(vault_manager.add_user("bob", "temppass1234", UserRole::STANDARD_USER));
    ASSERT_TRUE(vault_manager.save_vault());
    ASSERT_TRUE(vault_manager.close_vault());

    // Login as standard user
    ASSERT_TRUE(vault_manager.open_vault_v2(
        test_vault_path.string(), "bob", "temppass1234"));

    // Try to add user (should fail - not admin)
    auto result = vault_manager.add_user("charlie", "temppass4567", UserRole::STANDARD_USER);
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error(), VaultError::PermissionDenied);
}

TEST_F(VaultManagerV2Test, AddUserRejectsDuplicateUsername) {
    // Create vault
    VaultSecurityPolicy policy;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpass123", policy));

    // Add user
    ASSERT_TRUE(vault_manager.add_user("bob", "temppass1234", UserRole::STANDARD_USER));

    // Try to add same username again
    auto result = vault_manager.add_user("bob", "anotherpass", UserRole::STANDARD_USER);
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error(), VaultError::UserAlreadyExists);
}

TEST_F(VaultManagerV2Test, RemoveUserSuccessful) {
    // Create vault and add user
    VaultSecurityPolicy policy;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpass123", policy));
    ASSERT_TRUE(vault_manager.add_user("bob", "temppass1234", UserRole::STANDARD_USER));

    // Remove user
    auto result = vault_manager.remove_user("bob");
    ASSERT_TRUE(result) << "Failed to remove user";

    // Verify removed
    auto users = vault_manager.list_users();
    EXPECT_EQ(users.size(), 1);  // Only admin left
    EXPECT_EQ(users[0].username, "admin");
}

TEST_F(VaultManagerV2Test, RemoveUserPersistsDeactivationAcrossReopen) {
    VaultSecurityPolicy policy;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpass123", policy));
    ASSERT_TRUE(vault_manager.add_user("bob", "temppass1234", UserRole::STANDARD_USER));

    ASSERT_TRUE(vault_manager.remove_user("bob"));
    ASSERT_TRUE(vault_manager.save_vault());
    ASSERT_TRUE(vault_manager.close_vault());

    auto removed_user_session = vault_manager.open_vault_v2(
        test_vault_path.string(), "bob", "temppass1234");
    EXPECT_FALSE(removed_user_session);

    auto admin_session = vault_manager.open_vault_v2(
        test_vault_path.string(), "admin", "adminpass123");
    ASSERT_TRUE(admin_session);

    const auto users = vault_manager.list_users();
    ASSERT_EQ(users.size(), 1u);
    EXPECT_EQ(users.front().username, "admin");
}

TEST_F(VaultManagerV2Test, RemoveUserPreventsSelfRemoval) {
    // Create vault
    VaultSecurityPolicy policy;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpass123", policy));

    // Try to remove self
    auto result = vault_manager.remove_user("admin");
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error(), VaultError::SelfRemovalNotAllowed);
}

TEST_F(VaultManagerV2Test, RemoveUserPreventsLastAdmin) {
    // Create vault
    VaultSecurityPolicy policy;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpass123", policy));

    // Add standard user
    ASSERT_TRUE(vault_manager.add_user("bob", "temppass1234", UserRole::STANDARD_USER));

    // Try to remove last admin (self-removal prevented first)
    auto result = vault_manager.remove_user("admin");
    EXPECT_FALSE(result);
}

TEST_F(VaultManagerV2Test, RemoveUserAllowsMultipleAdmins) {
    // Create vault
    VaultSecurityPolicy policy;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin1", "adminpass123", policy));

    // Add second admin
    ASSERT_TRUE(vault_manager.add_user("admin2", "admin2pass12", UserRole::ADMINISTRATOR, false));
    ASSERT_TRUE(vault_manager.save_vault());
    ASSERT_TRUE(vault_manager.close_vault());

    // Login as admin2
    ASSERT_TRUE(vault_manager.open_vault_v2(
        test_vault_path.string(), "admin2", "admin2pass12"));

    // Remove admin1 (should succeed, admin2 still exists)
    auto result = vault_manager.remove_user("admin1");
    EXPECT_TRUE(result) << "Should allow removing admin when another admin exists";
}

TEST_F(VaultManagerV2Test, RemoveUserRequiresAdminPermission) {
    VaultSecurityPolicy policy;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpass123", policy));
    ASSERT_TRUE(vault_manager.add_user("alice", "alicepass123", UserRole::STANDARD_USER));
    ASSERT_TRUE(vault_manager.add_user("bob", "bobpass12345", UserRole::STANDARD_USER));
    ASSERT_TRUE(vault_manager.save_vault());
    ASSERT_TRUE(vault_manager.close_vault());

    ASSERT_TRUE(vault_manager.open_vault_v2(
        test_vault_path.string(), "alice", "alicepass123"));

    auto result = vault_manager.remove_user("bob");
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error(), VaultError::PermissionDenied);
}

TEST_F(VaultManagerV2Test, RemoveUserRequiresOpenVault) {
    auto result = vault_manager.remove_user("bob");
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error(), VaultError::VaultNotOpen);
}

TEST_F(VaultManagerV2Test, RemoveUserRejectsUnknownUser) {
    VaultSecurityPolicy policy;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpass123", policy));

    auto result = vault_manager.remove_user("missing-user");
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error(), VaultError::UserNotFound);
}

// ============================================================================
// Password Change Tests
// ============================================================================

TEST_F(VaultManagerV2Test, ChangePasswordSuccessful) {
    // Create vault
    VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "alice", "oldpassword123", policy));

    // Change password
    auto result = vault_manager.change_user_password("alice", "oldpassword123", "newpass45678");
    ASSERT_TRUE(result) << "Failed to change password";
    EXPECT_TRUE(vault_manager.save_vault());
    EXPECT_TRUE(vault_manager.close_vault());

    // Verify old password doesn't work
    auto fail_session = vault_manager.open_vault_v2(
        test_vault_path.string(), "alice", "oldpassword123");
    EXPECT_FALSE(fail_session);

    // Verify new password works
    auto success_session = vault_manager.open_vault_v2(
        test_vault_path.string(), "alice", "newpass45678");
    ASSERT_TRUE(success_session);
    EXPECT_EQ(success_session->username, "alice");
}

TEST_F(VaultManagerV2Test, ChangePasswordRequiresCorrectOldPassword) {
    // Create vault
    VaultSecurityPolicy policy;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "alice", "oldpassword123", policy));

    // Try with wrong old password
    auto result = vault_manager.change_user_password("alice", "wrongoldpass", "newpass45678");
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error(), VaultError::AuthenticationFailed);
}

TEST_F(VaultManagerV2Test, ChangePasswordEnforcesMinLength) {
    // Create vault with strict policy
    VaultSecurityPolicy policy;
    policy.min_password_length = 12;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "alice", "goodpassword123", policy));

    // Try with short new password
    auto result = vault_manager.change_user_password("alice", "goodpassword123", "short");
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error(), VaultError::WeakPassword);
}

TEST_F(VaultManagerV2Test, MustChangePasswordWorkflow) {
    // Create vault
    VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpass123", policy));

    // Add user with temporary password
    ASSERT_TRUE(vault_manager.add_user("bob", "temppass1234", UserRole::STANDARD_USER, true));
    ASSERT_TRUE(vault_manager.save_vault());
    ASSERT_TRUE(vault_manager.close_vault());

    // Login as bob
    auto session = vault_manager.open_vault_v2(test_vault_path.string(), "bob", "temppass1234");
    ASSERT_TRUE(session);
    EXPECT_TRUE(session->password_change_required) << "Should require password change";

    // Change password
    ASSERT_TRUE(vault_manager.change_user_password("bob", "temppass1234", "newpass45678"));

    // Verify flag cleared
    auto updated_session = vault_manager.get_current_user_session();
    ASSERT_TRUE(updated_session);
    EXPECT_FALSE(updated_session->password_change_required) << "Flag should be cleared after change";
}

TEST_F(VaultManagerV2Test, ChangePasswordUpdatesUserSlotPasswordState) {
    VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpass123", policy));

    ASSERT_TRUE(vault_manager.add_user("bob", "temppass1234", UserRole::STANDARD_USER, true));
    ASSERT_TRUE(vault_manager.save_vault());
    ASSERT_TRUE(vault_manager.close_vault());

    ASSERT_TRUE(vault_manager.open_vault_v2(
        test_vault_path.string(), "bob", "temppass1234"));

    const auto users_before_change = vault_manager.list_users();
    const auto bob_before = std::find_if(users_before_change.begin(), users_before_change.end(),
        [](const KeySlot& slot) { return slot.username == "bob"; });
    ASSERT_NE(bob_before, users_before_change.end());
    ASSERT_TRUE(bob_before->must_change_password);
    ASSERT_EQ(bob_before->password_changed_at, 0);

    ASSERT_TRUE(vault_manager.change_user_password("bob", "temppass1234", "newpass45678"));

    const auto users_after_change = vault_manager.list_users();
    const auto bob_after = std::find_if(users_after_change.begin(), users_after_change.end(),
        [](const KeySlot& slot) { return slot.username == "bob"; });
    ASSERT_NE(bob_after, users_after_change.end());
    EXPECT_FALSE(bob_after->must_change_password);
    EXPECT_NE(bob_after->password_changed_at, 0);
}

TEST_F(VaultManagerV2Test, AdminCanChangeAnyUserPassword) {
    // Create vault
    VaultSecurityPolicy policy;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpass123", policy));

    // Add user
    ASSERT_TRUE(vault_manager.add_user("bob", "bobpass12345", UserRole::STANDARD_USER, false));

    // Admin changes bob's password
    auto result = vault_manager.change_user_password("bob", "bobpass12345", "newbobpass12");
    ASSERT_TRUE(result) << "Admin should be able to change any user's password";

    auto admin_session = vault_manager.get_current_user_session();
    ASSERT_TRUE(admin_session);
    EXPECT_EQ(admin_session->username, "admin");
    EXPECT_FALSE(admin_session->password_change_required);

    const auto users_after_change = vault_manager.list_users();
    const auto bob_after = std::find_if(users_after_change.begin(), users_after_change.end(),
        [](const KeySlot& slot) { return slot.username == "bob"; });
    ASSERT_NE(bob_after, users_after_change.end());
    EXPECT_FALSE(bob_after->must_change_password);
    EXPECT_NE(bob_after->password_changed_at, 0);

    ASSERT_TRUE(vault_manager.save_vault());
    ASSERT_TRUE(vault_manager.close_vault());

    auto old_password_session = vault_manager.open_vault_v2(
        test_vault_path.string(), "bob", "bobpass12345");
    EXPECT_FALSE(old_password_session);

    auto new_password_session = vault_manager.open_vault_v2(
        test_vault_path.string(), "bob", "newbobpass12");
    ASSERT_TRUE(new_password_session);
    EXPECT_FALSE(new_password_session->password_change_required);
}

TEST_F(VaultManagerV2Test, StandardUserCanOnlyChangeOwnPassword) {
    // Create vault
    VaultSecurityPolicy policy;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpass123", policy));

    // Add two standard users
    ASSERT_TRUE(vault_manager.add_user("alice", "alicepass123", UserRole::STANDARD_USER, false));
    ASSERT_TRUE(vault_manager.add_user("bob", "bobpass12345", UserRole::STANDARD_USER, false));
    ASSERT_TRUE(vault_manager.save_vault());
    ASSERT_TRUE(vault_manager.close_vault());

    // Login as alice
    ASSERT_TRUE(vault_manager.open_vault_v2(test_vault_path.string(), "alice", "alicepass123"));

    // Try to change bob's password (should fail)
    auto result = vault_manager.change_user_password("bob", "bobpass12345", "newbobpass12");
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error(), VaultError::PermissionDenied);
}

TEST_F(VaultManagerV2Test, ChangePasswordAsyncClearsSessionFlagAndPersistsNewPassword) {
    VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpass123", policy));

    ASSERT_TRUE(vault_manager.add_user("bob", "temppass1234", UserRole::STANDARD_USER, true));
    ASSERT_TRUE(vault_manager.save_vault());
    ASSERT_TRUE(vault_manager.close_vault());

    auto session = vault_manager.open_vault_v2(
        test_vault_path.string(), "bob", "temppass1234");
    ASSERT_TRUE(session);
    EXPECT_TRUE(session->password_change_required);

    std::atomic<bool> completion_called{false};
    KeepTower::VaultResult<> completion_result = {};

    vault_manager.change_user_password_async(
        "bob",
        "temppass1234",
        "newpass45678",
        nullptr,
        [&](KeepTower::VaultResult<> result) {
            completion_result = result;
            completion_called.store(true);
        });

    ASSERT_TRUE(pump_main_context_until([&completion_called]() {
        return completion_called.load();
    }));

    ASSERT_TRUE(completion_result);

    auto updated_session = vault_manager.get_current_user_session();
    ASSERT_TRUE(updated_session);
    EXPECT_FALSE(updated_session->password_change_required);

    ASSERT_TRUE(vault_manager.save_vault());
    ASSERT_TRUE(vault_manager.close_vault());

    auto old_password_session = vault_manager.open_vault_v2(
        test_vault_path.string(), "bob", "temppass1234");
    EXPECT_FALSE(old_password_session);

    auto new_password_session = vault_manager.open_vault_v2(
        test_vault_path.string(), "bob", "newpass45678");
    ASSERT_TRUE(new_password_session);
    EXPECT_FALSE(new_password_session->password_change_required);
}

// ============================================================================
// Session Management Tests
// ============================================================================

TEST_F(VaultManagerV2Test, GetCurrentSessionReturnsCorrectInfo) {
    // Create vault
    VaultSecurityPolicy policy;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "alice", "validpass123", policy));

    // Get session
    auto session = vault_manager.get_current_user_session();
    ASSERT_TRUE(session);
    EXPECT_EQ(session->username, "alice");
    EXPECT_EQ(session->role, UserRole::ADMINISTRATOR);
    EXPECT_FALSE(session->password_change_required);
}

TEST_F(VaultManagerV2Test, ListUsersReturnsActiveOnly) {
    // Create vault
    VaultSecurityPolicy policy;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpass123", policy));

    // Add users
    ASSERT_TRUE(vault_manager.add_user("alice", "alicepass123", UserRole::STANDARD_USER));
    ASSERT_TRUE(vault_manager.add_user("bob", "bobpass12345", UserRole::STANDARD_USER));
    ASSERT_TRUE(vault_manager.add_user("charlie", "charliepass1", UserRole::ADMINISTRATOR, false));

    // List users
    auto users = vault_manager.list_users();
    ASSERT_EQ(users.size(), 4);  // admin + alice + bob + charlie

    // Remove bob
    ASSERT_TRUE(vault_manager.remove_user("bob"));

    // List again
    users = vault_manager.list_users();
    ASSERT_EQ(users.size(), 3);  // bob removed

    // Verify bob not in list
    auto bob_it = std::find_if(users.begin(), users.end(),
        [](const KeySlot& slot) { return slot.username == "bob"; });
    EXPECT_EQ(bob_it, users.end());
}

// ============================================================================
// Password Validation and History Tests
// ============================================================================

TEST_F(VaultManagerV2Test, ValidateNewPasswordEnforcesMinLength) {
    VaultSecurityPolicy policy;
    policy.min_password_length = 12;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpass123", policy));

    // Try password too short for admin
    auto result = vault_manager.validate_new_password("admin", "short");
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error(), VaultError::WeakPassword);
}

TEST_F(VaultManagerV2Test, ValidateNewPasswordRequiresOpenVault) {
    auto result = vault_manager.validate_new_password("admin", "long-enough-password");
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error(), VaultError::VaultNotOpen);
}

TEST_F(VaultManagerV2Test, ValidateNewPasswordRejectsUnknownUser) {
    VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpass123", policy));

    auto result = vault_manager.validate_new_password("missing-user", "long-enough-password");
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error(), VaultError::UserNotFound);
}

// Note: VaultSecurityPolicy doesn't have max_password_length - testing min length is sufficient

TEST_F(VaultManagerV2Test, ValidateNewPasswordRejectsPasswordHistory) {
    VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    policy.password_history_depth = 3;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "alice", "password001", policy));

    // Change password twice
    ASSERT_TRUE(vault_manager.change_user_password("alice", "password001", "password002"));
    ASSERT_TRUE(vault_manager.change_user_password("alice", "password002", "password003"));

    // Try to reuse password001 (should fail)
    auto result = vault_manager.change_user_password("alice", "password003", "password001");
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error(), VaultError::PasswordReused);
}

TEST_F(VaultManagerV2Test, ValidateNewPasswordRejectsReusedPasswordFromHistory) {
    VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    policy.password_history_depth = 3;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "alice", "password001", policy));

    ASSERT_TRUE(vault_manager.change_user_password("alice", "password001", "password002"));
    ASSERT_TRUE(vault_manager.change_user_password("alice", "password002", "password003"));

    auto result = vault_manager.validate_new_password("alice", "password001");
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error(), VaultError::PasswordReused);
}

TEST_F(VaultManagerV2Test, ClearUserPasswordHistorySuccessful) {
    VaultSecurityPolicy policy;
    policy.password_history_depth = 5;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpass123", policy));

    // Add user
    ASSERT_TRUE(vault_manager.add_user("bob", "bobpass12345", UserRole::STANDARD_USER));

    // Change password a few times to build history
    ASSERT_TRUE(vault_manager.change_user_password("bob", "bobpass12345", "bobpass23456"));
    ASSERT_TRUE(vault_manager.change_user_password("bob", "bobpass23456", "bobpass34567"));

    const auto users_before_clear = vault_manager.list_users();
    const auto bob_before = std::find_if(users_before_clear.begin(), users_before_clear.end(),
        [](const KeySlot& slot) { return slot.username == "bob"; });
    ASSERT_NE(bob_before, users_before_clear.end());
    ASSERT_FALSE(bob_before->password_history.empty());

    // Clear history (admin only)
    auto result = vault_manager.clear_user_password_history("bob");
    ASSERT_TRUE(result) << "Admin should be able to clear password history";

    const auto users_after_clear = vault_manager.list_users();
    const auto bob_after = std::find_if(users_after_clear.begin(), users_after_clear.end(),
        [](const KeySlot& slot) { return slot.username == "bob"; });
    ASSERT_NE(bob_after, users_after_clear.end());
    EXPECT_TRUE(bob_after->password_history.empty());

    // Now bob can reuse old password
    auto reuse_result = vault_manager.change_user_password("bob", "bobpass34567", "bobpass12345");
    EXPECT_TRUE(reuse_result) << "Should allow password reuse after history cleared";
}

TEST_F(VaultManagerV2Test, StandardUserCanClearOwnPasswordHistory) {
    VaultSecurityPolicy policy;
    policy.password_history_depth = 5;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpass123", policy));
    ASSERT_TRUE(vault_manager.add_user("bob", "bobpass12345", UserRole::STANDARD_USER));

    ASSERT_TRUE(vault_manager.change_user_password("bob", "bobpass12345", "bobpass23456"));
    ASSERT_TRUE(vault_manager.change_user_password("bob", "bobpass23456", "bobpass34567"));
    ASSERT_TRUE(vault_manager.save_vault());
    ASSERT_TRUE(vault_manager.close_vault());

    ASSERT_TRUE(vault_manager.open_vault_v2(
        test_vault_path.string(), "bob", "bobpass34567"));

    const auto users_before_clear = vault_manager.list_users();
    const auto bob_before = std::find_if(users_before_clear.begin(), users_before_clear.end(),
        [](const KeySlot& slot) { return slot.username == "bob"; });
    ASSERT_NE(bob_before, users_before_clear.end());
    ASSERT_FALSE(bob_before->password_history.empty());

    auto clear_result = vault_manager.clear_user_password_history("bob");
    ASSERT_TRUE(clear_result) << "Standard user should be able to clear their own history";

    const auto users_after_clear = vault_manager.list_users();
    const auto bob_after = std::find_if(users_after_clear.begin(), users_after_clear.end(),
        [](const KeySlot& slot) { return slot.username == "bob"; });
    ASSERT_NE(bob_after, users_after_clear.end());
    EXPECT_TRUE(bob_after->password_history.empty());

    auto reuse_result = vault_manager.change_user_password("bob", "bobpass34567", "bobpass12345");
    EXPECT_TRUE(reuse_result) << "Should allow password reuse after self-service history clear";
}

TEST_F(VaultManagerV2Test, ClearPasswordHistoryRequiresAdmin) {
    VaultSecurityPolicy policy;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpass123", policy));
    ASSERT_TRUE(vault_manager.add_user("alice", "alicepass123", UserRole::STANDARD_USER));
    ASSERT_TRUE(vault_manager.add_user("bob", "bobpass12345", UserRole::STANDARD_USER));
    ASSERT_TRUE(vault_manager.save_vault());
    ASSERT_TRUE(vault_manager.close_vault());

    // Login as alice (not admin)
    ASSERT_TRUE(vault_manager.open_vault_v2(test_vault_path.string(), "alice", "alicepass123"));

    // Try to clear bob's history (should fail)
    auto result = vault_manager.clear_user_password_history("bob");
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error(), VaultError::PermissionDenied);
}

TEST_F(VaultManagerV2Test, ClearPasswordHistoryRequiresOpenVault) {
    auto result = vault_manager.clear_user_password_history("bob");
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error(), VaultError::VaultNotOpen);
}

TEST_F(VaultManagerV2Test, ClearPasswordHistoryRejectsUnknownUser) {
    VaultSecurityPolicy policy;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpass123", policy));

    auto result = vault_manager.clear_user_password_history("missing-user");
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error(), VaultError::UserNotFound);
}

// ============================================================================
// Admin Password Reset Tests
// ============================================================================

TEST_F(VaultManagerV2Test, AdminResetUserPasswordSuccessful) {
    VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpass123", policy));
    ASSERT_TRUE(vault_manager.add_user("bob", "bobpass12345", UserRole::STANDARD_USER, false));

    // Admin resets bob's password
    auto result = vault_manager.admin_reset_user_password("bob", "newresetpass");
    ASSERT_TRUE(result) << "Admin should be able to reset user password";

    ASSERT_TRUE(vault_manager.save_vault());
    ASSERT_TRUE(vault_manager.close_vault());

    // Bob can login with new password
    auto bob_session = vault_manager.open_vault_v2(test_vault_path.string(), "bob", "newresetpass");
    ASSERT_TRUE(bob_session);
    EXPECT_TRUE(bob_session->password_change_required) << "Should require password change after admin reset";
}

TEST_F(VaultManagerV2Test, AdminResetPasswordUpdatesUserSlotPasswordState) {
    VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpass123", policy));
    ASSERT_TRUE(vault_manager.add_user("bob", "bobpass12345", UserRole::STANDARD_USER, false));

    const auto users_before_reset = vault_manager.list_users();
    const auto bob_before = std::find_if(users_before_reset.begin(), users_before_reset.end(),
        [](const KeySlot& slot) { return slot.username == "bob"; });
    ASSERT_NE(bob_before, users_before_reset.end());
    ASSERT_FALSE(bob_before->must_change_password);

    ASSERT_TRUE(vault_manager.admin_reset_user_password("bob", "newresetpass"));

    const auto users_after_reset = vault_manager.list_users();
    const auto bob_after = std::find_if(users_after_reset.begin(), users_after_reset.end(),
        [](const KeySlot& slot) { return slot.username == "bob"; });
    ASSERT_NE(bob_after, users_after_reset.end());
    EXPECT_TRUE(bob_after->must_change_password);
    EXPECT_EQ(bob_after->password_changed_at, 0);
}

TEST_F(VaultManagerV2Test, AdminResetPasswordRequiresAdmin) {
    VaultSecurityPolicy policy;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpass123", policy));
    ASSERT_TRUE(vault_manager.add_user("alice", "alicepass123", UserRole::STANDARD_USER));
    ASSERT_TRUE(vault_manager.add_user("bob", "bobpass12345", UserRole::STANDARD_USER));
    ASSERT_TRUE(vault_manager.save_vault());
    ASSERT_TRUE(vault_manager.close_vault());

    // Login as alice (not admin)
    ASSERT_TRUE(vault_manager.open_vault_v2(test_vault_path.string(), "alice", "alicepass123"));

    // Try to reset bob's password (should fail)
    auto result = vault_manager.admin_reset_user_password("bob", "newpassword123");
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error(), VaultError::PermissionDenied);
}

TEST_F(VaultManagerV2Test, AdminResetPasswordRejectsSelfReset) {
    VaultSecurityPolicy policy;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpass123", policy));

    auto result = vault_manager.admin_reset_user_password("admin", "newpassword123");
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error(), VaultError::PermissionDenied);
}

TEST_F(VaultManagerV2Test, AdminResetPasswordRejectsWeakTemporaryPassword) {
    VaultSecurityPolicy policy;
    policy.min_password_length = 12;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpass123", policy));
    ASSERT_TRUE(vault_manager.add_user("bob", "bobpassword1234", UserRole::STANDARD_USER));

    auto result = vault_manager.admin_reset_user_password("bob", "short");
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error(), VaultError::WeakPassword);
}

TEST_F(VaultManagerV2Test, AdminResetPasswordRejectsUnknownUser) {
    VaultSecurityPolicy policy;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpass123", policy));

    auto result = vault_manager.admin_reset_user_password("missing-user", "newpassword123");
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error(), VaultError::UserNotFound);
}

TEST_F(VaultManagerV2Test, AdminResetPasswordClearsHistory) {
    VaultSecurityPolicy policy;
    policy.password_history_depth = 3;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpass123", policy));
    ASSERT_TRUE(vault_manager.add_user("bob", "bobpass12345", UserRole::STANDARD_USER));

    // Bob changes password to build history
    ASSERT_TRUE(vault_manager.change_user_password("bob", "bobpass12345", "bobpass23456"));
    ASSERT_TRUE(vault_manager.change_user_password("bob", "bobpass23456", "bobpass34567"));

    const auto users_before_reset = vault_manager.list_users();
    const auto bob_before = std::find_if(users_before_reset.begin(), users_before_reset.end(),
        [](const KeySlot& slot) { return slot.username == "bob"; });
    ASSERT_NE(bob_before, users_before_reset.end());
    ASSERT_FALSE(bob_before->password_history.empty());

    // Admin resets password (clears history)
    ASSERT_TRUE(vault_manager.admin_reset_user_password("bob", "adminreset123"));

    const auto users_after_reset = vault_manager.list_users();
    const auto bob_after = std::find_if(users_after_reset.begin(), users_after_reset.end(),
        [](const KeySlot& slot) { return slot.username == "bob"; });
    ASSERT_NE(bob_after, users_after_reset.end());
    EXPECT_TRUE(bob_after->password_history.empty());

    ASSERT_TRUE(vault_manager.save_vault());
    ASSERT_TRUE(vault_manager.close_vault());

    // Bob logs in with reset password
    ASSERT_TRUE(vault_manager.open_vault_v2(test_vault_path.string(), "bob", "adminreset123"));

    // Bob can now use old password (history cleared)
    auto result = vault_manager.change_user_password("bob", "adminreset123", "bobpass12345");
    EXPECT_TRUE(result) << "Should allow old password after admin reset clears history";
}

// ============================================================================
// Permission Check Tests
// ============================================================================

// Note: can_view_account and can_delete_account tests require account ownership
// tracking which is not yet implemented in the account record protobuf schema

TEST_F(VaultManagerV2Test, GetSecurityPolicyReturnsCorrectValues) {
    VaultSecurityPolicy policy;
    policy.min_password_length = 16;
    policy.password_history_depth = 5;
    policy.pbkdf2_iterations = 200000;
    policy.require_yubikey = false;

    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpassword123", policy));

    auto retrieved_policy = vault_manager.get_vault_security_policy();
    ASSERT_TRUE(retrieved_policy);
    EXPECT_EQ(retrieved_policy->min_password_length, 16);
}

TEST_F(VaultManagerV2Test, UpdateSecurityPolicyRequiresOpenVault) {
    VaultSecurityPolicy policy;
    policy.min_password_length = 14;

    auto result = vault_manager.update_security_policy(policy);
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error(), VaultError::VaultNotOpen);
}

TEST_F(VaultManagerV2Test, UpdateSecurityPolicyRejectsInvalidHashAlgorithm) {
    VaultSecurityPolicy policy;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpassword123", policy));

    auto updated_policy = policy;
    updated_policy.username_hash_algorithm = 0x06;

    auto result = vault_manager.update_security_policy(updated_policy);
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error(), VaultError::InvalidData);
}

TEST_F(VaultManagerV2Test, UpdateSecurityPolicyAllowsLowRecommendedValues) {
    VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    policy.pbkdf2_iterations = 100000;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpassword123", policy));

    auto updated_policy = policy;
    updated_policy.min_password_length = 4;
    updated_policy.pbkdf2_iterations = 50000;

    auto result = vault_manager.update_security_policy(updated_policy);
    ASSERT_TRUE(result);

    auto retrieved_policy = vault_manager.get_vault_security_policy();
    ASSERT_TRUE(retrieved_policy);
    EXPECT_EQ(retrieved_policy->min_password_length, 4);
    EXPECT_EQ(retrieved_policy->pbkdf2_iterations, 50000);
}

TEST_F(VaultManagerV2Test, UpdateSecurityPolicyAllowsAlgorithmChangeWithoutMigration) {
    VaultSecurityPolicy policy;
    policy.username_hash_algorithm = 0x01;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpassword123", policy));
    ASSERT_TRUE(vault_manager.add_user("bob", "bobpassword123", UserRole::STANDARD_USER));

    auto updated_policy = policy;
    updated_policy.username_hash_algorithm = 0x02;
    updated_policy.migration_flags = 0x00;

    auto result = vault_manager.update_security_policy(updated_policy);
    ASSERT_TRUE(result);

    auto retrieved_policy = vault_manager.get_vault_security_policy();
    ASSERT_TRUE(retrieved_policy);
    EXPECT_EQ(retrieved_policy->username_hash_algorithm, 0x02);
    EXPECT_EQ(retrieved_policy->migration_flags, 0x00);
}

TEST_F(VaultManagerV2Test, UpdateSecurityPolicyPersistsAcrossReopen) {
    VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    policy.pbkdf2_iterations = 100000;
    policy.password_history_depth = 2;
    policy.username_hash_algorithm = 0x01;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpassword123", policy));

    auto updated_policy = policy;
    updated_policy.min_password_length = 14;
    updated_policy.password_history_depth = 4;

    ASSERT_TRUE(vault_manager.update_security_policy(updated_policy));
    ASSERT_TRUE(vault_manager.save_vault());
    ASSERT_TRUE(vault_manager.close_vault());

    ASSERT_TRUE(vault_manager.open_vault_v2(
        test_vault_path.string(), "admin", "adminpassword123"));

    auto reopened_policy = vault_manager.get_vault_security_policy();
    ASSERT_TRUE(reopened_policy);
    EXPECT_EQ(reopened_policy->min_password_length, 14);
    EXPECT_EQ(reopened_policy->pbkdf2_iterations, 100000u);
    EXPECT_EQ(reopened_policy->password_history_depth, 4u);
    EXPECT_EQ(reopened_policy->username_hash_algorithm, 0x01);
}

TEST_F(VaultManagerV2Test, OpenV2VaultMigratesUserWhenMigrationPolicyActive) {
    VaultSecurityPolicy policy;
    policy.username_hash_algorithm = 0x01;
    policy.pbkdf2_iterations = 100000;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpassword123", policy));
    ASSERT_TRUE(vault_manager.add_user("bob", "bobpassword123", UserRole::STANDARD_USER));
    ASSERT_TRUE(vault_manager.save_vault());
    ASSERT_TRUE(vault_manager.close_vault());

    ASSERT_TRUE(vault_manager.open_vault_v2(
        test_vault_path.string(), "admin", "adminpassword123"));

    auto migrated_policy = policy;
    migrated_policy.username_hash_algorithm_previous = 0x01;
    migrated_policy.username_hash_algorithm = 0x02;
    migrated_policy.migration_flags = 0x01;
    ASSERT_TRUE(vault_manager.update_security_policy(migrated_policy));
    ASSERT_TRUE(vault_manager.save_vault());
    ASSERT_TRUE(vault_manager.close_vault());

    auto bob_session = vault_manager.open_vault_v2(
        test_vault_path.string(), "bob", "bobpassword123");
    ASSERT_TRUE(bob_session);

    const auto users = vault_manager.list_users();
    const auto bob_it = std::find_if(users.begin(), users.end(),
        [](const KeySlot& slot) { return slot.username == "bob"; });
    ASSERT_NE(bob_it, users.end());
    EXPECT_EQ(bob_it->migration_status, 0x01);
    EXPECT_NE(bob_it->migrated_at, 0u);

    auto retrieved_policy = vault_manager.get_vault_security_policy();
    ASSERT_TRUE(retrieved_policy);
    EXPECT_EQ(retrieved_policy->username_hash_algorithm_previous, 0x01);
    EXPECT_EQ(retrieved_policy->username_hash_algorithm, 0x02);
    EXPECT_EQ(retrieved_policy->migration_flags, 0x01);
}

TEST_F(VaultManagerV2Test, UpdateSecurityPolicyResetsMigratedUsersForNewTargetAlgorithm) {
    VaultSecurityPolicy policy;
    policy.username_hash_algorithm = 0x01;
    policy.pbkdf2_iterations = 100000;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpassword123", policy));
    ASSERT_TRUE(vault_manager.add_user("bob", "bobpassword123", UserRole::STANDARD_USER));
    ASSERT_TRUE(vault_manager.save_vault());
    ASSERT_TRUE(vault_manager.close_vault());

    ASSERT_TRUE(vault_manager.open_vault_v2(
        test_vault_path.string(), "admin", "adminpassword123"));

    auto first_migration_policy = policy;
    first_migration_policy.username_hash_algorithm_previous = 0x01;
    first_migration_policy.username_hash_algorithm = 0x02;
    first_migration_policy.migration_flags = 0x01;
    ASSERT_TRUE(vault_manager.update_security_policy(first_migration_policy));
    ASSERT_TRUE(vault_manager.save_vault());
    ASSERT_TRUE(vault_manager.close_vault());

    auto admin_session = vault_manager.open_vault_v2(
        test_vault_path.string(), "admin", "adminpassword123");
    ASSERT_TRUE(admin_session);

    auto users_before_reset = vault_manager.list_users();
    const auto admin_before = std::find_if(users_before_reset.begin(), users_before_reset.end(),
        [](const KeySlot& slot) { return slot.username == "admin"; });
    ASSERT_NE(admin_before, users_before_reset.end());
    ASSERT_EQ(admin_before->migration_status, 0x01);

    auto second_migration_policy = first_migration_policy;
    second_migration_policy.username_hash_algorithm_previous = 0x02;
    second_migration_policy.username_hash_algorithm = 0x03;
    second_migration_policy.migration_flags = 0x01;

    ASSERT_TRUE(vault_manager.update_security_policy(second_migration_policy));

    auto users_after_reset = vault_manager.list_users();
    const auto admin_after = std::find_if(users_after_reset.begin(), users_after_reset.end(),
        [](const KeySlot& slot) { return slot.username == "admin"; });
    ASSERT_NE(admin_after, users_after_reset.end());
    EXPECT_EQ(admin_after->migration_status, 0x00);

    EXPECT_TRUE(std::all_of(users_after_reset.begin(), users_after_reset.end(),
        [](const KeySlot& slot) { return slot.migration_status == 0x00; }));

    auto retrieved_policy = vault_manager.get_vault_security_policy();
    ASSERT_TRUE(retrieved_policy);
    EXPECT_EQ(retrieved_policy->username_hash_algorithm_previous, 0x02);
    EXPECT_EQ(retrieved_policy->username_hash_algorithm, 0x03);
    EXPECT_EQ(retrieved_policy->migration_flags, 0x01);
}

TEST_F(VaultManagerV2Test, UpdateSecurityPolicyPreservesMigrationStatusWhenAlgorithmUnchanged) {
    VaultSecurityPolicy policy;
    policy.username_hash_algorithm = 0x01;
    policy.pbkdf2_iterations = 100000;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpassword123", policy));
    ASSERT_TRUE(vault_manager.add_user("bob", "bobpassword123", UserRole::STANDARD_USER));
    ASSERT_TRUE(vault_manager.save_vault());
    ASSERT_TRUE(vault_manager.close_vault());

    ASSERT_TRUE(vault_manager.open_vault_v2(
        test_vault_path.string(), "admin", "adminpassword123"));

    auto migrated_policy = policy;
    migrated_policy.username_hash_algorithm_previous = 0x01;
    migrated_policy.username_hash_algorithm = 0x02;
    migrated_policy.migration_flags = 0x01;
    ASSERT_TRUE(vault_manager.update_security_policy(migrated_policy));
    ASSERT_TRUE(vault_manager.save_vault());
    ASSERT_TRUE(vault_manager.close_vault());

    ASSERT_TRUE(vault_manager.open_vault_v2(
        test_vault_path.string(), "bob", "bobpassword123"));

    const auto migrated_users = vault_manager.list_users();
    const auto migrated_bob = std::find_if(migrated_users.begin(), migrated_users.end(),
        [](const KeySlot& slot) { return slot.username == "bob"; });
    ASSERT_NE(migrated_bob, migrated_users.end());
    ASSERT_EQ(migrated_bob->migration_status, 0x01);

    ASSERT_TRUE(vault_manager.close_vault());
    ASSERT_TRUE(vault_manager.open_vault_v2(
        test_vault_path.string(), "admin", "adminpassword123"));

    const auto users_before_policy_update = vault_manager.list_users();
    const auto migrated_before_count = std::count_if(
        users_before_policy_update.begin(), users_before_policy_update.end(),
        [](const KeySlot& slot) { return slot.migration_status == 0x01; });

    auto unchanged_algorithm_policy = migrated_policy;
    unchanged_algorithm_policy.min_password_length = 18;
    unchanged_algorithm_policy.password_history_depth = 7;

    ASSERT_TRUE(vault_manager.update_security_policy(unchanged_algorithm_policy));

    const auto users_after_policy_update = vault_manager.list_users();
    const auto migrated_after_count = std::count_if(
        users_after_policy_update.begin(), users_after_policy_update.end(),
        [](const KeySlot& slot) { return slot.migration_status == 0x01; });
    EXPECT_EQ(migrated_after_count, migrated_before_count);
    EXPECT_GT(migrated_after_count, 0);

    auto retrieved_policy = vault_manager.get_vault_security_policy();
    ASSERT_TRUE(retrieved_policy);
    EXPECT_EQ(retrieved_policy->min_password_length, 18u);
    EXPECT_EQ(retrieved_policy->password_history_depth, 7u);
    EXPECT_EQ(retrieved_policy->username_hash_algorithm, 0x02);
}

TEST_F(VaultManagerV2Test, StandardUserCannotUpdateSecurityPolicy) {
    VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpassword123", policy));
    ASSERT_TRUE(vault_manager.add_user("bob", "bobpassword123", UserRole::STANDARD_USER));
    ASSERT_TRUE(vault_manager.save_vault());
    ASSERT_TRUE(vault_manager.close_vault());

    ASSERT_TRUE(vault_manager.open_vault_v2(
        test_vault_path.string(), "bob", "bobpassword123"));

    auto updated_policy = policy;
    updated_policy.min_password_length = 14;

    auto result = vault_manager.update_security_policy(updated_policy);
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error(), VaultError::PermissionDenied);
}

TEST_F(VaultManagerV2Test, UpdatedSecurityPolicyAffectsSubsequentUserValidation) {
    VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    policy.pbkdf2_iterations = 100000;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpassword123", policy));

    auto updated_policy = policy;
    updated_policy.min_password_length = 14;
    updated_policy.password_history_depth = 4;

    ASSERT_TRUE(vault_manager.update_security_policy(updated_policy));

    auto retrieved_policy = vault_manager.get_vault_security_policy();
    ASSERT_TRUE(retrieved_policy);
    EXPECT_EQ(retrieved_policy->min_password_length, 14);
    EXPECT_EQ(retrieved_policy->password_history_depth, 4);

    auto weak_add = vault_manager.add_user(
        "bob", "short-pass-1", UserRole::STANDARD_USER, false);
    EXPECT_FALSE(weak_add);
    EXPECT_EQ(weak_add.error(), VaultError::WeakPassword);
}

TEST_F(VaultManagerV2Test, StandardUserCannotViewOrDeleteAdminOnlyAccounts) {
    VaultSecurityPolicy policy;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpassword123", policy));
    ASSERT_TRUE(vault_manager.add_user("bob", "bobpassword123", UserRole::STANDARD_USER));

    ASSERT_TRUE(vault_manager.add_account(
        make_v2_account_detail("shared", "Shared Account", false, false)));
    ASSERT_TRUE(vault_manager.add_account(
        make_v2_account_detail("restricted", "Restricted Account", true, true)));
    ASSERT_TRUE(vault_manager.save_vault());
    ASSERT_TRUE(vault_manager.close_vault());

    ASSERT_TRUE(vault_manager.open_vault_v2(
        test_vault_path.string(), "bob", "bobpassword123"));

    EXPECT_TRUE(vault_manager.can_view_account(0));
    EXPECT_TRUE(vault_manager.can_delete_account(0));
    EXPECT_FALSE(vault_manager.can_view_account(1));
    EXPECT_FALSE(vault_manager.can_delete_account(1));
    EXPECT_FALSE(vault_manager.can_view_account(99));
    EXPECT_FALSE(vault_manager.can_delete_account(99));
}

TEST_F(VaultManagerV2Test, AdministratorCanViewOrDeleteAdminOnlyAccounts) {
    VaultSecurityPolicy policy;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpassword123", policy));

    ASSERT_TRUE(vault_manager.add_account(
        make_v2_account_detail("restricted", "Restricted Account", true, true)));

    EXPECT_TRUE(vault_manager.can_view_account(0));
    EXPECT_TRUE(vault_manager.can_delete_account(0));
}

// ============================================================================
// Integration: Full Multi-User Workflow
// ============================================================================

TEST_F(VaultManagerV2Test, FullMultiUserWorkflow) {
    // 1. Admin creates vault
    VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    policy.pbkdf2_iterations = 100000;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpass123", policy));

    // 2. Admin adds users
    ASSERT_TRUE(vault_manager.add_user("alice", "temppass1234", UserRole::ADMINISTRATOR, true));
    ASSERT_TRUE(vault_manager.add_user("bob", "temppass2345", UserRole::STANDARD_USER, true));
    ASSERT_TRUE(vault_manager.add_user("charlie", "charlieperm", UserRole::STANDARD_USER, false));
    ASSERT_TRUE(vault_manager.save_vault());
    ASSERT_TRUE(vault_manager.close_vault());

    // 3. Alice logs in and must change password
    auto alice_session = vault_manager.open_vault_v2(test_vault_path.string(), "alice", "temppass1234");
    ASSERT_TRUE(alice_session);
    EXPECT_TRUE(alice_session->password_change_required);
    ASSERT_TRUE(vault_manager.change_user_password("alice", "temppass1234", "alicenewpass"));
    ASSERT_TRUE(vault_manager.save_vault());
    ASSERT_TRUE(vault_manager.close_vault());

    // 4. Bob logs in and must change password
    auto bob_session = vault_manager.open_vault_v2(test_vault_path.string(), "bob", "temppass2345");
    ASSERT_TRUE(bob_session);
    EXPECT_TRUE(bob_session->password_change_required);
    ASSERT_TRUE(vault_manager.change_user_password("bob", "temppass2345", "bobnewpass123"));
    ASSERT_TRUE(vault_manager.save_vault());
    ASSERT_TRUE(vault_manager.close_vault());

    // 5. Charlie logs in (no password change required)
    auto charlie_session = vault_manager.open_vault_v2(test_vault_path.string(), "charlie", "charlieperm");
    ASSERT_TRUE(charlie_session);
    EXPECT_FALSE(charlie_session->password_change_required);
    ASSERT_TRUE(vault_manager.close_vault());

    // 6. Alice (admin) removes bob
    ASSERT_TRUE(vault_manager.open_vault_v2(test_vault_path.string(), "alice", "alicenewpass"));
    ASSERT_TRUE(vault_manager.remove_user("bob"));
    ASSERT_TRUE(vault_manager.save_vault());
    ASSERT_TRUE(vault_manager.close_vault());

    // 7. Verify bob can't login
    auto bob_fail = vault_manager.open_vault_v2(test_vault_path.string(), "bob", "bobnewpass123");
    EXPECT_FALSE(bob_fail);

    // 8. Verify charlie can still login
    auto charlie_again = vault_manager.open_vault_v2(test_vault_path.string(), "charlie", "charlieperm");
    ASSERT_TRUE(charlie_again);

    // 9. Final user list
    auto final_users = vault_manager.list_users();
    EXPECT_EQ(final_users.size(), 3);  // admin, alice, charlie
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
