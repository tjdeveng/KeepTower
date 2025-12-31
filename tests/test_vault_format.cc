// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file test_vault_format.cc
 * @brief Unit tests for VaultFormat parsing and encoding
 *
 * Tests V1 vault format parsing, FEC detection, YubiKey metadata,
 * and format versioning.
 */

#include <gtest/gtest.h>
#include "../src/core/format/VaultFormat.h"
#include "../src/core/ReedSolomon.h"
#include <algorithm>

using namespace KeepTower;

// Constants for testing (match VaultFormat private constants)
static constexpr size_t SALT_LENGTH = 32;
static constexpr size_t IV_LENGTH = 12;
static constexpr size_t YUBIKEY_CHALLENGE_SIZE = 64;
static constexpr uint8_t FLAG_RS_ENABLED = 0x01;
static constexpr uint8_t FLAG_YUBIKEY_REQUIRED = 0x02;

// ============================================================================
// Test Fixture
// ============================================================================

class VaultFormatTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Sample salt and IV
        salt = std::vector<uint8_t>(SALT_LENGTH, 0xAB);
        iv = std::vector<uint8_t>(IV_LENGTH, 0xCD);

        // Sample ciphertext
        ciphertext = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

        // YubiKey test data
        yubikey_serial = "12345678";
        yubikey_challenge = std::vector<uint8_t>(YUBIKEY_CHALLENGE_SIZE, 0xEF);
    std::vector<uint8_t> create_basic_vault() {
        // Basic V1 format: salt + iv + ciphertext
        std::vector<uint8_t> data;
        data.insert(data.end(), salt.begin(), salt.end());
        data.insert(data.end(), iv.begin(), iv.end());
        data.insert(data.end(), ciphertext.begin(), ciphertext.end());
        return data;
    }

    std::vector<uint8_t> create_vault_with_flags(uint8_t flags) {
        // V1 format with flags byte: salt + iv + flags + ciphertext
        std::vector<uint8_t> data;
        data.insert(data.end(), salt.begin(), salt.end());
        data.insert(data.end(), iv.begin(), iv.end());
        data.push_back(flags);
        data.insert(data.end(), ciphertext.begin(), ciphertext.end());
        return data;
    }

    std::vector<uint8_t> salt;
    std::vector<uint8_t> iv;
    std::vector<uint8_t> ciphertext;
    std::string yubikey_serial;
    std::vector<uint8_t> yubikey_challenge;
};

// ============================================================================
// Basic Parsing Tests
// ============================================================================

TEST_F(VaultFormatTest, ParseBasicVault) {
    auto vault_data = create_basic_vault();

    auto result = VaultFormat::parse(vault_data);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->metadata.salt, salt);
    EXPECT_EQ(result->metadata.iv, iv);
    EXPECT_EQ(result->ciphertext, ciphertext);
    EXPECT_FALSE(result->metadata.has_fec);
    EXPECT_FALSE(result->metadata.requires_yubikey);
}

TEST_F(VaultFormatTest, ParseMinimumSizeVault) {
    // Minimum valid vault: 32 bytes salt + 12 bytes IV
    std::vector<uint8_t> min_vault;
    min_vault.insert(min_vault.end(), salt.begin(), salt.end());
    min_vault.insert(min_vault.end(), iv.begin(), iv.end());

    auto result = VaultFormat::parse(min_vault);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->ciphertext.size(), 0);  // No ciphertext
}

TEST_F(VaultFormatTest, ParseTooSmallVault) {
    // Vault smaller than minimum (< 44 bytes)
    std::vector<uint8_t> small_vault(40, 0xFF);

    auto result = VaultFormat::parse(small_vault);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), VaultError::CorruptedFile);
}

TEST_F(VaultFormatTest, ParseEmptyVault) {
    std::vector<uint8_t> empty_vault;

    auto result = VaultFormat::parse(empty_vault);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), VaultError::CorruptedFile);
}

TEST_F(VaultFormatTest, ParseExtractsSalt) {
    auto vault_data = create_basic_vault();

    // Modify salt to be distinctive
    std::fill(vault_data.begin(), vault_data.begin() + VaultFormat::SALT_LENGTH, 0x42);

    auto result = VaultFormat::parse(vault_data);

    ASSERT_TRUE(result.has_value());
    std::vector<uint8_t> expected_salt(VaultFormat::SALT_LENGTH, 0x42);
    EXPECT_EQ(result->metadata.salt, expected_salt);
}

TEST_F(VaultFormatTest, ParseExtractsIV) {
    auto vault_data = create_basic_vault();

    // Modify IV to be distinctive
    std::fill(vault_data.begin() + VaultFormat::SALT_LENGTH,
             vault_data.begin() + VaultFormat::SALT_LENGTH + VaultFormat::IV_LENGTH,
             0x88);

    auto result = VaultFormat::parse(vault_data);

    ASSERT_TRUE(result.has_value());
    std::vector<uint8_t> expected_iv(VaultFormat::IV_LENGTH, 0x88);
    EXPECT_EQ(result->metadata.iv, expected_iv);
}

// ============================================================================
// YubiKey Metadata Tests
// ============================================================================

TEST_F(VaultFormatTest, ParseVaultWithYubiKey) {
    std::vector<uint8_t> vault_data;
    vault_data.insert(vault_data.end(), salt.begin(), salt.end());
    vault_data.insert(vault_data.end(), iv.begin(), iv.end());
    vault_data.push_back(FLAG_YUBIKEY_REQUIRED);

    // Add YubiKey metadata
    vault_data.push_back(static_cast<uint8_t>(yubikey_serial.size()));
    vault_data.insert(vault_data.end(), yubikey_serial.begin(), yubikey_serial.end());
    vault_data.insert(vault_data.end(), yubikey_challenge.begin(), yubikey_challenge.end());

    vault_data.insert(vault_data.end(), ciphertext.begin(), ciphertext.end());

    auto result = VaultFormat::parse(vault_data);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->metadata.requires_yubikey);
    EXPECT_EQ(result->metadata.yubikey_serial, yubikey_serial);
    EXPECT_EQ(result->metadata.yubikey_challenge, yubikey_challenge);
    EXPECT_EQ(result->ciphertext, ciphertext);
}

TEST_F(VaultFormatTest, ParseYubiKeyInvalidSerialLength) {
    std::vector<uint8_t> vault_data;
    vault_data.insert(vault_data.end(), salt.begin(), salt.end());
    vault_data.insert(vault_data.end(), iv.begin(), iv.end());
    vault_data.push_back(FLAG_YUBIKEY_REQUIRED);

    // Invalid serial length (claims 100 bytes but not enough data)
    vault_data.push_back(100);
    vault_data.insert(vault_data.end(), yubikey_serial.begin(), yubikey_serial.end());

    auto result = VaultFormat::parse(vault_data);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), VaultError::CorruptedFile);
}

TEST_F(VaultFormatTest, ParseYubiKeyZeroSerialLength) {
    std::vector<uint8_t> vault_data;
    vault_data.insert(vault_data.end(), salt.begin(), salt.end());
    vault_data.insert(vault_data.end(), iv.begin(), iv.end());
    vault_data.push_back(FLAG_YUBIKEY_REQUIRED);

    // Zero serial length
    vault_data.push_back(0);

    auto result = VaultFormat::parse(vault_data);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), VaultError::CorruptedFile);
}

// ============================================================================
// Reed-Solomon FEC Tests
// ============================================================================

TEST_F(VaultFormatTest, ParseVaultWithFEC) {
    // Create vault with FEC header
    std::vector<uint8_t> vault_data;
    vault_data.insert(vault_data.end(), salt.begin(), salt.end());
    vault_data.insert(vault_data.end(), iv.begin(), iv.end());
    vault_data.push_back(FLAG_RS_ENABLED);
    vault_data.push_back(20);  // 20% redundancy

    // Original size (4 bytes, big-endian)
    uint32_t original_size = ciphertext.size();
    vault_data.push_back((original_size >> 24) & 0xFF);
    vault_data.push_back((original_size >> 16) & 0xFF);
    vault_data.push_back((original_size >> 8) & 0xFF);
    vault_data.push_back(original_size & 0xFF);

    // Encode ciphertext with Reed-Solomon
    ReedSolomon rs(20);
    auto encoded_result = rs.encode(ciphertext);
    ASSERT_TRUE(encoded_result.has_value());
    vault_data.insert(vault_data.end(), encoded_result->data.begin(), encoded_result->data.end());
    EXPECT_EQ(result->metadata.fec_redundancy, 20);
    EXPECT_EQ(result->ciphertext, ciphertext);  // Should be decoded
}

TEST_F(VaultFormatTest, ParseFECInvalidRedundancy) {
    std::vector<uint8_t> vault_data;
    vault_data.insert(vault_data.end(), salt.begin(), salt.end());
    vault_data.insert(vault_data.end(), iv.begin(), iv.end());
    vault_data.push_back(FLAG_RS_ENABLED);
    vault_data.push_back(5);  // Invalid: < MIN_RS_REDUNDANCY (10)

    uint32_t original_size = ciphertext.size();
    vault_data.push_back((original_size >> 24) & 0xFF);
    vault_data.push_back((original_size >> 16) & 0xFF);
    vault_data.push_back((original_size >> 8) & 0xFF);
    vault_data.push_back(original_size & 0xFF);

    vault_data.insert(vault_data.end(), ciphertext.begin(), ciphertext.end());

    auto result = VaultFormat::parse(vault_data);

    // Should fall back to legacy parsing (no FEC)
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->metadata.has_fec);
}

TEST_F(VaultFormatTest, ParseFECRedundancyTooHigh) {
    std::vector<uint8_t> vault_data;
    vault_data.insert(vault_data.end(), salt.begin(), salt.end());
    vault_data.insert(vault_data.end(), iv.begin(), iv.end());
    vault_data.push_back(FLAG_RS_ENABLED);
    vault_data.push_back(101);  // Invalid: > MAX_RS_REDUNDANCY (100)

    uint32_t original_size = ciphertext.size();
    vault_data.push_back((original_size >> 24) & 0xFF);
    vault_data.push_back((original_size >> 16) & 0xFF);
    vault_data.push_back((original_size >> 8) & 0xFF);
    vault_data.push_back(original_size & 0xFF);

    vault_data.insert(vault_data.end(), ciphertext.begin(), ciphertext.end());

    auto result = VaultFormat::parse(vault_data);

    // Should fall back to legacy parsing
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->metadata.has_fec);
}

TEST_F(VaultFormatTest, ParseFECInvalidOriginalSize) {
    std::vector<uint8_t> vault_data;
    vault_data.insert(vault_data.end(), salt.begin(), salt.end());
    vault_data.insert(vault_data.end(), iv.begin(), iv.end());
    vault_data.push_back(FLAG_RS_ENABLED);
    vault_data.push_back(20);

    // Original size larger than encoded size (impossible)
    uint32_t original_size = 1000000;
    vault_data.push_back((original_size >> 24) & 0xFF);
    vault_data.push_back((original_size >> 16) & 0xFF);
    vault_data.push_back((original_size >> 8) & 0xFF);
    vault_data.push_back(original_size & 0xFF);

    vault_data.insert(vault_data.end(), ciphertext.begin(), ciphertext.end());

    auto result = VaultFormat::parse(vault_data);

    // Should fall back to legacy parsing
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->metadata.has_fec);
}

TEST_F(VaultFormatTest, ParseFECWithYubiKey) {
    // Vault with both FEC and YubiKey
    std::vector<uint8_t> vault_data;
    vault_data.insert(vault_data.end(), salt.begin(), salt.end());
    vault_data.insert(vault_data.end(), iv.begin(), iv.end());
    vault_data.push_back(VaultFormat::FLAG_RS_ENABLED | VaultFormat::FLAG_YUBIKEY_REQUIRED);
    vault_data.push_back(20);  // 20% redundancy

    uint32_t original_size = ciphertext.size();
    vault_data.push_back((original_size >> 24) & 0xFF);
    vault_data.push_back((original_size >> 16) & 0xFF);
    vault_data.push_back((original_size >> 8) & 0xFF);
    vault_data.push_back(original_size & 0xFF);

    // YubiKey metadata (comes before encoded data)
    vault_data.push_back(static_cast<uint8_t>(yubikey_serial.size()));
    vault_data.insert(vault_data.end(), yubikey_serial.begin(), yubikey_serial.end());
    vault_data.insert(vault_data.end(), yubikey_challenge.begin(), yubikey_challenge.end());

    // Encoded ciphertext
    ReedSolomon rs(20);
    auto encoded_result = rs.encode(ciphertext);
    ASSERT_TRUE(encoded_result.has_value());
    vault_data.insert(vault_data.end(), encoded_result->data.begin(), encoded_result->data.end());

    auto result = VaultFormat::parse(vault_data);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->metadata.has_fec);
    EXPECT_TRUE(result->metadata.requires_yubikey);
    EXPECT_EQ(result->metadata.yubikey_serial, yubikey_serial);
    EXPECT_EQ(result->ciphertext, ciphertext);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(VaultFormatTest, ParseLargeCiphertext) {
    // Large ciphertext (1MB)
    std::vector<uint8_t> large_ciphertext(1024 * 1024, 0xAA);

    std::vector<uint8_t> vault_data;
    vault_data.insert(vault_data.end(), salt.begin(), salt.end());
    vault_data.insert(vault_data.end(), iv.begin(), iv.end());
    vault_data.insert(vault_data.end(), large_ciphertext.begin(), large_ciphertext.end());

    auto result = VaultFormat::parse(vault_data);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->ciphertext.size(), large_ciphertext.size());
}

TEST_F(VaultFormatTest, ParseZeroCiphertext) {
    // Vault with no ciphertext (only salt + IV)
    std::vector<uint8_t> vault_data;
    vault_data.insert(vault_data.end(), salt.begin(), salt.end());
    vault_data.insert(vault_data.end(), iv.begin(), iv.end());

    auto result = VaultFormat::parse(vault_data);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->ciphertext.size(), 0);
}

TEST_F(VaultFormatTest, ParseMultipleFlagsSet) {
    // Test with multiple flags
    uint8_t flags = FLAG_RS_ENABLED | FLAG_YUBIKEY_REQUIRED;

    std::vector<uint8_t> vault_data;
    vault_data.insert(vault_data.end(), salt.begin(), salt.end());
    vault_data.insert(vault_data.end(), iv.begin(), iv.end());
    vault_data.push_back(flags);
    vault_data.push_back(20);  // FEC redundancy

    uint32_t original_size = ciphertext.size();
    vault_data.push_back((original_size >> 24) & 0xFF);
    vault_data.push_back((original_size >> 16) & 0xFF);
    vault_data.push_back((original_size >> 8) & 0xFF);
    vault_data.push_back(original_size & 0xFF);

    // YubiKey metadata
    vault_data.push_back(static_cast<uint8_t>(yubikey_serial.size()));
    vault_data.insert(vault_data.end(), yubikey_serial.begin(), yubikey_serial.end());
    vault_data.insert(vault_data.end(), yubikey_challenge.begin(), yubikey_challenge.end());

    // FEC-encoded data
    ReedSolomon rs;
    auto encoded = rs.encode(ciphertext, 20);
    vault_data.insert(vault_data.end(), encoded.begin(), encoded.end());

    auto result = VaultFormat::parse(vault_data);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->metadata.has_fec);
    EXPECT_TRUE(result->metadata.requires_yubikey);
}

TEST_F(VaultFormatTest, ParseUnknownFlags) {
    // Test with unknown/future flags (should not break parsing)
    uint8_t unknown_flags = 0x80;  // High bit set (undefined)

    auto vault_data = create_vault_with_flags(unknown_flags);

    auto result = VaultFormat::parse(vault_data);

    // Should still parse successfully, treating as legacy
    ASSERT_TRUE(result.has_value());
}
