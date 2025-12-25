// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file test_password_history.cc
 * @brief Unit tests for password history feature (Phase 9)
 *
 * Tests password hashing, reuse detection, ring buffer management,
 * and VaultManager integration.
 */

#include <gtest/gtest.h>
#include "../src/core/PasswordHistory.h"
#include "../src/core/MultiUserTypes.h"
#include "../src/core/VaultManager.h"
#include <thread>
#include <chrono>
#include <filesystem>

using namespace KeepTower;

// ============================================================================
// PasswordHistory Class Tests
// ============================================================================

class PasswordHistoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // No setup needed - PasswordHistory uses static methods
    }

    void TearDown() override {
        // No teardown needed
    }
};

/**
 * Test: hash_password() generates valid entry
 */
TEST_F(PasswordHistoryTest, HashPasswordGeneratesValidEntry) {
    Glib::ustring password = "TestPassword123!";

    auto entry = PasswordHistory::hash_password(password);

    ASSERT_TRUE(entry.has_value());
    EXPECT_GT(entry->timestamp, 0);
    EXPECT_EQ(entry->salt.size(), 32);  // 32-byte salt
    EXPECT_EQ(entry->hash.size(), 48);  // 48-byte hash

    // Verify salt is not all zeros
    bool all_zeros = true;
    for (uint8_t byte : entry->salt) {
        if (byte != 0) {
            all_zeros = false;
            break;
        }
    }
    EXPECT_FALSE(all_zeros);

    // Verify hash is not all zeros
    all_zeros = true;
    for (uint8_t byte : entry->hash) {
        if (byte != 0) {
            all_zeros = false;
            break;
        }
    }
    EXPECT_FALSE(all_zeros);
}

/**
 * Test: hash_password() generates unique salts
 */
TEST_F(PasswordHistoryTest, HashPasswordGeneratesUniqueSalts) {
    Glib::ustring password = "SamePassword";

    auto entry1 = PasswordHistory::hash_password(password);
    auto entry2 = PasswordHistory::hash_password(password);

    ASSERT_TRUE(entry1.has_value());
    ASSERT_TRUE(entry2.has_value());

    // Different salts even for same password
    EXPECT_NE(entry1->salt, entry2->salt);

    // Different hashes due to different salts
    EXPECT_NE(entry1->hash, entry2->hash);
}

/**
 * Test: hash_password() handles empty password
 */
TEST_F(PasswordHistoryTest, HashPasswordHandlesEmptyPassword) {
    Glib::ustring empty_password = "";

    auto entry = PasswordHistory::hash_password(empty_password);

    // Should still generate valid entry (validation happens elsewhere)
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->salt.size(), 32);
    EXPECT_EQ(entry->hash.size(), 48);
}

/**
 * Test: hash_password() handles UTF-8 passwords
 */
TEST_F(PasswordHistoryTest, HashPasswordHandlesUTF8) {
    Glib::ustring utf8_password = "Pássw0rd™🔒";

    auto entry = PasswordHistory::hash_password(utf8_password);

    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->salt.size(), 32);
    EXPECT_EQ(entry->hash.size(), 48);
}

/**
 * Test: is_password_reused() detects matching password
 */
TEST_F(PasswordHistoryTest, IsPasswordReusedDetectsMatch) {
    Glib::ustring password = "MyPassword123";

    // Create history with the password
    auto entry = PasswordHistory::hash_password(password);
    ASSERT_TRUE(entry.has_value());

    std::vector<PasswordHistoryEntry> history = { entry.value() };

    // Should detect reuse
    EXPECT_TRUE(PasswordHistory::is_password_reused(password, history));
}

/**
 * Test: is_password_reused() rejects different password
 */
TEST_F(PasswordHistoryTest, IsPasswordReusedRejectsDifferent) {
    Glib::ustring password1 = "Password1";
    Glib::ustring password2 = "Password2";

    // Create history with password1
    auto entry = PasswordHistory::hash_password(password1);
    ASSERT_TRUE(entry.has_value());

    std::vector<PasswordHistoryEntry> history = { entry.value() };

    // Should NOT detect reuse for different password
    EXPECT_FALSE(PasswordHistory::is_password_reused(password2, history));
}

/**
 * Test: is_password_reused() handles empty history
 */
TEST_F(PasswordHistoryTest, IsPasswordReusedHandlesEmptyHistory) {
    Glib::ustring password = "TestPassword";
    std::vector<PasswordHistoryEntry> empty_history;

    // Should return false for empty history
    EXPECT_FALSE(PasswordHistory::is_password_reused(password, empty_history));
}

/**
 * Test: is_password_reused() checks all entries
 */
TEST_F(PasswordHistoryTest, IsPasswordReusedChecksAllEntries) {
    Glib::ustring password1 = "Old1";
    Glib::ustring password2 = "Old2";
    Glib::ustring password3 = "Old3";
    Glib::ustring new_password = "Old2";  // Matches middle entry

    // Create history with 3 entries
    auto entry1 = PasswordHistory::hash_password(password1);
    auto entry2 = PasswordHistory::hash_password(password2);
    auto entry3 = PasswordHistory::hash_password(password3);

    std::vector<PasswordHistoryEntry> history = {
        entry1.value(),
        entry2.value(),
        entry3.value()
    };

    // Should detect match in middle of history
    EXPECT_TRUE(PasswordHistory::is_password_reused(new_password, history));
}

/**
 * Test: is_password_reused() is case-sensitive
 */
TEST_F(PasswordHistoryTest, IsPasswordReusedCaseSensitive) {
    Glib::ustring password_lower = "password";
    Glib::ustring password_upper = "PASSWORD";

    auto entry = PasswordHistory::hash_password(password_lower);
    ASSERT_TRUE(entry.has_value());

    std::vector<PasswordHistoryEntry> history = { entry.value() };

    // Case matters - different passwords
    EXPECT_FALSE(PasswordHistory::is_password_reused(password_upper, history));
}

/**
 * Test: add_to_history() adds entry
 */
TEST_F(PasswordHistoryTest, AddToHistoryAddsEntry) {
    std::vector<PasswordHistoryEntry> history;

    auto entry = PasswordHistory::hash_password("Password1");
    ASSERT_TRUE(entry.has_value());

    PasswordHistory::add_to_history(history, entry.value(), 5);

    EXPECT_EQ(history.size(), 1);
}

/**
 * Test: add_to_history() respects max depth
 */
TEST_F(PasswordHistoryTest, AddToHistoryRespectsMaxDepth) {
    std::vector<PasswordHistoryEntry> history;
    uint32_t max_depth = 3;

    // Add 5 entries (exceeds max depth)
    for (int i = 0; i < 5; i++) {
        auto entry = PasswordHistory::hash_password("Password" + std::to_string(i));
        ASSERT_TRUE(entry.has_value());
        PasswordHistory::add_to_history(history, entry.value(), max_depth);
    }

    // Should only keep most recent 3
    EXPECT_EQ(history.size(), max_depth);
}

/**
 * Test: add_to_history() FIFO eviction
 */
TEST_F(PasswordHistoryTest, AddToHistoryFIFOEviction) {
    std::vector<PasswordHistoryEntry> history;
    uint32_t max_depth = 2;

    // Add 3 entries with small delays for timestamp differences
    auto entry1 = PasswordHistory::hash_password("Password1");
    ASSERT_TRUE(entry1.has_value());
    int64_t timestamp1 = entry1->timestamp;
    PasswordHistory::add_to_history(history, entry1.value(), max_depth);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto entry2 = PasswordHistory::hash_password("Password2");
    ASSERT_TRUE(entry2.has_value());
    int64_t timestamp2 = entry2->timestamp;
    PasswordHistory::add_to_history(history, entry2.value(), max_depth);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto entry3 = PasswordHistory::hash_password("Password3");
    ASSERT_TRUE(entry3.has_value());
    int64_t timestamp3 = entry3->timestamp;
    PasswordHistory::add_to_history(history, entry3.value(), max_depth);

    // Should keep most recent 2 (entry2 and entry3)
    EXPECT_EQ(history.size(), 2);

    // Oldest entry (entry1) should be removed
    bool has_entry1 = false;
    for (const auto& entry : history) {
        if (entry.timestamp == timestamp1) {
            has_entry1 = true;
            break;
        }
    }
    EXPECT_FALSE(has_entry1);

    // Newer entries should remain
    bool has_entry2 = false;
    bool has_entry3 = false;
    for (const auto& entry : history) {
        if (entry.timestamp == timestamp2) has_entry2 = true;
        if (entry.timestamp == timestamp3) has_entry3 = true;
    }
    EXPECT_TRUE(has_entry2);
    EXPECT_TRUE(has_entry3);
}

/**
 * Test: trim_history() reduces size
 */
TEST_F(PasswordHistoryTest, TrimHistoryReducesSize) {
    std::vector<PasswordHistoryEntry> history;

    // Add 5 entries
    for (int i = 0; i < 5; i++) {
        auto entry = PasswordHistory::hash_password("Password" + std::to_string(i));
        ASSERT_TRUE(entry.has_value());
        history.push_back(entry.value());
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    EXPECT_EQ(history.size(), 5);

    // Trim to 2
    PasswordHistory::trim_history(history, 2);

    EXPECT_EQ(history.size(), 2);
}

/**
 * Test: trim_history() preserves most recent
 */
TEST_F(PasswordHistoryTest, TrimHistoryPreservesMostRecent) {
    std::vector<PasswordHistoryEntry> history;

    // Add entries with identifiable timestamps
    for (int i = 0; i < 5; i++) {
        auto entry = PasswordHistory::hash_password("Password" + std::to_string(i));
        ASSERT_TRUE(entry.has_value());
        history.push_back(entry.value());
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    int64_t last_timestamp = history.back().timestamp;

    // Trim to 1
    PasswordHistory::trim_history(history, 1);

    EXPECT_EQ(history.size(), 1);
    EXPECT_EQ(history[0].timestamp, last_timestamp);
}

/**
 * Test: trim_history() does nothing if size <= max_depth
 */
TEST_F(PasswordHistoryTest, TrimHistoryDoesNothingIfBelowDepth) {
    std::vector<PasswordHistoryEntry> history;

    // Add 2 entries
    for (int i = 0; i < 2; i++) {
        auto entry = PasswordHistory::hash_password("Password" + std::to_string(i));
        ASSERT_TRUE(entry.has_value());
        history.push_back(entry.value());
    }

    // Trim with higher max_depth
    PasswordHistory::trim_history(history, 5);

    // Should remain unchanged
    EXPECT_EQ(history.size(), 2);
}

// ============================================================================
// VaultManager Integration Tests
// ============================================================================

class PasswordHistoryIntegrationTest : public ::testing::Test {
protected:
    VaultManager vault_manager;
    std::filesystem::path test_vault_path;

    void SetUp() override {
        test_vault_path = std::filesystem::temp_directory_path() / "test_password_history_vault.vault";
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
};

/**
 * Test: V2 vault creation respects password_history_depth
 */
TEST_F(PasswordHistoryIntegrationTest, V2VaultCreationWithHistory) {
    VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    policy.password_history_depth = 5;
    policy.pbkdf2_iterations = 600000;
    policy.require_yubikey = false;

    auto result = vault_manager.create_vault_v2(
        test_vault_path.string(),
        "admin",
        "AdminPass123!",
        policy);

    ASSERT_TRUE(result);
    EXPECT_TRUE(vault_manager.is_vault_open());
}

/**
 * Test: change_user_password() detects password reuse
 */
TEST_F(PasswordHistoryIntegrationTest, ChangePasswordDetectsReuse) {
    // Create vault with history enabled
    VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    policy.password_history_depth = 3;
    policy.pbkdf2_iterations = 100000;  // Lower for faster tests
    policy.require_yubikey = false;

    auto create_result = vault_manager.create_vault_v2(
        test_vault_path.string(),
        "testuser",
        "InitialPass123!",
        policy);
    ASSERT_TRUE(create_result);

    // Try to change to same password (requires old password as first arg)
    auto change_result = vault_manager.change_user_password(
        "InitialPass123!",  // Current password
        "InitialPass123!",  // New password (same!)
        "InitialPass123!"); // Confirmation

    EXPECT_FALSE(change_result);
    EXPECT_EQ(change_result.error(), VaultError::PasswordReused);
}

/**
 * Test: change_user_password() allows unique password
 */
TEST_F(PasswordHistoryIntegrationTest, ChangePasswordAllowsUnique) {
    // Create vault
    VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    policy.password_history_depth = 3;
    policy.pbkdf2_iterations = 100000;
    policy.require_yubikey = false;

    auto create_result = vault_manager.create_vault_v2(
        test_vault_path.string(),
        "testuser",
        "InitialPass123!",
        policy);
    ASSERT_TRUE(create_result);

    // Change to different password
    auto change_result = vault_manager.change_user_password(
        "InitialPass123!",
        "NewPassword456!",
        "NewPassword456!");

    EXPECT_TRUE(change_result);
}

/**
 * Test: change_user_password() tracks history
 */
TEST_F(PasswordHistoryIntegrationTest, ChangePasswordTracksHistory) {
    // Create vault
    VaultSecurityPolicy policy;
    policy.min_password_length = 4;
    policy.password_history_depth = 3;
    policy.pbkdf2_iterations = 100000;
    policy.require_yubikey = false;

    auto create_result = vault_manager.create_vault_v2(
        test_vault_path.string(),
        "testuser",
        "Pass1",
        policy);
    ASSERT_TRUE(create_result);

    // Change password 3 times
    EXPECT_TRUE(vault_manager.change_user_password("Pass1", "Pass2", "Pass2"));
    EXPECT_TRUE(vault_manager.change_user_password("Pass2", "Pass3", "Pass3"));
    EXPECT_TRUE(vault_manager.change_user_password("Pass3", "Pass4", "Pass4"));

    // Try to reuse Pass2 (should be in history)
    auto reuse_result = vault_manager.change_user_password("Pass4", "Pass2", "Pass2");
    EXPECT_FALSE(reuse_result);
    EXPECT_EQ(reuse_result.error(), VaultError::PasswordReused);
}

/**
 * Test: change_user_password() respects depth limit
 */
TEST_F(PasswordHistoryIntegrationTest, ChangePasswordRespectsDepth) {
    // Create vault with depth=2
    VaultSecurityPolicy policy;
    policy.min_password_length = 4;
    policy.password_history_depth = 2;  // Only remember last 2
    policy.pbkdf2_iterations = 100000;
    policy.require_yubikey = false;

    auto create_result = vault_manager.create_vault_v2(
        test_vault_path.string(),
        "testuser",
        "Pass1",
        policy);
    ASSERT_TRUE(create_result);

    // Change password 3 times
    EXPECT_TRUE(vault_manager.change_user_password("Pass1", "Pass2", "Pass2"));
    EXPECT_TRUE(vault_manager.change_user_password("Pass2", "Pass3", "Pass3"));
    EXPECT_TRUE(vault_manager.change_user_password("Pass3", "Pass4", "Pass4"));

    // Pass1 should have been evicted (depth=2)
    auto reuse_pass1 = vault_manager.change_user_password("Pass4", "Pass1", "Pass1");
    EXPECT_TRUE(reuse_pass1);  // Should succeed - Pass1 no longer in history

    // Pass3 should still be in history
    auto reuse_pass3 = vault_manager.change_user_password("Pass1", "Pass3", "Pass3");
    EXPECT_FALSE(reuse_pass3);
    EXPECT_EQ(reuse_pass3.error(), VaultError::PasswordReused);
}

/**
 * Test: add_user() initializes password history
 */
TEST_F(PasswordHistoryIntegrationTest, AddUserInitializesHistory) {
    // Create vault as admin
    VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    policy.password_history_depth = 3;
    policy.pbkdf2_iterations = 100000;
    policy.require_yubikey = false;

    auto create_result = vault_manager.create_vault_v2(
        test_vault_path.string(),
        "admin",
        "AdminPass123!",
        policy);
    ASSERT_TRUE(create_result);

    // Add new user
    auto add_result = vault_manager.add_user(
        "newuser",
        "TempPass456!",
        UserRole::STANDARD_USER,
        true);  // must_change_password
    ASSERT_TRUE(add_result);

    // Close and reopen as new user
    vault_manager.close_vault();
    auto open_result = vault_manager.open_vault_v2(
        test_vault_path.string(),
        "newuser",
        "TempPass456!");
    ASSERT_TRUE(open_result);

    // Try to change to same temp password (should be in history)
    auto change_result = vault_manager.change_user_password(
        "TempPass456!",
        "TempPass456!",
        "TempPass456!");
    EXPECT_FALSE(change_result);
    EXPECT_EQ(change_result.error(), VaultError::PasswordReused);
}

/**
 * Test: admin_reset_user_password() clears history
 */
TEST_F(PasswordHistoryIntegrationTest, AdminResetClearsHistory) {
    // Create vault as admin
    VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    policy.password_history_depth = 3;
    policy.pbkdf2_iterations = 100000;
    policy.require_yubikey = false;

    auto create_result = vault_manager.create_vault_v2(
        test_vault_path.string(),
        "admin",
        "AdminPass123!",
        policy);
    ASSERT_TRUE(create_result);

    // Add user
    ASSERT_TRUE(vault_manager.add_user("user1", "UserPass1!", UserRole::STANDARD_USER, false));

    // Close and reopen as user1
    vault_manager.close_vault();
    ASSERT_TRUE(vault_manager.open_vault_v2(test_vault_path.string(), "user1", "UserPass1!"));

    // Change password to build history
    EXPECT_TRUE(vault_manager.change_user_password("UserPass1!", "UserPass2!", "UserPass2!"));

    // Close and reopen as admin
    vault_manager.close_vault();
    ASSERT_TRUE(vault_manager.open_vault_v2(test_vault_path.string(), "admin", "AdminPass123!"));

    // Admin resets user password
    auto reset_result = vault_manager.admin_reset_user_password("user1", "NewReset123!");
    EXPECT_TRUE(reset_result);

    // Close and reopen as user1 with new password
    vault_manager.close_vault();
    ASSERT_TRUE(vault_manager.open_vault_v2(test_vault_path.string(), "user1", "NewReset123!"));

    // User should be able to reuse old password (history was cleared)
    auto change_result = vault_manager.change_user_password("NewReset123!", "UserPass1!", "UserPass1!");
    EXPECT_TRUE(change_result);  // Should succeed - history cleared
}

/**
 * Test: Password history disabled (depth=0)
 */
TEST_F(PasswordHistoryIntegrationTest, PasswordHistoryDisabled) {
    // Create vault with history disabled
    VaultSecurityPolicy policy;
    policy.min_password_length = 8;
    policy.password_history_depth = 0;  // Disabled
    policy.pbkdf2_iterations = 100000;
    policy.require_yubikey = false;

    auto create_result = vault_manager.create_vault_v2(
        test_vault_path.string(),
        "testuser",
        "Pass123!",
        policy);
    ASSERT_TRUE(create_result);

    // Should allow changing to same password (no history check)
    auto change_result = vault_manager.change_user_password("Pass123!", "Pass123!", "Pass123!");
    EXPECT_TRUE(change_result);  // Should succeed when history disabled
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
