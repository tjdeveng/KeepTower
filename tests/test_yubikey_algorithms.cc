// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include <gtest/gtest.h>
#include "managers/YubiKeyAlgorithm.h"
#include <cstdint>

/**
 * @brief Test suite for YubiKey algorithm specifications
 *
 * Tests FIPS-140-3 compliance framework for YubiKey HMAC algorithms.
 * Verifies algorithm properties, helper functions, and FIPS enforcement.
 */
class YubiKeyAlgorithmTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// Test Suite 1: Algorithm Response Sizes
// ============================================================================

// SHA-1 removed for FIPS-140-3 compliance

TEST_F(YubiKeyAlgorithmTest, ResponseSize_SHA256_Is32Bytes) {
    EXPECT_EQ(yubikey_algorithm_response_size(YubiKeyAlgorithm::HMAC_SHA256), 32);
}

TEST_F(YubiKeyAlgorithmTest, ResponseSize_SHA512_Is64Bytes) {
    EXPECT_EQ(yubikey_algorithm_response_size(YubiKeyAlgorithm::HMAC_SHA512), 64);
}

TEST_F(YubiKeyAlgorithmTest, ResponseSize_SHA3_256_Is32Bytes) {
    EXPECT_EQ(yubikey_algorithm_response_size(YubiKeyAlgorithm::HMAC_SHA3_256), 32);
}

TEST_F(YubiKeyAlgorithmTest, ResponseSize_SHA3_512_Is64Bytes) {
    EXPECT_EQ(yubikey_algorithm_response_size(YubiKeyAlgorithm::HMAC_SHA3_512), 64);
}

TEST_F(YubiKeyAlgorithmTest, ResponseSize_InvalidAlgorithm_ReturnsZero) {
    auto invalid = static_cast<YubiKeyAlgorithm>(0xFF);
    EXPECT_EQ(yubikey_algorithm_response_size(invalid), 0);
}

// ============================================================================
// Test Suite 2: Algorithm Names
// ============================================================================

// SHA-1 removed for FIPS-140-3 compliance

TEST_F(YubiKeyAlgorithmTest, AlgorithmName_SHA256_IsCorrect) {
    EXPECT_EQ(yubikey_algorithm_name(YubiKeyAlgorithm::HMAC_SHA256), "HMAC-SHA256");
}

TEST_F(YubiKeyAlgorithmTest, AlgorithmName_SHA512_IsCorrect) {
    EXPECT_EQ(yubikey_algorithm_name(YubiKeyAlgorithm::HMAC_SHA512), "HMAC-SHA512");
}

TEST_F(YubiKeyAlgorithmTest, AlgorithmName_SHA3_256_IsCorrect) {
    EXPECT_EQ(yubikey_algorithm_name(YubiKeyAlgorithm::HMAC_SHA3_256), "HMAC-SHA3-256");
}

TEST_F(YubiKeyAlgorithmTest, AlgorithmName_SHA3_512_IsCorrect) {
    EXPECT_EQ(yubikey_algorithm_name(YubiKeyAlgorithm::HMAC_SHA3_512), "HMAC-SHA3-512");
}

TEST_F(YubiKeyAlgorithmTest, AlgorithmName_InvalidAlgorithm_ReturnsUnknown) {
    auto invalid = static_cast<YubiKeyAlgorithm>(0xFF);
    EXPECT_EQ(yubikey_algorithm_name(invalid), "Unknown");
}

// ============================================================================
// Test Suite 3: FIPS-140-3 Compliance
// ============================================================================

// SHA-1 removed for FIPS-140-3 compliance

TEST_F(YubiKeyAlgorithmTest, FIPS_SHA256_IsApproved) {
    // SHA-256 is FIPS-140-3 approved per NIST SP 800-140B
    EXPECT_TRUE(yubikey_algorithm_is_fips_approved(YubiKeyAlgorithm::HMAC_SHA256));
}

TEST_F(YubiKeyAlgorithmTest, FIPS_SHA512_IsApproved) {
    // SHA-512 is FIPS-140-3 approved per NIST SP 800-140B
    EXPECT_TRUE(yubikey_algorithm_is_fips_approved(YubiKeyAlgorithm::HMAC_SHA512));
}

TEST_F(YubiKeyAlgorithmTest, FIPS_SHA3_256_IsApproved) {
    // SHA3-256 is FIPS-140-3 approved (FIPS 202)
    EXPECT_TRUE(yubikey_algorithm_is_fips_approved(YubiKeyAlgorithm::HMAC_SHA3_256));
}

TEST_F(YubiKeyAlgorithmTest, FIPS_SHA3_512_IsApproved) {
    // SHA3-512 is FIPS-140-3 approved (FIPS 202)
    EXPECT_TRUE(yubikey_algorithm_is_fips_approved(YubiKeyAlgorithm::HMAC_SHA3_512));
}

TEST_F(YubiKeyAlgorithmTest, FIPS_InvalidAlgorithm_IsNotApproved) {
    auto invalid = static_cast<YubiKeyAlgorithm>(0xFF);
    EXPECT_FALSE(yubikey_algorithm_is_fips_approved(invalid));
}

// ============================================================================
// Test Suite 4: Default Algorithms
// ============================================================================

TEST_F(YubiKeyAlgorithmTest, FIPSDefault_IsSHA256) {
    // Default FIPS algorithm should be SHA-256 (widely supported)
    EXPECT_EQ(yubikey_algorithm_fips_default(), YubiKeyAlgorithm::HMAC_SHA256);
}

TEST_F(YubiKeyAlgorithmTest, FIPSDefault_IsApproved) {
    // Verify the default is actually FIPS-approved
    EXPECT_TRUE(yubikey_algorithm_is_fips_approved(yubikey_algorithm_fips_default()));
}

// Legacy algorithm support removed for FIPS-140-3 compliance

// ============================================================================
// Test Suite 5: Constants
// ============================================================================

TEST_F(YubiKeyAlgorithmTest, MaxResponseSize_Is64Bytes) {
    // Maximum response size should accommodate SHA-512 and SHA3-512
    EXPECT_EQ(YUBIKEY_MAX_RESPONSE_SIZE, 64);
}

TEST_F(YubiKeyAlgorithmTest, ChallengeSize_Is64Bytes) {
    // Challenge size is fixed at 64 bytes for all algorithms
    EXPECT_EQ(YUBIKEY_CHALLENGE_SIZE, 64);
}

TEST_F(YubiKeyAlgorithmTest, MaxResponseSize_CoversAllAlgorithms) {
    // Verify max size is sufficient for all FIPS-approved algorithms
    EXPECT_LE(yubikey_algorithm_response_size(YubiKeyAlgorithm::HMAC_SHA256), YUBIKEY_MAX_RESPONSE_SIZE);
    EXPECT_LE(yubikey_algorithm_response_size(YubiKeyAlgorithm::HMAC_SHA512), YUBIKEY_MAX_RESPONSE_SIZE);
    EXPECT_LE(yubikey_algorithm_response_size(YubiKeyAlgorithm::HMAC_SHA3_256), YUBIKEY_MAX_RESPONSE_SIZE);
    EXPECT_LE(yubikey_algorithm_response_size(YubiKeyAlgorithm::HMAC_SHA3_512), YUBIKEY_MAX_RESPONSE_SIZE);
}

// ============================================================================
// Test Suite 6: Enum Value Mapping
// ============================================================================

// SHA-1 (0x01) removed for FIPS-140-3 compliance - SHA-256 is minimum

TEST_F(YubiKeyAlgorithmTest, EnumValue_SHA256_Is0x02) {
    EXPECT_EQ(static_cast<uint8_t>(YubiKeyAlgorithm::HMAC_SHA256), 0x02);
}

TEST_F(YubiKeyAlgorithmTest, EnumValue_SHA512_Is0x03) {
    EXPECT_EQ(static_cast<uint8_t>(YubiKeyAlgorithm::HMAC_SHA512), 0x03);
}

TEST_F(YubiKeyAlgorithmTest, EnumValue_SHA3_256_Is0x10) {
    EXPECT_EQ(static_cast<uint8_t>(YubiKeyAlgorithm::HMAC_SHA3_256), 0x10);
}

TEST_F(YubiKeyAlgorithmTest, EnumValue_SHA3_512_Is0x11) {
    EXPECT_EQ(static_cast<uint8_t>(YubiKeyAlgorithm::HMAC_SHA3_512), 0x11);
}

// ============================================================================
// Test Suite 7: Round-Trip Casting
// ============================================================================

// SHA-1 round-trip test removed for FIPS-140-3 compliance

TEST_F(YubiKeyAlgorithmTest, RoundTrip_SHA256_Preserves_Value) {
    auto value = static_cast<uint8_t>(YubiKeyAlgorithm::HMAC_SHA256);
    auto algorithm = static_cast<YubiKeyAlgorithm>(value);
    EXPECT_EQ(algorithm, YubiKeyAlgorithm::HMAC_SHA256);
    EXPECT_EQ(yubikey_algorithm_response_size(algorithm), 32);
}

// ============================================================================
// Test Suite 8: Constexpr Evaluation
// ============================================================================

TEST_F(YubiKeyAlgorithmTest, ResponseSize_IsConstexpr) {
    // Verify functions are actually constexpr by using in constant expression
    constexpr size_t size = yubikey_algorithm_response_size(YubiKeyAlgorithm::HMAC_SHA256);
    EXPECT_EQ(size, 32);
}

TEST_F(YubiKeyAlgorithmTest, FIPSApproval_IsConstexpr) {
    // Verify FIPS check is constexpr
    constexpr bool approved = yubikey_algorithm_is_fips_approved(YubiKeyAlgorithm::HMAC_SHA256);
    EXPECT_TRUE(approved);
}

TEST_F(YubiKeyAlgorithmTest, AlgorithmName_IsConstexpr) {
    // Verify name lookup is constexpr
    constexpr std::string_view name = yubikey_algorithm_name(YubiKeyAlgorithm::HMAC_SHA256);
    EXPECT_EQ(name, "HMAC-SHA256");
}

// Main entry point
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
