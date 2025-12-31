// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file test_key_wrapping.cc
 * @brief Unit tests for KeyWrapping AES-256-KW operations
 *
 * Tests key wrapping/unwrapping, PBKDF2 derivation, YubiKey integration,
 * and error handling for the key wrapping cryptographic primitives.
 */

#include <gtest/gtest.h>
#include "../src/core/KeyWrapping.h"
#include <openssl/rand.h>

using namespace KeepTower;

// ============================================================================
// Test Fixture
// ============================================================================

class KeyWrappingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Generate test KEK
        RAND_bytes(test_kek.data(), KeyWrapping::KEK_SIZE);

        // Generate test DEK
        RAND_bytes(test_dek.data(), KeyWrapping::DEK_SIZE);

        // Generate test salt
        RAND_bytes(test_salt.data(), KeyWrapping::SALT_SIZE);

        // Test password
        test_password = "TestPassword123!";
    }

    std::array<uint8_t, KeyWrapping::KEK_SIZE> test_kek;
    std::array<uint8_t, KeyWrapping::DEK_SIZE> test_dek;
    std::array<uint8_t, KeyWrapping::SALT_SIZE> test_salt;
    Glib::ustring test_password;
};

// ============================================================================
// Key Wrapping Tests
// ============================================================================

TEST_F(KeyWrappingTest, WrapKeySuccessful) {
    auto result = KeyWrapping::wrap_key(test_kek, test_dek);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().wrapped_key.size(), KeyWrapping::WRAPPED_KEY_SIZE);
}

TEST_F(KeyWrappingTest, WrapKeyDeterministic) {
    // Same KEK + DEK should produce same wrapped output
    auto result1 = KeyWrapping::wrap_key(test_kek, test_dek);
    auto result2 = KeyWrapping::wrap_key(test_kek, test_dek);

    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(result1.value().wrapped_key, result2.value().wrapped_key);
}

TEST_F(KeyWrappingTest, WrapKeyDifferentKEKProducesDifferentOutput) {
    std::array<uint8_t, KeyWrapping::KEK_SIZE> different_kek;
    RAND_bytes(different_kek.data(), KeyWrapping::KEK_SIZE);

    auto result1 = KeyWrapping::wrap_key(test_kek, test_dek);
    auto result2 = KeyWrapping::wrap_key(different_kek, test_dek);

    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());
    EXPECT_NE(result1.value().wrapped_key, result2.value().wrapped_key);
}

TEST_F(KeyWrappingTest, WrapKeyDifferentDEKProducesDifferentOutput) {
    std::array<uint8_t, KeyWrapping::DEK_SIZE> different_dek;
    RAND_bytes(different_dek.data(), KeyWrapping::DEK_SIZE);

    auto result1 = KeyWrapping::wrap_key(test_kek, test_dek);
    auto result2 = KeyWrapping::wrap_key(test_kek, different_dek);

    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());
    EXPECT_NE(result1.value().wrapped_key, result2.value().wrapped_key);
}

TEST_F(KeyWrappingTest, WrapKeyWithZeroKEK) {
    std::array<uint8_t, KeyWrapping::KEK_SIZE> zero_kek = {0};

    // Zero KEK is technically valid (though not secure)
    auto result = KeyWrapping::wrap_key(zero_kek, test_dek);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().wrapped_key.size(), KeyWrapping::WRAPPED_KEY_SIZE);
}

TEST_F(KeyWrappingTest, WrapKeyWithZeroDEK) {
    std::array<uint8_t, KeyWrapping::DEK_SIZE> zero_dek = {0};

    // Zero DEK is technically valid (though not secure)
    auto result = KeyWrapping::wrap_key(test_kek, zero_dek);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().wrapped_key.size(), KeyWrapping::WRAPPED_KEY_SIZE);
}

// ============================================================================
// Key Unwrapping Tests
// ============================================================================

TEST_F(KeyWrappingTest, UnwrapKeyRoundTrip) {
    // Wrap then unwrap should recover original DEK
    auto wrap_result = KeyWrapping::wrap_key(test_kek, test_dek);
    ASSERT_TRUE(wrap_result.has_value());

    auto unwrap_result = KeyWrapping::unwrap_key(test_kek, wrap_result.value().wrapped_key);

    ASSERT_TRUE(unwrap_result.has_value());
    EXPECT_EQ(unwrap_result.value().dek, test_dek);
}

TEST_F(KeyWrappingTest, UnwrapKeyWithWrongKEK) {
    // Wrap with one KEK
    auto wrap_result = KeyWrapping::wrap_key(test_kek, test_dek);
    ASSERT_TRUE(wrap_result.has_value());

    // Try to unwrap with different KEK (simulates wrong password)
    std::array<uint8_t, KeyWrapping::KEK_SIZE> wrong_kek;
    RAND_bytes(wrong_kek.data(), KeyWrapping::KEK_SIZE);

    auto unwrap_result = KeyWrapping::unwrap_key(wrong_kek, wrap_result.value().wrapped_key);

    ASSERT_FALSE(unwrap_result.has_value());
    EXPECT_EQ(unwrap_result.error(), KeyWrapping::Error::UNWRAP_FAILED);
}

TEST_F(KeyWrappingTest, UnwrapKeyWithCorruptedData) {
    // Wrap the DEK
    auto wrap_result = KeyWrapping::wrap_key(test_kek, test_dek);
    ASSERT_TRUE(wrap_result.has_value());

    // Corrupt the wrapped data
    auto corrupted = wrap_result.value().wrapped_key;
    corrupted[10] ^= 0xFF;
    corrupted[20] ^= 0xFF;

    auto unwrap_result = KeyWrapping::unwrap_key(test_kek, corrupted);

    ASSERT_FALSE(unwrap_result.has_value());
    EXPECT_EQ(unwrap_result.error(), KeyWrapping::Error::UNWRAP_FAILED);
}

TEST_F(KeyWrappingTest, UnwrapKeyWithZeroWrappedData) {
    std::array<uint8_t, KeyWrapping::WRAPPED_KEY_SIZE> zero_wrapped = {0};

    auto result = KeyWrapping::unwrap_key(test_kek, zero_wrapped);

    // Should fail - zero wrapped data is invalid
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), KeyWrapping::Error::UNWRAP_FAILED);
}

TEST_F(KeyWrappingTest, UnwrapKeyWithAllOnesData) {
    std::array<uint8_t, KeyWrapping::WRAPPED_KEY_SIZE> ones_data;
    ones_data.fill(0xFF);

    auto result = KeyWrapping::unwrap_key(test_kek, ones_data);

    // Should fail - invalid wrapped data
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), KeyWrapping::Error::UNWRAP_FAILED);
}

// ============================================================================
// PBKDF2 Key Derivation Tests
// ============================================================================

TEST_F(KeyWrappingTest, DeriveKEKFromPasswordSuccessful) {
    uint32_t iterations = 100000;

    auto result = KeyWrapping::derive_kek_from_password(test_password, test_salt, iterations);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), KeyWrapping::KEK_SIZE);
}

TEST_F(KeyWrappingTest, DeriveKEKDeterministic) {
    uint32_t iterations = 100000;

    // Same password + salt + iterations should produce same KEK
    auto result1 = KeyWrapping::derive_kek_from_password(test_password, test_salt, iterations);
    auto result2 = KeyWrapping::derive_kek_from_password(test_password, test_salt, iterations);

    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(result1.value(), result2.value());
}

TEST_F(KeyWrappingTest, DeriveKEKDifferentPasswordProducesDifferentKEK) {
    uint32_t iterations = 100000;
    Glib::ustring different_password = "DifferentPassword456!";

    auto result1 = KeyWrapping::derive_kek_from_password(test_password, test_salt, iterations);
    auto result2 = KeyWrapping::derive_kek_from_password(different_password, test_salt, iterations);

    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());
    EXPECT_NE(result1.value(), result2.value());
}

TEST_F(KeyWrappingTest, DeriveKEKDifferentSaltProducesDifferentKEK) {
    uint32_t iterations = 100000;
    std::array<uint8_t, KeyWrapping::SALT_SIZE> different_salt;
    RAND_bytes(different_salt.data(), KeyWrapping::SALT_SIZE);

    auto result1 = KeyWrapping::derive_kek_from_password(test_password, test_salt, iterations);
    auto result2 = KeyWrapping::derive_kek_from_password(test_password, different_salt, iterations);

    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());
    EXPECT_NE(result1.value(), result2.value());
}

TEST_F(KeyWrappingTest, DeriveKEKDifferentIterationsProducesDifferentKEK) {
    auto result1 = KeyWrapping::derive_kek_from_password(test_password, test_salt, 100000);
    auto result2 = KeyWrapping::derive_kek_from_password(test_password, test_salt, 200000);

    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());
    EXPECT_NE(result1.value(), result2.value());
}

TEST_F(KeyWrappingTest, DeriveKEKWithEmptyPassword) {
    Glib::ustring empty_password = "";
    uint32_t iterations = 100000;

    // Empty password is technically valid (though not secure)
    auto result = KeyWrapping::derive_kek_from_password(empty_password, test_salt, iterations);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), KeyWrapping::KEK_SIZE);
}

TEST_F(KeyWrappingTest, DeriveKEKWithUTF8Password) {
    Glib::ustring utf8_password = "Пароль123!";  // Russian characters
    uint32_t iterations = 100000;

    auto result = KeyWrapping::derive_kek_from_password(utf8_password, test_salt, iterations);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), KeyWrapping::KEK_SIZE);
}

TEST_F(KeyWrappingTest, DeriveKEKWithZeroSalt) {
    std::array<uint8_t, KeyWrapping::SALT_SIZE> zero_salt = {0};
    uint32_t iterations = 100000;

    // Zero salt is technically valid (though defeats the purpose)
    auto result = KeyWrapping::derive_kek_from_password(test_password, zero_salt, iterations);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), KeyWrapping::KEK_SIZE);
}

TEST_F(KeyWrappingTest, DeriveKEKWithLowIterations) {
    uint32_t low_iterations = 1;

    // Low iterations is valid (though not secure)
    auto result = KeyWrapping::derive_kek_from_password(test_password, test_salt, low_iterations);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), KeyWrapping::KEK_SIZE);
}

TEST_F(KeyWrappingTest, DeriveKEKWithHighIterations) {
    uint32_t high_iterations = 1000000;

    auto result = KeyWrapping::derive_kek_from_password(test_password, test_salt, high_iterations);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), KeyWrapping::KEK_SIZE);
}

// ============================================================================
// YubiKey Integration Tests
// ============================================================================

TEST_F(KeyWrappingTest, CombineWithYubiKeyXORsFirst20Bytes) {
    std::array<uint8_t, KeyWrapping::YUBIKEY_RESPONSE_SIZE> yubikey_response;
    RAND_bytes(yubikey_response.data(), KeyWrapping::YUBIKEY_RESPONSE_SIZE);

    auto combined_kek = KeyWrapping::combine_with_yubikey(test_kek, yubikey_response);

    // First 20 bytes should be XOR'd
    for (size_t i = 0; i < KeyWrapping::YUBIKEY_RESPONSE_SIZE; ++i) {
        EXPECT_EQ(combined_kek[i], test_kek[i] ^ yubikey_response[i]);
    }

    // Remaining 12 bytes should be unchanged
    for (size_t i = KeyWrapping::YUBIKEY_RESPONSE_SIZE; i < KeyWrapping::KEK_SIZE; ++i) {
        EXPECT_EQ(combined_kek[i], test_kek[i]);
    }
}

TEST_F(KeyWrappingTest, CombineWithYubiKeyIsReversible) {
    std::array<uint8_t, KeyWrapping::YUBIKEY_RESPONSE_SIZE> yubikey_response;
    RAND_bytes(yubikey_response.data(), KeyWrapping::YUBIKEY_RESPONSE_SIZE);

    // XOR twice should return to original
    auto combined_kek = KeyWrapping::combine_with_yubikey(test_kek, yubikey_response);
    auto restored_kek = KeyWrapping::combine_with_yubikey(combined_kek, yubikey_response);

    EXPECT_EQ(restored_kek, test_kek);
}

TEST_F(KeyWrappingTest, CombineWithYubiKeyZeroResponse) {
    std::array<uint8_t, KeyWrapping::YUBIKEY_RESPONSE_SIZE> zero_response = {0};

    // XOR with zero should leave KEK unchanged
    auto combined_kek = KeyWrapping::combine_with_yubikey(test_kek, zero_response);

    EXPECT_EQ(combined_kek, test_kek);
}

TEST_F(KeyWrappingTest, CombineWithYubiKeyAllOnesResponse) {
    std::array<uint8_t, KeyWrapping::YUBIKEY_RESPONSE_SIZE> ones_response;
    ones_response.fill(0xFF);

    auto combined_kek = KeyWrapping::combine_with_yubikey(test_kek, ones_response);

    // First 20 bytes should be flipped
    for (size_t i = 0; i < KeyWrapping::YUBIKEY_RESPONSE_SIZE; ++i) {
        EXPECT_EQ(combined_kek[i], test_kek[i] ^ 0xFF);
    }
}

// ============================================================================
// Random Generation Tests
// ============================================================================

TEST_F(KeyWrappingTest, GenerateRandomDEKSuccessful) {
    auto result = KeyWrapping::generate_random_dek();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), KeyWrapping::DEK_SIZE);
}

TEST_F(KeyWrappingTest, GenerateRandomDEKProducesDifferentValues) {
    auto result1 = KeyWrapping::generate_random_dek();
    auto result2 = KeyWrapping::generate_random_dek();

    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());

    // Extremely unlikely to be equal if random
    EXPECT_NE(result1.value(), result2.value());
}

TEST_F(KeyWrappingTest, GenerateRandomDEKNotAllZeros) {
    auto result = KeyWrapping::generate_random_dek();

    ASSERT_TRUE(result.has_value());

    // Check that not all bytes are zero
    bool has_nonzero = false;
    for (uint8_t byte : result.value()) {
        if (byte != 0) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero);
}

TEST_F(KeyWrappingTest, GenerateRandomSaltSuccessful) {
    auto result = KeyWrapping::generate_random_salt();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), KeyWrapping::SALT_SIZE);
}

TEST_F(KeyWrappingTest, GenerateRandomSaltProducesDifferentValues) {
    auto result1 = KeyWrapping::generate_random_salt();
    auto result2 = KeyWrapping::generate_random_salt();

    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());

    // Extremely unlikely to be equal if random
    EXPECT_NE(result1.value(), result2.value());
}

TEST_F(KeyWrappingTest, GenerateRandomSaltNotAllZeros) {
    auto result = KeyWrapping::generate_random_salt();

    ASSERT_TRUE(result.has_value());

    // Check that not all bytes are zero
    bool has_nonzero = false;
    for (uint8_t byte : result.value()) {
        if (byte != 0) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(KeyWrappingTest, CompletePasswordBasedWorkflow) {
    // Simulate complete password-based key wrapping workflow

    // 1. Generate random DEK (done once for vault)
    auto dek_result = KeyWrapping::generate_random_dek();
    ASSERT_TRUE(dek_result.has_value());
    auto dek = dek_result.value();

    // 2. Generate random salt for user
    auto salt_result = KeyWrapping::generate_random_salt();
    ASSERT_TRUE(salt_result.has_value());
    auto salt = salt_result.value();

    // 3. Derive KEK from password
    Glib::ustring password = "UserPassword123!";
    auto kek_result = KeyWrapping::derive_kek_from_password(password, salt, 100000);
    ASSERT_TRUE(kek_result.has_value());
    auto kek = kek_result.value();

    // 4. Wrap DEK with KEK
    auto wrap_result = KeyWrapping::wrap_key(kek, dek);
    ASSERT_TRUE(wrap_result.has_value());
    auto wrapped_dek = wrap_result.value().wrapped_key;

    // 5. Simulate storage (wrapped_dek would be stored in key slot)

    // 6. Simulate authentication: derive KEK from password again
    auto auth_kek_result = KeyWrapping::derive_kek_from_password(password, salt, 100000);
    ASSERT_TRUE(auth_kek_result.has_value());
    auto auth_kek = auth_kek_result.value();

    // 7. Unwrap DEK
    auto unwrap_result = KeyWrapping::unwrap_key(auth_kek, wrapped_dek);
    ASSERT_TRUE(unwrap_result.has_value());
    auto recovered_dek = unwrap_result.value().dek;

    // 8. Verify DEK matches original
    EXPECT_EQ(recovered_dek, dek);
}

TEST_F(KeyWrappingTest, CompleteYubiKeyWorkflow) {
    // Simulate complete YubiKey-enhanced workflow

    // 1. Generate DEK and salt
    auto dek_result = KeyWrapping::generate_random_dek();
    ASSERT_TRUE(dek_result.has_value());
    auto dek = dek_result.value();

    auto salt_result = KeyWrapping::generate_random_salt();
    ASSERT_TRUE(salt_result.has_value());
    auto salt = salt_result.value();

    // 2. Derive password-based KEK
    Glib::ustring password = "UserPassword123!";
    auto kek_result = KeyWrapping::derive_kek_from_password(password, salt, 100000);
    ASSERT_TRUE(kek_result.has_value());
    auto password_kek = kek_result.value();

    // 3. Combine with YubiKey response
    std::array<uint8_t, KeyWrapping::YUBIKEY_RESPONSE_SIZE> yubikey_response;
    RAND_bytes(yubikey_response.data(), KeyWrapping::YUBIKEY_RESPONSE_SIZE);
    auto combined_kek = KeyWrapping::combine_with_yubikey(password_kek, yubikey_response);

    // 4. Wrap DEK with combined KEK
    auto wrap_result = KeyWrapping::wrap_key(combined_kek, dek);
    ASSERT_TRUE(wrap_result.has_value());
    auto wrapped_dek = wrap_result.value().wrapped_key;

    // 5. Simulate authentication with correct password and YubiKey
    auto auth_kek_result = KeyWrapping::derive_kek_from_password(password, salt, 100000);
    ASSERT_TRUE(auth_kek_result.has_value());
    auto auth_password_kek = auth_kek_result.value();

    auto auth_combined_kek = KeyWrapping::combine_with_yubikey(auth_password_kek, yubikey_response);

    auto unwrap_result = KeyWrapping::unwrap_key(auth_combined_kek, wrapped_dek);
    ASSERT_TRUE(unwrap_result.has_value());
    EXPECT_EQ(unwrap_result.value().dek, dek);
}

TEST_F(KeyWrappingTest, YubiKeyWorkflowFailsWithoutYubiKey) {
    // Test that YubiKey-wrapped DEK cannot be unwrapped without YubiKey

    // Setup with YubiKey
    auto dek_result = KeyWrapping::generate_random_dek();
    ASSERT_TRUE(dek_result.has_value());
    auto dek = dek_result.value();

    auto salt_result = KeyWrapping::generate_random_salt();
    ASSERT_TRUE(salt_result.has_value());
    auto salt = salt_result.value();

    Glib::ustring password = "UserPassword123!";
    auto kek_result = KeyWrapping::derive_kek_from_password(password, salt, 100000);
    ASSERT_TRUE(kek_result.has_value());
    auto password_kek = kek_result.value();

    std::array<uint8_t, KeyWrapping::YUBIKEY_RESPONSE_SIZE> yubikey_response;
    RAND_bytes(yubikey_response.data(), KeyWrapping::YUBIKEY_RESPONSE_SIZE);
    auto combined_kek = KeyWrapping::combine_with_yubikey(password_kek, yubikey_response);

    auto wrap_result = KeyWrapping::wrap_key(combined_kek, dek);
    ASSERT_TRUE(wrap_result.has_value());
    auto wrapped_dek = wrap_result.value().wrapped_key;

    // Try to unwrap with password only (no YubiKey)
    auto auth_kek_result = KeyWrapping::derive_kek_from_password(password, salt, 100000);
    ASSERT_TRUE(auth_kek_result.has_value());
    auto auth_password_kek = auth_kek_result.value();

    // Should fail - missing YubiKey response
    auto unwrap_result = KeyWrapping::unwrap_key(auth_password_kek, wrapped_dek);
    ASSERT_FALSE(unwrap_result.has_value());
    EXPECT_EQ(unwrap_result.error(), KeyWrapping::Error::UNWRAP_FAILED);
}

// ============================================================================
// Error String Tests
// ============================================================================

TEST_F(KeyWrappingTest, ErrorToStringReturnsReadableStrings) {
    EXPECT_FALSE(KeyWrapping::error_to_string(KeyWrapping::Error::INVALID_KEK_SIZE).empty());
    EXPECT_FALSE(KeyWrapping::error_to_string(KeyWrapping::Error::INVALID_DEK_SIZE).empty());
    EXPECT_FALSE(KeyWrapping::error_to_string(KeyWrapping::Error::INVALID_WRAPPED_SIZE).empty());
    EXPECT_FALSE(KeyWrapping::error_to_string(KeyWrapping::Error::WRAP_FAILED).empty());
    EXPECT_FALSE(KeyWrapping::error_to_string(KeyWrapping::Error::UNWRAP_FAILED).empty());
    EXPECT_FALSE(KeyWrapping::error_to_string(KeyWrapping::Error::PBKDF2_FAILED).empty());
    EXPECT_FALSE(KeyWrapping::error_to_string(KeyWrapping::Error::OPENSSL_ERROR).empty());
}
// ============================================================================
// Advanced Key Wrapping Tests - RFC 3394 Compliance
// ============================================================================

TEST_F(KeyWrappingTest, WrapUnwrapMultipleDEKs) {
    // Test wrapping/unwrapping 10 different DEKs
    for (int i = 0; i < 10; ++i) {
        std::array<uint8_t, KeyWrapping::DEK_SIZE> dek;
        RAND_bytes(dek.data(), KeyWrapping::DEK_SIZE);

        auto wrap_result = KeyWrapping::wrap_key(test_kek, dek);
        ASSERT_TRUE(wrap_result.has_value());

        auto unwrap_result = KeyWrapping::unwrap_key(test_kek, wrap_result.value().wrapped_key);
        ASSERT_TRUE(unwrap_result.has_value());
        EXPECT_EQ(unwrap_result.value().dek, dek);
    }
}

TEST_F(KeyWrappingTest, UnwrapWithWrongKEKConsistentlyFails) {
    auto wrap_result = KeyWrapping::wrap_key(test_kek, test_dek);
    ASSERT_TRUE(wrap_result.has_value());

    // Try multiple wrong KEKs
    for (int i = 0; i < 5; ++i) {
        std::array<uint8_t, KeyWrapping::KEK_SIZE> wrong_kek;
        RAND_bytes(wrong_kek.data(), KeyWrapping::KEK_SIZE);

        auto unwrap_result = KeyWrapping::unwrap_key(wrong_kek, wrap_result.value().wrapped_key);
        EXPECT_FALSE(unwrap_result.has_value());
        EXPECT_EQ(unwrap_result.error(), KeyWrapping::Error::UNWRAP_FAILED);
    }
}

TEST_F(KeyWrappingTest, UnwrapTruncatedWrappedKey) {
    auto wrap_result = KeyWrapping::wrap_key(test_kek, test_dek);
    ASSERT_TRUE(wrap_result.has_value());

    // Create truncated version (simulate corrupted storage)
    std::array<uint8_t, KeyWrapping::WRAPPED_KEY_SIZE> truncated;
    std::copy_n(wrap_result.value().wrapped_key.begin(),
                KeyWrapping::WRAPPED_KEY_SIZE - 8,
                truncated.begin());
    std::fill_n(truncated.begin() + KeyWrapping::WRAPPED_KEY_SIZE - 8, 8, 0);

    auto unwrap_result = KeyWrapping::unwrap_key(test_kek, truncated);
    EXPECT_FALSE(unwrap_result.has_value());
}

TEST_F(KeyWrappingTest, UnwrapWithSingleBitFlip) {
    auto wrap_result = KeyWrapping::wrap_key(test_kek, test_dek);
    ASSERT_TRUE(wrap_result.has_value());

    // Flip a single bit at various positions
    for (size_t byte_pos = 0; byte_pos < KeyWrapping::WRAPPED_KEY_SIZE; byte_pos += 8) {
        for (int bit = 0; bit < 8; ++bit) {
            auto corrupted = wrap_result.value().wrapped_key;
            corrupted[byte_pos] ^= (1 << bit);

            auto unwrap_result = KeyWrapping::unwrap_key(test_kek, corrupted);
            EXPECT_FALSE(unwrap_result.has_value());
        }
    }
}

// ============================================================================
// Advanced PBKDF2 Tests
// ============================================================================

TEST_F(KeyWrappingTest, PBKDF2WithMinimumIterations) {
    auto result = KeyWrapping::derive_kek_from_password(test_password, test_salt, 1);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), KeyWrapping::KEK_SIZE);
}

TEST_F(KeyWrappingTest, PBKDF2WithVeryHighIterations) {
    // 1 million iterations - should still work but be slow
    auto result = KeyWrapping::derive_kek_from_password(test_password, test_salt, 1000000);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), KeyWrapping::KEK_SIZE);
}

TEST_F(KeyWrappingTest, PBKDF2WithVeryLongPassword) {
    Glib::ustring long_password(1000, 'x');

    auto result = KeyWrapping::derive_kek_from_password(long_password, test_salt, 10000);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), KeyWrapping::KEK_SIZE);
}

TEST_F(KeyWrappingTest, PBKDF2WithUnicodePassword) {
    Glib::ustring unicode_password = "测试密码🔐パスワード";

    auto result = KeyWrapping::derive_kek_from_password(unicode_password, test_salt, 10000);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), KeyWrapping::KEK_SIZE);
}

TEST_F(KeyWrappingTest, PBKDF2WithSpecialCharacters) {
    Glib::ustring special_password = "!@#$%^&*()_+-=[]{}|;':\",./<>?`~";

    auto result = KeyWrapping::derive_kek_from_password(special_password, test_salt, 10000);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), KeyWrapping::KEK_SIZE);
}

TEST_F(KeyWrappingTest, PBKDF2WithWhitespacePassword) {
    Glib::ustring whitespace_password = "  password  with  spaces  ";

    auto result = KeyWrapping::derive_kek_from_password(whitespace_password, test_salt, 10000);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), KeyWrapping::KEK_SIZE);
}

TEST_F(KeyWrappingTest, PBKDF2WithZeroSalt) {
    std::array<uint8_t, KeyWrapping::SALT_SIZE> zero_salt = {0};

    auto result = KeyWrapping::derive_kek_from_password(test_password, zero_salt, 10000);

    // Should work but is not secure
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), KeyWrapping::KEK_SIZE);
}

TEST_F(KeyWrappingTest, PBKDF2WithAllOnesSalt) {
    std::array<uint8_t, KeyWrapping::SALT_SIZE> ones_salt;
    ones_salt.fill(0xFF);

    auto result = KeyWrapping::derive_kek_from_password(test_password, ones_salt, 10000);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), KeyWrapping::KEK_SIZE);
}

// ============================================================================
// YubiKey Response Integration Tests
// ============================================================================

TEST_F(KeyWrappingTest, YubiKeyResponseCombinationIsReversible) {
    std::array<uint8_t, KeyWrapping::YUBIKEY_RESPONSE_SIZE> yubikey_response;
    RAND_bytes(yubikey_response.data(), KeyWrapping::YUBIKEY_RESPONSE_SIZE);

    auto combined_kek = KeyWrapping::combine_with_yubikey(test_kek, yubikey_response);

    // Combining with same response again should XOR back to original
    auto original_kek = KeyWrapping::combine_with_yubikey(combined_kek, yubikey_response);

    EXPECT_EQ(original_kek, test_kek);
}

TEST_F(KeyWrappingTest, YubiKeyResponseWithZeroResponse) {
    std::array<uint8_t, KeyWrapping::YUBIKEY_RESPONSE_SIZE> zero_response = {0};

    // XOR with zeros should leave KEK unchanged
    auto combined_kek = KeyWrapping::combine_with_yubikey(test_kek, zero_response);

    EXPECT_EQ(combined_kek, test_kek);
}

TEST_F(KeyWrappingTest, YubiKeyResponseWithAllOnesResponse) {
    std::array<uint8_t, KeyWrapping::YUBIKEY_RESPONSE_SIZE> ones_response;
    ones_response.fill(0xFF);

    // XOR with 0xFF should flip all bits
    auto combined_kek = KeyWrapping::combine_with_yubikey(test_kek, ones_response);

    EXPECT_NE(combined_kek, test_kek);

    // Each byte should be inverted
    for (size_t i = 0; i < std::min(test_kek.size(), ones_response.size()); ++i) {
        EXPECT_EQ(combined_kek[i], static_cast<uint8_t>(~test_kek[i]));
    }
}

TEST_F(KeyWrappingTest, YubiKeyResponseWithDifferentResponses) {
    std::array<uint8_t, KeyWrapping::YUBIKEY_RESPONSE_SIZE> response1;
    std::array<uint8_t, KeyWrapping::YUBIKEY_RESPONSE_SIZE> response2;

    RAND_bytes(response1.data(), KeyWrapping::YUBIKEY_RESPONSE_SIZE);
    RAND_bytes(response2.data(), KeyWrapping::YUBIKEY_RESPONSE_SIZE);

    auto combined1 = KeyWrapping::combine_with_yubikey(test_kek, response1);
    auto combined2 = KeyWrapping::combine_with_yubikey(test_kek, response2);

    EXPECT_NE(combined1, combined2);
}

// ============================================================================
// DEK and Salt Generation Tests
// ============================================================================

TEST_F(KeyWrappingTest, GenerateDEKProducesUniqueKeys) {
    std::vector<std::array<uint8_t, KeyWrapping::DEK_SIZE>> deks;

    // Generate 20 DEKs
    for (int i = 0; i < 20; ++i) {
        auto result = KeyWrapping::generate_random_dek();
        ASSERT_TRUE(result.has_value());
        deks.push_back(result.value());
    }

    // Verify all are unique
    for (size_t i = 0; i < deks.size(); ++i) {
        for (size_t j = i + 1; j < deks.size(); ++j) {
            EXPECT_NE(deks[i], deks[j]);
        }
    }
}

TEST_F(KeyWrappingTest, GenerateDEKProducesNonZeroKeys) {
    for (int i = 0; i < 10; ++i) {
        auto result = KeyWrapping::generate_random_dek();
        ASSERT_TRUE(result.has_value());

        // Check not all zeros
        bool has_nonzero = false;
        for (uint8_t byte : result.value()) {
            if (byte != 0) {
                has_nonzero = true;
                break;
            }
        }
        EXPECT_TRUE(has_nonzero);
    }
}

TEST_F(KeyWrappingTest, GenerateSaltProducesUniqueSalts) {
    std::vector<std::array<uint8_t, KeyWrapping::SALT_SIZE>> salts;

    // Generate 20 salts
    for (int i = 0; i < 20; ++i) {
        auto result = KeyWrapping::generate_random_salt();
        ASSERT_TRUE(result.has_value());
        salts.push_back(result.value());
    }

    // Verify all are unique
    for (size_t i = 0; i < salts.size(); ++i) {
        for (size_t j = i + 1; j < salts.size(); ++j) {
            EXPECT_NE(salts[i], salts[j]);
        }
    }
}

TEST_F(KeyWrappingTest, GenerateSaltProducesNonZeroSalts) {
    for (int i = 0; i < 10; ++i) {
        auto result = KeyWrapping::generate_random_salt();
        ASSERT_TRUE(result.has_value());

        // Check not all zeros
        bool has_nonzero = false;
        for (uint8_t byte : result.value()) {
            if (byte != 0) {
                has_nonzero = true;
                break;
            }
        }
        EXPECT_TRUE(has_nonzero);
    }
}

// ============================================================================
// Full Workflow Integration Tests
// ============================================================================

TEST_F(KeyWrappingTest, CompleteWorkflowPasswordOnly) {
    // 1. Generate random DEK
    auto dek_result = KeyWrapping::generate_random_dek();
    ASSERT_TRUE(dek_result.has_value());
    auto dek = dek_result.value();

    // 2. Generate random salt
    auto salt_result = KeyWrapping::generate_random_salt();
    ASSERT_TRUE(salt_result.has_value());
    auto salt = salt_result.value();

    // 3. Derive KEK from password
    Glib::ustring password = "SecurePassword123!";
    auto kek_result = KeyWrapping::derive_kek_from_password(password, salt, 100000);
    ASSERT_TRUE(kek_result.has_value());
    auto kek = kek_result.value();

    // 4. Wrap DEK
    auto wrap_result = KeyWrapping::wrap_key(kek, dek);
    ASSERT_TRUE(wrap_result.has_value());
    auto wrapped = wrap_result.value().wrapped_key;

    // 5. Simulate authentication: derive same KEK
    auto auth_kek_result = KeyWrapping::derive_kek_from_password(password, salt, 100000);
    ASSERT_TRUE(auth_kek_result.has_value());

    // 6. Unwrap DEK
    auto unwrap_result = KeyWrapping::unwrap_key(auth_kek_result.value(), wrapped);
    ASSERT_TRUE(unwrap_result.has_value());

    // 7. Verify recovered DEK matches original
    EXPECT_EQ(unwrap_result.value().dek, dek);
}

TEST_F(KeyWrappingTest, CompleteWorkflowPasswordPlusYubiKey) {
    // 1. Generate random DEK
    auto dek_result = KeyWrapping::generate_random_dek();
    ASSERT_TRUE(dek_result.has_value());
    auto dek = dek_result.value();

    // 2. Generate random salt
    auto salt_result = KeyWrapping::generate_random_salt();
    ASSERT_TRUE(salt_result.has_value());
    auto salt = salt_result.value();

    // 3. Derive password-based KEK
    Glib::ustring password = "SecurePassword123!";
    auto password_kek_result = KeyWrapping::derive_kek_from_password(password, salt, 100000);
    ASSERT_TRUE(password_kek_result.has_value());

    // 4. Simulate YubiKey response
    std::array<uint8_t, KeyWrapping::YUBIKEY_RESPONSE_SIZE> yubikey_response;
    RAND_bytes(yubikey_response.data(), KeyWrapping::YUBIKEY_RESPONSE_SIZE);

    // 5. Combine password KEK with YubiKey response
    auto combined_kek = KeyWrapping::combine_with_yubikey(
        password_kek_result.value(), yubikey_response);

    // 6. Wrap DEK with combined KEK
    auto wrap_result = KeyWrapping::wrap_key(combined_kek, dek);
    ASSERT_TRUE(wrap_result.has_value());

    // 7. Simulate authentication: derive password KEK again
    auto auth_password_kek_result = KeyWrapping::derive_kek_from_password(password, salt, 100000);
    ASSERT_TRUE(auth_password_kek_result.has_value());

    // 8. Combine with same YubiKey response
    auto auth_combined_kek = KeyWrapping::combine_with_yubikey(
        auth_password_kek_result.value(), yubikey_response);

    // 9. Unwrap DEK
    auto unwrap_result = KeyWrapping::unwrap_key(auth_combined_kek, wrap_result.value().wrapped_key);
    ASSERT_TRUE(unwrap_result.has_value());

    // 10. Verify recovered DEK matches original
    EXPECT_EQ(unwrap_result.value().dek, dek);
}