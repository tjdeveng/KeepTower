// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file test_vault_io.cc
 * @brief Unit tests for VaultIO file operations
 *
 * Tests file reading/writing, atomic operations, backup management,
 * and secure permissions for vault persistence.
 */

#include <gtest/gtest.h>
#include "../src/core/io/VaultIO.h"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>

#ifdef __linux__
#include <sys/stat.h>
#endif

using namespace KeepTower;

// ============================================================================
// Test Fixture
// ============================================================================

class VaultIOTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary test directory
        test_dir = std::filesystem::temp_directory_path() / "keeptower_test_io";
        std::filesystem::create_directories(test_dir);

        test_file = test_dir / "test_vault.dat";

        // Sample test data
        test_data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

        // V1 vault header: magic + version + iterations
        v1_header = {
            0x54, 0x56, 0x54, 0x4B,  // VAULT_MAGIC (KTVT in little-endian)
            0x01, 0x00, 0x00, 0x00,  // version = 1
            0xC0, 0x27, 0x09, 0x00   // iterations = 600000 (0x927C0)
        };
    }

    void TearDown() override {
        // Clean up test files
        if (std::filesystem::exists(test_dir)) {
            std::filesystem::remove_all(test_dir);
        }
    }

    std::filesystem::path test_dir;
    std::filesystem::path test_file;
    std::vector<uint8_t> test_data;
    std::vector<uint8_t> v1_header;
};

// ============================================================================
// File Reading Tests
// ============================================================================

TEST_F(VaultIOTest, ReadFileV1WithHeader) {
    // Write V1 vault file with header
    std::vector<uint8_t> file_content;
    file_content.insert(file_content.end(), v1_header.begin(), v1_header.end());
    file_content.insert(file_content.end(), test_data.begin(), test_data.end());

    std::ofstream file(test_file, std::ios::binary);
    file.write(reinterpret_cast<const char*>(file_content.data()), file_content.size());
    file.close();
    // Set secure permissions (owner read/write only)
    chmod(test_file.c_str(), S_IRUSR | S_IWUSR);
    std::vector<uint8_t> read_data;
    int iterations = 0;

    bool result = VaultIO::read_file(test_file.string(), read_data, false, iterations);

    ASSERT_TRUE(result);
    EXPECT_EQ(read_data, test_data);  // Header should be stripped
    EXPECT_EQ(iterations, 600000);
}

TEST_F(VaultIOTest, ReadFileV2IncludesHeader) {
    // Write V2 vault file (header is part of data)
    std::vector<uint8_t> file_content;
    file_content.insert(file_content.end(), v1_header.begin(), v1_header.end());
    file_content.insert(file_content.end(), test_data.begin(), test_data.end());

    std::ofstream file(test_file, std::ios::binary);
    file.write(reinterpret_cast<const char*>(file_content.data()), file_content.size());
    file.close();

    // Set secure permissions (owner read/write only)
    chmod(test_file.c_str(), S_IRUSR | S_IWUSR);

    std::vector<uint8_t> read_data;
    int iterations = 0;

    // is_v2_vault = true means header should be included in output
    bool result = VaultIO::read_file(test_file.string(), read_data, true, iterations);

    ASSERT_TRUE(result);
    EXPECT_EQ(read_data, file_content);  // Should include header
    EXPECT_EQ(iterations, 600000);
}

TEST_F(VaultIOTest, ReadFileLegacyFormat) {
    // Write file without magic header
    std::ofstream file(test_file, std::ios::binary);
    file.write(reinterpret_cast<const char*>(test_data.data()), test_data.size());
    file.close();

    // Set secure permissions (owner read/write only)
    chmod(test_file.c_str(), S_IRUSR | S_IWUSR);

    std::vector<uint8_t> read_data;
    int iterations = 0;

    bool result = VaultIO::read_file(test_file.string(), read_data, false, iterations);

    ASSERT_TRUE(result);
    EXPECT_EQ(read_data, test_data);
    EXPECT_EQ(iterations, VaultIO::DEFAULT_PBKDF2_ITERATIONS);  // Default for legacy
}

TEST_F(VaultIOTest, ReadFileNonExistent) {
    std::vector<uint8_t> read_data;
    int iterations = 0;

    bool result = VaultIO::read_file("/nonexistent/file.dat", read_data, false, iterations);

    EXPECT_FALSE(result);
}

TEST_F(VaultIOTest, ReadFileEmpty) {
    // Create empty file
    std::ofstream file(test_file, std::ios::binary);
    file.close();

    // Set secure permissions (owner read/write only)
    chmod(test_file.c_str(), S_IRUSR | S_IWUSR);

    std::vector<uint8_t> read_data;
    int iterations = 0;

    bool result = VaultIO::read_file(test_file.string(), read_data, false, iterations);

    ASSERT_TRUE(result);
    EXPECT_EQ(read_data.size(), 0);
}

TEST_F(VaultIOTest, ReadFileTooShortForHeader) {
    // Write file with only 8 bytes (too short for 12-byte header)
    std::vector<uint8_t> short_data = {1, 2, 3, 4, 5, 6, 7, 8};

    std::ofstream file(test_file, std::ios::binary);
    file.write(reinterpret_cast<const char*>(short_data.data()), short_data.size());
    file.close();

    // Set secure permissions (owner read/write only)
    chmod(test_file.c_str(), S_IRUSR | S_IWUSR);

    std::vector<uint8_t> read_data;
    int iterations = 0;

    bool result = VaultIO::read_file(test_file.string(), read_data, false, iterations);

    ASSERT_TRUE(result);
    EXPECT_EQ(read_data, short_data);  // Treats as legacy format
    EXPECT_EQ(iterations, VaultIO::DEFAULT_PBKDF2_ITERATIONS);
}

TEST_F(VaultIOTest, ReadFileInvalidMagic) {
    // Write file with invalid magic number
    std::vector<uint8_t> file_content = {
        0xFF, 0xFF, 0xFF, 0xFF,  // Invalid magic
        0x01, 0x00, 0x00, 0x00,  // version = 1
        0xC0, 0x27, 0x09, 0x00   // iterations = 600000
    };
    file_content.insert(file_content.end(), test_data.begin(), test_data.end());

    std::ofstream file(test_file, std::ios::binary);
    file.write(reinterpret_cast<const char*>(file_content.data()), file_content.size());
    file.close();

    // Set secure permissions (owner read/write only)
    chmod(test_file.c_str(), S_IRUSR | S_IWUSR);

    std::vector<uint8_t> read_data;
    int iterations = 0;

    bool result = VaultIO::read_file(test_file.string(), read_data, false, iterations);

    ASSERT_TRUE(result);
    EXPECT_EQ(read_data, file_content);  // Treats as legacy, includes all bytes
    EXPECT_EQ(iterations, VaultIO::DEFAULT_PBKDF2_ITERATIONS);
}

#ifdef __linux__
TEST_F(VaultIOTest, ReadFileRejectsInsecurePermissions) {
    // Create file with insecure permissions (readable by group/others)
    std::ofstream file(test_file, std::ios::binary);
    file.write(reinterpret_cast<const char*>(test_data.data()), test_data.size());
    file.close();

    // Set insecure permissions: 0644 (readable by all)
    chmod(test_file.c_str(), S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    std::vector<uint8_t> read_data;
    int iterations = 0;

    bool result = VaultIO::read_file(test_file.string(), read_data, false, iterations);

    EXPECT_FALSE(result);  // Should reject insecure file
}
#endif

// ============================================================================
// File Writing Tests
// ============================================================================

TEST_F(VaultIOTest, WriteFileV1CreatesHeader) {
    bool result = VaultIO::write_file(test_file.string(), test_data, false, 600000);

    ASSERT_TRUE(result);

    // Read back and verify header was added
    std::ifstream file(test_file, std::ios::binary);
    ASSERT_TRUE(file.is_open());

    std::vector<uint8_t> read_content(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());

    // Check header
    EXPECT_EQ(read_content[0], 0x54);  // K
    EXPECT_EQ(read_content[1], 0x56);  // T
    EXPECT_EQ(read_content[2], 0x54);  // V
    EXPECT_EQ(read_content[3], 0x4B);  // T

    // Check data follows header
    std::vector<uint8_t> data_part(read_content.begin() + 12, read_content.end());
    EXPECT_EQ(data_part, test_data);
}

TEST_F(VaultIOTest, WriteFileV2NoHeader) {
    // V2 data already contains header
    std::vector<uint8_t> v2_data;
    v2_data.insert(v2_data.end(), v1_header.begin(), v1_header.end());
    v2_data.insert(v2_data.end(), test_data.begin(), test_data.end());

    bool result = VaultIO::write_file(test_file.string(), v2_data, true, 600000);

    ASSERT_TRUE(result);

    // Read back and verify data is exactly as provided
    std::ifstream file(test_file, std::ios::binary);
    std::vector<uint8_t> read_content(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());

    EXPECT_EQ(read_content, v2_data);
}

TEST_F(VaultIOTest, WriteFileOverwritesExisting) {
    // Write initial file
    VaultIO::write_file(test_file.string(), test_data, false, 600000);

    // Overwrite with new data
    std::vector<uint8_t> new_data = {99, 88, 77};
    bool result = VaultIO::write_file(test_file.string(), new_data, false, 700000);

    ASSERT_TRUE(result);

    // Verify new data
    std::vector<uint8_t> read_data;
    int iterations = 0;
    VaultIO::read_file(test_file.string(), read_data, false, iterations);

    EXPECT_EQ(read_data, new_data);
    EXPECT_EQ(iterations, 700000);
}

TEST_F(VaultIOTest, WriteFileEmptyData) {
    std::vector<uint8_t> empty_data;

    bool result = VaultIO::write_file(test_file.string(), empty_data, false, 600000);

    ASSERT_TRUE(result);
    EXPECT_TRUE(std::filesystem::exists(test_file));
}

TEST_F(VaultIOTest, WriteFileLargeData) {
    // Create large data (1MB)
    std::vector<uint8_t> large_data(1024 * 1024);
    for (size_t i = 0; i < large_data.size(); ++i) {
        large_data[i] = static_cast<uint8_t>(i & 0xFF);
    }

    bool result = VaultIO::write_file(test_file.string(), large_data, false, 600000);

    ASSERT_TRUE(result);

    // Verify round-trip
    std::vector<uint8_t> read_data;
    int iterations = 0;
    VaultIO::read_file(test_file.string(), read_data, false, iterations);

    EXPECT_EQ(read_data, large_data);
}

TEST_F(VaultIOTest, WriteFileInvalidPath) {
    bool result = VaultIO::write_file("/invalid/nonexistent/path/file.dat", test_data, false, 600000);

    EXPECT_FALSE(result);
}

#ifdef __linux__
TEST_F(VaultIOTest, WriteFileSetsSecurePermissions) {
    VaultIO::write_file(test_file.string(), test_data, false, 600000);

    // Check file permissions
    struct stat st;
    stat(test_file.c_str(), &st);

    // Should be 0600 (owner read/write only)
    EXPECT_EQ(st.st_mode & 0777, S_IRUSR | S_IWUSR);
}
#endif

TEST_F(VaultIOTest, WriteFileAtomicRename) {
    // Write file
    VaultIO::write_file(test_file.string(), test_data, false, 600000);

    // Verify temp file doesn't exist after successful write
    auto temp_file = test_file.string() + ".tmp";
    EXPECT_FALSE(std::filesystem::exists(temp_file));
}
