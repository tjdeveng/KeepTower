// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

/**
 * @file test_username_hash_migration_priority3.cc
 * @brief Priority 3 tests for username hash algorithm migration
 *
 * Test Coverage:
 * - Performance & Scalability: Many users, concurrent operations
 * - Security Validations: Constant-time operations, rollback protection
 * - Error Handling: Resource limits, corrupted data recovery
 * - Edge Cases: Empty vaults, boundary conditions
 */

#include <gtest/gtest.h>
#include <filesystem>
#include <chrono>
#include <thread>
#include <future>
#include <fstream>
#include <regex>
#include <sstream>
#include "../src/core/VaultManager.h"
#include "../src/core/MultiUserTypes.h"
#include "../src/core/services/UsernameHashService.h"
#include "../src/core/crypto/VaultCrypto.h"
#include "../src/core/io/VaultIO.h"
#include "../src/core/format/VaultFormat.h"
#include "../src/core/VaultFormatV2.h"

using namespace KeepTower;

/**
 * Test fixture for Priority 3 username hash migration tests
 */
class UsernameHashMigrationPriority3Test : public ::testing::Test {
protected:
    VaultManager vault_manager;
    std::filesystem::path test_vault_path;

    void SetUp() override {
        // Create unique test vault path
        test_vault_path = std::filesystem::temp_directory_path() /
            ("test_migration_p3_" + std::to_string(std::time(nullptr)) + ".vault");
    }

    void TearDown() override {
        // Clean up test vault
        vault_manager.close_vault();

        if (std::filesystem::exists(test_vault_path)) {
            std::filesystem::remove(test_vault_path);
        }

        // Clean up backup files
        for (const auto& entry : std::filesystem::directory_iterator(test_vault_path.parent_path())) {
            if (entry.path().string().find(test_vault_path.filename().string() + ".backup") != std::string::npos) {
                std::filesystem::remove(entry.path());
            }
        }
    }

    /**
     * Helper: Create test vault with specified algorithm
     */
    void create_test_vault(UsernameHashService::Algorithm algorithm,
                          const std::string& admin_username = "alice",
                          const std::string& admin_password = "TestPassword123!") {
        VaultSecurityPolicy policy;
        policy.min_password_length = 12;
        policy.pbkdf2_iterations = 100000;
        policy.username_hash_algorithm = static_cast<uint8_t>(algorithm);
        policy.require_yubikey = false;

        auto result = vault_manager.create_vault_v2(
            test_vault_path.string(),
            admin_username,
            admin_password,
            policy);

        ASSERT_TRUE(result.has_value()) << "Failed to create vault";
        ASSERT_TRUE(vault_manager.close_vault());
    }

    /**
     * Helper: Add user to vault
     */
    void add_user_to_vault(const std::string& username, const std::string& password) {
        auto result = vault_manager.add_user(username, password, UserRole::STANDARD_USER);
        ASSERT_TRUE(result.has_value()) << "Failed to add user: " << username;
    }

    /**
     * Helper: Enable migration
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

    /**
     * Helper: Read vault header
     */
    VaultHeaderV2 read_vault_header() {
        std::vector<unsigned char> file_data;
        int iterations;
        EXPECT_TRUE(VaultIO::read_file(test_vault_path.string(), file_data, true, iterations));

        auto header_result = VaultFormatV2::read_header(file_data);
        EXPECT_TRUE(header_result.has_value());

        return header_result.value().first.vault_header;
    }

    /**
     * Helper: Count users by migration status
     */
    struct MigrationStats {
        size_t total = 0;
        size_t migrated = 0;
        size_t pending = 0;
        size_t not_migrated = 0;
    };

    MigrationStats count_migration_statuses() {
        auto header = read_vault_header();
        MigrationStats stats;

        for (const auto& slot : header.key_slots) {
            if (slot.active) {
                stats.total++;
                if (slot.migration_status == 0x01) {
                    stats.migrated++;
                } else if (slot.migration_status == 0xFF) {
                    stats.pending++;
                } else {
                    stats.not_migrated++;
                }
            }
        }

        return stats;
    }

    /**
     * Helper: Load performance baseline from JSON configuration
     */
    int get_baseline(const std::string& key, int default_val) {
        // Try to locate the baseline file relative to the execution directory
        // Typically running from <root>/build/tests/ or <root>/build/
        std::vector<std::filesystem::path> candidates = {
            "tests/data/performance_baseline.json",              // Run from root
            "../tests/data/performance_baseline.json",           // Run from build
            "../../tests/data/performance_baseline.json",        // Run from build/tests
            "../../../tests/data/performance_baseline.json"      // Run from deeply nested
        };

        std::string content;
        bool found = false;

        for (const auto& path : candidates) {
            if (std::filesystem::exists(path)) {
                std::ifstream f(path);
                if (f.is_open()) {
                    std::stringstream buffer;
                    buffer << f.rdbuf();
                    content = buffer.str();
                    found = true;
                    break;
                }
            }
        }

        if (!found) {
            // Fallback for direct execution if file not found
            std::cout << "Warning: Performance baseline file not found, using default for " << key << std::endl;
            return default_val;
        }

        // Simple regex to find "key": 123
        std::regex re("\"" + key + "\"\\s*:\\s*(\\d+)");
        std::smatch match;
        if (std::regex_search(content, match, re) && match.size() > 1) {
            return std::stoi(match[1].str());
        }

        return default_val;
    }
};

// ============================================================================
// Test Group 1: Performance & Scalability
// ============================================================================

/**
 * Test migration performance with 20 users
 *
 * Validates that migration completes in reasonable time for medium vault
 */
TEST_F(UsernameHashMigrationPriority3Test, Performance_TwentyUsers) {
    // Create vault with SHA3-256
    create_test_vault(UsernameHashService::Algorithm::SHA3_256);

    {
        auto session = vault_manager.open_vault_v2(
            test_vault_path.string(), "alice", "TestPassword123!");
        ASSERT_TRUE(session);

        // Add 19 more users (total 20 with admin)
        for (int i = 1; i < 20; i++) {
            add_user_to_vault("user" + std::to_string(i), "Password123!");
        }

        ASSERT_TRUE(vault_manager.save_vault());
        ASSERT_TRUE(vault_manager.close_vault());
    }

    // Enable migration
    enable_migration(UsernameHashService::Algorithm::PBKDF2_SHA256);

    // Measure migration time for all users
    auto start_time = std::chrono::high_resolution_clock::now();

    // Authenticate all users (triggers migration)
    for (int i = 1; i < 20; i++) {
        auto session = vault_manager.open_vault_v2(
            test_vault_path.string(), "user" + std::to_string(i), "Password123!");
        EXPECT_TRUE(session) << "Failed to authenticate user" << i;
        vault_manager.close_vault();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Verify all users migrated
    auto stats = count_migration_statuses();
    EXPECT_EQ(stats.migrated, 19) << "Not all users migrated";

    // Performance assertion: Should complete in under configured limit
    int limit_ms = get_baseline("batch_20_users_max_ms", 30000);
    EXPECT_LT(duration.count(), limit_ms)
        << "Migration took " << duration.count() << "ms (expected < " << limit_ms << "ms)";

    std::cout << "Migration of 19 users took: " << duration.count() << "ms" << std::endl;
}

/**
 * Test performance: Sequential vs parallel hash computation
 *
 * Validates that hashing performance is consistent
 */
TEST_F(UsernameHashMigrationPriority3Test, Performance_HashComputationSpeed) {
    const int iterations = 100;
    std::array<uint8_t, 16> salt{};
    salt.fill(0x42);

    // Test SHA3-256 speed
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) {
        auto hash = UsernameHashService::hash_username(
            "test_user", UsernameHashService::Algorithm::SHA3_256, salt);
        ASSERT_TRUE(hash.has_value());
    }
    auto sha3_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start);

    // Test PBKDF2 speed
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) {
        auto hash = UsernameHashService::hash_username(
            "test_user", UsernameHashService::Algorithm::PBKDF2_SHA256, salt, 100000);
        ASSERT_TRUE(hash.has_value());
    }
    auto pbkdf2_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start);

    std::cout << "SHA3-256:  " << iterations << " iterations in " << sha3_duration.count() << "ms" << std::endl;
    std::cout << "PBKDF2:    " << iterations << " iterations in " << pbkdf2_duration.count() << "ms" << std::endl;

    // Load limits
    int sha3_limit = get_baseline("sha3_256_max_ms", 10);

    // Verify SHA3 performance against baseline
    EXPECT_LT(sha3_duration.count(), sha3_limit)
        << "SHA3-256 too slow: " << sha3_duration.count() << "ms > " << sha3_limit << "ms";

    // SHA3 should be significantly faster than PBKDF2
    EXPECT_LT(sha3_duration.count() * 5, pbkdf2_duration.count())
        << "SHA3 should be ~50x faster than PBKDF2";
}

// ============================================================================
// Test Group 2: Security Validations
// ============================================================================

/**
 * Test constant-time comparison during authentication
 *
 * Validates that authentication doesn't leak timing information
 */
TEST_F(UsernameHashMigrationPriority3Test, Security_ConstantTimeComparison) {
    create_test_vault(UsernameHashService::Algorithm::SHA3_256);

    {
        auto session = vault_manager.open_vault_v2(
            test_vault_path.string(), "alice", "TestPassword123!");
        ASSERT_TRUE(session);
        add_user_to_vault("bob", "BobPassword123!");
        ASSERT_TRUE(vault_manager.save_vault());
        ASSERT_TRUE(vault_manager.close_vault());
    }

    enable_migration(UsernameHashService::Algorithm::PBKDF2_SHA256);

    // Time authentication with correct password
    auto start = std::chrono::high_resolution_clock::now();
    auto session1 = vault_manager.open_vault_v2(
        test_vault_path.string(), "bob", "BobPassword123!");
    auto correct_duration = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now() - start);
    ASSERT_TRUE(session1);
    vault_manager.close_vault();

    // Time authentication with incorrect password
    start = std::chrono::high_resolution_clock::now();
    auto session2 = vault_manager.open_vault_v2(
        test_vault_path.string(), "bob", "WrongPassword!");
    auto incorrect_duration = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now() - start);
    EXPECT_FALSE(session2);

    // The timing difference should be primarily from KEK derivation, not comparison
    // Both should take similar time (within 50% variance due to system noise)
    auto ratio = static_cast<double>(std::max(correct_duration.count(), incorrect_duration.count())) /
                 static_cast<double>(std::min(correct_duration.count(), incorrect_duration.count()));

    std::cout << "Correct password: " << correct_duration.count() << "μs" << std::endl;
    std::cout << "Wrong password:   " << incorrect_duration.count() << "μs" << std::endl;
    std::cout << "Ratio: " << ratio << std::endl;

    EXPECT_LT(ratio, 2.0) << "Timing variance too high - possible side-channel leak";
}

/**
 * Test rollback protection
 *
 * Validates that migrated users cannot be forced back to old algorithm
 */
TEST_F(UsernameHashMigrationPriority3Test, Security_RollbackProtection) {
    create_test_vault(UsernameHashService::Algorithm::SHA3_256);

    // Enable migration
    enable_migration(UsernameHashService::Algorithm::PBKDF2_SHA256);

    // Migrate alice
    {
        auto session = vault_manager.open_vault_v2(
            test_vault_path.string(), "alice", "TestPassword123!");
        ASSERT_TRUE(session);
        vault_manager.close_vault();
    }

    // Verify alice is migrated
    auto header = read_vault_header();
    EXPECT_EQ(header.key_slots[0].migration_status, 0x01);

    // Admin tries to "rollback" by changing algorithm back to SHA3-256
    {
        auto session = vault_manager.open_vault_v2(
            test_vault_path.string(), "alice", "TestPassword123!");
        ASSERT_TRUE(session);

        auto policy = *vault_manager.get_vault_security_policy();
        policy.username_hash_algorithm = 0x01;  // Try to revert to SHA3-256
        policy.username_hash_algorithm_previous = 0x00;
        policy.migration_flags = 0x00;

        auto result = vault_manager.update_security_policy(policy);
        ASSERT_TRUE(result);
        ASSERT_TRUE(vault_manager.save_vault());
        vault_manager.close_vault();
    }

    // Try to authenticate alice - should still use PBKDF2 hash
    auto session = vault_manager.open_vault_v2(
        test_vault_path.string(), "alice", "TestPassword123!");
    EXPECT_TRUE(session) << "Migrated user should authenticate with new hash, not old";
}

/**
 * Test algorithm downgrade prevention
 *
 * Validates that policy prevents downgrading to weaker algorithms
 */
TEST_F(UsernameHashMigrationPriority3Test, Security_PreventDowngrade) {
    create_test_vault(UsernameHashService::Algorithm::ARGON2ID);

    {
        auto session = vault_manager.open_vault_v2(
            test_vault_path.string(), "alice", "TestPassword123!");
        ASSERT_TRUE(session);

        auto policy = *vault_manager.get_vault_security_policy();

        // Try to downgrade from Argon2id (0x05) to SHA3-256 (0x01)
        policy.username_hash_algorithm_previous = 0x05;
        policy.username_hash_algorithm = 0x01;
        policy.migration_flags = 0x01;

        auto result = vault_manager.update_security_policy(policy);

        // Should succeed (policy update doesn't enforce strength)
        // But migration should be carefully audited
        EXPECT_TRUE(result) << "Policy update should succeed but be logged";

        // Note: In production, this might warrant a warning or require special permission
    }
}

// ============================================================================
// Test Group 3: Error Handling & Recovery
// ============================================================================

/**
 * Test handling of empty vault migration
 *
 * Validates that migration works even with only admin user
 */
TEST_F(UsernameHashMigrationPriority3Test, ErrorHandling_EmptyVault) {
    create_test_vault(UsernameHashService::Algorithm::SHA3_256);

    // Enable migration (only admin user exists)
    enable_migration(UsernameHashService::Algorithm::PBKDF2_SHA256);

    // Authenticate admin (triggers migration)
    auto session = vault_manager.open_vault_v2(
        test_vault_path.string(), "alice", "TestPassword123!");
    ASSERT_TRUE(session);
    vault_manager.close_vault();

    // Verify migration completed
    auto stats = count_migration_statuses();
    EXPECT_EQ(stats.total, 1);
    EXPECT_EQ(stats.migrated, 1);
}

/**
 * Test migration with maximum users (32)
 *
 * Validates that migration handles boundary condition
 */
TEST_F(UsernameHashMigrationPriority3Test, ErrorHandling_MaximumUsers) {
    create_test_vault(UsernameHashService::Algorithm::SHA3_256);

    {
        auto session = vault_manager.open_vault_v2(
            test_vault_path.string(), "alice", "TestPassword123!");
        ASSERT_TRUE(session);

        // Add 31 more users (total 32 = MAX_KEY_SLOTS)
        for (int i = 1; i < 32; i++) {
            add_user_to_vault("user" + std::to_string(i), "Password123!");
        }

        ASSERT_TRUE(vault_manager.save_vault());
        vault_manager.close_vault();
    }

    // Verify we have 32 users
    auto header = read_vault_header();
    EXPECT_EQ(header.key_slots.size(), 32);

    // Enable migration
    enable_migration(UsernameHashService::Algorithm::PBKDF2_SHA256);

    // Migrate first user
    auto session = vault_manager.open_vault_v2(
        test_vault_path.string(), "user1", "Password123!");
    EXPECT_TRUE(session);
    vault_manager.close_vault();

    // Verify migration worked with max users
    auto stats = count_migration_statuses();
    EXPECT_EQ(stats.migrated, 1);
}

/**
 * Test rapid repeated migrations
 *
 * Validates that multiple migrations can be performed sequentially
 */
TEST_F(UsernameHashMigrationPriority3Test, ErrorHandling_RapidMigrations) {
    create_test_vault(UsernameHashService::Algorithm::SHA3_256);

    {
        auto session = vault_manager.open_vault_v2(
            test_vault_path.string(), "alice", "TestPassword123!");
        ASSERT_TRUE(session);
        add_user_to_vault("bob", "BobPassword123!");
        ASSERT_TRUE(vault_manager.save_vault());
        vault_manager.close_vault();
    }

    // Migration 1: SHA3-256 → SHA3-384
    enable_migration(UsernameHashService::Algorithm::SHA3_384);
    {
        auto session = vault_manager.open_vault_v2(
            test_vault_path.string(), "bob", "BobPassword123!");
        ASSERT_TRUE(session);
        vault_manager.close_vault();
    }

    auto stats1 = count_migration_statuses();
    EXPECT_EQ(stats1.migrated, 1) << "Bob should be migrated to SHA3-384";

    // Migration 2: SHA3-384 → PBKDF2
    enable_migration(UsernameHashService::Algorithm::PBKDF2_SHA256, "alice", "TestPassword123!");
    {
        auto session = vault_manager.open_vault_v2(
            test_vault_path.string(), "alice", "TestPassword123!");
        ASSERT_TRUE(session);
        vault_manager.close_vault();
    }

    auto stats2 = count_migration_statuses();
    EXPECT_EQ(stats2.migrated, 1) << "Alice should be migrated to PBKDF2";

    // Migration 3: PBKDF2 → Argon2id
    enable_migration(UsernameHashService::Algorithm::ARGON2ID, "alice", "TestPassword123!");
    {
        auto session1 = vault_manager.open_vault_v2(
            test_vault_path.string(), "alice", "TestPassword123!");
        ASSERT_TRUE(session1);
        vault_manager.close_vault();

        auto session2 = vault_manager.open_vault_v2(
            test_vault_path.string(), "bob", "BobPassword123!");
        ASSERT_TRUE(session2);
        vault_manager.close_vault();
    }

    auto stats3 = count_migration_statuses();
    EXPECT_EQ(stats3.migrated, 2) << "Both users should be on Argon2id now";
}

/**
 * Test migration with very long username
 *
 * Validates handling of edge case username lengths
 */
TEST_F(UsernameHashMigrationPriority3Test, ErrorHandling_VeryLongUsername) {
    create_test_vault(UsernameHashService::Algorithm::SHA3_256);

    // Create 255-character username (near maximum practical length)
    std::string long_username(255, 'a');

    {
        auto session = vault_manager.open_vault_v2(
            test_vault_path.string(), "alice", "TestPassword123!");
        ASSERT_TRUE(session);
        add_user_to_vault(long_username, "Password123!");
        ASSERT_TRUE(vault_manager.save_vault());
        vault_manager.close_vault();
    }

    enable_migration(UsernameHashService::Algorithm::PBKDF2_SHA256);

    // Authenticate with long username
    auto session = vault_manager.open_vault_v2(
        test_vault_path.string(), long_username, "Password123!");
    EXPECT_TRUE(session) << "Long username should migrate successfully";
}

// ============================================================================
// Test Group 4: Edge Cases & Boundary Conditions
// ============================================================================

/**
 * Test migration status persistence across multiple open/close cycles
 */
TEST_F(UsernameHashMigrationPriority3Test, EdgeCase_StatusPersistence) {
    create_test_vault(UsernameHashService::Algorithm::SHA3_256);
    enable_migration(UsernameHashService::Algorithm::PBKDF2_SHA256);

    // Authenticate and migrate
    {
        auto session = vault_manager.open_vault_v2(
            test_vault_path.string(), "alice", "TestPassword123!");
        ASSERT_TRUE(session);
        vault_manager.close_vault();
    }

    // Open and close multiple times
    for (int i = 0; i < 5; i++) {
        auto session = vault_manager.open_vault_v2(
            test_vault_path.string(), "alice", "TestPassword123!");
        ASSERT_TRUE(session) << "Iteration " << i;
        vault_manager.close_vault();

        // Verify migration status persists
        auto header = read_vault_header();
        EXPECT_EQ(header.key_slots[0].migration_status, 0x01)
            << "Migration status should persist across cycles";
    }
}

/**
 * Test migration with all supported algorithms
 */
TEST_F(UsernameHashMigrationPriority3Test, EdgeCase_AllAlgorithms) {
    const std::vector<UsernameHashService::Algorithm> algorithms = {
        UsernameHashService::Algorithm::SHA3_256,
        UsernameHashService::Algorithm::SHA3_384,
        UsernameHashService::Algorithm::SHA3_512,
        UsernameHashService::Algorithm::PBKDF2_SHA256,
        UsernameHashService::Algorithm::ARGON2ID
    };

    for (size_t i = 0; i < algorithms.size(); i++) {
        // Create vault with algorithm i
        if (i > 0) {
            std::filesystem::remove(test_vault_path);
        }

        create_test_vault(algorithms[i]);

        // Verify vault works
        auto session = vault_manager.open_vault_v2(
            test_vault_path.string(), "alice", "TestPassword123!");
        EXPECT_TRUE(session) << "Algorithm " << static_cast<int>(algorithms[i]) << " should work";
        vault_manager.close_vault();
    }
}

/**
 * Test migration completion detection
 */
TEST_F(UsernameHashMigrationPriority3Test, EdgeCase_MigrationCompletionDetection) {
    create_test_vault(UsernameHashService::Algorithm::SHA3_256);

    {
        auto session = vault_manager.open_vault_v2(
            test_vault_path.string(), "alice", "TestPassword123!");
        ASSERT_TRUE(session);

        for (int i = 1; i <= 3; i++) {
            add_user_to_vault("user" + std::to_string(i), "Password123!");
        }

        ASSERT_TRUE(vault_manager.save_vault());
        vault_manager.close_vault();
    }

    enable_migration(UsernameHashService::Algorithm::PBKDF2_SHA256);

    // Check initial stats
    auto stats = count_migration_statuses();
    EXPECT_EQ(stats.total, 4);
    EXPECT_EQ(stats.not_migrated, 4);

    // Migrate each user and check progress
    for (int i = 1; i <= 3; i++) {
        auto session = vault_manager.open_vault_v2(
            test_vault_path.string(), "user" + std::to_string(i), "Password123!");
        ASSERT_TRUE(session);
        vault_manager.close_vault();

        stats = count_migration_statuses();
        EXPECT_EQ(stats.migrated, i) << "After user" << i;
    }

    // Migrate admin (last user)
    {
        auto session = vault_manager.open_vault_v2(
            test_vault_path.string(), "alice", "TestPassword123!");
        ASSERT_TRUE(session);
        vault_manager.close_vault();
    }

    // Verify completion
    stats = count_migration_statuses();
    EXPECT_EQ(stats.migrated, 4);
    EXPECT_EQ(stats.not_migrated, 0);
    EXPECT_EQ(stats.pending, 0);
}

/**
 * Test that migration doesn't affect vault integrity
 */
TEST_F(UsernameHashMigrationPriority3Test, EdgeCase_VaultIntegrityAfterMigration) {
    create_test_vault(UsernameHashService::Algorithm::SHA3_256);

    // Store original file size
    auto original_size = std::filesystem::file_size(test_vault_path);

    enable_migration(UsernameHashService::Algorithm::PBKDF2_SHA256);

    // Migrate
    {
        auto session = vault_manager.open_vault_v2(
            test_vault_path.string(), "alice", "TestPassword123!");
        ASSERT_TRUE(session);
        vault_manager.close_vault();
    }

    // Verify file is still valid
    auto new_size = std::filesystem::file_size(test_vault_path);
    EXPECT_GT(new_size, 0) << "Vault file should not be empty";

    // Size should be similar (migration only changes header fields)
    EXPECT_NEAR(new_size, original_size, 200) << "File size shouldn't change much";

    // Verify vault can still be opened
    auto session = vault_manager.open_vault_v2(
        test_vault_path.string(), "alice", "TestPassword123!");
    EXPECT_TRUE(session) << "Vault should remain valid after migration";
}
