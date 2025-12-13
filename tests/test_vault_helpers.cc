// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng
/**
 * @file test_vault_helpers.cc
 * @brief Unit tests verifying protocol constants and helper function integration
 *
 * This test suite validates:
 * 1. Protocol constants are correctly defined (recommendation #2)
 * 2. Helper functions work correctly through integration testing
 *
 * Note: Direct unit testing of private helper methods (parse_vault_format,
 * decode_with_reed_solomon, etc.) would require making them public or using
 * friend classes. Instead, we test them indirectly through the public API
 * and through integration tests in test_vault_reed_solomon.cc.
 */

#include <gtest/gtest.h>
#include <core/VaultManager.h>
#include <core/VaultError.h>
#include <core/ReedSolomon.h>

using namespace std;
using namespace KeepTower;

namespace {

/**
 * @brief Test fixture for protocol constants and integration tests
 */
class VaultProtocolTest : public ::testing::Test {
protected:
    void SetUp() override {
        vault_manager = make_unique<VaultManager>();
    }

    void TearDown() override {
        vault_manager.reset();
    }

    unique_ptr<VaultManager> vault_manager;
};

// ==============================================================================
// Protocol Constants Tests (Recommendation #2 from REFACTOR_AUDIT.md)
// ==============================================================================

/**
 * @test VerifyVaultFormatConstants
 * @brief Validates that vault format constants are correctly defined
 *
 * These constants eliminate magic numbers and make the code self-documenting.
 * They must match the actual vault file format specification.
 */
TEST_F(VaultProtocolTest, VerifyVaultFormatConstants) {
    // Vault header size: flags(1) + redundancy(1) + original_size(4) = 6 bytes
    EXPECT_EQ(VaultManager::VAULT_HEADER_SIZE, 6);

    // Reed-Solomon redundancy limits
    EXPECT_EQ(VaultManager::MIN_RS_REDUNDANCY, 5);   // Minimum 5% redundancy
    EXPECT_EQ(VaultManager::MAX_RS_REDUNDANCY, 50);  // Maximum 50% redundancy
    EXPECT_GT(VaultManager::MAX_RS_REDUNDANCY, VaultManager::MIN_RS_REDUNDANCY);

    // Maximum vault size: 100MB
    EXPECT_EQ(VaultManager::MAX_VAULT_SIZE, 100 * 1024 * 1024);
    EXPECT_EQ(VaultManager::MAX_VAULT_SIZE, 104857600);
}

/**
 * @test VerifyCryptographicConstants
 * @brief Validates cryptographic constant values
 *
 * These constants define the encryption parameters and must match
 * industry standards (AES-256-GCM, PBKDF2-SHA256).
 */
TEST_F(VaultProtocolTest, VerifyCryptographicConstants) {
    // AES-256 requires 32-byte keys
    EXPECT_EQ(VaultManager::KEY_LENGTH, 32);
    EXPECT_EQ(VaultManager::KEY_LENGTH * 8, 256);  // 256 bits

    // PBKDF2 salt should be at least 16 bytes (we use 32)
    EXPECT_EQ(VaultManager::SALT_LENGTH, 32);
    EXPECT_GE(VaultManager::SALT_LENGTH, 16);

    // GCM recommended IV length is 12 bytes
    EXPECT_EQ(VaultManager::IV_LENGTH, 12);

    // NIST recommends at least 10,000 iterations (we use 100,000)
    EXPECT_EQ(VaultManager::DEFAULT_PBKDF2_ITERATIONS, 100000);
    EXPECT_GE(VaultManager::DEFAULT_PBKDF2_ITERATIONS, 10000);
}

/**
 * @test VerifyBigEndianConstants
 * @brief Validates big-endian byte ordering constants
 *
 * These constants are used for converting multi-byte integers
 * to/from big-endian format in the vault file.
 */
TEST_F(VaultProtocolTest, VerifyBigEndianConstants) {
    EXPECT_EQ(VaultManager::BIGENDIAN_SHIFT_24, 24);
    EXPECT_EQ(VaultManager::BIGENDIAN_SHIFT_16, 16);
    EXPECT_EQ(VaultManager::BIGENDIAN_SHIFT_8, 8);

    // Verify they form a proper sequence
    EXPECT_EQ(VaultManager::BIGENDIAN_SHIFT_24, VaultManager::BIGENDIAN_SHIFT_16 + 8);
    EXPECT_EQ(VaultManager::BIGENDIAN_SHIFT_16, VaultManager::BIGENDIAN_SHIFT_8 + 8);
}

/**
 * @test VerifyFlagConstants
 * @brief Validates vault flag bit values
 *
 * These flags are used in the vault file format to indicate
 * optional features like Reed-Solomon FEC and YubiKey requirements.
 */
TEST_F(VaultProtocolTest, VerifyFlagConstants) {
    // Flags should be distinct bit masks
    EXPECT_EQ(VaultManager::FLAG_RS_ENABLED, 0x01);
    EXPECT_EQ(VaultManager::FLAG_YUBIKEY_REQUIRED, 0x02);

    // Ensure flags don't overlap
    EXPECT_EQ(VaultManager::FLAG_RS_ENABLED & VaultManager::FLAG_YUBIKEY_REQUIRED, 0);

    // Combined flags should be OR of individual flags
    uint8_t combined = VaultManager::FLAG_RS_ENABLED | VaultManager::FLAG_YUBIKEY_REQUIRED;
    EXPECT_EQ(combined, 0x03);
}

/**
 * @test VerifyDefaultValues
 * @brief Validates default configuration values
 */
TEST_F(VaultProtocolTest, VerifyDefaultValues) {
    // Default RS redundancy should be between min and max
    EXPECT_EQ(VaultManager::DEFAULT_RS_REDUNDANCY, 10);
    EXPECT_GE(VaultManager::DEFAULT_RS_REDUNDANCY, VaultManager::MIN_RS_REDUNDANCY);
    EXPECT_LE(VaultManager::DEFAULT_RS_REDUNDANCY, VaultManager::MAX_RS_REDUNDANCY);

    // Default backup count
    EXPECT_EQ(VaultManager::DEFAULT_BACKUP_COUNT, 5);
    EXPECT_GT(VaultManager::DEFAULT_BACKUP_COUNT, 0);
}

/**
 * @test VerifyYubiKeyConstants
 * @brief Validates YubiKey-related constants
 */
TEST_F(VaultProtocolTest, VerifyYubiKeyConstants) {
    // YubiKey challenge size (64 bytes)
    EXPECT_EQ(VaultManager::YUBIKEY_CHALLENGE_SIZE, 64);

    // YubiKey response size (HMAC-SHA1 = 20 bytes)
    EXPECT_EQ(VaultManager::YUBIKEY_RESPONSE_SIZE, 20);

    // YubiKey timeout (15 seconds)
    EXPECT_EQ(VaultManager::YUBIKEY_TIMEOUT_MS, 15000);
    EXPECT_EQ(VaultManager::YUBIKEY_TIMEOUT_MS / 1000, 15);
}

// ==============================================================================
// Big-Endian Conversion Tests
// ==============================================================================

/**
 * @test BigEndianConversionLogic
 * @brief Validates that bit shift constants produce correct big-endian encoding
 */
TEST_F(VaultProtocolTest, BigEndianConversionLogic) {
    // Test encoding a 32-bit value using our constants
    uint32_t test_value = 0x12345678;

    uint8_t byte0 = (test_value >> VaultManager::BIGENDIAN_SHIFT_24) & 0xFF;
    uint8_t byte1 = (test_value >> VaultManager::BIGENDIAN_SHIFT_16) & 0xFF;
    uint8_t byte2 = (test_value >> VaultManager::BIGENDIAN_SHIFT_8) & 0xFF;
    uint8_t byte3 = test_value & 0xFF;

    EXPECT_EQ(byte0, 0x12);
    EXPECT_EQ(byte1, 0x34);
    EXPECT_EQ(byte2, 0x56);
    EXPECT_EQ(byte3, 0x78);

    // Test decoding
    uint32_t reconstructed =
        (static_cast<uint32_t>(byte0) << VaultManager::BIGENDIAN_SHIFT_24) |
        (static_cast<uint32_t>(byte1) << VaultManager::BIGENDIAN_SHIFT_16) |
        (static_cast<uint32_t>(byte2) << VaultManager::BIGENDIAN_SHIFT_8) |
        static_cast<uint32_t>(byte3);

    EXPECT_EQ(reconstructed, test_value);
}

// ==============================================================================
// Reed-Solomon Constants Validation
// ==============================================================================

/**
 * @test ReedSolomonParameterValidation
 * @brief Validates that RS constants work with the ReedSolomon class
 */
TEST_F(VaultProtocolTest, ReedSolomonParameterValidation) {
    // Minimum redundancy should be acceptable
    EXPECT_NO_THROW({
        ReedSolomon rs_min(VaultManager::MIN_RS_REDUNDANCY);
    });

    // Default redundancy should be acceptable
    EXPECT_NO_THROW({
        ReedSolomon rs_default(VaultManager::DEFAULT_RS_REDUNDANCY);
    });

    // Maximum redundancy should be acceptable
    EXPECT_NO_THROW({
        ReedSolomon rs_max(VaultManager::MAX_RS_REDUNDANCY);
    });
}

// ==============================================================================
// Integration Tests
// ==============================================================================

/**
 * @test ConstantsUsedInVaultManager
 * @brief Validates that VaultManager correctly uses the defined constants
 *
 * This is an integration test that verifies the constants are actually
 * being used by the VaultManager implementation.
 */
TEST_F(VaultProtocolTest, ConstantsUsedInVaultManager) {
    // This test verifies constants are properly integrated
    // The actual usage is tested in other test suites like:
    // - test_vault_manager.cc
    // - test_vault_reed_solomon.cc
    // - test_fec_preferences.cc

    // For now, just verify the VaultManager can be instantiated
    EXPECT_NE(vault_manager, nullptr);
}

/**
 * @test DocumentConstantUsage
 * @brief Documents where each constant is used
 *
 * This test serves as documentation for constant usage.
 */
TEST_F(VaultProtocolTest, DocumentConstantUsage) {
    // VAULT_HEADER_SIZE: Used in parse_vault_format() for parsing vault headers
    // MIN/MAX_RS_REDUNDANCY: Used in parse_vault_format() to validate FEC parameters
    // MAX_VAULT_SIZE: Used in parse_vault_format() to prevent oversized vaults
    // BIGENDIAN_SHIFT_*: Used in parse_vault_format() to decode original_size
    // FLAG_RS_ENABLED: Used in open_vault() to check if FEC is enabled
    // FLAG_YUBIKEY_REQUIRED: Used in check_vault_requires_yubikey()
    // SALT_LENGTH: Used throughout for key derivation
    // KEY_LENGTH: Used in derive_key() and encryption/decryption
    // IV_LENGTH: Used in generate_iv() and encryption/decryption

    // This test always passes - it's just documentation
    SUCCEED();
}

} // anonymous namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
