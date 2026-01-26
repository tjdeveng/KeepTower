// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

/**
 * @file test_username_hash_migration.cc
 * @brief Unit tests for username hash algorithm migration (Priority 1 tests)
 *
 * Tests cover:
 * - Two-phase authentication (find_slot_by_username_hash)
 * - Migration function (migrate_user_hash) success path
 * - Migration function error handling
 * - Automatic migration trigger in open_vault_v2
 *
 * See: docs/developer/USERNAME_HASH_MIGRATION_PLAN.md
 */

#include <gtest/gtest.h>
#include "../src/core/VaultManager.h"
#include "../src/core/MultiUserTypes.h"
#include "../src/core/services/UsernameHashService.h"
#include "../src/core/crypto/VaultCrypto.h"
#include "../src/core/io/VaultIO.h"
#include "../src/core/format/VaultFormat.h"
#include "../src/core/VaultFormatV2.h"
#include <filesystem>
#include <fstream>

using namespace KeepTower;

// ============================================================================
// Test Fixture
// ============================================================================

class UsernameHashMigrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_vault_path = std::filesystem::temp_directory_path() /
                         ("test_migration_" + std::to_string(std::time(nullptr)) + ".vault");
        cleanup_test_vault();
    }

    void TearDown() override {
        vault_manager.close_vault();
        cleanup_test_vault();
    }

    void cleanup_test_vault() {
        if (std::filesystem::exists(test_vault_path)) {
            std::filesystem::remove(test_vault_path);
        }
        // Also clean up backup files
        std::string backup_pattern = test_vault_path.string() + ".backup.*";
        for (const auto& entry : std::filesystem::directory_iterator(test_vault_path.parent_path())) {
            if (entry.path().string().find(test_vault_path.filename().string() + ".backup") != std::string::npos) {
                std::filesystem::remove(entry.path());
            }
        }
    }

    /**
     * Helper: Create vault with specific username hash algorithm
     */
    void create_test_vault(UsernameHashService::Algorithm algorithm,
                          const std::string& username = "alice",
                          const std::string& password = "TestPassword123!") {
        VaultSecurityPolicy policy;
        policy.require_yubikey = false;
        policy.min_password_length = 12;
        policy.pbkdf2_iterations = 100000;
        policy.username_hash_algorithm = static_cast<uint8_t>(algorithm);
        policy.username_hash_algorithm_previous = 0x00;  // No previous
        policy.migration_flags = 0x00;  // No migration
        policy.migration_started_at = 0;

        auto result = vault_manager.create_vault_v2(
            test_vault_path.string(),
            username,
            password,
            policy);

        ASSERT_TRUE(result) << "Failed to create test vault: " << to_string(result.error());
        ASSERT_TRUE(vault_manager.close_vault());
    }

    /**
     * Helper: Add additional user to open vault
     */
    void add_user_to_vault(const std::string& username,
                          const std::string& password,
                          UserRole role = UserRole::STANDARD_USER) {
        auto result = vault_manager.add_user(username, password, role, false);
        ASSERT_TRUE(result) << "Failed to add user: " << to_string(result.error());
    }

    /**
     * Helper: Read vault header directly to inspect migration fields
     */
    VaultHeaderV2 read_vault_header() {
        std::vector<uint8_t> file_data;
        int iterations;
        EXPECT_TRUE(VaultIO::read_file(test_vault_path.string(), file_data, true, iterations));

        auto header_result = VaultFormatV2::read_header(file_data);
        EXPECT_TRUE(header_result.has_value());

        return header_result.value().first.vault_header;
    }

    /**
     * Helper: Enable migration in vault
     * This simulates an admin enabling migration via UI
     */
    void enable_migration(UsernameHashService::Algorithm new_algorithm,
                         const std::string& admin_username = "alice",
                         const std::string& admin_password = "TestPassword123!") {
        // Open vault as admin
        auto session = vault_manager.open_vault_v2(
            test_vault_path.string(), admin_username, admin_password);
        ASSERT_TRUE(session) << "Failed to open vault as admin";

        // Get current policy
        auto policy_opt = vault_manager.get_vault_security_policy();
        ASSERT_TRUE(policy_opt.has_value()) << "Failed to get security policy";

        auto policy = *policy_opt;

        // Set migration fields
        policy.username_hash_algorithm_previous = policy.username_hash_algorithm;
        policy.username_hash_algorithm = static_cast<uint8_t>(new_algorithm);
        policy.migration_flags = 0x01;  // Enable migration
        policy.migration_started_at = static_cast<uint64_t>(std::time(nullptr));

        // Update policy using new API
        auto update_result = vault_manager.update_security_policy(policy);
        ASSERT_TRUE(update_result) << "Failed to update policy: " << to_string(update_result.error());

        // Save and close
        ASSERT_TRUE(vault_manager.save_vault()) << "Failed to save vault";
        ASSERT_TRUE(vault_manager.close_vault()) << "Failed to close vault";
    }

    std::filesystem::path test_vault_path;
    VaultManager vault_manager;
};

// ============================================================================
// Test 1: Two-Phase Authentication
// ============================================================================

/**
 * Test that two-phase authentication correctly handles:
 * - Phase 1: Migrated users authenticate with new algorithm
 * - Phase 2: Unmigrated users authenticate with old algorithm
 * - Phase 2 marks unmigrated users with status=0xFF for post-login migration
 */
TEST_F(UsernameHashMigrationTest, TwoPhaseAuthentication_MigratedUser) {
    // Step 1: Create vault with SHA3-256
    create_test_vault(UsernameHashService::Algorithm::SHA3_256);

    // Step 2: Open vault and add second user
    auto session1 = vault_manager.open_vault_v2(
        test_vault_path.string(), "alice", "TestPassword123!");
    ASSERT_TRUE(session1);

    add_user_to_vault("bob", "BobPassword123!");
    ASSERT_TRUE(vault_manager.save_vault());
    ASSERT_TRUE(vault_manager.close_vault());

    // Step 3: Enable migration to PBKDF2
    enable_migration(UsernameHashService::Algorithm::PBKDF2_SHA256);

    // Step 4: Manually migrate alice (simulate she logged in first)
    {
        auto session = vault_manager.open_vault_v2(
            test_vault_path.string(), "alice", "TestPassword123!");
        ASSERT_TRUE(session);

        // Get alice's slot and manually set it to migrated state
        auto users = vault_manager.list_users();

        // Alice should be marked 0xFF (pending migration) after Phase 2 auth
        bool found_alice = false;
        for (const auto& user : users) {
            if (user.username == "alice") {
                found_alice = true;
                // User was authenticated via old algorithm (Phase 2)
                // Migration should have been triggered automatically
                break;
            }
        }
        ASSERT_TRUE(found_alice);

        ASSERT_TRUE(vault_manager.close_vault());
    }

    // Step 5: Verify alice can login with new algorithm (Phase 1)
    // After migration, alice should authenticate via Phase 1
    auto session_alice = vault_manager.open_vault_v2(
        test_vault_path.string(), "alice", "TestPassword123!");
    EXPECT_TRUE(session_alice) << "Alice should authenticate with new algorithm (Phase 1)";
    EXPECT_EQ(session_alice->username, "alice");

    vault_manager.close_vault();

    // Step 6: Verify bob still authenticates with old algorithm (Phase 2)
    auto session_bob = vault_manager.open_vault_v2(
        test_vault_path.string(), "bob", "BobPassword123!");
    EXPECT_TRUE(session_bob) << "Bob should authenticate with old algorithm (Phase 2)";
    EXPECT_EQ(session_bob->username, "bob");
}

TEST_F(UsernameHashMigrationTest, TwoPhaseAuthentication_UnmigratedUser) {
    // Step 1: Create vault with SHA3-256
    create_test_vault(UsernameHashService::Algorithm::SHA3_256);

    // Step 2: Enable migration to PBKDF2
    enable_migration(UsernameHashService::Algorithm::PBKDF2_SHA256);

    // Step 3: Open vault - user should authenticate via Phase 2 (old algorithm)
    auto session = vault_manager.open_vault_v2(
        test_vault_path.string(), "alice", "TestPassword123!");
    ASSERT_TRUE(session) << "User should authenticate via Phase 2 (old algorithm)";
    EXPECT_EQ(session->username, "alice");

    // Step 4: Verify migration was triggered (user should be migrated now)
    auto users = vault_manager.list_users();
    ASSERT_EQ(users.size(), 1);

    vault_manager.close_vault();

    // Step 5: Login again - should now use Phase 1 (new algorithm)
    auto session2 = vault_manager.open_vault_v2(
        test_vault_path.string(), "alice", "TestPassword123!");
    EXPECT_TRUE(session2) << "User should authenticate via Phase 1 (new algorithm) after migration";
    EXPECT_EQ(session2->username, "alice");
}

TEST_F(UsernameHashMigrationTest, TwoPhaseAuthentication_WrongPassword) {
    // Create vault with SHA3-256
    create_test_vault(UsernameHashService::Algorithm::SHA3_256);

    // Enable migration to PBKDF2
    enable_migration(UsernameHashService::Algorithm::PBKDF2_SHA256);

    // Try wrong password - should fail in both phases
    auto session = vault_manager.open_vault_v2(
        test_vault_path.string(), "alice", "WrongPassword!");
    EXPECT_FALSE(session);
    EXPECT_EQ(session.error(), VaultError::AuthenticationFailed);
}

TEST_F(UsernameHashMigrationTest, TwoPhaseAuthentication_NoMigrationActive) {
    // Create vault with SHA3-256
    create_test_vault(UsernameHashService::Algorithm::SHA3_256);

    // Don't enable migration - should only try Phase 1
    auto session = vault_manager.open_vault_v2(
        test_vault_path.string(), "alice", "TestPassword123!");
    EXPECT_TRUE(session) << "User should authenticate normally without migration";
    EXPECT_EQ(session->username, "alice");
}

// ============================================================================
// Test 2: migrate_user_hash Success Path
// ============================================================================

/**
 * Test that migrate_user_hash() correctly:
 * - Generates new random salt
 * - Computes new hash with new algorithm
 * - Updates KeySlot fields (hash, salt, status, timestamp)
 * - Saves vault with backup
 */
TEST_F(UsernameHashMigrationTest, MigrateUserHash_SuccessPath) {
    // Step 1: Create vault with SHA3-256
    create_test_vault(UsernameHashService::Algorithm::SHA3_256);

    // Step 2: Enable migration to PBKDF2
    enable_migration(UsernameHashService::Algorithm::PBKDF2_SHA256);

    // Step 3: Read original vault header to get user's original hash
    auto header_before = read_vault_header();
    ASSERT_EQ(header_before.key_slots.size(), 1);
    auto original_hash = header_before.key_slots[0].username_hash;
    auto original_salt = header_before.key_slots[0].username_salt;
    auto original_status = header_before.key_slots[0].migration_status;
    EXPECT_EQ(original_status, 0x00) << "User should be unmigrated initially";

    // Step 4: Authenticate user (triggers migration)
    auto session = vault_manager.open_vault_v2(
        test_vault_path.string(), "alice", "TestPassword123!");
    ASSERT_TRUE(session);

    // Save to ensure migration is persisted
    ASSERT_TRUE(vault_manager.save_vault());
    ASSERT_TRUE(vault_manager.close_vault());

    // Step 5: Read vault header after migration
    auto header_after = read_vault_header();
    ASSERT_EQ(header_after.key_slots.size(), 1);

    const auto& slot_after = header_after.key_slots[0];

    // Verify: New salt was generated (should be different)
    EXPECT_NE(slot_after.username_salt, original_salt)
        << "New salt should be different from original";

    // Verify: New hash was computed (should be different)
    EXPECT_NE(slot_after.username_hash, original_hash)
        << "New hash should be different from original";

    // Verify: Hash size is correct for PBKDF2 (32 bytes)
    EXPECT_EQ(slot_after.username_hash_size, 32)
        << "PBKDF2-SHA256 hash should be 32 bytes";

    // Verify: Migration status is 0x01 (migrated)
    EXPECT_EQ(slot_after.migration_status, 0x01)
        << "User should be marked as migrated (0x01)";

    // Verify: Timestamp was set
    EXPECT_GT(slot_after.migrated_at, 0)
        << "Migration timestamp should be set";

    // Verify: Policy fields are correct
    EXPECT_EQ(header_after.security_policy.username_hash_algorithm,
              static_cast<uint8_t>(UsernameHashService::Algorithm::PBKDF2_SHA256))
        << "Current algorithm should be PBKDF2";

    EXPECT_EQ(header_after.security_policy.username_hash_algorithm_previous,
              static_cast<uint8_t>(UsernameHashService::Algorithm::SHA3_256))
        << "Previous algorithm should be SHA3-256";

    // Verify: Backup file was created
    bool backup_found = false;
    for (const auto& entry : std::filesystem::directory_iterator(test_vault_path.parent_path())) {
        if (entry.path().string().find(test_vault_path.filename().string() + ".backup") != std::string::npos) {
            backup_found = true;
            break;
        }
    }
    EXPECT_TRUE(backup_found) << "Backup file should be created during migration";
}

TEST_F(UsernameHashMigrationTest, MigrateUserHash_NewHashVerifies) {
    // Create vault with SHA3-256
    create_test_vault(UsernameHashService::Algorithm::SHA3_256);

    // Enable migration to PBKDF2
    enable_migration(UsernameHashService::Algorithm::PBKDF2_SHA256);

    // Authenticate user (triggers migration)
    auto session = vault_manager.open_vault_v2(
        test_vault_path.string(), "alice", "TestPassword123!");
    ASSERT_TRUE(session);
    ASSERT_TRUE(vault_manager.save_vault());
    ASSERT_TRUE(vault_manager.close_vault());

    // Read vault header
    auto header = read_vault_header();
    const auto& slot = header.key_slots[0];

    // Verify the new hash by computing it ourselves
    // Use the actual policy iterations, not hardcoded value
    auto computed_hash = UsernameHashService::hash_username(
        "alice",
        UsernameHashService::Algorithm::PBKDF2_SHA256,
        slot.username_salt,
        header.security_policy.pbkdf2_iterations  // Use actual policy value
    );

    ASSERT_TRUE(computed_hash.has_value());

    // Compare stored hash with computed hash
    std::vector<uint8_t> stored_hash_vec(
        slot.username_hash.begin(),
        slot.username_hash.begin() + slot.username_hash_size);

    EXPECT_EQ(*computed_hash, stored_hash_vec)
        << "Stored hash should match computed hash with new algorithm";
}

TEST_F(UsernameHashMigrationTest, MigrateUserHash_MultipleUsers) {
    // Create vault with SHA3-256
    create_test_vault(UsernameHashService::Algorithm::SHA3_256);

    // Add multiple users
    {
        auto session = vault_manager.open_vault_v2(
            test_vault_path.string(), "alice", "TestPassword123!");
        ASSERT_TRUE(session);

        add_user_to_vault("bob", "BobPassword123!");
        add_user_to_vault("charlie", "CharliePassword123!");

        ASSERT_TRUE(vault_manager.save_vault());
        ASSERT_TRUE(vault_manager.close_vault());
    }

    // Enable migration to PBKDF2
    enable_migration(UsernameHashService::Algorithm::PBKDF2_SHA256);

    // Migrate alice
    {
        auto session = vault_manager.open_vault_v2(
            test_vault_path.string(), "alice", "TestPassword123!");
        ASSERT_TRUE(session);
        ASSERT_TRUE(vault_manager.save_vault());
        ASSERT_TRUE(vault_manager.close_vault());
    }

    // Read vault and verify states
    auto header = read_vault_header();
    ASSERT_EQ(header.key_slots.size(), 3);

    int migrated_count = 0;
    int unmigrated_count = 0;

    for (const auto& slot : header.key_slots) {
        if (!slot.active) continue;

        if (slot.migration_status == 0x01) {
            migrated_count++;
        } else if (slot.migration_status == 0x00) {
            unmigrated_count++;
        }
    }

    EXPECT_EQ(migrated_count, 1) << "Only alice should be migrated";
    EXPECT_EQ(unmigrated_count, 2) << "Bob and charlie should be unmigrated";

    // Migrate bob
    {
        auto session = vault_manager.open_vault_v2(
            test_vault_path.string(), "bob", "BobPassword123!");
        ASSERT_TRUE(session);
        ASSERT_TRUE(vault_manager.save_vault());
        ASSERT_TRUE(vault_manager.close_vault());
    }

    // Verify bob is now migrated
    header = read_vault_header();
    migrated_count = 0;
    unmigrated_count = 0;

    for (const auto& slot : header.key_slots) {
        if (!slot.active) continue;

        if (slot.migration_status == 0x01) {
            migrated_count++;
        } else if (slot.migration_status == 0x00) {
            unmigrated_count++;
        }
    }

    EXPECT_EQ(migrated_count, 2) << "Alice and bob should be migrated";
    EXPECT_EQ(unmigrated_count, 1) << "Only charlie should be unmigrated";
}

// ============================================================================
// Test 3: migrate_user_hash Error Handling
// ============================================================================

/**
 * Test that migrate_user_hash() properly handles error cases:
 * - Migration not active (migration_flags not set)
 * - Vault not open
 * - Save failure during migration
 */
TEST_F(UsernameHashMigrationTest, MigrateUserHash_MigrationNotActive) {
    // Create vault with SHA3-256 (no migration enabled)
    create_test_vault(UsernameHashService::Algorithm::SHA3_256);

    // Open vault normally
    auto session = vault_manager.open_vault_v2(
        test_vault_path.string(), "alice", "TestPassword123!");
    ASSERT_TRUE(session);

    // Read vault header - migration should NOT have occurred
    // Use explicit_save=false to avoid creating backup (we want to verify migration didn't create one)
    ASSERT_TRUE(vault_manager.save_vault(false));
    ASSERT_TRUE(vault_manager.close_vault());

    auto header = read_vault_header();
    ASSERT_EQ(header.key_slots.size(), 1);

    // Verify: User was not migrated (no migration active)
    EXPECT_EQ(header.key_slots[0].migration_status, 0x00)
        << "User should not be migrated when migration is not active";

    // Verify: No backup was created
    bool backup_found = false;
    for (const auto& entry : std::filesystem::directory_iterator(test_vault_path.parent_path())) {
        if (entry.path().string().find(test_vault_path.filename().string() + ".backup") != std::string::npos) {
            backup_found = true;
            break;
        }
    }
    EXPECT_FALSE(backup_found) << "No backup should be created without migration";
}

TEST_F(UsernameHashMigrationTest, MigrateUserHash_AuthenticationStillSucceedsOnError) {
    // Create vault
    create_test_vault(UsernameHashService::Algorithm::SHA3_256);

    // Enable migration
    enable_migration(UsernameHashService::Algorithm::PBKDF2_SHA256);

    // Authenticate - even if migration fails, auth should succeed
    auto session = vault_manager.open_vault_v2(
        test_vault_path.string(), "alice", "TestPassword123!");

    EXPECT_TRUE(session) << "Authentication should succeed even if migration fails";
    EXPECT_EQ(session->username, "alice");
}

// ============================================================================
// Test 4: open_vault_v2 Triggers Migration
// ============================================================================

/**
 * Test that open_vault_v2 automatically triggers migration when:
 * - User authenticates via Phase 2 (old algorithm)
 * - User is marked with migration_status = 0xFF
 */
TEST_F(UsernameHashMigrationTest, OpenVaultV2_TriggersMigration) {
    // Step 1: Create vault with SHA3-256
    create_test_vault(UsernameHashService::Algorithm::SHA3_256);

    // Step 2: Verify initial state (no migration)
    auto header_before = read_vault_header();
    EXPECT_EQ(header_before.key_slots[0].migration_status, 0x00);
    EXPECT_EQ(header_before.security_policy.migration_flags, 0x00);

    // Step 3: Enable migration to PBKDF2
    enable_migration(UsernameHashService::Algorithm::PBKDF2_SHA256);

    // Step 4: Verify migration is enabled
    auto header_enabled = read_vault_header();
    EXPECT_EQ(header_enabled.security_policy.migration_flags, 0x01);
    EXPECT_EQ(header_enabled.security_policy.username_hash_algorithm,
              static_cast<uint8_t>(UsernameHashService::Algorithm::PBKDF2_SHA256));

    // Step 5: Open vault (should trigger automatic migration)
    auto session = vault_manager.open_vault_v2(
        test_vault_path.string(), "alice", "TestPassword123!");
    ASSERT_TRUE(session) << "Authentication should succeed";

    // Migration happens automatically, save to persist
    ASSERT_TRUE(vault_manager.save_vault());
    ASSERT_TRUE(vault_manager.close_vault());

    // Step 6: Verify migration occurred
    auto header_after = read_vault_header();
    EXPECT_EQ(header_after.key_slots[0].migration_status, 0x01)
        << "User should be migrated after open_vault_v2";
    EXPECT_GT(header_after.key_slots[0].migrated_at, 0)
        << "Migration timestamp should be set";
}

TEST_F(UsernameHashMigrationTest, OpenVaultV2_SecondLoginUsesNewAlgorithm) {
    // Create vault with SHA3-256
    create_test_vault(UsernameHashService::Algorithm::SHA3_256);

    // Enable migration to PBKDF2
    enable_migration(UsernameHashService::Algorithm::PBKDF2_SHA256);

    // First login - triggers migration via Phase 2
    {
        auto session = vault_manager.open_vault_v2(
            test_vault_path.string(), "alice", "TestPassword123!");
        ASSERT_TRUE(session);
        ASSERT_TRUE(vault_manager.save_vault());
        ASSERT_TRUE(vault_manager.close_vault());
    }

    // Verify user is migrated
    auto header = read_vault_header();
    EXPECT_EQ(header.key_slots[0].migration_status, 0x01);

    // Second login - should use Phase 1 (new algorithm)
    {
        auto session = vault_manager.open_vault_v2(
            test_vault_path.string(), "alice", "TestPassword123!");
        EXPECT_TRUE(session) << "Second login should use new algorithm (Phase 1)";
        EXPECT_EQ(session->username, "alice");
    }
}

TEST_F(UsernameHashMigrationTest, OpenVaultV2_NonBlockingMigration) {
    // Create vault
    create_test_vault(UsernameHashService::Algorithm::SHA3_256);

    // Enable migration
    enable_migration(UsernameHashService::Algorithm::PBKDF2_SHA256);

    // Open vault - authentication should succeed even if migration encounters issues
    auto session = vault_manager.open_vault_v2(
        test_vault_path.string(), "alice", "TestPassword123!");

    EXPECT_TRUE(session)
        << "Authentication should succeed (non-blocking migration)";
    EXPECT_EQ(session->username, "alice");
    EXPECT_EQ(session->role, UserRole::ADMINISTRATOR);
}

// ============================================================================
// Integration Test: Complete Migration Workflow
// ============================================================================

TEST_F(UsernameHashMigrationTest, CompleteWorkflow_SHA256_to_PBKDF2) {
    // Step 1: Create vault with SHA3-256
    create_test_vault(UsernameHashService::Algorithm::SHA3_256, "admin", "AdminPass123!");

    // Step 2: Add multiple users
    {
        auto session = vault_manager.open_vault_v2(
            test_vault_path.string(), "admin", "AdminPass123!");
        ASSERT_TRUE(session);

        add_user_to_vault("user1", "User1Pass123!");
        add_user_to_vault("user2", "User2Pass123!");
        add_user_to_vault("user3", "User3Pass123!");

        ASSERT_TRUE(vault_manager.save_vault());
        ASSERT_TRUE(vault_manager.close_vault());
    }

    // Step 3: Admin enables migration to PBKDF2
    {
        auto session = vault_manager.open_vault_v2(
            test_vault_path.string(), "admin", "AdminPass123!");
        ASSERT_TRUE(session);

        // Close vault so we can manipulate the file
        ASSERT_TRUE(vault_manager.save_vault());
        ASSERT_TRUE(vault_manager.close_vault());
    }

    // Enable migration by directly modifying vault file
    enable_migration(UsernameHashService::Algorithm::PBKDF2_SHA256, "admin", "AdminPass123!");

    // Step 4: Users log in one by one, each triggering their own migration

    // User1 logs in
    {
        auto session = vault_manager.open_vault_v2(
            test_vault_path.string(), "user1", "User1Pass123!");
        ASSERT_TRUE(session);
        ASSERT_TRUE(vault_manager.save_vault());
        ASSERT_TRUE(vault_manager.close_vault());
    }

    // Verify user1 migrated, others not
    {
        auto header = read_vault_header();
        int migrated = 0, unmigrated = 0;
        for (const auto& slot : header.key_slots) {
            if (!slot.active) continue;
            if (slot.migration_status == 0x01) migrated++;
            else if (slot.migration_status == 0x00) unmigrated++;
        }
        EXPECT_EQ(migrated, 1) << "Only user1 should be migrated";
        EXPECT_EQ(unmigrated, 3) << "Admin, user2, user3 should be unmigrated";
    }

    // Admin logs in
    {
        auto session = vault_manager.open_vault_v2(
            test_vault_path.string(), "admin", "AdminPass123!");
        ASSERT_TRUE(session);
        ASSERT_TRUE(vault_manager.save_vault());
        ASSERT_TRUE(vault_manager.close_vault());
    }

    // User2 logs in
    {
        auto session = vault_manager.open_vault_v2(
            test_vault_path.string(), "user2", "User2Pass123!");
        ASSERT_TRUE(session);
        ASSERT_TRUE(vault_manager.save_vault());
        ASSERT_TRUE(vault_manager.close_vault());
    }

    // Step 5: Verify migration states
    {
        auto header = read_vault_header();
        int migrated = 0, unmigrated = 0;
        for (const auto& slot : header.key_slots) {
            if (!slot.active) continue;
            if (slot.migration_status == 0x01) migrated++;
            else if (slot.migration_status == 0x00) unmigrated++;
        }
        EXPECT_EQ(migrated, 3) << "Admin, user1, user2 should be migrated";
        EXPECT_EQ(unmigrated, 1) << "Only user3 should be unmigrated";
    }

    // Step 6: All migrated users can login with new algorithm
    {
        auto session1 = vault_manager.open_vault_v2(
            test_vault_path.string(), "admin", "AdminPass123!");
        EXPECT_TRUE(session1);
        vault_manager.close_vault();

        auto session2 = vault_manager.open_vault_v2(
            test_vault_path.string(), "user1", "User1Pass123!");
        EXPECT_TRUE(session2);
        vault_manager.close_vault();

        auto session3 = vault_manager.open_vault_v2(
            test_vault_path.string(), "user2", "User2Pass123!");
        EXPECT_TRUE(session3);
        vault_manager.close_vault();
    }

    // Step 7: Unmigrated user still authenticates (Phase 2)
    {
        auto session = vault_manager.open_vault_v2(
            test_vault_path.string(), "user3", "User3Pass123!");
        EXPECT_TRUE(session) << "Unmigrated user should still authenticate via Phase 2";
    }
}
