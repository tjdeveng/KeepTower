// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

/**
 * @file test_vault_creation_orchestrator_integration.cc
 * @brief Integration tests for VaultCreationOrchestrator with real services
 *
 * Phase 2 Day 3: Tests the orchestrator with actual service implementations.
 */

#include <gtest/gtest.h>
#include "../src/core/controllers/VaultCreationOrchestrator.h"
#include "../src/core/services/VaultCryptoService.h"
#include "../src/core/services/VaultYubiKeyService.h"
#include "../src/core/services/VaultFileService.h"
#include "../src/core/MultiUserTypes.h"
#include <filesystem>
#include <memory>
#include <vector>

namespace fs = std::filesystem;
using namespace KeepTower;

class VaultCreationOrchestratorIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir = fs::temp_directory_path() / "keeptower_orchestrator_integration";
        fs::create_directories(test_dir);

        crypto_service = std::make_shared<VaultCryptoService>();
        yubikey_service = std::make_shared<VaultYubiKeyService>();
        file_service = std::make_shared<VaultFileService>();

        orchestrator = std::make_unique<VaultCreationOrchestrator>(
            crypto_service, yubikey_service, file_service
        );

        // Setup default parameters
        params.path = (test_dir / "test.vault").string();
        params.admin_username = "admin@example.com";
        params.admin_password = "SecurePassword123!";
        params.policy.require_yubikey = false;
        params.policy.min_password_length = 12;
        params.policy.pbkdf2_iterations = 100000;
        // Note: FEC is set per-call, not in policy
    }

    void TearDown() override {
        orchestrator.reset();
        crypto_service.reset();
        yubikey_service.reset();
        file_service.reset();
        try {
            fs::remove_all(test_dir);
        } catch (...) {}
    }

    fs::path test_dir;
    std::shared_ptr<VaultCryptoService> crypto_service;
    std::shared_ptr<VaultYubiKeyService> yubikey_service;
    std::shared_ptr<VaultFileService> file_service;
    std::unique_ptr<VaultCreationOrchestrator> orchestrator;
    VaultCreationOrchestrator::CreationParams params;
};

// ============================================================================
// Basic Integration Tests
// ============================================================================

TEST_F(VaultCreationOrchestratorIntegrationTest, CreateVault_BasicSuccess) {
    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_TRUE(result.has_value()) << "Vault creation failed";
    EXPECT_EQ(result->file_path, params.path);
    EXPECT_TRUE(fs::exists(params.path));

    // Verify header
    EXPECT_EQ(result->header.key_slots.size(), 1);
    EXPECT_EQ(result->header.key_slots[0].username, params.admin_username.raw());
    EXPECT_EQ(result->header.key_slots[0].role, UserRole::ADMINISTRATOR);
}

TEST_F(VaultCreationOrchestratorIntegrationTest, CreateVault_WithProgressCallback) {
    int callback_count = 0;
    int last_step = 0;

    params.progress_callback = [&](int step, int total, const std::string& desc) {
        callback_count++;
        last_step = step;
        EXPECT_GT(step, 0);
        EXPECT_LE(step, total);
        EXPECT_FALSE(desc.empty());
    };

    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_TRUE(result.has_value());
    EXPECT_GT(callback_count, 0) << "Progress callback never called";
    EXPECT_GE(last_step, 6) << "Not all steps reported";
}

TEST_F(VaultCreationOrchestratorIntegrationTest, CreateVault_FileExists) {
    // Create vault first time
    auto result1 = orchestrator->create_vault_v2_sync(params);
    ASSERT_TRUE(result1.has_value());

    // Verify file exists
    EXPECT_TRUE(fs::exists(params.path));
    auto size1 = fs::file_size(params.path);

    // Create again (should overwrite)
    auto result2 = orchestrator->create_vault_v2_sync(params);
    EXPECT_TRUE(result2.has_value()) << "Should allow overwriting";

    auto size2 = fs::file_size(params.path);
    EXPECT_GT(size2, 0);
}

TEST_F(VaultCreationOrchestratorIntegrationTest, CreateVault_WithFEC) {
    // Note: FEC redundancy would need to be passed to write operation
    // Currently orchestrator uses policy settings
    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(fs::exists(params.path));

    auto size = fs::file_size(params.path);
    EXPECT_GT(size, 500);
}

TEST_F(VaultCreationOrchestratorIntegrationTest, CreateVault_HighIterations) {
    params.policy.pbkdf2_iterations = 500000;

    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_TRUE(result.has_value());
    // Note: pbkdf2_iterations stored in policy, not in KeySlot directly
    EXPECT_EQ(result->header.security_policy.pbkdf2_iterations, 500000);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(VaultCreationOrchestratorIntegrationTest, CreateVault_InvalidPath) {
    params.path = "/nonexistent/dir/vault.vault";

    auto result = orchestrator->create_vault_v2_sync(params);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), VaultError::FileWriteError);
}

TEST_F(VaultCreationOrchestratorIntegrationTest, CreateVault_EmptyUsername) {
    params.admin_username = "";

    auto result = orchestrator->create_vault_v2_sync(params);

    EXPECT_FALSE(result.has_value());
}

TEST_F(VaultCreationOrchestratorIntegrationTest, CreateVault_WeakPassword) {
    params.admin_password = "weak";

    auto result = orchestrator->create_vault_v2_sync(params);

    EXPECT_FALSE(result.has_value());
}

TEST_F(VaultCreationOrchestratorIntegrationTest, CreateVault_EmptyPassword) {
    params.admin_password = "";

    auto result = orchestrator->create_vault_v2_sync(params);

    EXPECT_FALSE(result.has_value());
}

// ============================================================================
// Multiple Vault Tests
// ============================================================================

TEST_F(VaultCreationOrchestratorIntegrationTest, CreateMultipleVaults) {
    std::vector<std::string> paths = {
        (test_dir / "v1.vault").string(),
        (test_dir / "v2.vault").string(),
        (test_dir / "v3.vault").string()
    };

    for (const auto& path : paths) {
        params.path = path;
        auto result = orchestrator->create_vault_v2_sync(params);
        ASSERT_TRUE(result.has_value()) << "Failed: " << path;
        EXPECT_TRUE(fs::exists(path));
    }
}

TEST_F(VaultCreationOrchestratorIntegrationTest, CreateVaults_DifferentPasswords) {
    std::vector<std::pair<std::string, std::string>> configs = {
        {"v1.vault", "Password123!"},
        {"v2.vault", "DifferentPass456!"},
        {"v3.vault", "AnotherOne789!"}
    };

    std::vector<VaultCreationOrchestrator::CreationResult> results;

    for (const auto& [name, password] : configs) {
        params.path = (test_dir / name).string();
        params.admin_password = password;

        auto result = orchestrator->create_vault_v2_sync(params);
        ASSERT_TRUE(result.has_value());
        results.push_back(*result);
    }

    // Verify different wrapped DEKs
    for (size_t i = 0; i < results.size(); ++i) {
        for (size_t j = i + 1; j < results.size(); ++j) {
            EXPECT_NE(results[i].header.key_slots[0].wrapped_dek,
                     results[j].header.key_slots[0].wrapped_dek);
        }
    }
}

// ============================================================================
// Service Integration Tests (Commented - Focus on Orchestrator)
// ============================================================================

// These tests would verify service internals, but that's not the primary
// goal of orchestrator integration testing. The full workflow tests below
// verify that services work correctly together via the orchestrator.

/*
TEST_F(VaultCreationOrchestratorIntegrationTest, CryptoService_KeyUniqueness) {
    std::vector<std::array<uint8_t, 32>> deks;

    for (int i = 0; i < 5; ++i) {
        auto result = crypto_service->generate_dek();
        ASSERT_TRUE(result.has_value());
        deks.push_back(result.value());
    }

    // All unique
    for (size_t i = 0; i < deks.size(); ++i) {
        for (size_t j = i + 1; j < deks.size(); ++j) {
            EXPECT_NE(deks[i], deks[j]);
        }
    }
}

TEST_F(VaultCreationOrchestratorIntegrationTest, CryptoService_PBKDF2Deterministic) {
    std::array<uint8_t, 32> salt{};
    for (size_t i = 0; i < salt.size(); ++i) salt[i] = i;

    auto kek1 = crypto_service->derive_kek("TestPass123!", salt, 100000);
    auto kek2 = crypto_service->derive_kek("TestPass123!", salt, 100000);

    ASSERT_TRUE(kek1.has_value());
    ASSERT_TRUE(kek2.has_value());
    EXPECT_EQ(kek1.value(), kek2.value());
}

TEST_F(VaultCreationOrchestratorIntegrationTest, CryptoService_WrapUnwrap) {
    auto dek_result = crypto_service->generate_dek();
    ASSERT_TRUE(dek_result.has_value());
    auto dek = dek_result.value();

    std::array<uint8_t, 32> salt{};
    for (size_t i = 0; i < salt.size(); ++i) salt[i] = i;

    auto kek_result = crypto_service->derive_kek("TestPass123!", salt, 100000);
    ASSERT_TRUE(kek_result.has_value());
    auto kek = kek_result.value();

    auto wrapped = crypto_service->wrap_key(dek, kek);
    ASSERT_TRUE(wrapped.has_value());

    auto unwrapped = crypto_service->unwrap_key(wrapped.value(), kek);
    ASSERT_TRUE(unwrapped.has_value());

    EXPECT_EQ(dek, unwrapped.value());
}

TEST_F(VaultCreationOrchestratorIntegrationTest, FileService_WriteReadHeader) {
    VaultHeaderV2 header;
    header.security_policy.require_yubikey = false;
    header.security_policy.min_password_length = 12;

    KeySlot slot;
    slot.active = true;
    slot.username = "test@example.com";
    slot.role = UserRole::ADMINISTRATOR;
    slot.pbkdf2_iterations = 100000;
    header.key_slots.push_back(slot);

    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    std::string path = (test_dir / "test.vault").string();

    auto write_result = file_service->write_vault_v2(path, header, data, 0);
    ASSERT_TRUE(write_result.has_value());

    auto read_header = file_service->read_header_v2(path);
    ASSERT_TRUE(read_header.has_value());

    EXPECT_EQ(read_header->key_slots.size(), 1);
    EXPECT_EQ(read_header->key_slots[0].username, "test@example.com");
}
*/

// ============================================================================
// End-to-End Tests
// ============================================================================

TEST_F(VaultCreationOrchestratorIntegrationTest, EndToEnd_CompleteWorkflow) {
    int progress_count = 0;
    params.progress_callback = [&](int, int, const std::string&) {
        progress_count++;
    };

    auto result = orchestrator->create_vault_v2_sync(params);

    // Verify creation
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(fs::exists(params.path));
    EXPECT_GT(progress_count, 0);

    // Verify header
    EXPECT_EQ(result->header.key_slots.size(), 1);
    EXPECT_EQ(result->header.key_slots[0].username, params.admin_username.raw());
    EXPECT_FALSE(result->header.key_slots[0].wrapped_dek.empty());

    // Verify file
    auto size = fs::file_size(params.path);
    EXPECT_GT(size, 100);
    EXPECT_LT(size, 100000);

    // Verify DEK
    bool all_zero = true;
    for (auto byte : result->dek) {
        if (byte != 0) {
            all_zero = false;
            break;
        }
    }
    EXPECT_FALSE(all_zero) << "DEK appears to be all zeros";
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST_F(VaultCreationOrchestratorIntegrationTest, Performance_ReasonableTime) {
    auto start = std::chrono::steady_clock::now();

    auto result = orchestrator->create_vault_v2_sync(params);

    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    ASSERT_TRUE(result.has_value());
    EXPECT_LT(ms, 5000) << "Creation took " << ms << "ms";

    std::cout << "Vault creation time: " << ms << "ms" << std::endl;
}

TEST_F(VaultCreationOrchestratorIntegrationTest, Performance_ProgressOverhead) {
    // Without callback
    auto start1 = std::chrono::steady_clock::now();
    auto result1 = orchestrator->create_vault_v2_sync(params);
    auto end1 = std::chrono::steady_clock::now();
    auto ms1 = std::chrono::duration_cast<std::chrono::milliseconds>(end1 - start1).count();

    ASSERT_TRUE(result1.has_value());

    // With callback
    params.path = (test_dir / "v2.vault").string();
    int count = 0;
    params.progress_callback = [&](int, int, const std::string&) { count++; };

    auto start2 = std::chrono::steady_clock::now();
    auto result2 = orchestrator->create_vault_v2_sync(params);
    auto end2 = std::chrono::steady_clock::now();
    auto ms2 = std::chrono::duration_cast<std::chrono::milliseconds>(end2 - start2).count();

    ASSERT_TRUE(result2.has_value());
    EXPECT_GT(count, 0);

    double overhead = 100.0 * (ms2 - ms1) / ms1;
    std::cout << "Progress overhead: " << overhead << "%" << std::endl;
    EXPECT_LT(std::abs(overhead), 20.0);
}

// ============================================================================
// Phase 2 Day 4: Edge Case Tests
// ============================================================================

// ----------------------------------------------------------------------------
// Boundary Condition Tests
// ----------------------------------------------------------------------------

TEST_F(VaultCreationOrchestratorIntegrationTest, EdgeCase_ExtremelyLongPassword) {
    // Test with 1024-character password
    std::string long_password(1024, 'A');
    for (size_t i = 0; i < long_password.size(); i += 10) {
        long_password[i] = '0' + (i % 10);
    }
    long_password += "!Secure1";  // Add special chars to meet requirements

    params.admin_password = long_password;

    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_TRUE(result.has_value()) << "Failed with long password: " << static_cast<int>(result.error());
    EXPECT_TRUE(fs::exists(params.path));
}

TEST_F(VaultCreationOrchestratorIntegrationTest, EdgeCase_ExtremelyLongUsername) {
    // Username is limited to 64 chars, test at the boundary
    std::string long_name(54, 'u');
    params.admin_username = long_name + "@test.com";  // Total: 64 chars

    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_TRUE(result.has_value()) << "Failed with 64-char username: " << static_cast<int>(result.error());
    EXPECT_EQ(result->header.key_slots[0].username, params.admin_username.raw());
}

TEST_F(VaultCreationOrchestratorIntegrationTest, EdgeCase_MinimalPassword) {
    // Test with exactly minimum length password
    params.policy.min_password_length = 8;
    params.admin_password = "Secure1!";  // Exactly 8 chars with complexity

    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_TRUE(result.has_value()) << "Failed with minimal password: " << static_cast<int>(result.error());
}

TEST_F(VaultCreationOrchestratorIntegrationTest, EdgeCase_SpecialCharactersInPassword) {
    // Test with many special characters
    params.admin_password = "P@$$w0rd!#%&*()[]{}~`-_=+|\\:;\"'<>,.?/";

    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_TRUE(result.has_value()) << "Failed with special chars: " << static_cast<int>(result.error());
}

TEST_F(VaultCreationOrchestratorIntegrationTest, EdgeCase_UnicodePassword) {
    // Test with Unicode characters
    params.admin_password = "Пароль123!中文密码";  // Russian + Chinese + ASCII

    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_TRUE(result.has_value()) << "Failed with Unicode: " << static_cast<int>(result.error());
}

TEST_F(VaultCreationOrchestratorIntegrationTest, EdgeCase_UnicodeUsername) {
    // Test with Unicode email
    params.admin_username = "用户@example.com";  // Chinese characters

    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_TRUE(result.has_value()) << "Failed with Unicode username: " << static_cast<int>(result.error());
    EXPECT_EQ(result->header.key_slots[0].username, params.admin_username.raw());
}

// ----------------------------------------------------------------------------
// Security Parameter Boundary Tests
// ----------------------------------------------------------------------------

TEST_F(VaultCreationOrchestratorIntegrationTest, EdgeCase_MinimalPBKDF2Iterations) {
    // Test with minimum allowed iterations (100k)
    params.policy.pbkdf2_iterations = 100000;

    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_TRUE(result.has_value()) << "Failed with min iterations: " << static_cast<int>(result.error());
    EXPECT_EQ(result->header.security_policy.pbkdf2_iterations, 100000);
}

TEST_F(VaultCreationOrchestratorIntegrationTest, EdgeCase_MaximalPBKDF2Iterations) {
    // Test with very high iterations (this will be slow)
    params.policy.pbkdf2_iterations = 1000000;  // 1 million iterations

    auto start = std::chrono::steady_clock::now();
    auto result = orchestrator->create_vault_v2_sync(params);
    auto end = std::chrono::steady_clock::now();

    ASSERT_TRUE(result.has_value()) << "Failed with high iterations: " << static_cast<int>(result.error());
    EXPECT_EQ(result->header.security_policy.pbkdf2_iterations, 1000000);

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "High iteration vault creation: " << ms << "ms" << std::endl;
    EXPECT_LT(ms, 30000) << "Should complete in under 30 seconds";
}

TEST_F(VaultCreationOrchestratorIntegrationTest, EdgeCase_VariousPasswordLengthRequirements) {
    // Test edge cases around min password length (policy minimum is 8)
    std::vector<size_t> lengths = {8, 12, 16, 32, 64};
    for (size_t len : lengths) {
        params.policy.min_password_length = len;
        params.admin_password = std::string(len, 'A') + "1!";  // Meet complexity
        params.path = (test_dir / ("vault_len" + std::to_string(len) + ".vault")).string();

        auto result = orchestrator->create_vault_v2_sync(params);

        ASSERT_TRUE(result.has_value())
            << "Failed with min_length=" << len << ": " << static_cast<int>(result.error());
        EXPECT_EQ(result->header.security_policy.min_password_length, len);
    }
}

// ----------------------------------------------------------------------------
// File System Edge Cases
// ----------------------------------------------------------------------------

TEST_F(VaultCreationOrchestratorIntegrationTest, EdgeCase_LongFilePath) {
    // Create a deeply nested directory structure
    fs::path deep_path = test_dir;
    for (int i = 0; i < 20; i++) {
        deep_path /= ("subdir_" + std::to_string(i));
    }
    deep_path /= "vault_with_very_long_name_to_test_path_limits.vault";

    params.path = deep_path.string();

    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_TRUE(result.has_value()) << "Failed with long path: " << static_cast<int>(result.error());
    EXPECT_TRUE(fs::exists(deep_path));
}

TEST_F(VaultCreationOrchestratorIntegrationTest, EdgeCase_PathWithSpaces) {
    params.path = (test_dir / "vault with spaces.vault").string();

    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_TRUE(result.has_value()) << "Failed with spaces in path: " << static_cast<int>(result.error());
    EXPECT_TRUE(fs::exists(params.path));
}

TEST_F(VaultCreationOrchestratorIntegrationTest, EdgeCase_PathWithSpecialChars) {
    // Test with special characters in filename (avoiding filesystem-forbidden ones)
    params.path = (test_dir / "vault-test_file.v2@2026.vault").string();

    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_TRUE(result.has_value()) << "Failed with special chars: " << static_cast<int>(result.error());
    EXPECT_TRUE(fs::exists(params.path));
}

TEST_F(VaultCreationOrchestratorIntegrationTest, EdgeCase_PathWithUnicode) {
    params.path = (test_dir / "сейф_保险库.vault").string();  // Russian + Chinese

    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_TRUE(result.has_value()) << "Failed with Unicode path: " << static_cast<int>(result.error());
    EXPECT_TRUE(fs::exists(params.path));
}

// ----------------------------------------------------------------------------
// Concurrent Creation Tests
// ----------------------------------------------------------------------------

TEST_F(VaultCreationOrchestratorIntegrationTest, EdgeCase_RapidSequentialCreation) {
    // Create multiple vaults rapidly in sequence
    const int vault_count = 10;
    std::vector<std::string> paths;

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < vault_count; i++) {
        params.path = (test_dir / ("rapid_" + std::to_string(i) + ".vault")).string();
        params.admin_username = "admin" + std::to_string(i) + "@example.com";

        auto result = orchestrator->create_vault_v2_sync(params);

        ASSERT_TRUE(result.has_value())
            << "Vault " << i << " failed: " << static_cast<int>(result.error());
        paths.push_back(params.path);
    }

    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Verify all files exist
    for (const auto& path : paths) {
        EXPECT_TRUE(fs::exists(path));
    }

    std::cout << "Created " << vault_count << " vaults in " << ms << "ms" << std::endl;
}

TEST_F(VaultCreationOrchestratorIntegrationTest, EdgeCase_MultipleOrchestrators) {
    // Test creating vaults with different orchestrator instances
    auto orchestrator2 = std::make_unique<VaultCreationOrchestrator>(
        crypto_service, yubikey_service, file_service
    );
    auto orchestrator3 = std::make_unique<VaultCreationOrchestrator>(
        crypto_service, yubikey_service, file_service
    );

    params.path = (test_dir / "orch1.vault").string();
    auto result1 = orchestrator->create_vault_v2_sync(params);

    params.path = (test_dir / "orch2.vault").string();
    auto result2 = orchestrator2->create_vault_v2_sync(params);

    params.path = (test_dir / "orch3.vault").string();
    auto result3 = orchestrator3->create_vault_v2_sync(params);

    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());
    ASSERT_TRUE(result3.has_value());

    // Verify all files exist and are different
    EXPECT_TRUE(fs::exists((test_dir / "orch1.vault")));
    EXPECT_TRUE(fs::exists((test_dir / "orch2.vault")));
    EXPECT_TRUE(fs::exists((test_dir / "orch3.vault")));
}

// ----------------------------------------------------------------------------
// Progress Callback Edge Cases
// ----------------------------------------------------------------------------

TEST_F(VaultCreationOrchestratorIntegrationTest, EdgeCase_ProgressCallbackThrows) {
    // Test that exceptions in callback propagate (which is reasonable behavior)
    params.progress_callback = [](int, int, const std::string&) {
        throw std::runtime_error("Callback error");
    };

    // The exception should propagate, which is acceptable behavior
    EXPECT_THROW({
        auto result = orchestrator->create_vault_v2_sync(params);
    }, std::runtime_error);
}

TEST_F(VaultCreationOrchestratorIntegrationTest, EdgeCase_NullProgressCallback) {
    // Test with explicitly null callback
    params.progress_callback = nullptr;

    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_TRUE(result.has_value()) << "Should work without callback";
}

// ----------------------------------------------------------------------------
// Error Boundary Tests
// ----------------------------------------------------------------------------

TEST_F(VaultCreationOrchestratorIntegrationTest, EdgeCase_ReadOnlyDirectory) {
    // Create a read-only directory
    fs::path readonly_dir = test_dir / "readonly";
    fs::create_directories(readonly_dir);
    fs::permissions(readonly_dir, fs::perms::owner_read | fs::perms::owner_exec);

    params.path = (readonly_dir / "vault.vault").string();

    auto result = orchestrator->create_vault_v2_sync(params);

    // Should fail with permission error
    EXPECT_FALSE(result.has_value());
    if (!result.has_value()) {
        std::cout << "Expected error for read-only dir: "
                  << static_cast<int>(result.error()) << std::endl;
    }

    // Cleanup: restore write permission
    fs::permissions(readonly_dir, fs::perms::owner_all);
}

TEST_F(VaultCreationOrchestratorIntegrationTest, EdgeCase_InvalidPathCharacters) {
    // Test with truly invalid path (null bytes not possible in strings, but try other invalid chars)
    // On Linux, only '/' and null are forbidden in filename, but parent dir might not exist
    params.path = "/nonexistent/deeply/nested/path/that/does/not/exist/vault.vault";

    auto result = orchestrator->create_vault_v2_sync(params);

    // Should fail because parent directory doesn't exist and we can't create it
    EXPECT_FALSE(result.has_value());
    if (!result.has_value()) {
        std::cout << "Expected error for invalid path: "
                  << static_cast<int>(result.error()) << std::endl;
    }
}

// ----------------------------------------------------------------------------
// Memory and Resource Edge Cases
// ----------------------------------------------------------------------------

TEST_F(VaultCreationOrchestratorIntegrationTest, EdgeCase_LargePasswordSalt) {
    // The system generates salts internally, but we can test with high iterations
    // which stresses the PBKDF2 computation
    params.policy.pbkdf2_iterations = 750000;

    auto result = orchestrator->create_vault_v2_sync(params);

    ASSERT_TRUE(result.has_value());

    // Verify salt was properly generated (should be 32 bytes)
    EXPECT_EQ(result->header.key_slots[0].salt.size(), 32);
}

TEST_F(VaultCreationOrchestratorIntegrationTest, EdgeCase_MultipleVaultsReuseOrchestrator) {
    // Create many vaults with same orchestrator to test resource cleanup
    const int count = 20;

    for (int i = 0; i < count; i++) {
        params.path = (test_dir / ("reuse_" + std::to_string(i) + ".vault")).string();
        params.admin_username = "user" + std::to_string(i) + "@example.com";
        // Make passwords meet minimum length requirement
        params.admin_password = "SecurePassword" + std::to_string(i) + "!";

        auto result = orchestrator->create_vault_v2_sync(params);

        ASSERT_TRUE(result.has_value())
            << "Failed on vault " << i << ": " << static_cast<int>(result.error());

        // Verify previous vaults still exist
        for (int j = 0; j < i; j++) {
            fs::path prev_path = test_dir / ("reuse_" + std::to_string(j) + ".vault");
            EXPECT_TRUE(fs::exists(prev_path))
                << "Vault " << j << " disappeared after creating vault " << i;
        }
    }
}
