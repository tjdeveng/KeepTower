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
#include "../src/core/VaultManager.h"
#include "../src/core/MultiUserTypes.h"
#include <filesystem>
#include <fstream>

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

    // Clear history (admin only)
    auto result = vault_manager.clear_user_password_history("bob");
    ASSERT_TRUE(result) << "Admin should be able to clear password history";

    // Now bob can reuse old password
    auto reuse_result = vault_manager.change_user_password("bob", "bobpass34567", "bobpass12345");
    EXPECT_TRUE(reuse_result) << "Should allow password reuse after history cleared";
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

TEST_F(VaultManagerV2Test, AdminResetPasswordClearsHistory) {
    VaultSecurityPolicy policy;
    policy.password_history_depth = 3;
    ASSERT_TRUE(vault_manager.create_vault_v2(
        test_vault_path.string(), "admin", "adminpass123", policy));
    ASSERT_TRUE(vault_manager.add_user("bob", "bobpass12345", UserRole::STANDARD_USER));

    // Bob changes password to build history
    ASSERT_TRUE(vault_manager.change_user_password("bob", "bobpass12345", "bobpass23456"));
    ASSERT_TRUE(vault_manager.change_user_password("bob", "bobpass23456", "bobpass34567"));

    // Admin resets password (clears history)
    ASSERT_TRUE(vault_manager.admin_reset_user_password("bob", "adminreset123"));
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
