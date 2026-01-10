// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

/**
 * @file test_vault_crypto_service.cc
 * @brief Unit tests for VaultCryptoService
 *
 * Tests all cryptographic operations provided by VaultCryptoService,
 * including DEK generation, KEK derivation, key wrapping, encryption/decryption,
 * and PIN operations.
 */

#include <gtest/gtest.h>
#include "../src/core/services/VaultCryptoService.h"
#include "../src/core/crypto/VaultCrypto.h"
#include <algorithm>

using namespace KeepTower;

// Constants
constexpr uint32_t DEFAULT_PBKDF2_ITERATIONS = 100000;

// ============================================================================
// Test Fixtures
// ============================================================================

class VaultCryptoServiceTest : public ::testing::Test {
protected:
    VaultCryptoService service;
};

// ============================================================================
// DEK Generation Tests
// ============================================================================

TEST_F(VaultCryptoServiceTest, GenerateDEK_Success) {
    auto result = service.generate_dek();

    ASSERT_TRUE(result.has_value()) << "DEK generation should succeed";
    EXPECT_EQ(result->dek.size(), 32) << "DEK should be 256 bits (32 bytes)";

    // Verify DEK is not all zeros
    bool all_zeros = std::all_of(result->dek.begin(), result->dek.end(),
                                   [](uint8_t b) { return b == 0; });
    EXPECT_FALSE(all_zeros) << "DEK should contain random data, not all zeros";
}

TEST_F(VaultCryptoServiceTest, GenerateDEK_UniqueKeys) {
    auto result1 = service.generate_dek();
    auto result2 = service.generate_dek();

    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());

    // Two DEKs should be different (cryptographically unique)
    EXPECT_NE(result1->dek, result2->dek) << "Sequential DEK generations should produce unique keys";
}

// ============================================================================
// KEK Derivation Tests
// ============================================================================

TEST_F(VaultCryptoServiceTest, DeriveKEK_Success) {
    const std::string password = "TestPassword123!";

    auto result = service.derive_kek_from_password(password, DEFAULT_PBKDF2_ITERATIONS);

    ASSERT_TRUE(result.has_value()) << "KEK derivation should succeed";
    EXPECT_EQ(result->kek.size(), 32) << "KEK should be 256 bits (32 bytes)";
    EXPECT_EQ(result->salt.size(), 32) << "Salt should be 256 bits (32 bytes)";

    // Verify KEK and salt are not all zeros
    bool kek_all_zeros = std::all_of(result->kek.begin(), result->kek.end(),
                                       [](uint8_t b) { return b == 0; });
    bool salt_all_zeros = std::all_of(result->salt.begin(), result->salt.end(),
                                        [](uint8_t b) { return b == 0; });

    EXPECT_FALSE(kek_all_zeros) << "KEK should not be all zeros";
    EXPECT_FALSE(salt_all_zeros) << "Salt should not be all zeros";
}

TEST_F(VaultCryptoServiceTest, DeriveKEK_DifferentPasswords) {
    const std::string password1 = "Password1";
    const std::string password2 = "Password2";

    auto result1 = service.derive_kek_from_password(password1, DEFAULT_PBKDF2_ITERATIONS);
    auto result2 = service.derive_kek_from_password(password2, DEFAULT_PBKDF2_ITERATIONS);

    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());

    // Different passwords should produce different KEKs
    EXPECT_NE(result1->kek, result2->kek) << "Different passwords should produce different KEKs";
    EXPECT_NE(result1->salt, result2->salt) << "Each derivation should use unique salt";
}

TEST_F(VaultCryptoServiceTest, DeriveKEK_SamePasswordDifferentSalt) {
    const std::string password = "SamePassword";

    auto result1 = service.derive_kek_from_password(password, DEFAULT_PBKDF2_ITERATIONS);
    auto result2 = service.derive_kek_from_password(password, DEFAULT_PBKDF2_ITERATIONS);

    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());

    // Same password with different salts should produce different KEKs
    EXPECT_NE(result1->kek, result2->kek) << "Same password with different salts should produce different KEKs";
    EXPECT_NE(result1->salt, result2->salt) << "Each call should generate unique salt";
}

TEST_F(VaultCryptoServiceTest, DeriveKEKWithSalt_Deterministic) {
    const std::string password = "TestPassword";

    // First derive with new salt
    auto result1 = service.derive_kek_from_password(password, DEFAULT_PBKDF2_ITERATIONS);
    ASSERT_TRUE(result1.has_value());

    // Derive again using the same salt
    auto result2 = service.derive_kek_with_salt(password, result1->salt, DEFAULT_PBKDF2_ITERATIONS);
    ASSERT_TRUE(result2.has_value());

    // Should produce the same KEK when using same password and salt
    EXPECT_EQ(result1->kek, *result2) << "Same password and salt should produce same KEK";
}

TEST_F(VaultCryptoServiceTest, DeriveKEK_EmptyPassword) {
    const std::string empty_password = "";

    auto result = service.derive_kek_from_password(empty_password, DEFAULT_PBKDF2_ITERATIONS);

    // Should still succeed (PBKDF2 can handle empty passwords)
    ASSERT_TRUE(result.has_value()) << "KEK derivation should handle empty password";
    EXPECT_EQ(result->kek.size(), 32);
}

// ============================================================================
// Key Wrapping Tests
// ============================================================================

TEST_F(VaultCryptoServiceTest, WrapUnwrapDEK_RoundTrip) {
    const std::string password = "WrapTestPassword";

    // Generate DEK
    auto dek_result = service.generate_dek();
    ASSERT_TRUE(dek_result.has_value());
    const auto& original_dek = dek_result->dek;

    // Derive KEK
    auto kek_result = service.derive_kek_from_password(password, DEFAULT_PBKDF2_ITERATIONS);
    ASSERT_TRUE(kek_result.has_value());
    const auto& kek = kek_result->kek;

    // Wrap DEK
    auto wrapped_result = service.wrap_dek(kek, original_dek);
    ASSERT_TRUE(wrapped_result.has_value()) << "DEK wrapping should succeed";
    EXPECT_EQ(wrapped_result->size(), 40) << "Wrapped DEK should be 40 bytes (RFC 3394)";

    // Unwrap DEK
    auto unwrapped_result = service.unwrap_dek(kek, *wrapped_result);
    ASSERT_TRUE(unwrapped_result.has_value()) << "DEK unwrapping should succeed";

    // Verify round-trip produces original DEK
    EXPECT_EQ(original_dek, *unwrapped_result) << "Unwrapped DEK should match original";
}

TEST_F(VaultCryptoServiceTest, UnwrapDEK_WrongKEK) {
    const std::string password1 = "CorrectPassword";
    const std::string password2 = "WrongPassword";

    // Generate and wrap DEK with first KEK
    auto dek_result = service.generate_dek();
    ASSERT_TRUE(dek_result.has_value());

    auto kek1_result = service.derive_kek_from_password(password1, DEFAULT_PBKDF2_ITERATIONS);
    ASSERT_TRUE(kek1_result.has_value());

    auto wrapped_result = service.wrap_dek(kek1_result->kek, dek_result->dek);
    ASSERT_TRUE(wrapped_result.has_value());

    // Try to unwrap with different KEK
    auto kek2_result = service.derive_kek_from_password(password2, DEFAULT_PBKDF2_ITERATIONS);
    ASSERT_TRUE(kek2_result.has_value());

    auto unwrapped_result = service.unwrap_dek(kek2_result->kek, *wrapped_result);

    // Should fail (wrong KEK)
    EXPECT_FALSE(unwrapped_result.has_value()) << "Unwrapping with wrong KEK should fail";
}

TEST_F(VaultCryptoServiceTest, UnwrapDEK_InvalidSize) {
    std::array<uint8_t, 32> kek{};
    std::vector<uint8_t> invalid_wrapped_dek(30);  // Wrong size (should be 40)

    auto result = service.unwrap_dek(kek, invalid_wrapped_dek);

    EXPECT_FALSE(result.has_value()) << "Unwrapping with invalid size should fail";
    EXPECT_EQ(result.error(), VaultError::CryptoError);
}

// ============================================================================
// Vault Data Encryption Tests
// ============================================================================

TEST_F(VaultCryptoServiceTest, EncryptDecryptVaultData_RoundTrip) {
    // Generate DEK
    auto dek_result = service.generate_dek();
    ASSERT_TRUE(dek_result.has_value());
    const auto& dek = dek_result->dek;

    // Test data
    const std::vector<uint8_t> plaintext = {
        'H', 'e', 'l', 'l', 'o', ' ', 'V', 'a', 'u', 'l', 't', '!'
    };

    // Encrypt
    auto encrypted_result = service.encrypt_vault_data(plaintext, dek);
    ASSERT_TRUE(encrypted_result.has_value()) << "Encryption should succeed";
    EXPECT_GT(encrypted_result->ciphertext.size(), 0) << "Ciphertext should not be empty";
    EXPECT_EQ(encrypted_result->iv.size(), 12) << "IV should be 96 bits (12 bytes) for GCM";

    // Ciphertext should be different from plaintext
    EXPECT_NE(encrypted_result->ciphertext, plaintext) << "Ciphertext should differ from plaintext";

    // Decrypt
    auto decrypted_result = service.decrypt_vault_data(
        encrypted_result->ciphertext,
        dek,
        encrypted_result->iv
    );
    ASSERT_TRUE(decrypted_result.has_value()) << "Decryption should succeed";

    // Verify round-trip
    EXPECT_EQ(*decrypted_result, plaintext) << "Decrypted data should match original";
}

TEST_F(VaultCryptoServiceTest, EncryptVaultData_UniqueIV) {
    auto dek_result = service.generate_dek();
    ASSERT_TRUE(dek_result.has_value());
    const auto& dek = dek_result->dek;

    const std::vector<uint8_t> plaintext = {'T', 'e', 's', 't'};

    // Encrypt same plaintext twice
    auto result1 = service.encrypt_vault_data(plaintext, dek);
    auto result2 = service.encrypt_vault_data(plaintext, dek);

    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());

    // IVs should be different (random)
    EXPECT_NE(result1->iv, result2->iv) << "Each encryption should use unique IV";

    // Ciphertexts should be different (due to different IVs)
    EXPECT_NE(result1->ciphertext, result2->ciphertext) << "Different IVs should produce different ciphertexts";
}

TEST_F(VaultCryptoServiceTest, DecryptVaultData_WrongDEK) {
    // Generate two different DEKs
    auto dek1_result = service.generate_dek();
    auto dek2_result = service.generate_dek();
    ASSERT_TRUE(dek1_result.has_value());
    ASSERT_TRUE(dek2_result.has_value());

    const std::vector<uint8_t> plaintext = {'S', 'e', 'c', 'r', 'e', 't'};

    // Encrypt with first DEK
    auto encrypted_result = service.encrypt_vault_data(plaintext, dek1_result->dek);
    ASSERT_TRUE(encrypted_result.has_value());

    // Try to decrypt with second DEK
    auto decrypted_result = service.decrypt_vault_data(
        encrypted_result->ciphertext,
        dek2_result->dek,
        encrypted_result->iv
    );

    // Should fail (wrong DEK)
    EXPECT_FALSE(decrypted_result.has_value()) << "Decryption with wrong DEK should fail";
}

TEST_F(VaultCryptoServiceTest, DecryptVaultData_CorruptedCiphertext) {
    auto dek_result = service.generate_dek();
    ASSERT_TRUE(dek_result.has_value());
    const auto& dek = dek_result->dek;

    const std::vector<uint8_t> plaintext = {'D', 'a', 't', 'a'};

    // Encrypt
    auto encrypted_result = service.encrypt_vault_data(plaintext, dek);
    ASSERT_TRUE(encrypted_result.has_value());

    // Corrupt the ciphertext
    auto corrupted_ciphertext = encrypted_result->ciphertext;
    if (!corrupted_ciphertext.empty()) {
        corrupted_ciphertext[0] ^= 0xFF;  // Flip all bits in first byte
    }

    // Try to decrypt corrupted data
    auto decrypted_result = service.decrypt_vault_data(
        corrupted_ciphertext,
        dek,
        encrypted_result->iv
    );

    // Should fail (authentication tag mismatch in GCM mode)
    EXPECT_FALSE(decrypted_result.has_value()) << "Decryption of corrupted data should fail";
}

TEST_F(VaultCryptoServiceTest, EncryptDecryptVaultData_EmptyData) {
    auto dek_result = service.generate_dek();
    ASSERT_TRUE(dek_result.has_value());
    const auto& dek = dek_result->dek;

    const std::vector<uint8_t> empty_plaintext;

    // Encrypt empty data
    auto encrypted_result = service.encrypt_vault_data(empty_plaintext, dek);
    ASSERT_TRUE(encrypted_result.has_value()) << "Encrypting empty data should succeed";

    // Decrypt
    auto decrypted_result = service.decrypt_vault_data(
        encrypted_result->ciphertext,
        dek,
        encrypted_result->iv
    );
    ASSERT_TRUE(decrypted_result.has_value());

    EXPECT_EQ(*decrypted_result, empty_plaintext) << "Should round-trip empty data correctly";
}

TEST_F(VaultCryptoServiceTest, EncryptDecryptVaultData_LargeData) {
    auto dek_result = service.generate_dek();
    ASSERT_TRUE(dek_result.has_value());
    const auto& dek = dek_result->dek;

    // Create 1MB of test data
    std::vector<uint8_t> large_plaintext(1024 * 1024);
    for (size_t i = 0; i < large_plaintext.size(); ++i) {
        large_plaintext[i] = static_cast<uint8_t>(i % 256);
    }

    // Encrypt
    auto encrypted_result = service.encrypt_vault_data(large_plaintext, dek);
    ASSERT_TRUE(encrypted_result.has_value()) << "Encrypting large data should succeed";

    // Decrypt
    auto decrypted_result = service.decrypt_vault_data(
        encrypted_result->ciphertext,
        dek,
        encrypted_result->iv
    );
    ASSERT_TRUE(decrypted_result.has_value()) << "Decrypting large data should succeed";

    EXPECT_EQ(*decrypted_result, large_plaintext) << "Should round-trip large data correctly";
}

// ============================================================================
// PIN Encryption Tests
// ============================================================================

TEST_F(VaultCryptoServiceTest, EncryptDecryptPIN_RoundTrip) {
    auto dek_result = service.generate_dek();
    ASSERT_TRUE(dek_result.has_value());
    const auto& dek = dek_result->dek;

    const std::string pin = "123456";

    // Encrypt PIN
    auto encrypted_result = service.encrypt_pin(pin, dek);
    ASSERT_TRUE(encrypted_result.has_value()) << "PIN encryption should succeed";
    EXPECT_GT(encrypted_result->encrypted_pin.size(), 12) << "Encrypted PIN should include IV + ciphertext";

    // Decrypt PIN
    auto decrypted_result = service.decrypt_pin(encrypted_result->encrypted_pin, dek);
    ASSERT_TRUE(decrypted_result.has_value()) << "PIN decryption should succeed";

    EXPECT_EQ(*decrypted_result, pin) << "Decrypted PIN should match original";
}

TEST_F(VaultCryptoServiceTest, EncryptPIN_UniqueOutput) {
    auto dek_result = service.generate_dek();
    ASSERT_TRUE(dek_result.has_value());
    const auto& dek = dek_result->dek;

    const std::string pin = "654321";

    // Encrypt same PIN twice
    auto result1 = service.encrypt_pin(pin, dek);
    auto result2 = service.encrypt_pin(pin, dek);

    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());

    // Encrypted PINs should be different (different IVs)
    EXPECT_NE(result1->encrypted_pin, result2->encrypted_pin)
        << "Each PIN encryption should produce unique output";
}

TEST_F(VaultCryptoServiceTest, DecryptPIN_WrongDEK) {
    auto dek1_result = service.generate_dek();
    auto dek2_result = service.generate_dek();
    ASSERT_TRUE(dek1_result.has_value());
    ASSERT_TRUE(dek2_result.has_value());

    const std::string pin = "789012";

    // Encrypt with first DEK
    auto encrypted_result = service.encrypt_pin(pin, dek1_result->dek);
    ASSERT_TRUE(encrypted_result.has_value());

    // Try to decrypt with second DEK
    auto decrypted_result = service.decrypt_pin(encrypted_result->encrypted_pin, dek2_result->dek);

    // Should fail
    EXPECT_FALSE(decrypted_result.has_value()) << "PIN decryption with wrong DEK should fail";
}

TEST_F(VaultCryptoServiceTest, DecryptPIN_InvalidData) {
    auto dek_result = service.generate_dek();
    ASSERT_TRUE(dek_result.has_value());
    const auto& dek = dek_result->dek;

    // Too short data (less than IV size)
    std::vector<uint8_t> invalid_data(10);

    auto result = service.decrypt_pin(invalid_data, dek);

    EXPECT_FALSE(result.has_value()) << "Decrypting invalid PIN data should fail";
}

// ============================================================================
// YubiKey KEK Combination Tests
// ============================================================================

TEST_F(VaultCryptoServiceTest, CombineKEKWithYubiKey_Success) {
    auto kek_result = service.derive_kek_from_password("TestPassword", DEFAULT_PBKDF2_ITERATIONS);
    ASSERT_TRUE(kek_result.has_value());
    const auto& password_kek = kek_result->kek;

    // Simulate YubiKey response (20 bytes)
    std::vector<uint8_t> yubikey_response = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
        0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14
    };

    auto combined_result = service.combine_kek_with_yubikey(password_kek, yubikey_response);

    ASSERT_TRUE(combined_result.has_value()) << "KEK combination should succeed";
    EXPECT_EQ(combined_result->size(), 32) << "Combined KEK should be 256 bits";

    // Combined KEK should be different from password-only KEK
    std::array<uint8_t, 32> combined_array;
    std::copy(combined_result->begin(), combined_result->end(), combined_array.begin());
    EXPECT_NE(combined_array, password_kek) << "Combined KEK should differ from password-only KEK";
}

TEST_F(VaultCryptoServiceTest, CombineKEKWithYubiKey_Deterministic) {
    auto kek_result = service.derive_kek_from_password("Password", DEFAULT_PBKDF2_ITERATIONS);
    ASSERT_TRUE(kek_result.has_value());
    const auto& kek = kek_result->kek;

    std::vector<uint8_t> yubikey_response = {
        0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11, 0x22, 0x33,
        0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD
    };

    // Combine twice with same inputs
    auto result1 = service.combine_kek_with_yubikey(kek, yubikey_response);
    auto result2 = service.combine_kek_with_yubikey(kek, yubikey_response);

    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());

    // Should produce same result (deterministic)
    EXPECT_EQ(*result1, *result2) << "KEK combination should be deterministic";
}

// ============================================================================
// Secure Clear Tests
// ============================================================================

TEST_F(VaultCryptoServiceTest, SecureClear_RawPointer) {
    std::array<uint8_t, 32> sensitive_data;
    sensitive_data.fill(0xAB);

    // Clear the data
    service.secure_clear(sensitive_data.data(), sensitive_data.size());

    // Verify all bytes are zero
    bool all_zeros = std::all_of(sensitive_data.begin(), sensitive_data.end(),
                                   [](uint8_t b) { return b == 0; });
    EXPECT_TRUE(all_zeros) << "secure_clear should zero all bytes";
}

TEST_F(VaultCryptoServiceTest, SecureClear_Vector) {
    std::vector<uint8_t> sensitive_data(64, 0xCD);

    // Clear the data
    service.secure_clear(sensitive_data);

    // Verify all bytes are zero
    bool all_zeros = std::all_of(sensitive_data.begin(), sensitive_data.end(),
                                   [](uint8_t b) { return b == 0; });
    EXPECT_TRUE(all_zeros) << "secure_clear should zero all vector bytes";
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
