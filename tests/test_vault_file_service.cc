// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2024 KeepTower Contributors
// File: tests/test_vault_file_service.cc

#include <gtest/gtest.h>
#include "../src/core/services/VaultFileService.h"
#include <filesystem>
#include <fstream>
#include <vector>

using namespace KeepTower;
namespace fs = std::filesystem;

/**
 * @brief Unit tests for VaultFileService
 *
 * Tests cover all file I/O operations, format detection, backup management,
 * and error handling WITHOUT requiring actual crypto operations or vault data.
 *
 * Test Categories:
 * 1. File Reading - Valid/invalid files, error conditions
 * 2. File Writing - Atomic operations, permissions, error recovery
 * 3. Format Detection - V1/V2/invalid detection
 * 4. Backup Management - Create, restore, list, cleanup
 * 5. Error Handling - Permission errors, disk full, invalid paths
 * 6. Utility Functions - file_exists, get_file_size
 */
class VaultFileServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temp directory for test files
        test_dir = fs::temp_directory_path() / "keeptower_file_service_test";
        fs::create_directories(test_dir);

        test_vault_path = test_dir / "test_vault.vault";
        test_backup_dir = test_dir / "backups";
    }

    void TearDown() override {
        // Cleanup test directory
        if (fs::exists(test_dir)) {
            fs::remove_all(test_dir);
        }
    }

    // Helper: Create a V1 vault file with proper header
    void create_v1_vault_file(const fs::path& path, int pbkdf2_iterations = 100000) {
        std::ofstream file(path, std::ios::binary);

        // V1 Header: [Magic: "KPT\0"] [Version: 1] [Iterations: uint32_t]
        const uint8_t header[] = {
            'K', 'P', 'T', 0x00,                                    // Magic
            0x01, 0x00, 0x00, 0x00,                                 // Version 1
            static_cast<uint8_t>(pbkdf2_iterations & 0xFF),         // Iterations (LE)
            static_cast<uint8_t>((pbkdf2_iterations >> 8) & 0xFF),
            static_cast<uint8_t>((pbkdf2_iterations >> 16) & 0xFF),
            static_cast<uint8_t>((pbkdf2_iterations >> 24) & 0xFF)
        };

        file.write(reinterpret_cast<const char*>(header), sizeof(header));

        // Add some dummy data
        const std::string dummy_data = "encrypted_vault_data_v1";
        file.write(dummy_data.c_str(), static_cast<std::streamsize>(dummy_data.size()));
    }

    // Helper: Create a V2 vault file with proper header
    void create_v2_vault_file(const fs::path& path) {
        std::ofstream file(path, std::ios::binary);

        // V2 Header magic: "KPTV2" (simplified for testing)
        // Real V2 has complex header with FEC, using minimal valid V2 structure
        const uint8_t header[] = {
            'K', 'P', 'T', 'V',  '2', 0x00, 0x00, 0x00,  // Magic + version
            0x02, 0x00, 0x00, 0x00                       // Version 2
        };

        file.write(reinterpret_cast<const char*>(header), sizeof(header));

        // Add dummy V2 data
        const std::string dummy_data = "encrypted_vault_data_v2_multi_user";
        file.write(dummy_data.c_str(), static_cast<std::streamsize>(dummy_data.size()));
    }

    fs::path test_dir;
    fs::path test_vault_path;
    fs::path test_backup_dir;
};

// ============================================================================
// File Reading Tests
// ============================================================================

TEST_F(VaultFileServiceTest, ReadVaultFile_ValidV1File) {
    create_v1_vault_file(test_vault_path, 150000);

    std::vector<uint8_t> data;
    int iterations;
    auto result = VaultFileService::read_vault_file(test_vault_path.string(), data, iterations);

    ASSERT_TRUE(result.has_value()) << "Should read V1 vault successfully";
    EXPECT_EQ(iterations, 150000) << "Should extract correct PBKDF2 iterations";
    EXPECT_GT(data.size(), 12) << "Should read complete file";
}

TEST_F(VaultFileServiceTest, ReadVaultFile_FileNotFound) {
    std::vector<uint8_t> data;
    int iterations;
    auto result = VaultFileService::read_vault_file("/nonexistent/vault.vault", data, iterations);

    ASSERT_FALSE(result.has_value()) << "Should fail for non-existent file";
    EXPECT_EQ(result.error(), VaultError::FileNotFound);
}

TEST_F(VaultFileServiceTest, ReadVaultFile_EmptyFile) {
    // Create empty file
    {
        std::ofstream file(test_vault_path);
    }

    std::vector<uint8_t> data;
    int iterations;
    auto result = VaultFileService::read_vault_file(test_vault_path.string(), data, iterations);

    ASSERT_FALSE(result.has_value()) << "Should reject empty file";
    EXPECT_EQ(result.error(), VaultError::InvalidData);
}

TEST_F(VaultFileServiceTest, ReadVaultFile_InvalidFormat) {
    // Create file with invalid magic
    std::ofstream file(test_vault_path, std::ios::binary);
    file << "INVALID_MAGIC_DATA";
    file.close();

    std::vector<uint8_t> data;
    int iterations;
    auto result = VaultFileService::read_vault_file(test_vault_path.string(), data, iterations);

    ASSERT_FALSE(result.has_value()) << "Should reject invalid format";
    EXPECT_EQ(result.error(), VaultError::InvalidData);
}

// ============================================================================
// File Writing Tests
// ============================================================================

TEST_F(VaultFileServiceTest, WriteVaultFile_V1Format) {
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04, 0x05};
    const int iterations = 200000;

    auto result = VaultFileService::write_vault_file(
        test_vault_path.string(), data, false, iterations);

    ASSERT_TRUE(result.has_value()) << "Should write V1 vault successfully";
    ASSERT_TRUE(fs::exists(test_vault_path)) << "File should exist";

    // Verify header was prepended
    std::ifstream file(test_vault_path, std::ios::binary);
    uint8_t header[12];
    file.read(reinterpret_cast<char*>(header), 12);

    EXPECT_EQ(header[0], 'K');
    EXPECT_EQ(header[1], 'P');
    EXPECT_EQ(header[2], 'T');
    EXPECT_EQ(header[3], 0x00);
    EXPECT_EQ(header[4], 0x01);  // Version 1
}

TEST_F(VaultFileServiceTest, WriteVaultFile_V2Format) {
    // V2 data already has header
    std::vector<uint8_t> data(100, 0xAB);

    auto result = VaultFileService::write_vault_file(
        test_vault_path.string(), data, true, 0);

    ASSERT_TRUE(result.has_value()) << "Should write V2 vault successfully";
    ASSERT_TRUE(fs::exists(test_vault_path)) << "File should exist";

    // Verify data written directly (no header prepended)
    std::ifstream file(test_vault_path, std::ios::binary);
    std::vector<uint8_t> read_data(100);
    file.read(reinterpret_cast<char*>(read_data.data()), 100);

    EXPECT_EQ(read_data, data) << "V2 data should be written as-is";
}

TEST_F(VaultFileServiceTest, WriteVaultFile_AtomicOperation) {
    // Write initial data
    std::vector<uint8_t> data1 = {0x01, 0x02, 0x03};
    auto result1 = VaultFileService::write_vault_file(
        test_vault_path.string(), data1, true, 0);
    ASSERT_TRUE(result1.has_value());

    // Overwrite with new data
    std::vector<uint8_t> data2 = {0x0A, 0x0B, 0x0C, 0x0D};
    auto result2 = VaultFileService::write_vault_file(
        test_vault_path.string(), data2, true, 0);
    ASSERT_TRUE(result2.has_value());

    // Verify only new data exists (atomic replace)
    // Read raw file contents directly (not using read_vault_file which requires valid headers)
    std::ifstream file(test_vault_path, std::ios::binary);
    std::vector<uint8_t> read_data(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());

    EXPECT_EQ(read_data, data2) << "Should contain only new data (atomic replace)";
}

TEST_F(VaultFileServiceTest, WriteVaultFile_CreateParentDirectories) {
    fs::path deep_path = test_dir / "level1" / "level2" / "vault.vault";
    std::vector<uint8_t> data = {0x01, 0x02};

    auto result = VaultFileService::write_vault_file(
        deep_path.string(), data, true, 0);

    ASSERT_TRUE(result.has_value()) << "Should create parent directories";
    ASSERT_TRUE(fs::exists(deep_path)) << "Vault should exist in deep directory";
}

#ifndef _WIN32
TEST_F(VaultFileServiceTest, WriteVaultFile_SecurePermissions) {
    std::vector<uint8_t> data = {0x01, 0x02, 0x03};
    auto result = VaultFileService::write_vault_file(
        test_vault_path.string(), data, true, 0);
    ASSERT_TRUE(result.has_value());

    // Check permissions (should be 0600 - owner read/write only)
    auto perms = fs::status(test_vault_path).permissions();
    EXPECT_TRUE((perms & fs::perms::owner_read) != fs::perms::none);
    EXPECT_TRUE((perms & fs::perms::owner_write) != fs::perms::none);
    EXPECT_TRUE((perms & fs::perms::group_read) == fs::perms::none);
    EXPECT_TRUE((perms & fs::perms::others_read) == fs::perms::none);
}
#endif

// ============================================================================
// Format Detection Tests
// ============================================================================

TEST_F(VaultFileServiceTest, DetectVaultVersion_V1Format) {
    create_v1_vault_file(test_vault_path);

    std::vector<uint8_t> data;
    int iterations;
    auto read_result = VaultFileService::read_vault_file(test_vault_path.string(), data, iterations);
    ASSERT_TRUE(read_result.has_value());

    auto version = VaultFileService::detect_vault_version(data);
    ASSERT_TRUE(version.has_value()) << "Should detect format";
    EXPECT_EQ(*version, 1) << "Should detect V1 format";
}

TEST_F(VaultFileServiceTest, DetectVaultVersion_InvalidMagic) {
    std::vector<uint8_t> data = {'X', 'Y', 'Z', 0x00, 0x01, 0x00, 0x00, 0x00};

    auto version = VaultFileService::detect_vault_version(data);
    EXPECT_FALSE(version.has_value()) << "Should reject invalid magic";
}

TEST_F(VaultFileServiceTest, DetectVaultVersion_TooShort) {
    std::vector<uint8_t> data = {'K', 'P', 'T'};  // Only 3 bytes

    auto version = VaultFileService::detect_vault_version(data);
    EXPECT_FALSE(version.has_value()) << "Should reject too-short data";
}

TEST_F(VaultFileServiceTest, DetectVaultVersionFromFile_Valid) {
    create_v1_vault_file(test_vault_path);

    auto version = VaultFileService::detect_vault_version_from_file(test_vault_path.string());
    ASSERT_TRUE(version.has_value()) << "Should detect version from file";
    EXPECT_EQ(*version, 1);
}

TEST_F(VaultFileServiceTest, DetectVaultVersionFromFile_FileNotFound) {
    auto version = VaultFileService::detect_vault_version_from_file("/nonexistent/vault.vault");
    EXPECT_FALSE(version.has_value()) << "Should return nullopt for missing file";
}

// ============================================================================
// Backup Management Tests
// ============================================================================

TEST_F(VaultFileServiceTest, CreateBackup_Success) {
    create_v1_vault_file(test_vault_path);

    auto result = VaultFileService::create_backup(test_vault_path.string());

    ASSERT_TRUE(result.has_value()) << "Should create backup successfully";

    const std::string backup_path = result.value();
    ASSERT_TRUE(fs::exists(backup_path)) << "Backup file should exist";
    EXPECT_TRUE(backup_path.find(".backup") != std::string::npos)
        << "Backup should have .backup extension";
}

TEST_F(VaultFileServiceTest, CreateBackup_CustomDirectory) {
    create_v1_vault_file(test_vault_path);
    fs::create_directories(test_backup_dir);

    auto result = VaultFileService::create_backup(
        test_vault_path.string(), test_backup_dir.string());

    ASSERT_TRUE(result.has_value());

    const std::string backup_path = result.value();
    EXPECT_TRUE(backup_path.find(test_backup_dir.string()) != std::string::npos)
        << "Backup should be in custom directory";
}

TEST_F(VaultFileServiceTest, CreateBackup_SourceNotFound) {
    auto result = VaultFileService::create_backup("/nonexistent/vault.vault");

    ASSERT_FALSE(result.has_value()) << "Should fail when source doesn't exist";
    EXPECT_EQ(result.error(), VaultError::FileNotFound);
}

TEST_F(VaultFileServiceTest, CreateBackup_MultipleBackups) {
    create_v1_vault_file(test_vault_path);

    // Create multiple backups
    auto backup1 = VaultFileService::create_backup(test_vault_path.string());
    ASSERT_TRUE(backup1.has_value());

    // Small delay to ensure different timestamps
    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto backup2 = VaultFileService::create_backup(test_vault_path.string());
    ASSERT_TRUE(backup2.has_value());

    EXPECT_NE(backup1.value(), backup2.value())
        << "Backups should have unique timestamps";
}

TEST_F(VaultFileServiceTest, ListBackups_Empty) {
    create_v1_vault_file(test_vault_path);

    auto backups = VaultFileService::list_backups(test_vault_path.string());
    EXPECT_TRUE(backups.empty()) << "Should return empty list when no backups exist";
}

TEST_F(VaultFileServiceTest, ListBackups_MultipleBackups) {
    create_v1_vault_file(test_vault_path);

    // Create 3 backups
    for (int i = 0; i < 3; ++i) {
        auto backup_result = VaultFileService::create_backup(test_vault_path.string());
        ASSERT_TRUE(backup_result.has_value());
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    auto backups = VaultFileService::list_backups(test_vault_path.string());
    ASSERT_EQ(backups.size(), 3) << "Should find all 3 backups";

    // Verify sorted (newest first)
    for (size_t i = 1; i < backups.size(); ++i) {
        EXPECT_GT(backups[i-1], backups[i])
            << "Backups should be sorted newest first";
    }
}

TEST_F(VaultFileServiceTest, RestoreFromBackup_Success) {
    // Create original vault
    create_v1_vault_file(test_vault_path, 100000);

    // Create backup
    auto backup_result = VaultFileService::create_backup(test_vault_path.string());
    ASSERT_TRUE(backup_result.has_value());

    // Modify vault
    create_v1_vault_file(test_vault_path, 200000);

    // Restore from backup
    auto restore_result = VaultFileService::restore_from_backup(test_vault_path.string());
    ASSERT_TRUE(restore_result.has_value()) << "Should restore successfully";

    // Verify restored content
    std::vector<uint8_t> data;
    int iterations;
    auto read_result = VaultFileService::read_vault_file(test_vault_path.string(), data, iterations);
    ASSERT_TRUE(read_result.has_value());
    EXPECT_EQ(iterations, 100000) << "Should restore original iterations value";
}

TEST_F(VaultFileServiceTest, RestoreFromBackup_NoBackups) {
    create_v1_vault_file(test_vault_path);

    auto result = VaultFileService::restore_from_backup(test_vault_path.string());

    ASSERT_FALSE(result.has_value()) << "Should fail when no backups exist";
    EXPECT_EQ(result.error(), VaultError::FileNotFound);
}

TEST_F(VaultFileServiceTest, CleanupOldBackups_KeepsMax) {
    create_v1_vault_file(test_vault_path);

    // Create 5 backups with 1-second delays to ensure unique timestamps
    // (backup filenames use YYYY-MM-DDTHH-MM-SS format with second-level precision)
    for (int i = 0; i < 5; ++i) {
        auto backup_result = VaultFileService::create_backup(test_vault_path.string());
        ASSERT_TRUE(backup_result.has_value());
        if (i < 4) {  // No need to sleep after last backup
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    auto backups_before = VaultFileService::list_backups(test_vault_path.string());
    ASSERT_EQ(backups_before.size(), 5) << "Should have created 5 unique backups";

    // Keep only 2
    VaultFileService::cleanup_old_backups(test_vault_path.string(), 2);

    auto backups_after = VaultFileService::list_backups(test_vault_path.string());
    EXPECT_EQ(backups_after.size(), 2) << "Should keep only 2 most recent backups";
}

TEST_F(VaultFileServiceTest, CleanupOldBackups_KeepsNewest) {
    create_v1_vault_file(test_vault_path);

    // Create 3 backups
    auto backup1 = VaultFileService::create_backup(test_vault_path.string());
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto backup2 = VaultFileService::create_backup(test_vault_path.string());
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto backup3 = VaultFileService::create_backup(test_vault_path.string());

    // Keep only 1 (newest)
    VaultFileService::cleanup_old_backups(test_vault_path.string(), 1);

    auto remaining = VaultFileService::list_backups(test_vault_path.string());
    ASSERT_EQ(remaining.size(), 1);
    EXPECT_EQ(remaining[0], backup3.value()) << "Should keep newest backup";
}

TEST_F(VaultFileServiceTest, CleanupOldBackups_InvalidMax) {
    create_v1_vault_file(test_vault_path);
    auto backup_result = VaultFileService::create_backup(test_vault_path.string());
    ASSERT_TRUE(backup_result.has_value());

    // Should handle invalid max_backups gracefully
    VaultFileService::cleanup_old_backups(test_vault_path.string(), 0);
    VaultFileService::cleanup_old_backups(test_vault_path.string(), -1);

    // Backup should still exist
    auto backups = VaultFileService::list_backups(test_vault_path.string());
    EXPECT_EQ(backups.size(), 1) << "Invalid max should not delete backups";
}

// ============================================================================
// Utility Function Tests
// ============================================================================

TEST_F(VaultFileServiceTest, FileExists_ExistingFile) {
    create_v1_vault_file(test_vault_path);
    EXPECT_TRUE(VaultFileService::file_exists(test_vault_path.string()));
}

TEST_F(VaultFileServiceTest, FileExists_NonExistentFile) {
    EXPECT_FALSE(VaultFileService::file_exists("/nonexistent/file.vault"));
}

TEST_F(VaultFileServiceTest, FileExists_Directory) {
    fs::create_directories(test_dir / "subdir");
    EXPECT_FALSE(VaultFileService::file_exists((test_dir / "subdir").string()))
        << "Should return false for directories";
}

TEST_F(VaultFileServiceTest, GetFileSize_ValidFile) {
    create_v1_vault_file(test_vault_path);

    size_t size = VaultFileService::get_file_size(test_vault_path.string());
    EXPECT_GT(size, 0) << "Should return non-zero size for valid file";
    EXPECT_GE(size, 12) << "V1 vault should be at least 12 bytes (header)";
}

TEST_F(VaultFileServiceTest, GetFileSize_NonExistentFile) {
    size_t size = VaultFileService::get_file_size("/nonexistent/file.vault");
    EXPECT_EQ(size, 0) << "Should return 0 for non-existent file";
}

TEST_F(VaultFileServiceTest, GetFileSize_EmptyFile) {
    {
        std::ofstream file(test_vault_path);  // Create empty file
    }

    size_t size = VaultFileService::get_file_size(test_vault_path.string());
    EXPECT_EQ(size, 0) << "Should return 0 for empty file";
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(VaultFileServiceTest, ReadVaultFile_DirectoryAsPath) {
    fs::create_directories(test_dir / "not_a_file");

    std::vector<uint8_t> data;
    int iterations;
    auto result = VaultFileService::read_vault_file(
        (test_dir / "not_a_file").string(), data, iterations);

    ASSERT_FALSE(result.has_value()) << "Should reject directory as vault file";
}

TEST_F(VaultFileServiceTest, WriteVaultFile_LargeData) {
    // Test with 10MB of data
    std::vector<uint8_t> large_data(10 * 1024 * 1024, 0xAB);

    auto result = VaultFileService::write_vault_file(
        test_vault_path.string(), large_data, true, 0);

    ASSERT_TRUE(result.has_value()) << "Should handle large files";
    EXPECT_EQ(fs::file_size(test_vault_path), large_data.size());
}

TEST_F(VaultFileServiceTest, BackupOperations_LongFilenames) {
    // Test with long filename (but within filesystem limits)
    std::string long_name(200, 'a');
    long_name += ".vault";
    fs::path long_path = test_dir / long_name;

    create_v1_vault_file(long_path);

    auto backup_result = VaultFileService::create_backup(long_path.string());
    EXPECT_TRUE(backup_result.has_value())
        << "Should handle long filenames";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
