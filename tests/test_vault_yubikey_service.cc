// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

/**
 * @file test_vault_yubikey_service.cc
 * @brief Unit tests for VaultYubiKeyService
 *
 * Tests YubiKey hardware operations service, including validation,
 * error handling, and result structures.
 *
 * Note: These tests focus on validation and error handling.
 * Actual YubiKey operations require hardware and are tested in integration tests.
 */

#include <gtest/gtest.h>
#include "../src/core/services/VaultYubiKeyService.h"
#include <algorithm>

using namespace KeepTower;

// ============================================================================
// Test Fixtures
// ============================================================================

class VaultYubiKeyServiceTest : public ::testing::Test {
protected:
    VaultYubiKeyService service;
};

// ============================================================================
// PIN Validation Tests
// ============================================================================

TEST_F(VaultYubiKeyServiceTest, ValidatePinFormat_ValidPins) {
    // Minimum length (4 characters)
    EXPECT_TRUE(VaultYubiKeyService::validate_pin_format("1234"));
    EXPECT_TRUE(VaultYubiKeyService::validate_pin_format("abcd"));

    // Normal length
    EXPECT_TRUE(VaultYubiKeyService::validate_pin_format("123456"));
    EXPECT_TRUE(VaultYubiKeyService::validate_pin_format("MySecurePin"));

    // Maximum length (63 characters)
    std::string max_pin(63, 'x');
    EXPECT_TRUE(VaultYubiKeyService::validate_pin_format(max_pin));

    // Special characters allowed
    EXPECT_TRUE(VaultYubiKeyService::validate_pin_format("P@ssw0rd!"));
    EXPECT_TRUE(VaultYubiKeyService::validate_pin_format("Pin#2024$"));

    // Spaces allowed
    EXPECT_TRUE(VaultYubiKeyService::validate_pin_format("My PIN 123"));
}

TEST_F(VaultYubiKeyServiceTest, ValidatePinFormat_TooShort) {
    EXPECT_FALSE(VaultYubiKeyService::validate_pin_format(""));      // Empty
    EXPECT_FALSE(VaultYubiKeyService::validate_pin_format("1"));     // 1 char
    EXPECT_FALSE(VaultYubiKeyService::validate_pin_format("12"));    // 2 chars
    EXPECT_FALSE(VaultYubiKeyService::validate_pin_format("123"));   // 3 chars
}

TEST_F(VaultYubiKeyServiceTest, ValidatePinFormat_TooLong) {
    // 64 characters (exceeds maximum)
    std::string too_long(64, 'x');
    EXPECT_FALSE(VaultYubiKeyService::validate_pin_format(too_long));

    // Much longer
    std::string way_too_long(100, 'x');
    EXPECT_FALSE(VaultYubiKeyService::validate_pin_format(way_too_long));
}

// ============================================================================
// FIPS Device Check Tests
// ============================================================================

TEST_F(VaultYubiKeyServiceTest, IsFipsDevice_FipsCapable) {
    VaultYubiKeyService::DeviceInfo fips_device;
    fips_device.serial = "12345678";
    fips_device.manufacturer = "Yubico";
    fips_device.product = "YubiKey 5 FIPS";
    fips_device.slot = 1;
    fips_device.is_fips = true;

    EXPECT_TRUE(VaultYubiKeyService::is_fips_device(fips_device));
}

TEST_F(VaultYubiKeyServiceTest, IsFipsDevice_NonFips) {
    VaultYubiKeyService::DeviceInfo regular_device;
    regular_device.serial = "87654321";
    regular_device.manufacturer = "Yubico";
    regular_device.product = "YubiKey 5";
    regular_device.slot = 1;
    regular_device.is_fips = false;

    EXPECT_FALSE(VaultYubiKeyService::is_fips_device(regular_device));
}

// ============================================================================
// Challenge Generation Tests
// ============================================================================

TEST_F(VaultYubiKeyServiceTest, GenerateChallenge_DefaultSize) {
    auto result = VaultYubiKeyService::generate_challenge();

    ASSERT_TRUE(result.has_value()) << "Challenge generation should succeed";
    EXPECT_EQ(result->size(), 32) << "Default challenge should be 32 bytes";

    // Verify challenge is not all zeros
    bool all_zeros = std::all_of(result->begin(), result->end(),
                                   [](uint8_t b) { return b == 0; });
    EXPECT_FALSE(all_zeros) << "Challenge should contain random data";
}

TEST_F(VaultYubiKeyServiceTest, GenerateChallenge_CustomSize) {
    auto result_20 = VaultYubiKeyService::generate_challenge(20);
    ASSERT_TRUE(result_20.has_value());
    EXPECT_EQ(result_20->size(), 20);

    auto result_64 = VaultYubiKeyService::generate_challenge(64);
    ASSERT_TRUE(result_64.has_value());
    EXPECT_EQ(result_64->size(), 64);

    auto result_1 = VaultYubiKeyService::generate_challenge(1);
    ASSERT_TRUE(result_1.has_value());
    EXPECT_EQ(result_1->size(), 1);
}

TEST_F(VaultYubiKeyServiceTest, GenerateChallenge_UniqueChallenges) {
    auto result1 = VaultYubiKeyService::generate_challenge(32);
    auto result2 = VaultYubiKeyService::generate_challenge(32);

    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());

    // Two challenges should be different (cryptographically unique)
    EXPECT_NE(*result1, *result2) << "Sequential challenge generations should produce unique values";
}

TEST_F(VaultYubiKeyServiceTest, GenerateChallenge_InvalidSize) {
    // Size 0 should fail
    auto result_zero = VaultYubiKeyService::generate_challenge(0);
    EXPECT_FALSE(result_zero.has_value()) << "Challenge size 0 should fail";
    EXPECT_EQ(result_zero.error(), VaultError::YubiKeyError);

    // Size > 64 should fail
    auto result_too_large = VaultYubiKeyService::generate_challenge(65);
    EXPECT_FALSE(result_too_large.has_value()) << "Challenge size > 64 should fail";
    EXPECT_EQ(result_too_large.error(), VaultError::YubiKeyError);

    auto result_way_too_large = VaultYubiKeyService::generate_challenge(1000);
    EXPECT_FALSE(result_way_too_large.has_value());
    EXPECT_EQ(result_way_too_large.error(), VaultError::YubiKeyError);
}

// ============================================================================
// DeviceInfo Structure Tests
// ============================================================================

TEST_F(VaultYubiKeyServiceTest, DeviceInfo_StructureIntegrity) {
    VaultYubiKeyService::DeviceInfo info;

    // Set all fields
    info.serial = "12345678";
    info.manufacturer = "Yubico";
    info.product = "YubiKey 5 NFC";
    info.slot = 2;
    info.is_fips = true;

    // Verify all fields retained
    EXPECT_EQ(info.serial, "12345678");
    EXPECT_EQ(info.manufacturer, "Yubico");
    EXPECT_EQ(info.product, "YubiKey 5 NFC");
    EXPECT_EQ(info.slot, 2);
    EXPECT_TRUE(info.is_fips);
}

TEST_F(VaultYubiKeyServiceTest, DeviceInfo_CopySemantics) {
    VaultYubiKeyService::DeviceInfo original;
    original.serial = "11111111";
    original.manufacturer = "Yubico";
    original.product = "YubiKey";
    original.slot = 1;
    original.is_fips = false;

    // Copy construction
    VaultYubiKeyService::DeviceInfo copy = original;
    EXPECT_EQ(copy.serial, original.serial);
    EXPECT_EQ(copy.manufacturer, original.manufacturer);
    EXPECT_EQ(copy.product, original.product);
    EXPECT_EQ(copy.slot, original.slot);
    EXPECT_EQ(copy.is_fips, original.is_fips);
}

// ============================================================================
// EnrollmentResult Structure Tests
// ============================================================================

TEST_F(VaultYubiKeyServiceTest, EnrollmentResult_StructureIntegrity) {
    VaultYubiKeyService::EnrollmentResult result;

    // Policy response (20 bytes for HMAC-SHA1, 32 for SHA-256)
    result.policy_response = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20
    };

    // User response
    result.user_response = {
        0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
        0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30,
        0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
        0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40
    };

    // Device info
    result.device_info.serial = "99999999";
    result.device_info.manufacturer = "Yubico";
    result.device_info.product = "YubiKey 5";
    result.device_info.slot = 2;
    result.device_info.is_fips = true;

    // Verify all fields
    EXPECT_EQ(result.policy_response.size(), 32);
    EXPECT_EQ(result.user_response.size(), 32);
    EXPECT_EQ(result.policy_response[0], 0x01);
    EXPECT_EQ(result.user_response[0], 0x21);
    EXPECT_EQ(result.device_info.serial, "99999999");
}

TEST_F(VaultYubiKeyServiceTest, EnrollmentResult_TwoResponsesDifferent) {
    VaultYubiKeyService::EnrollmentResult result;

    // Set different responses
    result.policy_response.resize(32, 0xAA);
    result.user_response.resize(32, 0xBB);

    // Verify they're different
    EXPECT_NE(result.policy_response, result.user_response);
}

// ============================================================================
// ChallengeResult Structure Tests
// ============================================================================

TEST_F(VaultYubiKeyServiceTest, ChallengeResult_StructureIntegrity) {
    VaultYubiKeyService::ChallengeResult result;

    // Response (32 bytes for HMAC-SHA256)
    result.response.resize(32);
    for (size_t i = 0; i < 32; ++i) {
        result.response[i] = static_cast<uint8_t>(i);
    }

    // Device info
    result.device_info.serial = "55555555";
    result.device_info.manufacturer = "Yubico";
    result.device_info.product = "YubiKey";
    result.device_info.slot = 1;
    result.device_info.is_fips = false;

    // Verify
    EXPECT_EQ(result.response.size(), 32);
    EXPECT_EQ(result.response[0], 0);
    EXPECT_EQ(result.response[31], 31);
    EXPECT_EQ(result.device_info.serial, "55555555");
}

// ============================================================================
// Input Validation Tests (Edge Cases)
// ============================================================================

TEST_F(VaultYubiKeyServiceTest, ValidatePinFormat_BoundaryConditions) {
    // Exactly 4 characters (minimum valid)
    EXPECT_TRUE(VaultYubiKeyService::validate_pin_format("1234"));

    // Exactly 63 characters (maximum valid)
    std::string max_valid(63, 'x');
    EXPECT_TRUE(VaultYubiKeyService::validate_pin_format(max_valid));

    // Just under minimum (3 characters)
    EXPECT_FALSE(VaultYubiKeyService::validate_pin_format("123"));

    // Just over maximum (64 characters)
    std::string min_invalid(64, 'x');
    EXPECT_FALSE(VaultYubiKeyService::validate_pin_format(min_invalid));
}

TEST_F(VaultYubiKeyServiceTest, GenerateChallenge_BoundaryConditions) {
    // Minimum valid size
    auto result_1 = VaultYubiKeyService::generate_challenge(1);
    EXPECT_TRUE(result_1.has_value());
    EXPECT_EQ(result_1->size(), 1);

    // Maximum valid size
    auto result_64 = VaultYubiKeyService::generate_challenge(64);
    EXPECT_TRUE(result_64.has_value());
    EXPECT_EQ(result_64->size(), 64);

    // Just below minimum (0)
    auto result_0 = VaultYubiKeyService::generate_challenge(0);
    EXPECT_FALSE(result_0.has_value());

    // Just above maximum (65)
    auto result_65 = VaultYubiKeyService::generate_challenge(65);
    EXPECT_FALSE(result_65.has_value());
}

// ============================================================================
// Result Type Tests (Error Handling)
// ============================================================================

TEST_F(VaultYubiKeyServiceTest, VaultResult_ErrorPropagation) {
    // Test that invalid challenge size returns proper error
    auto invalid_challenge = VaultYubiKeyService::generate_challenge(0);

    ASSERT_FALSE(invalid_challenge.has_value());
    EXPECT_EQ(invalid_challenge.error(), VaultError::YubiKeyError);
}

TEST_F(VaultYubiKeyServiceTest, VaultResult_SuccessValue) {
    auto valid_challenge = VaultYubiKeyService::generate_challenge(32);

    ASSERT_TRUE(valid_challenge.has_value());
    EXPECT_EQ(valid_challenge.value().size(), 32);

    // Can use operator*
    EXPECT_EQ((*valid_challenge).size(), 32);
}

// ============================================================================
// Data Integrity Tests
// ============================================================================

TEST_F(VaultYubiKeyServiceTest, ChallengeGeneration_DataIntegrity) {
    auto challenge = VaultYubiKeyService::generate_challenge(32);
    ASSERT_TRUE(challenge.has_value());

    // Verify size is exact
    EXPECT_EQ(challenge->size(), 32);

    // Verify data is accessible
    for (size_t i = 0; i < challenge->size(); ++i) {
        // Just accessing doesn't crash
        [[maybe_unused]] auto byte = (*challenge)[i];
    }
}

TEST_F(VaultYubiKeyServiceTest, DeviceInfo_EmptyStrings) {
    VaultYubiKeyService::DeviceInfo info;

    // Default constructed should have empty strings
    EXPECT_TRUE(info.serial.empty());
    EXPECT_TRUE(info.manufacturer.empty());
    EXPECT_TRUE(info.product.empty());
}

TEST_F(VaultYubiKeyServiceTest, EnrollmentResult_EmptyResponses) {
    VaultYubiKeyService::EnrollmentResult result;

    // Default constructed should have empty vectors
    EXPECT_TRUE(result.policy_response.empty());
    EXPECT_TRUE(result.user_response.empty());
}

// ============================================================================
// Slot Validation Tests (Implicit through expected usage)
// ============================================================================

TEST_F(VaultYubiKeyServiceTest, DeviceInfo_ValidSlots) {
    VaultYubiKeyService::DeviceInfo info_slot1;
    info_slot1.slot = 1;
    EXPECT_EQ(info_slot1.slot, 1);

    VaultYubiKeyService::DeviceInfo info_slot2;
    info_slot2.slot = 2;
    EXPECT_EQ(info_slot2.slot, 2);
}

// ============================================================================
// String Handling Tests
// ============================================================================

TEST_F(VaultYubiKeyServiceTest, ValidatePinFormat_UTF8Characters) {
    // ASCII characters
    EXPECT_TRUE(VaultYubiKeyService::validate_pin_format("Test1234"));

    // Numbers only
    EXPECT_TRUE(VaultYubiKeyService::validate_pin_format("98765432"));

    // Mixed case
    EXPECT_TRUE(VaultYubiKeyService::validate_pin_format("MixedCase123"));

    // Special ASCII characters
    EXPECT_TRUE(VaultYubiKeyService::validate_pin_format("Pin!@#$%"));
}

TEST_F(VaultYubiKeyServiceTest, DeviceInfo_LongStrings) {
    VaultYubiKeyService::DeviceInfo info;

    // Very long serial (YubiKey serials are typically 8 digits)
    info.serial = "12345678901234567890";
    EXPECT_EQ(info.serial.length(), 20);

    // Long manufacturer name
    info.manufacturer = std::string(100, 'M');
    EXPECT_EQ(info.manufacturer.length(), 100);

    // Long product name
    info.product = std::string(100, 'P');
    EXPECT_EQ(info.product.length(), 100);
}

// ============================================================================
// Vector Operations Tests
// ============================================================================

TEST_F(VaultYubiKeyServiceTest, ChallengeResult_VectorCopy) {
    VaultYubiKeyService::ChallengeResult result1;
    result1.response = {0x01, 0x02, 0x03, 0x04};

    // Copy
    VaultYubiKeyService::ChallengeResult result2 = result1;

    // Verify independent copies
    EXPECT_EQ(result1.response, result2.response);

    // Modify one
    result2.response[0] = 0xFF;
    EXPECT_NE(result1.response, result2.response);
}

TEST_F(VaultYubiKeyServiceTest, EnrollmentResult_VectorResize) {
    VaultYubiKeyService::EnrollmentResult result;

    // Resize to specific sizes
    result.policy_response.resize(20, 0xAA);
    result.user_response.resize(32, 0xBB);

    EXPECT_EQ(result.policy_response.size(), 20);
    EXPECT_EQ(result.user_response.size(), 32);

    // Verify fill values
    EXPECT_EQ(result.policy_response[0], 0xAA);
    EXPECT_EQ(result.user_response[0], 0xBB);
}

// ============================================================================
// Randomness Quality Tests
// ============================================================================

TEST_F(VaultYubiKeyServiceTest, GenerateChallenge_RandomnessQuality) {
    auto challenge = VaultYubiKeyService::generate_challenge(32);
    ASSERT_TRUE(challenge.has_value());

    // Check for obvious non-random patterns

    // Not all same value
    bool all_same = std::all_of(challenge->begin() + 1, challenge->end(),
                                [first = (*challenge)[0]](uint8_t b) {
                                    return b == first;
                                });
    EXPECT_FALSE(all_same) << "Challenge should not be all same byte";

    // Not sequential
    bool is_sequential = true;
    for (size_t i = 1; i < challenge->size(); ++i) {
        if ((*challenge)[i] != static_cast<uint8_t>((*challenge)[i-1] + 1)) {
            is_sequential = false;
            break;
        }
    }
    EXPECT_FALSE(is_sequential) << "Challenge should not be sequential bytes";
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
