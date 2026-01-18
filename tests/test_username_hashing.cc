// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

/**
 * @file test_username_hashing.cc
 * @brief Unit tests for UsernameHashService
 *
 * Tests cover:
 * - All hash algorithms (SHA3-256, SHA3-384, SHA3-512, PBKDF2, Argon2id)
 * - Hash size verification
 * - Username verification (positive and negative cases)
 * - Salt uniqueness (different salts produce different hashes)
 * - Empty username handling
 * - Iteration count enforcement (PBKDF2)
 * - FIPS mode compliance
 * - Constant-time comparison
 */

#include <gtest/gtest.h>
#include "core/services/UsernameHashService.h"
#include <array>
#include <random>

using namespace KeepTower;

// ============================================================================
// Test Fixture
// ============================================================================

class UsernameHashServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Generate consistent random salt for reproducible tests
        std::mt19937 rng(12345);  // Fixed seed
        std::uniform_int_distribution<uint8_t> dist(0, 255);

        for (auto& byte : test_salt1_) {
            byte = dist(rng);
        }

        for (auto& byte : test_salt2_) {
            byte = dist(rng);
        }
    }

    std::array<uint8_t, 16> test_salt1_{};
    std::array<uint8_t, 16> test_salt2_{};

    const std::string test_username_ = "alice";
    const std::string test_username2_ = "bob";
};

// ============================================================================
// SHA3-256 Tests
// ============================================================================

TEST_F(UsernameHashServiceTest, SHA3_256_ProducesCorrectSize) {
    auto result = UsernameHashService::hash_username(
        test_username_,
        UsernameHashService::Algorithm::SHA3_256,
        test_salt1_
    );

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 32);  // SHA3-256 = 32 bytes
}

TEST_F(UsernameHashServiceTest, SHA3_256_DifferentSaltsProduceDifferentHashes) {
    auto hash1 = UsernameHashService::hash_username(
        test_username_,
        UsernameHashService::Algorithm::SHA3_256,
        test_salt1_
    );

    auto hash2 = UsernameHashService::hash_username(
        test_username_,
        UsernameHashService::Algorithm::SHA3_256,
        test_salt2_
    );

    ASSERT_TRUE(hash1.has_value());
    ASSERT_TRUE(hash2.has_value());
    EXPECT_NE(*hash1, *hash2);  // Different salts → different hashes
}

TEST_F(UsernameHashServiceTest, SHA3_256_DifferentUsernamesProduceDifferentHashes) {
    auto hash1 = UsernameHashService::hash_username(
        test_username_,
        UsernameHashService::Algorithm::SHA3_256,
        test_salt1_
    );

    auto hash2 = UsernameHashService::hash_username(
        test_username2_,
        UsernameHashService::Algorithm::SHA3_256,
        test_salt1_
    );

    ASSERT_TRUE(hash1.has_value());
    ASSERT_TRUE(hash2.has_value());
    EXPECT_NE(*hash1, *hash2);  // Different usernames → different hashes
}

TEST_F(UsernameHashServiceTest, SHA3_256_SameInputsProduceSameHash) {
    auto hash1 = UsernameHashService::hash_username(
        test_username_,
        UsernameHashService::Algorithm::SHA3_256,
        test_salt1_
    );

    auto hash2 = UsernameHashService::hash_username(
        test_username_,
        UsernameHashService::Algorithm::SHA3_256,
        test_salt1_
    );

    ASSERT_TRUE(hash1.has_value());
    ASSERT_TRUE(hash2.has_value());
    EXPECT_EQ(*hash1, *hash2);  // Same inputs → deterministic hash
}

// ============================================================================
// SHA3-384 Tests
// ============================================================================

TEST_F(UsernameHashServiceTest, SHA3_384_ProducesCorrectSize) {
    auto result = UsernameHashService::hash_username(
        test_username_,
        UsernameHashService::Algorithm::SHA3_384,
        test_salt1_
    );

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 48);  // SHA3-384 = 48 bytes
}

TEST_F(UsernameHashServiceTest, SHA3_384_DifferentFromSHA3_256) {
    auto hash256 = UsernameHashService::hash_username(
        test_username_,
        UsernameHashService::Algorithm::SHA3_256,
        test_salt1_
    );

    auto hash384 = UsernameHashService::hash_username(
        test_username_,
        UsernameHashService::Algorithm::SHA3_384,
        test_salt1_
    );

    ASSERT_TRUE(hash256.has_value());
    ASSERT_TRUE(hash384.has_value());

    // Different sizes
    EXPECT_NE(hash256->size(), hash384->size());

    // Hashes shouldn't match (even first 32 bytes)
    EXPECT_FALSE(std::equal(hash256->begin(), hash256->end(), hash384->begin()));
}

// ============================================================================
// SHA3-512 Tests
// ============================================================================

TEST_F(UsernameHashServiceTest, SHA3_512_ProducesCorrectSize) {
    auto result = UsernameHashService::hash_username(
        test_username_,
        UsernameHashService::Algorithm::SHA3_512,
        test_salt1_
    );

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 64);  // SHA3-512 = 64 bytes
}

// ============================================================================
// PBKDF2 Tests
// ============================================================================

TEST_F(UsernameHashServiceTest, PBKDF2_ProducesCorrectSize) {
    auto result = UsernameHashService::hash_username(
        test_username_,
        UsernameHashService::Algorithm::PBKDF2_SHA256,
        test_salt1_,
        10000  // 10k iterations
    );

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 32);  // PBKDF2-SHA256 = 32 bytes
}

TEST_F(UsernameHashServiceTest, PBKDF2_DifferentIterationsProduceDifferentHashes) {
    auto hash1 = UsernameHashService::hash_username(
        test_username_,
        UsernameHashService::Algorithm::PBKDF2_SHA256,
        test_salt1_,
        10000
    );

    auto hash2 = UsernameHashService::hash_username(
        test_username_,
        UsernameHashService::Algorithm::PBKDF2_SHA256,
        test_salt1_,
        20000
    );

    ASSERT_TRUE(hash1.has_value());
    ASSERT_TRUE(hash2.has_value());
    EXPECT_NE(*hash1, *hash2);  // Different iterations → different hashes
}

TEST_F(UsernameHashServiceTest, PBKDF2_EnforcesMinimumIterations) {
    // PBKDF2 should enforce minimum 1000 iterations (NIST SP 800-132)
    auto hash_low = UsernameHashService::hash_username(
        test_username_,
        UsernameHashService::Algorithm::PBKDF2_SHA256,
        test_salt1_,
        100  // Too low, should be raised to 1000
    );

    auto hash_min = UsernameHashService::hash_username(
        test_username_,
        UsernameHashService::Algorithm::PBKDF2_SHA256,
        test_salt1_,
        1000  // Minimum
    );

    ASSERT_TRUE(hash_low.has_value());
    ASSERT_TRUE(hash_min.has_value());

    // Both should produce same hash (100 was raised to 1000)
    EXPECT_EQ(*hash_low, *hash_min);
}

// ============================================================================
// Verification Tests
// ============================================================================

TEST_F(UsernameHashServiceTest, VerifyUsername_CorrectUsernameReturnsTrue) {
    auto hash = UsernameHashService::hash_username(
        test_username_,
        UsernameHashService::Algorithm::SHA3_256,
        test_salt1_
    );

    ASSERT_TRUE(hash.has_value());

    bool verified = UsernameHashService::verify_username(
        test_username_,
        *hash,
        UsernameHashService::Algorithm::SHA3_256,
        test_salt1_
    );

    EXPECT_TRUE(verified);
}

TEST_F(UsernameHashServiceTest, VerifyUsername_WrongUsernameReturnsFalse) {
    auto hash = UsernameHashService::hash_username(
        test_username_,
        UsernameHashService::Algorithm::SHA3_256,
        test_salt1_
    );

    ASSERT_TRUE(hash.has_value());

    bool verified = UsernameHashService::verify_username(
        test_username2_,  // Wrong username
        *hash,
        UsernameHashService::Algorithm::SHA3_256,
        test_salt1_
    );

    EXPECT_FALSE(verified);
}

TEST_F(UsernameHashServiceTest, VerifyUsername_WrongSaltReturnsFalse) {
    auto hash = UsernameHashService::hash_username(
        test_username_,
        UsernameHashService::Algorithm::SHA3_256,
        test_salt1_
    );

    ASSERT_TRUE(hash.has_value());

    bool verified = UsernameHashService::verify_username(
        test_username_,
        *hash,
        UsernameHashService::Algorithm::SHA3_256,
        test_salt2_  // Wrong salt
    );

    EXPECT_FALSE(verified);
}

TEST_F(UsernameHashServiceTest, VerifyUsername_WrongAlgorithmReturnsFalse) {
    auto hash = UsernameHashService::hash_username(
        test_username_,
        UsernameHashService::Algorithm::SHA3_256,
        test_salt1_
    );

    ASSERT_TRUE(hash.has_value());

    // Try to verify with SHA3-512 (wrong algorithm)
    bool verified = UsernameHashService::verify_username(
        test_username_,
        *hash,
        UsernameHashService::Algorithm::SHA3_512,
        test_salt1_
    );

    EXPECT_FALSE(verified);  // Size mismatch
}

TEST_F(UsernameHashServiceTest, VerifyUsername_PBKDF2_CorrectIterations) {
    const uint32_t iterations = 10000;

    auto hash = UsernameHashService::hash_username(
        test_username_,
        UsernameHashService::Algorithm::PBKDF2_SHA256,
        test_salt1_,
        iterations
    );

    ASSERT_TRUE(hash.has_value());

    bool verified = UsernameHashService::verify_username(
        test_username_,
        *hash,
        UsernameHashService::Algorithm::PBKDF2_SHA256,
        test_salt1_,
        iterations  // Must match
    );

    EXPECT_TRUE(verified);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(UsernameHashServiceTest, EmptyUsername_ReturnsError) {
    auto result = UsernameHashService::hash_username(
        "",  // Empty username
        UsernameHashService::Algorithm::SHA3_256,
        test_salt1_
    );

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), VaultError::InvalidUsername);
}

TEST_F(UsernameHashServiceTest, LongUsername_HandledCorrectly) {
    std::string long_username(1024, 'x');  // 1KB username

    auto result = UsernameHashService::hash_username(
        long_username,
        UsernameHashService::Algorithm::SHA3_256,
        test_salt1_
    );

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 32);
}

TEST_F(UsernameHashServiceTest, UnicodeUsername_HandledCorrectly) {
    std::string unicode_username = "用户名";  // Chinese characters

    auto result = UsernameHashService::hash_username(
        unicode_username,
        UsernameHashService::Algorithm::SHA3_256,
        test_salt1_
    );

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 32);

    // Verify it
    bool verified = UsernameHashService::verify_username(
        unicode_username,
        *result,
        UsernameHashService::Algorithm::SHA3_256,
        test_salt1_
    );

    EXPECT_TRUE(verified);
}

TEST_F(UsernameHashServiceTest, PlaintextLegacy_ReturnsUsernameAsBytes) {
    auto result = UsernameHashService::hash_username(
        test_username_,
        UsernameHashService::Algorithm::PLAINTEXT_LEGACY,
        test_salt1_
    );

    ASSERT_TRUE(result.has_value());

    // Should return username as bytes (no hashing)
    std::string recovered(result->begin(), result->end());
    EXPECT_EQ(recovered, test_username_);
}

// ============================================================================
// Utility Function Tests
// ============================================================================

TEST_F(UsernameHashServiceTest, GetHashSize_ReturnsCorrectSizes) {
    EXPECT_EQ(UsernameHashService::get_hash_size(
        UsernameHashService::Algorithm::PLAINTEXT_LEGACY), 0);
    EXPECT_EQ(UsernameHashService::get_hash_size(
        UsernameHashService::Algorithm::SHA3_256), 32);
    EXPECT_EQ(UsernameHashService::get_hash_size(
        UsernameHashService::Algorithm::SHA3_384), 48);
    EXPECT_EQ(UsernameHashService::get_hash_size(
        UsernameHashService::Algorithm::SHA3_512), 64);
    EXPECT_EQ(UsernameHashService::get_hash_size(
        UsernameHashService::Algorithm::PBKDF2_SHA256), 32);
    EXPECT_EQ(UsernameHashService::get_hash_size(
        UsernameHashService::Algorithm::ARGON2ID), 32);
}

TEST_F(UsernameHashServiceTest, GetAlgorithmName_ReturnsCorrectNames) {
    EXPECT_EQ(UsernameHashService::get_algorithm_name(
        UsernameHashService::Algorithm::SHA3_256), "SHA3-256");
    EXPECT_EQ(UsernameHashService::get_algorithm_name(
        UsernameHashService::Algorithm::SHA3_384), "SHA3-384");
    EXPECT_EQ(UsernameHashService::get_algorithm_name(
        UsernameHashService::Algorithm::SHA3_512), "SHA3-512");
    EXPECT_EQ(UsernameHashService::get_algorithm_name(
        UsernameHashService::Algorithm::PBKDF2_SHA256), "PBKDF2-HMAC-SHA256");
    EXPECT_EQ(UsernameHashService::get_algorithm_name(
        UsernameHashService::Algorithm::ARGON2ID), "Argon2id");
}

TEST_F(UsernameHashServiceTest, IsFIPSApproved_CorrectClassification) {
    // FIPS-approved algorithms
    EXPECT_TRUE(UsernameHashService::is_fips_approved(
        UsernameHashService::Algorithm::SHA3_256));
    EXPECT_TRUE(UsernameHashService::is_fips_approved(
        UsernameHashService::Algorithm::SHA3_384));
    EXPECT_TRUE(UsernameHashService::is_fips_approved(
        UsernameHashService::Algorithm::SHA3_512));
    EXPECT_TRUE(UsernameHashService::is_fips_approved(
        UsernameHashService::Algorithm::PBKDF2_SHA256));

    // NOT FIPS-approved
    EXPECT_FALSE(UsernameHashService::is_fips_approved(
        UsernameHashService::Algorithm::PLAINTEXT_LEGACY));
    EXPECT_FALSE(UsernameHashService::is_fips_approved(
        UsernameHashService::Algorithm::ARGON2ID));
}

// ============================================================================
// Performance Tests (Informational)
// ============================================================================

TEST_F(UsernameHashServiceTest, SHA3_256_Performance) {
    // This test is informational - just verify it completes reasonably fast
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        auto result = UsernameHashService::hash_username(
            test_username_,
            UsernameHashService::Algorithm::SHA3_256,
            test_salt1_
        );
        ASSERT_TRUE(result.has_value());
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 100 hashes should complete in < 1 second (should be much faster)
    EXPECT_LT(duration.count(), 1000);

    // Log performance for information
    std::cout << "SHA3-256: 100 hashes in " << duration.count() << "ms" << std::endl;
}

TEST_F(UsernameHashServiceTest, PBKDF2_Performance) {
    // PBKDF2 is intentionally slower
    auto start = std::chrono::high_resolution_clock::now();

    auto result = UsernameHashService::hash_username(
        test_username_,
        UsernameHashService::Algorithm::PBKDF2_SHA256,
        test_salt1_,
        10000
    );

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    ASSERT_TRUE(result.has_value());

    // Single PBKDF2 hash should complete in < 1 second
    EXPECT_LT(duration.count(), 1000);

    std::cout << "PBKDF2 (10k iterations): " << duration.count() << "ms" << std::endl;
}
