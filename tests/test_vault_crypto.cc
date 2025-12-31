// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file test_vault_crypto.cc
 * @brief Unit tests for VaultCrypto AES-256-GCM encryption
 *
 * Tests key derivation, encryption/decryption, authentication,
 * and error handling for vault cryptographic operations.
 */

#include <gtest/gtest.h>
#include "../src/core/crypto/VaultCrypto.h"
#include <algorithm>
#include <cstring>

using namespace KeepTower;

// ============================================================================
// Test Fixture
// ============================================================================

class VaultCryptoTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Generate test key
        test_key = VaultCrypto::generate_random_bytes(VaultCrypto::KEY_LENGTH);

        // Generate test IV
        test_iv = VaultCrypto::generate_random_bytes(VaultCrypto::IV_LENGTH);

        // Test plaintext
        test_plaintext = {
            'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd', '!'
        };

        // Test password and salt for key derivation
        test_password = "TestPassword123!";
        test_salt = VaultCrypto::generate_random_bytes(VaultCrypto::SALT_LENGTH);
    }

    std::vector<uint8_t> test_key;
    std::vector<uint8_t> test_iv;
    std::vector<uint8_t> test_plaintext;
    Glib::ustring test_password;
    std::vector<uint8_t> test_salt;
};

// ============================================================================
// Key Derivation Tests
// ============================================================================

TEST_F(VaultCryptoTest, DeriveKeySuccessful) {
    std::vector<uint8_t> key;

    bool result = VaultCrypto::derive_key(test_password, test_salt, key, 100000);

    ASSERT_TRUE(result);
    EXPECT_EQ(key.size(), VaultCrypto::KEY_LENGTH);
}

TEST_F(VaultCryptoTest, DeriveKeyDeterministic) {
    std::vector<uint8_t> key1;
    std::vector<uint8_t> key2;

    // Same password + salt + iterations should produce same key
    VaultCrypto::derive_key(test_password, test_salt, key1, 100000);
    VaultCrypto::derive_key(test_password, test_salt, key2, 100000);

    EXPECT_EQ(key1, key2);
}

TEST_F(VaultCryptoTest, DeriveKeyDifferentPasswordProducesDifferentKey) {
    Glib::ustring password1 = "Password1";
    Glib::ustring password2 = "Password2";

    std::vector<uint8_t> key1;
    std::vector<uint8_t> key2;

    VaultCrypto::derive_key(password1, test_salt, key1, 100000);
    VaultCrypto::derive_key(password2, test_salt, key2, 100000);

    EXPECT_NE(key1, key2);
}

TEST_F(VaultCryptoTest, DeriveKeyDifferentSaltProducesDifferentKey) {
    auto salt1 = VaultCrypto::generate_random_bytes(VaultCrypto::SALT_LENGTH);
    auto salt2 = VaultCrypto::generate_random_bytes(VaultCrypto::SALT_LENGTH);

    std::vector<uint8_t> key1;
    std::vector<uint8_t> key2;

    VaultCrypto::derive_key(test_password, salt1, key1, 100000);
    VaultCrypto::derive_key(test_password, salt2, key2, 100000);

    EXPECT_NE(key1, key2);
}

TEST_F(VaultCryptoTest, DeriveKeyDifferentIterationsProducesDifferentKey) {
    std::vector<uint8_t> key1;
    std::vector<uint8_t> key2;

    VaultCrypto::derive_key(test_password, test_salt, key1, 100000);
    VaultCrypto::derive_key(test_password, test_salt, key2, 200000);

    EXPECT_NE(key1, key2);
}

TEST_F(VaultCryptoTest, DeriveKeyWithEmptyPassword) {
    Glib::ustring empty_password = "";
    std::vector<uint8_t> key;

    // Empty password is technically valid (though not secure)
    bool result = VaultCrypto::derive_key(empty_password, test_salt, key, 100000);

    ASSERT_TRUE(result);
    EXPECT_EQ(key.size(), VaultCrypto::KEY_LENGTH);
}

TEST_F(VaultCryptoTest, DeriveKeyWithZeroSalt) {
    std::vector<uint8_t> zero_salt(VaultCrypto::SALT_LENGTH, 0);
    std::vector<uint8_t> key;

    // Zero salt is technically valid (though defeats the purpose)
    bool result = VaultCrypto::derive_key(test_password, zero_salt, key, 100000);

    ASSERT_TRUE(result);
    EXPECT_EQ(key.size(), VaultCrypto::KEY_LENGTH);
}

TEST_F(VaultCryptoTest, DeriveKeyWithLowIterations) {
    std::vector<uint8_t> key;

    // Low iterations (1) is valid but not secure
    bool result = VaultCrypto::derive_key(test_password, test_salt, key, 1);

    ASSERT_TRUE(result);
    EXPECT_EQ(key.size(), VaultCrypto::KEY_LENGTH);
}

TEST_F(VaultCryptoTest, DeriveKeyResizesOutputBuffer) {
    std::vector<uint8_t> key;  // Empty buffer

    bool result = VaultCrypto::derive_key(test_password, test_salt, key, 100000);

    ASSERT_TRUE(result);
    EXPECT_EQ(key.size(), VaultCrypto::KEY_LENGTH);
}

TEST_F(VaultCryptoTest, DeriveKeyWithPreallocatedBuffer) {
    std::vector<uint8_t> key(VaultCrypto::KEY_LENGTH);

    bool result = VaultCrypto::derive_key(test_password, test_salt, key, 100000);

    ASSERT_TRUE(result);
    EXPECT_EQ(key.size(), VaultCrypto::KEY_LENGTH);
}

// ============================================================================
// Encryption Tests
// ============================================================================

TEST_F(VaultCryptoTest, EncryptDataSuccessful) {
    std::vector<uint8_t> ciphertext;

    bool result = VaultCrypto::encrypt_data(test_plaintext, test_key, ciphertext, test_iv);

    ASSERT_TRUE(result);
    EXPECT_GT(ciphertext.size(), test_plaintext.size());  // Includes auth tag
    EXPECT_EQ(ciphertext.size(), test_plaintext.size() + VaultCrypto::TAG_LENGTH);
}

TEST_F(VaultCryptoTest, EncryptDataProducesDifferentCiphertext) {
    std::vector<uint8_t> ciphertext1;
    std::vector<uint8_t> ciphertext2;

    // Same plaintext with different IVs produces different ciphertext
    auto iv1 = VaultCrypto::generate_random_bytes(VaultCrypto::IV_LENGTH);
    auto iv2 = VaultCrypto::generate_random_bytes(VaultCrypto::IV_LENGTH);

    ASSERT_TRUE(VaultCrypto::encrypt_data(test_plaintext, test_key, ciphertext1, iv1));
    ASSERT_TRUE(VaultCrypto::encrypt_data(test_plaintext, test_key, ciphertext2, iv2));

    EXPECT_NE(ciphertext1, ciphertext2);
}

TEST_F(VaultCryptoTest, EncryptDataDifferentKeyProducesDifferentCiphertext) {
    std::vector<uint8_t> ciphertext1;
    std::vector<uint8_t> ciphertext2;

    auto key1 = VaultCrypto::generate_random_bytes(VaultCrypto::KEY_LENGTH);
    auto key2 = VaultCrypto::generate_random_bytes(VaultCrypto::KEY_LENGTH);

    ASSERT_TRUE(VaultCrypto::encrypt_data(test_plaintext, key1, ciphertext1, test_iv));
    ASSERT_TRUE(VaultCrypto::encrypt_data(test_plaintext, key2, ciphertext2, test_iv));

    EXPECT_NE(ciphertext1, ciphertext2);
}

TEST_F(VaultCryptoTest, EncryptDataRejectsInvalidKeySize) {
    std::vector<uint8_t> invalid_key = {1, 2, 3, 4};  // Too short
    std::vector<uint8_t> ciphertext;

    bool result = VaultCrypto::encrypt_data(test_plaintext, invalid_key, ciphertext, test_iv);

    EXPECT_FALSE(result);
}

TEST_F(VaultCryptoTest, EncryptDataRejectsInvalidIVSize) {
    std::vector<uint8_t> invalid_iv = {1, 2, 3, 4};  // Too short
    std::vector<uint8_t> ciphertext;

    bool result = VaultCrypto::encrypt_data(test_plaintext, test_key, ciphertext, invalid_iv);

    EXPECT_FALSE(result);
}

TEST_F(VaultCryptoTest, EncryptDataWithEmptyPlaintext) {
    std::vector<uint8_t> empty_plaintext;
    std::vector<uint8_t> ciphertext;

    bool result = VaultCrypto::encrypt_data(empty_plaintext, test_key, ciphertext, test_iv);

    ASSERT_TRUE(result);
    EXPECT_EQ(ciphertext.size(), VaultCrypto::TAG_LENGTH);  // Only tag
}

TEST_F(VaultCryptoTest, EncryptDataWithLargePlaintext) {
    std::vector<uint8_t> large_plaintext(1024 * 1024);  // 1MB
    for (size_t i = 0; i < large_plaintext.size(); ++i) {
        large_plaintext[i] = static_cast<uint8_t>(i & 0xFF);
    }

    std::vector<uint8_t> ciphertext;

    bool result = VaultCrypto::encrypt_data(large_plaintext, test_key, ciphertext, test_iv);

    ASSERT_TRUE(result);
    EXPECT_EQ(ciphertext.size(), large_plaintext.size() + VaultCrypto::TAG_LENGTH);
}

// ============================================================================
// Decryption Tests
// ============================================================================

TEST_F(VaultCryptoTest, DecryptDataRoundTrip) {
    std::vector<uint8_t> ciphertext;
    std::vector<uint8_t> decrypted;

    // Encrypt
    ASSERT_TRUE(VaultCrypto::encrypt_data(test_plaintext, test_key, ciphertext, test_iv));

    // Decrypt
    bool result = VaultCrypto::decrypt_data(ciphertext, test_key, test_iv, decrypted);

    ASSERT_TRUE(result);
    EXPECT_EQ(decrypted, test_plaintext);
}

TEST_F(VaultCryptoTest, DecryptDataWithWrongKey) {
    std::vector<uint8_t> ciphertext;
    std::vector<uint8_t> decrypted;

    // Encrypt with one key
    ASSERT_TRUE(VaultCrypto::encrypt_data(test_plaintext, test_key, ciphertext, test_iv));

    // Try to decrypt with different key
    auto wrong_key = VaultCrypto::generate_random_bytes(VaultCrypto::KEY_LENGTH);
    bool result = VaultCrypto::decrypt_data(ciphertext, wrong_key, test_iv, decrypted);

    // Should fail authentication
    EXPECT_FALSE(result);
}

TEST_F(VaultCryptoTest, DecryptDataWithWrongIV) {
    std::vector<uint8_t> ciphertext;
    std::vector<uint8_t> decrypted;

    // Encrypt with one IV
    ASSERT_TRUE(VaultCrypto::encrypt_data(test_plaintext, test_key, ciphertext, test_iv));

    // Try to decrypt with different IV
    auto wrong_iv = VaultCrypto::generate_random_bytes(VaultCrypto::IV_LENGTH);
    bool result = VaultCrypto::decrypt_data(ciphertext, test_key, wrong_iv, decrypted);

    // Should fail authentication
    EXPECT_FALSE(result);
}

TEST_F(VaultCryptoTest, DecryptDataDetectsCorruptedCiphertext) {
    std::vector<uint8_t> ciphertext;
    std::vector<uint8_t> decrypted;

    // Encrypt
    ASSERT_TRUE(VaultCrypto::encrypt_data(test_plaintext, test_key, ciphertext, test_iv));

    // Corrupt the ciphertext
    ciphertext[5] ^= 0xFF;

    // Try to decrypt
    bool result = VaultCrypto::decrypt_data(ciphertext, test_key, test_iv, decrypted);

    // Should fail authentication (GCM detects tampering)
    EXPECT_FALSE(result);
}

TEST_F(VaultCryptoTest, DecryptDataDetectsCorruptedTag) {
    std::vector<uint8_t> ciphertext;
    std::vector<uint8_t> decrypted;

    // Encrypt
    ASSERT_TRUE(VaultCrypto::encrypt_data(test_plaintext, test_key, ciphertext, test_iv));

    // Corrupt the authentication tag (last 16 bytes)
    ciphertext[ciphertext.size() - 1] ^= 0xFF;

    // Try to decrypt
    bool result = VaultCrypto::decrypt_data(ciphertext, test_key, test_iv, decrypted);

    // Should fail authentication
    EXPECT_FALSE(result);
}

TEST_F(VaultCryptoTest, DecryptDataRejectsInvalidKeySize) {
    std::vector<uint8_t> ciphertext = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    std::vector<uint8_t> invalid_key = {1, 2, 3, 4};  // Too short
    std::vector<uint8_t> decrypted;

    bool result = VaultCrypto::decrypt_data(ciphertext, invalid_key, test_iv, decrypted);

    EXPECT_FALSE(result);
}

TEST_F(VaultCryptoTest, DecryptDataRejectsInvalidIVSize) {
    std::vector<uint8_t> ciphertext = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    std::vector<uint8_t> invalid_iv = {1, 2, 3, 4};  // Too short
    std::vector<uint8_t> decrypted;

    bool result = VaultCrypto::decrypt_data(ciphertext, test_key, invalid_iv, decrypted);

    EXPECT_FALSE(result);
}

TEST_F(VaultCryptoTest, DecryptDataRejectsTooShortCiphertext) {
    std::vector<uint8_t> too_short = {1, 2, 3};  // Less than TAG_LENGTH
    std::vector<uint8_t> decrypted;

    bool result = VaultCrypto::decrypt_data(too_short, test_key, test_iv, decrypted);

    EXPECT_FALSE(result);
}

TEST_F(VaultCryptoTest, DecryptDataWithEmptyPlaintext) {
    std::vector<uint8_t> empty_plaintext;
    std::vector<uint8_t> ciphertext;
    std::vector<uint8_t> decrypted;

    // Encrypt empty data
    ASSERT_TRUE(VaultCrypto::encrypt_data(empty_plaintext, test_key, ciphertext, test_iv));

    // Decrypt
    bool result = VaultCrypto::decrypt_data(ciphertext, test_key, test_iv, decrypted);

    ASSERT_TRUE(result);
    EXPECT_EQ(decrypted.size(), 0);
}

TEST_F(VaultCryptoTest, DecryptDataWithLargeCiphertext) {
    std::vector<uint8_t> large_plaintext(1024 * 1024);  // 1MB
    std::fill(large_plaintext.begin(), large_plaintext.end(), 0xAB);

    std::vector<uint8_t> ciphertext;
    std::vector<uint8_t> decrypted;

    // Encrypt
    ASSERT_TRUE(VaultCrypto::encrypt_data(large_plaintext, test_key, ciphertext, test_iv));

    // Decrypt
    bool result = VaultCrypto::decrypt_data(ciphertext, test_key, test_iv, decrypted);

    ASSERT_TRUE(result);
    EXPECT_EQ(decrypted, large_plaintext);
}

// ============================================================================
// Random Generation Tests
// ============================================================================

TEST_F(VaultCryptoTest, GenerateRandomBytesProducesCorrectLength) {
    auto bytes = VaultCrypto::generate_random_bytes(32);

    EXPECT_EQ(bytes.size(), 32);
}

TEST_F(VaultCryptoTest, GenerateRandomBytesProducesDifferentValues) {
    auto bytes1 = VaultCrypto::generate_random_bytes(32);
    auto bytes2 = VaultCrypto::generate_random_bytes(32);

    // Extremely unlikely to be equal if random
    EXPECT_NE(bytes1, bytes2);
}

TEST_F(VaultCryptoTest, GenerateRandomBytesNotAllZeros) {
    auto bytes = VaultCrypto::generate_random_bytes(32);

    bool has_nonzero = false;
    for (uint8_t byte : bytes) {
        if (byte != 0) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero);
}

TEST_F(VaultCryptoTest, GenerateRandomBytesNotAllOnes) {
    auto bytes = VaultCrypto::generate_random_bytes(32);

    bool has_non_ff = false;
    for (uint8_t byte : bytes) {
        if (byte != 0xFF) {
            has_non_ff = true;
            break;
        }
    }
    EXPECT_TRUE(has_non_ff);
}

TEST_F(VaultCryptoTest, GenerateRandomBytesWithZeroLength) {
    auto bytes = VaultCrypto::generate_random_bytes(0);

    EXPECT_EQ(bytes.size(), 0);
}

TEST_F(VaultCryptoTest, GenerateRandomBytesWithLargeLength) {
    auto bytes = VaultCrypto::generate_random_bytes(1024);

    EXPECT_EQ(bytes.size(), 1024);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(VaultCryptoTest, CompleteEncryptionWorkflow) {
    // Simulate complete encryption workflow

    // 1. Generate salt for key derivation
    auto salt = VaultCrypto::generate_random_bytes(VaultCrypto::SALT_LENGTH);

    // 2. Derive key from password
    std::vector<uint8_t> key;
    ASSERT_TRUE(VaultCrypto::derive_key("UserPassword123", salt, key, 100000));

    // 3. Generate IV for encryption
    auto iv = VaultCrypto::generate_random_bytes(VaultCrypto::IV_LENGTH);

    // 4. Encrypt data
    std::vector<uint8_t> plaintext = {'S', 'e', 'c', 'r', 'e', 't', ' ', 'D', 'a', 't', 'a'};
    std::vector<uint8_t> ciphertext;
    ASSERT_TRUE(VaultCrypto::encrypt_data(plaintext, key, ciphertext, iv));

    // 5. Simulate storage (ciphertext, salt, iv would be stored)

    // 6. Simulate authentication: derive key from password again
    std::vector<uint8_t> auth_key;
    ASSERT_TRUE(VaultCrypto::derive_key("UserPassword123", salt, auth_key, 100000));

    // 7. Decrypt data
    std::vector<uint8_t> decrypted;
    ASSERT_TRUE(VaultCrypto::decrypt_data(ciphertext, auth_key, iv, decrypted));

    // 8. Verify plaintext matches
    EXPECT_EQ(decrypted, plaintext);
}

TEST_F(VaultCryptoTest, CompleteWorkflowFailsWithWrongPassword) {
    // Simulate encryption with one password
    auto salt = VaultCrypto::generate_random_bytes(VaultCrypto::SALT_LENGTH);
    std::vector<uint8_t> key;
    ASSERT_TRUE(VaultCrypto::derive_key("CorrectPassword", salt, key, 100000));

    auto iv = VaultCrypto::generate_random_bytes(VaultCrypto::IV_LENGTH);
    std::vector<uint8_t> plaintext = {'S', 'e', 'c', 'r', 'e', 't'};
    std::vector<uint8_t> ciphertext;
    ASSERT_TRUE(VaultCrypto::encrypt_data(plaintext, key, ciphertext, iv));

    // Try to decrypt with wrong password
    std::vector<uint8_t> wrong_key;
    ASSERT_TRUE(VaultCrypto::derive_key("WrongPassword", salt, wrong_key, 100000));

    std::vector<uint8_t> decrypted;
    bool result = VaultCrypto::decrypt_data(ciphertext, wrong_key, iv, decrypted);

    // Should fail authentication
    EXPECT_FALSE(result);
}

TEST_F(VaultCryptoTest, EncryptDecryptMultipleBlocksizes) {
    // Test various plaintext sizes
    std::vector<size_t> sizes = {0, 1, 15, 16, 17, 31, 32, 33, 64, 127, 128, 129, 256};

    for (size_t size : sizes) {
        std::vector<uint8_t> plaintext(size);
        std::fill(plaintext.begin(), plaintext.end(), static_cast<uint8_t>(size & 0xFF));

        std::vector<uint8_t> ciphertext;
        std::vector<uint8_t> decrypted;

        auto iv = VaultCrypto::generate_random_bytes(VaultCrypto::IV_LENGTH);

        ASSERT_TRUE(VaultCrypto::encrypt_data(plaintext, test_key, ciphertext, iv));
        ASSERT_TRUE(VaultCrypto::decrypt_data(ciphertext, test_key, iv, decrypted));

        EXPECT_EQ(decrypted, plaintext);
    }
}

TEST_F(VaultCryptoTest, EncryptionWithDifferentKeysIsIndependent) {
    // Encrypt same plaintext with two different keys
    auto key1 = VaultCrypto::generate_random_bytes(VaultCrypto::KEY_LENGTH);
    auto key2 = VaultCrypto::generate_random_bytes(VaultCrypto::KEY_LENGTH);
    auto iv1 = VaultCrypto::generate_random_bytes(VaultCrypto::IV_LENGTH);
    auto iv2 = VaultCrypto::generate_random_bytes(VaultCrypto::IV_LENGTH);

    std::vector<uint8_t> ciphertext1;
    std::vector<uint8_t> ciphertext2;

    ASSERT_TRUE(VaultCrypto::encrypt_data(test_plaintext, key1, ciphertext1, iv1));
    ASSERT_TRUE(VaultCrypto::encrypt_data(test_plaintext, key2, ciphertext2, iv2));

    // Ciphertexts should be different
    EXPECT_NE(ciphertext1, ciphertext2);

    // Each key should only decrypt its own ciphertext
    std::vector<uint8_t> decrypted1;
    std::vector<uint8_t> decrypted2;

    ASSERT_TRUE(VaultCrypto::decrypt_data(ciphertext1, key1, iv1, decrypted1));
    ASSERT_TRUE(VaultCrypto::decrypt_data(ciphertext2, key2, iv2, decrypted2));

    EXPECT_EQ(decrypted1, test_plaintext);
    EXPECT_EQ(decrypted2, test_plaintext);

    // Wrong key should fail
    EXPECT_FALSE(VaultCrypto::decrypt_data(ciphertext1, key2, iv1, decrypted2));
    EXPECT_FALSE(VaultCrypto::decrypt_data(ciphertext2, key1, iv2, decrypted1));
}
// ============================================================================
// Advanced Security Tests - Tamper Detection
// ============================================================================

TEST_F(VaultCryptoTest, TamperedCiphertextDetected) {
    std::vector<uint8_t> ciphertext;
    ASSERT_TRUE(VaultCrypto::encrypt_data(test_plaintext, test_key, ciphertext, test_iv));

    // Tamper with ciphertext byte (not the tag)
    if (ciphertext.size() > VaultCrypto::TAG_LENGTH + 5) {
        ciphertext[5] ^= 0xFF;
    }

    std::vector<uint8_t> decrypted;
    EXPECT_FALSE(VaultCrypto::decrypt_data(ciphertext, test_key, test_iv, decrypted));
}

TEST_F(VaultCryptoTest, TamperedAuthenticationTagDetected) {
    std::vector<uint8_t> ciphertext;
    ASSERT_TRUE(VaultCrypto::encrypt_data(test_plaintext, test_key, ciphertext, test_iv));

    // Tamper with last byte (authentication tag)
    ciphertext[ciphertext.size() - 1] ^= 0xFF;

    std::vector<uint8_t> decrypted;
    EXPECT_FALSE(VaultCrypto::decrypt_data(ciphertext, test_key, test_iv, decrypted));
}

TEST_F(VaultCryptoTest, TruncatedCiphertextDetected) {
    std::vector<uint8_t> ciphertext;
    ASSERT_TRUE(VaultCrypto::encrypt_data(test_plaintext, test_key, ciphertext, test_iv));

    // Truncate ciphertext (remove part of tag)
    if (ciphertext.size() > 1) {
        ciphertext.resize(ciphertext.size() - 1);
    }

    std::vector<uint8_t> decrypted;
    EXPECT_FALSE(VaultCrypto::decrypt_data(ciphertext, test_key, test_iv, decrypted));
}

TEST_F(VaultCryptoTest, WrongKeyDetected) {
    std::vector<uint8_t> ciphertext;
    ASSERT_TRUE(VaultCrypto::encrypt_data(test_plaintext, test_key, ciphertext, test_iv));

    // Try to decrypt with different key
    auto wrong_key = VaultCrypto::generate_random_bytes(VaultCrypto::KEY_LENGTH);
    std::vector<uint8_t> decrypted;

    EXPECT_FALSE(VaultCrypto::decrypt_data(ciphertext, wrong_key, test_iv, decrypted));
}

TEST_F(VaultCryptoTest, WrongIVDetected) {
    std::vector<uint8_t> ciphertext;
    ASSERT_TRUE(VaultCrypto::encrypt_data(test_plaintext, test_key, ciphertext, test_iv));

    // Try to decrypt with different IV
    auto wrong_iv = VaultCrypto::generate_random_bytes(VaultCrypto::IV_LENGTH);
    std::vector<uint8_t> decrypted;

    EXPECT_FALSE(VaultCrypto::decrypt_data(ciphertext, test_key, wrong_iv, decrypted));
}

// ============================================================================
// Advanced Input Validation Tests
// ============================================================================

TEST_F(VaultCryptoTest, KeySizeValidation31Bytes) {
    std::vector<uint8_t> invalid_key(31);  // One byte too short
    std::vector<uint8_t> ciphertext;

    EXPECT_FALSE(VaultCrypto::encrypt_data(test_plaintext, invalid_key, ciphertext, test_iv));
}

TEST_F(VaultCryptoTest, KeySizeValidation33Bytes) {
    std::vector<uint8_t> invalid_key(33);  // One byte too long
    std::vector<uint8_t> ciphertext;

    EXPECT_FALSE(VaultCrypto::encrypt_data(test_plaintext, invalid_key, ciphertext, test_iv));
}

TEST_F(VaultCryptoTest, IVSizeValidation11Bytes) {
    std::vector<uint8_t> invalid_iv(11);  // One byte too short
    std::vector<uint8_t> ciphertext;

    EXPECT_FALSE(VaultCrypto::encrypt_data(test_plaintext, test_key, ciphertext, invalid_iv));
}

TEST_F(VaultCryptoTest, IVSizeValidation13Bytes) {
    std::vector<uint8_t> invalid_iv(13);  // One byte too long
    std::vector<uint8_t> ciphertext;

    EXPECT_FALSE(VaultCrypto::encrypt_data(test_plaintext, test_key, ciphertext, invalid_iv));
}

TEST_F(VaultCryptoTest, DecryptCiphertextTooShort) {
    std::vector<uint8_t> short_ciphertext(VaultCrypto::TAG_LENGTH - 1);
    std::vector<uint8_t> decrypted;

    EXPECT_FALSE(VaultCrypto::decrypt_data(short_ciphertext, test_key, test_iv, decrypted));
}

// ============================================================================
// Large Data and Performance Tests
// ============================================================================

TEST_F(VaultCryptoTest, Encrypt10MBData) {
    std::vector<uint8_t> large_plaintext(10 * 1024 * 1024);  // 10MB
    for (size_t i = 0; i < large_plaintext.size(); ++i) {
        large_plaintext[i] = static_cast<uint8_t>(i & 0xFF);
    }

    std::vector<uint8_t> ciphertext;
    std::vector<uint8_t> decrypted;

    ASSERT_TRUE(VaultCrypto::encrypt_data(large_plaintext, test_key, ciphertext, test_iv));
    EXPECT_EQ(ciphertext.size(), large_plaintext.size() + VaultCrypto::TAG_LENGTH);

    ASSERT_TRUE(VaultCrypto::decrypt_data(ciphertext, test_key, test_iv, decrypted));
    EXPECT_EQ(decrypted, large_plaintext);
}

TEST_F(VaultCryptoTest, EncryptSingleByte) {
    std::vector<uint8_t> single_byte = {0x42};
    std::vector<uint8_t> ciphertext;
    std::vector<uint8_t> decrypted;

    ASSERT_TRUE(VaultCrypto::encrypt_data(single_byte, test_key, ciphertext, test_iv));
    EXPECT_EQ(ciphertext.size(), 1 + VaultCrypto::TAG_LENGTH);

    ASSERT_TRUE(VaultCrypto::decrypt_data(ciphertext, test_key, test_iv, decrypted));
    EXPECT_EQ(decrypted, single_byte);
}

TEST_F(VaultCryptoTest, EncryptBlockBoundary16Bytes) {
    std::vector<uint8_t> plaintext(16);  // AES block size
    for (size_t i = 0; i < plaintext.size(); ++i) {
        plaintext[i] = static_cast<uint8_t>(i);
    }

    std::vector<uint8_t> ciphertext;
    std::vector<uint8_t> decrypted;

    ASSERT_TRUE(VaultCrypto::encrypt_data(plaintext, test_key, ciphertext, test_iv));
    ASSERT_TRUE(VaultCrypto::decrypt_data(ciphertext, test_key, test_iv, decrypted));
    EXPECT_EQ(decrypted, plaintext);
}

// ============================================================================
// Random Bytes Quality Tests
// ============================================================================

TEST_F(VaultCryptoTest, RandomBytesGeneratesRequestedLength) {
    std::vector<size_t> sizes = {1, 16, 32, 64, 128, 256, 1024};

    for (size_t size : sizes) {
        auto random = VaultCrypto::generate_random_bytes(size);
        EXPECT_EQ(random.size(), size);
    }
}

TEST_F(VaultCryptoTest, RandomBytesAreUnique) {
    auto random1 = VaultCrypto::generate_random_bytes(32);
    auto random2 = VaultCrypto::generate_random_bytes(32);
    auto random3 = VaultCrypto::generate_random_bytes(32);

    EXPECT_NE(random1, random2);
    EXPECT_NE(random2, random3);
    EXPECT_NE(random1, random3);
}

TEST_F(VaultCryptoTest, RandomBytesNotAllZeros) {
    auto random = VaultCrypto::generate_random_bytes(32);

    bool has_nonzero = false;
    for (uint8_t byte : random) {
        if (byte != 0) {
            has_nonzero = true;
            break;
        }
    }

    EXPECT_TRUE(has_nonzero);
}

TEST_F(VaultCryptoTest, RandomBytesNotAllOnes) {
    auto random = VaultCrypto::generate_random_bytes(32);

    bool has_non_ff = false;
    for (uint8_t byte : random) {
        if (byte != 0xFF) {
            has_non_ff = true;
            break;
        }
    }

    EXPECT_TRUE(has_non_ff);
}

// ============================================================================
// PBKDF2 Advanced Tests
// ============================================================================

TEST_F(VaultCryptoTest, PBKDF2WithVeryLongPassword) {
    Glib::ustring long_password(1000, 'x');
    std::vector<uint8_t> key;

    ASSERT_TRUE(VaultCrypto::derive_key(long_password, test_salt, key, 10000));
    EXPECT_EQ(key.size(), VaultCrypto::KEY_LENGTH);
}

TEST_F(VaultCryptoTest, PBKDF2WithUnicodePassword) {
    Glib::ustring unicode_password = "测试密码🔐";
    std::vector<uint8_t> key;

    ASSERT_TRUE(VaultCrypto::derive_key(unicode_password, test_salt, key, 10000));
    EXPECT_EQ(key.size(), VaultCrypto::KEY_LENGTH);
}

TEST_F(VaultCryptoTest, PBKDF2WithSpecialCharacters) {
    Glib::ustring special_password = "!@#$%^&*()_+-=[]{}|;':\",./<>?";
    std::vector<uint8_t> key;

    ASSERT_TRUE(VaultCrypto::derive_key(special_password, test_salt, key, 10000));
    EXPECT_EQ(key.size(), VaultCrypto::KEY_LENGTH);
}

TEST_F(VaultCryptoTest, PBKDF2WithHighIterations) {
    std::vector<uint8_t> key;

    // High iterations (should still work, just slower)
    ASSERT_TRUE(VaultCrypto::derive_key(test_password, test_salt, key, 1000000));
    EXPECT_EQ(key.size(), VaultCrypto::KEY_LENGTH);
}

TEST_F(VaultCryptoTest, PBKDF2IterationsAffectOutput) {
    std::vector<uint8_t> key_low;
    std::vector<uint8_t> key_high;

    ASSERT_TRUE(VaultCrypto::derive_key(test_password, test_salt, key_low, 10000));
    ASSERT_TRUE(VaultCrypto::derive_key(test_password, test_salt, key_high, 20000));

    EXPECT_NE(key_low, key_high);
}