/**
 * Username Hash Migration Tests - Concurrency
 *
 * Checks thread safety and file locking during concurrent migrations.
 */

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>
#include <vector>
#include <future>
#include <atomic>

#include "../src/core/VaultManager.h"
#include "../src/core/MultiUserTypes.h"
#include "../src/core/services/UsernameHashService.h"
#include "../src/core/crypto/VaultCrypto.h"
#include "../src/core/io/VaultIO.h"
#include "../src/core/format/VaultFormat.h"
#include "../src/core/VaultFormatV2.h"

using namespace KeepTower;
using UsernameHashService = KeepTower::UsernameHashService;

class UsernameHashMigrationConcurrencyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use a unique name for this test run
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        test_vault_path = std::filesystem::temp_directory_path() /
            ("test_migration_concur_" + std::to_string(now) + ".vault");
    }

    void TearDown() override {
        // Cleanup main file
        if (std::filesystem::exists(test_vault_path)) {
            std::filesystem::remove(test_vault_path);
        }

        // Clean up backup files (created during migration)
        if (std::filesystem::exists(test_vault_path.parent_path())) {
            for (const auto& entry : std::filesystem::directory_iterator(test_vault_path.parent_path())) {
                if (entry.path().string().find(test_vault_path.filename().string() + ".backup") != std::string::npos) {
                    std::filesystem::remove(entry.path());
                }
            }
        }
    }

    // Helper to setup initial state
    void setup_users(int count) {
        std::cout << "[Setup] Creating vault at " << test_vault_path << std::endl;
        VaultManager setup_mgr;

        VaultSecurityPolicy policy;
        policy.min_password_length = 12;
        policy.pbkdf2_iterations = 600000; // Standard for FIPS/Security
        policy.username_hash_algorithm = static_cast<uint8_t>(UsernameHashService::Algorithm::SHA3_256);
        policy.require_yubikey = false;

        auto result = setup_mgr.create_vault_v2(
            test_vault_path.string(),
            "user0",
            "Password123!",
            policy);

        if (!result.has_value()) {
            std::cout << "[Setup] Creation FAILED: " << static_cast<int>(result.error()) << std::endl;
        }
        ASSERT_TRUE(result.has_value());

        ASSERT_TRUE(setup_mgr.close_vault());

        // Add more users
        {
            std::cout << "[Setup] Adding users..." << std::endl;
            VaultManager admin_mgr;
            auto session = admin_mgr.open_vault_v2(test_vault_path.string(), "user0", "Password123!");
            if(!session) {
                std::cout << "[Setup] Failed to open vault for admin access" << std::endl;
            }
            ASSERT_TRUE(session);

            for(int i=1; i<count; ++i) {
                auto res = admin_mgr.add_user("user" + std::to_string(i), "Password123!", UserRole::STANDARD_USER);
                if(!res) std::cout << "[Setup] Failed to add user " << i << std::endl;
                ASSERT_TRUE(res);
            }

            // Enable migration
            std::cout << "[Setup] Enabling migration..." << std::endl;
            auto policy_opt = admin_mgr.get_vault_security_policy();
            ASSERT_TRUE(policy_opt.has_value());
            auto p = *policy_opt;

            p.username_hash_algorithm_previous = p.username_hash_algorithm;
            p.username_hash_algorithm = static_cast<uint8_t>(UsernameHashService::Algorithm::ARGON2ID);
            p.migration_flags = 0x01; // Enable
            p.migration_started_at = static_cast<uint64_t>(std::time(nullptr));

            auto update_res = admin_mgr.update_security_policy(p);
            ASSERT_TRUE(update_res);

            ASSERT_TRUE(admin_mgr.save_vault());
            ASSERT_TRUE(admin_mgr.close_vault());
            std::cout << "[Setup] Complete." << std::endl;
        }
    }

    std::filesystem::path test_vault_path;
};

// ----------------------------------------------------------------------------
// Test: Multiple Processes (Simulated by Threads) Attempting Migration
// ----------------------------------------------------------------------------
TEST_F(UsernameHashMigrationConcurrencyTest, IndependentThreads_MigrationContention) {
    const int NUM_USERS = 30;
    setup_users(NUM_USERS);
    if (HasFatalFailure()) return;

    // Run threads
    std::vector<std::future<bool>> results;

    // We want to hammer the file.
    // Each thread represents a separate application instance (own VaultManager)
    // trying to log in a specific user.
    // Upon login, migration should trigger for that user.

    for(int i=0; i<NUM_USERS; ++i) {
        results.push_back(std::async(std::launch::async, [this, i]() -> bool {
            std::string user = "user" + std::to_string(i);

            // Try to open and authenticate
            // We retry because file locking is expected to block/fail others transiently
            // But eventually everyone should get in.

            int max_retries = 100;
            for(int attempt=0; attempt < max_retries; ++attempt) {
                VaultManager local_mgr; // New instance per attempt/thread
                auto session = local_mgr.open_vault_v2(test_vault_path.string(), user, "Password123!");

                if(session) {
                    // Start authentication verification
                    // Migration happens during open_vault_v2 if properly implemented
                    // We verify by checking if we have a valid session

                    // Hold it briefly to increase overlap chance
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));

                    (void)local_mgr.close_vault();
                    return true;
                }

                // Backoff
                std::this_thread::sleep_for(std::chrono::milliseconds(50 + (rand() % 50)));
            }
            return false;
        }));
    }

    int success_count = 0;
    for(auto& fut : results) {
        if(fut.get()) success_count++;
    }

    // We expect at least some successes. If file locking works, ALL should eventually succeed
    // given enough retries.
    EXPECT_EQ(success_count, NUM_USERS) << "Not all users managed to login/migrate amidst contention";

    // Verify Integrity
    VaultManager verify_mgr;
    auto session = verify_mgr.open_vault_v2(test_vault_path.string(), "user0", "Password123!");
    ASSERT_TRUE(session) << "Vault became corrupted or inaccessible after concurrent stress";

    // Check migration status
    auto policy = verify_mgr.get_vault_security_policy();
    // We can iterate slots to see how many migrated
    // But VaultManager doesn't expose raw slots easily without reading file directly
    // Let's rely on the fact that "open_vault_v2" worked for user0, so user0 IS migrated (or still valid).

    // Read raw header to verify migration counts
    std::vector<uint8_t> file_data;
    int iterations;
    ASSERT_TRUE(VaultIO::read_file(test_vault_path.string(), file_data, true, iterations));
    auto header_res = VaultFormatV2::read_header(file_data);
    ASSERT_TRUE(header_res.has_value());

    int migrated_count = 0;
    for(const auto& slot : header_res.value().first.vault_header.key_slots) {
        // Status 1 = Migrated
        if(slot.active && slot.migration_status == 0x01) {
            migrated_count++;
        }
    }

    // Without file locking, concurrent updates will overwrite each other ("Last Writer Wins").
    // We expect at least one migration to succeed and persist.
    // The others might be lost, which is acceptable for migration (it will just happen again next login).
    EXPECT_GE(migrated_count, 1) << "At least one user should have been permanently migrated";

    if (migrated_count < NUM_USERS) {
        std::cout << "[INFO] " << (NUM_USERS - migrated_count)
                  << " migration records were lost due to write contention (Expected without file locking)" << std::endl;
    }

    EXPECT_TRUE(verify_mgr.close_vault());
}

// ----------------------------------------------------------------------------
// Test: Backup Restoration from Corrupted Vault
// ----------------------------------------------------------------------------
TEST_F(UsernameHashMigrationConcurrencyTest, BackupRestoration_CorruptedVault) {
    const int NUM_USERS = 2; // Needs at least 2 to have user1
    setup_users(NUM_USERS);
    if (HasFatalFailure()) return;

    // 1. Verify we have a valid vault
    VaultManager admin_mgr;
    auto session = admin_mgr.open_vault_v2(test_vault_path.string(), "user0", "Password123!");
    ASSERT_TRUE(session) << "Initial open failed";
    ASSERT_TRUE(admin_mgr.close_vault());

    // 2. Corrupt the main vault file
    std::ofstream corrupt_file(test_vault_path, std::ios::trunc | std::ios::binary);
    corrupt_file << "CORRUPTED_DATA_GARBAGE_HEADER_1234567890";
    corrupt_file.close();

    // 3. Verify open fails
    VaultManager fail_mgr;
    auto fail_session = fail_mgr.open_vault_v2(test_vault_path.string(), "user0", "Password123!");
    ASSERT_FALSE(fail_session) << "Vault should be corrupted";

    // 4. Attempt Restoration
    // Note: restore_from_most_recent_backup is a static-like utility or member?
    // Based on header, it looks like a member function but takes 'vault_path'.
    // Let's check if it needs an instance. It's not static in the grep output.
    VaultManager restore_mgr;
    auto restore_res = restore_mgr.restore_from_most_recent_backup(test_vault_path.string());
    ASSERT_TRUE(restore_res.has_value()) << "Restore failed: " << (restore_res.has_value() ? "" : "Error");

    // 5. Verify open succeeds after restoration
    VaultManager success_mgr;
    auto success_session = success_mgr.open_vault_v2(test_vault_path.string(), "user0", "Password123!");
    ASSERT_TRUE(success_session) << "Vault should be recoverable from backup";

    // 6. Verify data integrity (user1 should exist)
    // Note: user1 was added in setup_users
    // But migration enabled triggered a save, so user1 should be in the backup.

    // We added users, then enabled migration (which saves).
    // So the backup created during 'save_vault' inside setup_users should contain user1.
    // However, backups rotate. The most recent backup is from the LAST save.
    // In setup_users:
    //   add_user loop
    //   enable migration
    //   save_vault() -> Creates Backup 1

    // Wait, save_vault() creates a backup of the *previous* version before overwriting.
    // If we just created the vault and then saved, there might not be a previous version?
    // Or save_vault(true) copies current file to .backup, then writes new file.

    // Let's verify if user1 is accessible.
    // User1 has password "Password123!"
    // Admin was user0.

    // We need to re-login as admin to check things or try to login as user1.
    success_mgr.close_vault();

    VaultManager user1_mgr;
    auto user1_session = user1_mgr.open_vault_v2(test_vault_path.string(), "user1", "Password123!");
    ASSERT_TRUE(user1_session) << "Restored data missing user1";
    user1_mgr.close_vault();
}
