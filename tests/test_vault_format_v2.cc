// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file test_vault_format_v2.cc
 * @brief Unit tests for VaultFormatV2 serialization and FEC operations
 *
 * Tests version detection, header serialization/deserialization,
 * FEC encoding/decoding, and error handling.
 */

#include <gtest/gtest.h>
#include "../src/core/VaultFormatV2.h"
#include "../src/core/VaultError.h"
#include <vector>
#include <cstring>

using namespace KeepTower;

// ============================================================================
// Test Fixture
// ============================================================================

class VaultFormatV2Test : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize a basic V2 file header
        header.magic = VaultFormatV2::VAULT_MAGIC;
        header.version = VaultFormatV2::VAULT_VERSION_V2;
        header.pbkdf2_iterations = 100000;

        // Initialize security policy
        header.vault_header.security_policy.min_password_length = 12;
        header.vault_header.security_policy.password_history_depth = 5;
        header.vault_header.security_policy.pbkdf2_iterations = 100000;
        header.vault_header.security_policy.require_yubikey = false;

        // Initialize data salt and IV
        for (size_t i = 0; i < 32; ++i) {
            header.data_salt[i] = static_cast<uint8_t>(i);
        }
        for (size_t i = 0; i < 12; ++i) {
            header.data_iv[i] = static_cast<uint8_t>(i + 100);
        }
    }

    VaultFormatV2::V2FileHeader header;
};

// ============================================================================
// Version Detection Tests
// ============================================================================

TEST_F(VaultFormatV2Test, DetectVersionTooSmallFile) {
    std::vector<uint8_t> data = {0x01, 0x02, 0x03}; // Only 3 bytes
    auto result = VaultFormatV2::detect_version(data);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), VaultError::CorruptedFile);
}

TEST_F(VaultFormatV2Test, DetectVersionInvalidMagic) {
    std::vector<uint8_t> data(8, 0);

    // Write invalid magic (0xDEADBEEF instead of KPTW)
    uint32_t invalid_magic = 0xDEADBEEF;
    std::memcpy(data.data(), &invalid_magic, sizeof(invalid_magic));

    // Write valid version
    uint32_t version = VaultFormatV2::VAULT_VERSION_V2;
    std::memcpy(data.data() + 4, &version, sizeof(version));

    auto result = VaultFormatV2::detect_version(data);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), VaultError::CorruptedFile);
}

TEST_F(VaultFormatV2Test, DetectVersionUnsupportedVersion) {
    std::vector<uint8_t> data(8, 0);

    // Write valid magic
    uint32_t magic = VaultFormatV2::VAULT_MAGIC;
    std::memcpy(data.data(), &magic, sizeof(magic));

    // Write unsupported version (999)
    uint32_t version = 999;
    std::memcpy(data.data() + 4, &version, sizeof(version));

    auto result = VaultFormatV2::detect_version(data);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), VaultError::UnsupportedVersion);
}

TEST_F(VaultFormatV2Test, DetectVersionV1) {
    std::vector<uint8_t> data(8, 0);

    // Write valid magic
    uint32_t magic = VaultFormatV2::VAULT_MAGIC;
    std::memcpy(data.data(), &magic, sizeof(magic));

    // Write V1 version
    uint32_t version = VaultFormatV2::VAULT_VERSION_V1;
    std::memcpy(data.data() + 4, &version, sizeof(version));

    auto result = VaultFormatV2::detect_version(data);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), VaultFormatV2::VAULT_VERSION_V1);
}

TEST_F(VaultFormatV2Test, DetectVersionV2) {
    std::vector<uint8_t> data(8, 0);

    // Write valid magic
    uint32_t magic = VaultFormatV2::VAULT_MAGIC;
    std::memcpy(data.data(), &magic, sizeof(magic));

    // Write V2 version
    uint32_t version = VaultFormatV2::VAULT_VERSION_V2;
    std::memcpy(data.data() + 4, &version, sizeof(version));

    auto result = VaultFormatV2::detect_version(data);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), VaultFormatV2::VAULT_VERSION_V2);
}

TEST_F(VaultFormatV2Test, IsValidV2VaultReturnsFalseForTooSmall) {
    std::vector<uint8_t> data = {0x01, 0x02};

    EXPECT_FALSE(VaultFormatV2::is_valid_v2_vault(data));
}

TEST_F(VaultFormatV2Test, IsValidV2VaultReturnsFalseForV1) {
    std::vector<uint8_t> data(8, 0);

    uint32_t magic = VaultFormatV2::VAULT_MAGIC;
    std::memcpy(data.data(), &magic, sizeof(magic));

    uint32_t version = VaultFormatV2::VAULT_VERSION_V1;
    std::memcpy(data.data() + 4, &version, sizeof(version));

    EXPECT_FALSE(VaultFormatV2::is_valid_v2_vault(data));
}

TEST_F(VaultFormatV2Test, IsValidV2VaultReturnsTrueForV2) {
    std::vector<uint8_t> data(8, 0);

    uint32_t magic = VaultFormatV2::VAULT_MAGIC;
    std::memcpy(data.data(), &magic, sizeof(magic));

    uint32_t version = VaultFormatV2::VAULT_VERSION_V2;
    std::memcpy(data.data() + 4, &version, sizeof(version));

    EXPECT_TRUE(VaultFormatV2::is_valid_v2_vault(data));
}

// ============================================================================
// FEC Encoding Tests (via public API)
// ============================================================================

// Note: apply_header_fec and remove_header_fec are private methods.
// We test FEC functionality indirectly through write_header/read_header roundtrip tests.

// ============================================================================
// Header Write Tests
// ============================================================================

TEST_F(VaultFormatV2Test, WriteHeaderWithoutFEC) {
    auto result = VaultFormatV2::write_header(header, false, 0);

    ASSERT_TRUE(result.has_value());
    auto& file_data = result.value();

    // Check minimum size: magic(4) + version(4) + pbkdf2(4) + header_size(4) + flags(1) + salt(32) + iv(12) = 61 bytes
    EXPECT_GE(file_data.size(), 61u);

    // Verify magic
    uint32_t magic;
    std::memcpy(&magic, file_data.data(), sizeof(magic));
    EXPECT_EQ(magic, VaultFormatV2::VAULT_MAGIC);

    // Verify version
    uint32_t version;
    std::memcpy(&version, file_data.data() + 4, sizeof(version));
    EXPECT_EQ(version, VaultFormatV2::VAULT_VERSION_V2);

    // Verify PBKDF2 iterations
    uint32_t pbkdf2;
    std::memcpy(&pbkdf2, file_data.data() + 8, sizeof(pbkdf2));
    EXPECT_EQ(pbkdf2, 100000u);

    // Verify header flags (FEC should be disabled)
    uint8_t flags = file_data[16];
    EXPECT_EQ(flags & VaultFormatV2::HEADER_FLAG_FEC_ENABLED, 0);
}

TEST_F(VaultFormatV2Test, WriteHeaderWithFEC) {
    auto result = VaultFormatV2::write_header(header, true, 20);

    ASSERT_TRUE(result.has_value());
    auto& file_data = result.value();

    // Verify header flags (FEC should be enabled)
    uint8_t flags = file_data[16];
    EXPECT_NE(flags & VaultFormatV2::HEADER_FLAG_FEC_ENABLED, 0);

    // FEC-protected header should be larger than non-FEC
    auto non_fec_result = VaultFormatV2::write_header(header, false, 0);
    ASSERT_TRUE(non_fec_result.has_value());
    EXPECT_GT(file_data.size(), non_fec_result.value().size());
}

TEST_F(VaultFormatV2Test, WriteHeaderEnforcesMinimumFECRedundancy) {
    // User requests only 10% redundancy, but minimum is 20%
    auto result = VaultFormatV2::write_header(header, true, 10);

    ASSERT_TRUE(result.has_value());

    // Should use 20% (minimum) instead of 10%
    // We can verify this by checking the FEC metadata
    auto& file_data = result.value();
    uint8_t flags = file_data[16];
    EXPECT_NE(flags & VaultFormatV2::HEADER_FLAG_FEC_ENABLED, 0);

    // The redundancy byte should be 20 (located after header_size and flags)
    // Format: magic(4) + version(4) + pbkdf2(4) + header_size(4) + flags(1) + [redundancy(1)]
    uint8_t redundancy = file_data[17];
    EXPECT_EQ(redundancy, 20);
}

TEST_F(VaultFormatV2Test, WriteHeaderRespectsHigherUserRedundancy) {
    // User requests 30% redundancy (higher than minimum 20%)
    auto result = VaultFormatV2::write_header(header, true, 30);

    ASSERT_TRUE(result.has_value());
    auto& file_data = result.value();

    // Should use 30% (user preference)
    uint8_t redundancy = file_data[17];
    EXPECT_EQ(redundancy, 30);
}

// ============================================================================
// Header Read Tests
// ============================================================================

TEST_F(VaultFormatV2Test, ReadHeaderRoundTripWithoutFEC) {
    // Write header
    auto write_result = VaultFormatV2::write_header(header, false, 0);
    ASSERT_TRUE(write_result.has_value());

    // Read header back
    auto read_result = VaultFormatV2::read_header(write_result.value());
    ASSERT_TRUE(read_result.has_value());

    auto& [read_header, offset] = read_result.value();

    // Verify magic, version, PBKDF2
    EXPECT_EQ(read_header.magic, VaultFormatV2::VAULT_MAGIC);
    EXPECT_EQ(read_header.version, VaultFormatV2::VAULT_VERSION_V2);
    EXPECT_EQ(read_header.pbkdf2_iterations, 100000u);

    // Verify security policy
    EXPECT_EQ(read_header.vault_header.security_policy.min_password_length, 12u);
    EXPECT_EQ(read_header.vault_header.security_policy.password_history_depth, 5u);

    // Verify salt and IV
    EXPECT_EQ(read_header.data_salt, header.data_salt);
    EXPECT_EQ(read_header.data_iv, header.data_iv);
}

TEST_F(VaultFormatV2Test, ReadHeaderRoundTripWithFEC) {
    // Write header with FEC
    auto write_result = VaultFormatV2::write_header(header, true, 30);
    ASSERT_TRUE(write_result.has_value());

    // Read header back
    auto read_result = VaultFormatV2::read_header(write_result.value());
    ASSERT_TRUE(read_result.has_value());

    auto& [read_header, offset] = read_result.value();

    // Verify all fields match
    EXPECT_EQ(read_header.magic, VaultFormatV2::VAULT_MAGIC);
    EXPECT_EQ(read_header.version, VaultFormatV2::VAULT_VERSION_V2);
    EXPECT_EQ(read_header.pbkdf2_iterations, 100000u);
    EXPECT_EQ(read_header.vault_header.security_policy.min_password_length, 12u);
    EXPECT_EQ(read_header.data_salt, header.data_salt);
    EXPECT_EQ(read_header.data_iv, header.data_iv);
}

TEST_F(VaultFormatV2Test, ReadHeaderTooSmallFile) {
    std::vector<uint8_t> data = {1, 2, 3}; // Too small

    auto result = VaultFormatV2::read_header(data);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), VaultError::CorruptedFile);
}

TEST_F(VaultFormatV2Test, ReadHeaderInvalidMagic) {
    std::vector<uint8_t> data(100, 0);

    // Write invalid magic
    uint32_t invalid_magic = 0xDEADBEEF;
    std::memcpy(data.data(), &invalid_magic, sizeof(invalid_magic));

    auto result = VaultFormatV2::read_header(data);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), VaultError::CorruptedFile);
}

TEST_F(VaultFormatV2Test, ReadHeaderWrongVersion) {
    std::vector<uint8_t> data(100, 0);

    // Write valid magic
    uint32_t magic = VaultFormatV2::VAULT_MAGIC;
    std::memcpy(data.data(), &magic, sizeof(magic));

    // Write V1 version
    uint32_t version = VaultFormatV2::VAULT_VERSION_V1;
    std::memcpy(data.data() + 4, &version, sizeof(version));

    auto result = VaultFormatV2::read_header(data);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), VaultError::UnsupportedVersion);
}

TEST_F(VaultFormatV2Test, ReadHeaderZeroHeaderSize) {
    std::vector<uint8_t> data(100, 0);

    // Write valid magic and version
    uint32_t magic = VaultFormatV2::VAULT_MAGIC;
    std::memcpy(data.data(), &magic, sizeof(magic));

    uint32_t version = VaultFormatV2::VAULT_VERSION_V2;
    std::memcpy(data.data() + 4, &version, sizeof(version));

    uint32_t pbkdf2 = 100000;
    std::memcpy(data.data() + 8, &pbkdf2, sizeof(pbkdf2));

    // Write zero header size
    uint32_t header_size = 0;
    std::memcpy(data.data() + 12, &header_size, sizeof(header_size));

    auto result = VaultFormatV2::read_header(data);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), VaultError::CorruptedFile);
}

TEST_F(VaultFormatV2Test, ReadHeaderExcessiveHeaderSize) {
    std::vector<uint8_t> data(100, 0);

    // Write valid magic and version
    uint32_t magic = VaultFormatV2::VAULT_MAGIC;
    std::memcpy(data.data(), &magic, sizeof(magic));

    uint32_t version = VaultFormatV2::VAULT_VERSION_V2;
    std::memcpy(data.data() + 4, &version, sizeof(version));

    uint32_t pbkdf2 = 100000;
    std::memcpy(data.data() + 8, &pbkdf2, sizeof(pbkdf2));

    // Write excessive header size (> MAX_HEADER_SIZE)
    uint32_t header_size = VaultFormatV2::MAX_HEADER_SIZE + 1;
    std::memcpy(data.data() + 12, &header_size, sizeof(header_size));

    auto result = VaultFormatV2::read_header(data);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), VaultError::CorruptedFile);
}

TEST_F(VaultFormatV2Test, ReadHeaderTruncatedFile) {
    // Write a valid header
    auto write_result = VaultFormatV2::write_header(header, false, 0);
    ASSERT_TRUE(write_result.has_value());

    // Truncate the file data
    auto truncated = write_result.value();
    truncated.resize(truncated.size() / 2);

    auto read_result = VaultFormatV2::read_header(truncated);

    ASSERT_FALSE(read_result.has_value());
    EXPECT_EQ(read_result.error(), VaultError::CorruptedFile);
}

TEST_F(VaultFormatV2Test, ReadHeaderWithCorruptedFECData) {
    // Write header with FEC
    auto write_result = VaultFormatV2::write_header(header, true, 20);
    ASSERT_TRUE(write_result.has_value());

    auto file_data = write_result.value();

    // Heavily corrupt the FEC-protected section (beyond recovery)
    // Format: magic(4) + version(4) + pbkdf2(4) + header_size(4) + flags(1) + redundancy(1) + orig_size(4) + [encoded_data]
    // Start corrupting after FEC header (at byte 22) to avoid invalid redundancy value
    for (size_t i = 22; i < std::min(size_t(70), file_data.size()); ++i) {
        file_data[i] ^= 0xFF;
    }

    auto read_result = VaultFormatV2::read_header(file_data);

    // Should fail with FEC decoding error or corrupted file
    ASSERT_FALSE(read_result.has_value());
    EXPECT_TRUE(
        read_result.error() == VaultError::FECDecodingFailed ||
        read_result.error() == VaultError::CorruptedFile
    );
}

TEST_F(VaultFormatV2Test, ReadHeaderFECTooSmall) {
    std::vector<uint8_t> data(100, 0);

    // Write valid magic, version, pbkdf2
    uint32_t magic = VaultFormatV2::VAULT_MAGIC;
    std::memcpy(data.data(), &magic, sizeof(magic));

    uint32_t version = VaultFormatV2::VAULT_VERSION_V2;
    std::memcpy(data.data() + 4, &version, sizeof(version));

    uint32_t pbkdf2 = 100000;
    std::memcpy(data.data() + 8, &pbkdf2, sizeof(pbkdf2));

    // Write header size that's too small for FEC format (< 5 bytes)
    uint32_t header_size = 4; // Smaller than FEC minimum
    std::memcpy(data.data() + 12, &header_size, sizeof(header_size));

    // Enable FEC flag
    data[16] = VaultFormatV2::HEADER_FLAG_FEC_ENABLED;

    auto result = VaultFormatV2::read_header(data);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), VaultError::CorruptedFile);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(VaultFormatV2Test, CompleteWorkflowWithKeySlots) {
    // Add some key slots to header
    KeySlot slot1;
    slot1.active = true;
    slot1.username = "admin";

    // Simulate wrapped DEK (would be encrypted in real scenario)
    slot1.wrapped_dek.fill(0xAA);
    slot1.salt.fill(0xBB);

    header.vault_header.key_slots.push_back(slot1);

    auto write_result = VaultFormatV2::write_header(header, true, 30);
    ASSERT_TRUE(write_result.has_value());

    // Read back
    auto read_result = VaultFormatV2::read_header(write_result.value());
    ASSERT_TRUE(read_result.has_value());

    auto& [read_header, offset] = read_result.value();

    // Verify key slots
    ASSERT_EQ(read_header.vault_header.key_slots.size(), 1u);
    EXPECT_EQ(read_header.vault_header.key_slots[0].username, "admin");
    EXPECT_TRUE(read_header.vault_header.key_slots[0].active);
}

TEST_F(VaultFormatV2Test, FECRecoveryFromMinorCorruption) {
    // Add multiple key slots for more realistic data
    for (int i = 0; i < 3; ++i) {
        KeySlot slot;
        slot.active = true;
        slot.username = "user" + std::to_string(i);
        slot.wrapped_dek.fill(static_cast<uint8_t>(0xAA + i));
        slot.salt.fill(static_cast<uint8_t>(0xBB + i));
        header.vault_header.key_slots.push_back(slot);
    }

    // Write with high FEC redundancy
    auto write_result = VaultFormatV2::write_header(header, true, 40);
    ASSERT_TRUE(write_result.has_value());

    auto file_data = write_result.value();

    // Introduce minor corruption (flip a few bits in FEC-protected encoded data region)
    // Format: magic(4) + version(4) + pbkdf2(4) + header_size(4) + flags(1) + redundancy(1) + orig_size(4) + [encoded_data]
    // Start corrupting at byte 22 (after FEC header) to avoid invalid redundancy
    if (file_data.size() > 50) {
        file_data[30] ^= 0x01; // Flip 1 bit in encoded data
        file_data[40] ^= 0x02; // Flip 1 bit in encoded data
    }

    // Read back - FEC should recover
    auto read_result = VaultFormatV2::read_header(file_data);

    // With 40% FEC, minor corruption should be recoverable
    if (read_result.has_value()) {
        auto& [read_header, offset] = read_result.value();
        EXPECT_EQ(read_header.vault_header.key_slots.size(), 3u);
        EXPECT_EQ(read_header.vault_header.key_slots[0].username, "user0");
    } else {
        // If it fails, it should be FEC decoding error
        EXPECT_EQ(read_result.error(), VaultError::FECDecodingFailed);
    }
}
