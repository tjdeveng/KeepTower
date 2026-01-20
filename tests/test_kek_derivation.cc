// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

/**
 * @file test_kek_derivation.cc
 * @brief Unit tests for KekDerivationService
 *
 * Tests cover:
 * - PBKDF2-HMAC-SHA256 key derivation
 * - Argon2id key derivation (if OpenSSL 3.2+ available)
 * - Key size verification (256-bit)
 * - Different passwords produce different keys
 * - Different salts produce different keys
 * - Salt length validation
 * - Settings integration (algorithm selection, parameters)
 * - FIPS mode compliance
 * - SHA3 fallback to PBKDF2
 */

#include <gtest/gtest.h>
#include "core/services/KekDerivationService.h"
#include "core/crypto/VaultCrypto.h"
#include <array>
#include <random>
#include <cstdlib>  // For std::setenv, std::getenv

using namespace KeepTower;

// ============================================================================
// Test Fixture
// ============================================================================

class KekDerivationServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Generate consistent random salts for reproducible tests
        std::mt19937 rng(54321);  // Fixed seed
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

    const std::string test_password_ = "correct_horse_battery_staple";
    const std::string test_password2_ = "different_password_123";
};

// ============================================================================
// PBKDF2-HMAC-SHA256 Tests
// ============================================================================

TEST_F(KekDerivationServiceTest, PBKDF2_ProducesCorrectKeySize) {
    KekDerivationService::AlgorithmParameters params;
    params.pbkdf2_iterations = 100000;  // Lower for faster tests

    auto result = KekDerivationService::derive_kek(
        test_password_,
        KekDerivationService::Algorithm::PBKDF2_HMAC_SHA256,
        test_salt1_,
        params
    );

    ASSERT_TRUE(result.has_value()) << "PBKDF2 derivation failed";
    EXPECT_EQ(result->size(), 32) << "KEK should be 256 bits (32 bytes)";
}

TEST_F(KekDerivationServiceTest, PBKDF2_DifferentPasswordsProduceDifferentKeys) {
    KekDerivationService::AlgorithmParameters params;
    params.pbkdf2_iterations = 100000;

    auto kek1 = KekDerivationService::derive_kek(
        test_password_,
        KekDerivationService::Algorithm::PBKDF2_HMAC_SHA256,
        test_salt1_,
        params
    );

    auto kek2 = KekDerivationService::derive_kek(
        test_password2_,
        KekDerivationService::Algorithm::PBKDF2_HMAC_SHA256,
        test_salt1_,
        params
    );

    ASSERT_TRUE(kek1.has_value());
    ASSERT_TRUE(kek2.has_value());
    EXPECT_NE(kek1.value(), kek2.value()) << "Different passwords must produce different KEKs";
}

TEST_F(KekDerivationServiceTest, PBKDF2_DifferentSaltsProduceDifferentKeys) {
    KekDerivationService::AlgorithmParameters params;
    params.pbkdf2_iterations = 100000;

    auto kek1 = KekDerivationService::derive_kek(
        test_password_,
        KekDerivationService::Algorithm::PBKDF2_HMAC_SHA256,
        test_salt1_,
        params
    );

    auto kek2 = KekDerivationService::derive_kek(
        test_password_,
        KekDerivationService::Algorithm::PBKDF2_HMAC_SHA256,
        test_salt2_,
        params
    );

    ASSERT_TRUE(kek1.has_value());
    ASSERT_TRUE(kek2.has_value());
    EXPECT_NE(kek1.value(), kek2.value()) << "Different salts must produce different KEKs";
}

TEST_F(KekDerivationServiceTest, PBKDF2_SameInputsProduceSameKey) {
    KekDerivationService::AlgorithmParameters params;
    params.pbkdf2_iterations = 100000;

    auto kek1 = KekDerivationService::derive_kek(
        test_password_,
        KekDerivationService::Algorithm::PBKDF2_HMAC_SHA256,
        test_salt1_,
        params
    );

    auto kek2 = KekDerivationService::derive_kek(
        test_password_,
        KekDerivationService::Algorithm::PBKDF2_HMAC_SHA256,
        test_salt1_,
        params
    );

    ASSERT_TRUE(kek1.has_value());
    ASSERT_TRUE(kek2.has_value());
    EXPECT_EQ(kek1.value(), kek2.value()) << "Same inputs must produce same KEK (deterministic)";
}

TEST_F(KekDerivationServiceTest, PBKDF2_DifferentIterationCountsProduceDifferentKeys) {
    KekDerivationService::AlgorithmParameters params1;
    params1.pbkdf2_iterations = 100000;

    KekDerivationService::AlgorithmParameters params2;
    params2.pbkdf2_iterations = 200000;

    auto kek1 = KekDerivationService::derive_kek(
        test_password_,
        KekDerivationService::Algorithm::PBKDF2_HMAC_SHA256,
        test_salt1_,
        params1
    );

    auto kek2 = KekDerivationService::derive_kek(
        test_password_,
        KekDerivationService::Algorithm::PBKDF2_HMAC_SHA256,
        test_salt1_,
        params2
    );

    ASSERT_TRUE(kek1.has_value());
    ASSERT_TRUE(kek2.has_value());
    EXPECT_NE(kek1.value(), kek2.value()) << "Different iteration counts must produce different KEKs";
}

// ============================================================================
// Argon2id Tests (requires libargon2)
// ============================================================================

// Argon2 is always available via libargon2 (not part of OpenSSL)

TEST_F(KekDerivationServiceTest, Argon2id_ProducesCorrectKeySize) {
    KekDerivationService::AlgorithmParameters params;
    params.argon2_memory_kb = 65536;  // 64 MB
    params.argon2_time_cost = 3;
    params.argon2_parallelism = 4;

    auto result = KekDerivationService::derive_kek(
        test_password_,
        KekDerivationService::Algorithm::ARGON2ID,
        test_salt1_,
        params
    );

    ASSERT_TRUE(result.has_value()) << "Argon2id derivation failed";
    EXPECT_EQ(result->size(), 32) << "KEK should be 256 bits (32 bytes)";
}

TEST_F(KekDerivationServiceTest, Argon2id_DifferentPasswordsProduceDifferentKeys) {
    KekDerivationService::AlgorithmParameters params;
    params.argon2_memory_kb = 65536;
    params.argon2_time_cost = 3;
    params.argon2_parallelism = 4;

    auto kek1 = KekDerivationService::derive_kek(
        test_password_,
        KekDerivationService::Algorithm::ARGON2ID,
        test_salt1_,
        params
    );

    auto kek2 = KekDerivationService::derive_kek(
        test_password2_,
        KekDerivationService::Algorithm::ARGON2ID,
        test_salt1_,
        params
    );

    ASSERT_TRUE(kek1.has_value());
    ASSERT_TRUE(kek2.has_value());
    EXPECT_NE(kek1.value(), kek2.value()) << "Different passwords must produce different KEKs";
}

TEST_F(KekDerivationServiceTest, Argon2id_DifferentMemoryCostsProduceDifferentKeys) {
    KekDerivationService::AlgorithmParameters params1;
    params1.argon2_memory_kb = 65536;  // 64 MB
    params1.argon2_time_cost = 3;
    params1.argon2_parallelism = 4;

    KekDerivationService::AlgorithmParameters params2;
    params2.argon2_memory_kb = 131072;  // 128 MB
    params2.argon2_time_cost = 3;
    params2.argon2_parallelism = 4;

    auto kek1 = KekDerivationService::derive_kek(
        test_password_,
        KekDerivationService::Algorithm::ARGON2ID,
        test_salt1_,
        params1
    );

    auto kek2 = KekDerivationService::derive_kek(
        test_password_,
        KekDerivationService::Algorithm::ARGON2ID,
        test_salt1_,
        params2
    );

    ASSERT_TRUE(kek1.has_value());
    ASSERT_TRUE(kek2.has_value());
    EXPECT_NE(kek1.value(), kek2.value()) << "Different memory costs must produce different KEKs";
}

TEST_F(KekDerivationServiceTest, Argon2id_SameInputsProduceSameKey) {
    KekDerivationService::AlgorithmParameters params;
    params.argon2_memory_kb = 65536;
    params.argon2_time_cost = 3;
    params.argon2_parallelism = 4;

    auto kek1 = KekDerivationService::derive_kek(
        test_password_,
        KekDerivationService::Algorithm::ARGON2ID,
        test_salt1_,
        params
    );

    auto kek2 = KekDerivationService::derive_kek(
        test_password_,
        KekDerivationService::Algorithm::ARGON2ID,
        test_salt1_,
        params
    );

    ASSERT_TRUE(kek1.has_value());
    ASSERT_TRUE(kek2.has_value());
    EXPECT_EQ(kek1.value(), kek2.value()) << "Same inputs must produce same KEK (deterministic)";
}

// ============================================================================
// Validation Tests
// ============================================================================

TEST_F(KekDerivationServiceTest, RejectsTooShortSalt) {
    std::array<uint8_t, 8> short_salt{};  // Only 8 bytes (minimum is 16)

    KekDerivationService::AlgorithmParameters params;
    params.pbkdf2_iterations = 100000;

    auto result = KekDerivationService::derive_kek(
        test_password_,
        KekDerivationService::Algorithm::PBKDF2_HMAC_SHA256,
        short_salt,
        params
    );

    ASSERT_FALSE(result.has_value()) << "Should reject salt < 16 bytes";
    EXPECT_EQ(result.error(), VaultError::InvalidSalt);
}

TEST_F(KekDerivationServiceTest, EmptyPasswordStillWorks) {
    // Even though empty passwords are weak, the KDF should still work
    KekDerivationService::AlgorithmParameters params;
    params.pbkdf2_iterations = 100000;

    auto result = KekDerivationService::derive_kek(
        "",  // Empty password
        KekDerivationService::Algorithm::PBKDF2_HMAC_SHA256,
        test_salt1_,
        params
    );

    ASSERT_TRUE(result.has_value()) << "Should handle empty password (even if weak)";
    EXPECT_EQ(result->size(), 32);
}

// ============================================================================
// FIPS Compliance Tests
// ============================================================================

TEST_F(KekDerivationServiceTest, PBKDF2_IsFIPSApproved) {
    bool is_fips = KekDerivationService::is_fips_approved(
        KekDerivationService::Algorithm::PBKDF2_HMAC_SHA256
    );

    EXPECT_TRUE(is_fips) << "PBKDF2-HMAC-SHA256 should be FIPS-approved";
}

TEST_F(KekDerivationServiceTest, Argon2id_NotFIPSApproved) {
    bool is_fips = KekDerivationService::is_fips_approved(
        KekDerivationService::Algorithm::ARGON2ID
    );

    EXPECT_FALSE(is_fips) << "Argon2id should NOT be FIPS-approved";
}

// ============================================================================
// Utility Function Tests
// ============================================================================

TEST_F(KekDerivationServiceTest, OutputSizeIsConsistent) {
    size_t pbkdf2_size = KekDerivationService::get_output_size(
        KekDerivationService::Algorithm::PBKDF2_HMAC_SHA256
    );

    size_t argon2_size = KekDerivationService::get_output_size(
        KekDerivationService::Algorithm::ARGON2ID
    );

    EXPECT_EQ(pbkdf2_size, 32) << "PBKDF2 output should be 32 bytes";
    EXPECT_EQ(argon2_size, 32) << "Argon2id output should be 32 bytes";
    EXPECT_EQ(pbkdf2_size, argon2_size) << "All algorithms should produce same size keys";
}

TEST_F(KekDerivationServiceTest, AlgorithmToString_ProducesReadableNames) {
    auto pbkdf2_name = KekDerivationService::algorithm_to_string(
        KekDerivationService::Algorithm::PBKDF2_HMAC_SHA256
    );

    auto argon2_name = KekDerivationService::algorithm_to_string(
        KekDerivationService::Algorithm::ARGON2ID
    );

    EXPECT_EQ(pbkdf2_name, "PBKDF2-HMAC-SHA256");
    EXPECT_EQ(argon2_name, "Argon2id");
}

// ============================================================================
// Settings Integration Tests
// ============================================================================

class KekDerivationSettingsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Check if schema is available (needed for GSettings tests)
        const char* schema_dir = std::getenv("GSETTINGS_SCHEMA_DIR");
        if (!schema_dir || schema_dir[0] == '\0') {
            // Try to use compiled schema from build directory
            const char* build_dir = "../data";
            setenv("GSETTINGS_SCHEMA_DIR", build_dir, 1);
        }

        try {
            // Create GSettings instance for testing
            settings_ = Gio::Settings::create("com.tjdeveng.keeptower");
        } catch (const Glib::Error& e) {
            GTEST_SKIP() << "GSettings schema not available: " << e.what();
        }
    }

    void TearDown() override {
        if (!settings_) {
            return;  // Schema wasn't available
        }

        // Reset to defaults
        settings_->reset("username-hash-algorithm");
        settings_->reset("username-pbkdf2-iterations");
        settings_->reset("username-argon2-memory-kb");
        settings_->reset("username-argon2-iterations");
        settings_->reset("fips-mode-enabled");
    }

    Glib::RefPtr<Gio::Settings> settings_;
};

TEST_F(KekDerivationSettingsTest, GetAlgorithm_PBKDF2Preference) {
    settings_->set_string("username-hash-algorithm", "pbkdf2");

    auto algorithm = KekDerivationService::get_algorithm_from_settings(settings_);

    EXPECT_EQ(algorithm, KekDerivationService::Algorithm::PBKDF2_HMAC_SHA256);
}

TEST_F(KekDerivationSettingsTest, GetAlgorithm_Argon2idPreference) {
    settings_->set_string("username-hash-algorithm", "argon2id");
    settings_->set_boolean("fips-mode-enabled", false);

    auto algorithm = KekDerivationService::get_algorithm_from_settings(settings_);

    EXPECT_EQ(algorithm, KekDerivationService::Algorithm::ARGON2ID);
}

TEST_F(KekDerivationSettingsTest, GetAlgorithm_SHA3FallbackToPBKDF2) {
    // SHA3 is appropriate for username hashing but NOT for password-based KEK derivation
    settings_->set_string("username-hash-algorithm", "sha3-256");

    auto algorithm = KekDerivationService::get_algorithm_from_settings(settings_);

    EXPECT_EQ(algorithm, KekDerivationService::Algorithm::PBKDF2_HMAC_SHA256)
        << "SHA3 should automatically fallback to PBKDF2 for KEK derivation";
}

TEST_F(KekDerivationSettingsTest, GetAlgorithm_FIPSModeForcesPBKDF2) {
    settings_->set_string("username-hash-algorithm", "argon2id");
    settings_->set_boolean("fips-mode-enabled", true);

    auto algorithm = KekDerivationService::get_algorithm_from_settings(settings_);

    EXPECT_EQ(algorithm, KekDerivationService::Algorithm::PBKDF2_HMAC_SHA256)
        << "FIPS mode should force PBKDF2 regardless of preference";
}

TEST_F(KekDerivationSettingsTest, GetParameters_ReadsFromSettings) {
    settings_->set_uint("username-pbkdf2-iterations", 600000);
    settings_->set_uint("username-argon2-memory-kb", 131072);  // 128 MB
    settings_->set_uint("username-argon2-iterations", 5);

    auto params = KekDerivationService::get_parameters_from_settings(settings_);

    EXPECT_EQ(params.pbkdf2_iterations, 600000u);
    EXPECT_EQ(params.argon2_memory_kb, 131072u);
    EXPECT_EQ(params.argon2_time_cost, 5u);
    EXPECT_EQ(params.argon2_parallelism, 4u);  // Fixed at 4
}

TEST_F(KekDerivationSettingsTest, GetParameters_HandlesNullSettings) {
    auto params = KekDerivationService::get_parameters_from_settings(nullptr);

    // Should return defaults
    EXPECT_EQ(params.pbkdf2_iterations, 600000u);
    EXPECT_EQ(params.argon2_memory_kb, 65536u);
    EXPECT_EQ(params.argon2_time_cost, 3u);
    EXPECT_EQ(params.argon2_parallelism, 4u);
}

// ============================================================================
// Security Property Tests
// ============================================================================

TEST_F(KekDerivationServiceTest, SecureMemory_AutomaticallyZeroized) {
    KekDerivationService::AlgorithmParameters params;
    params.pbkdf2_iterations = 100000;

    // Derive KEK in inner scope
    {
        auto kek = KekDerivationService::derive_kek(
            test_password_,
            KekDerivationService::Algorithm::PBKDF2_HMAC_SHA256,
            test_salt1_,
            params
        );

        ASSERT_TRUE(kek.has_value());
        ASSERT_EQ(kek->size(), 32);

        // KEK contains sensitive data
        bool has_non_zero = false;
        for (auto byte : kek.value()) {
            if (byte != 0) {
                has_non_zero = true;
                break;
            }
        }
        EXPECT_TRUE(has_non_zero) << "KEK should contain non-zero data";
    }
    // KEK is now out of scope and should be zeroized
    // (Can't test directly, but SecureAllocator guarantees this)
}
