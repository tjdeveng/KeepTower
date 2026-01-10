// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

/**
 * @file VaultCryptoService.h
 * @brief Service class for vault cryptographic operations
 *
 * This service handles ALL cryptographic operations for vault creation
 * and management, following the Single Responsibility Principle.
 *
 * Responsibilities:
 * - DEK (Data Encryption Key) generation
 * - KEK (Key Encryption Key) derivation from passwords
 * - Key wrapping/unwrapping
 * - Vault data encryption/decryption
 * - PIN encryption
 *
 * NOT responsible for:
 * - YubiKey hardware operations (see VaultYubiKeyService)
 * - File I/O operations (see VaultFileService)
 * - Vault state management (see VaultManager)
 */

#pragma once

#include "../VaultError.h"
#include <array>
#include <vector>
#include <glibmm/ustring.h>
#include <expected>

namespace KeepTower {

/**
 * @class VaultCryptoService
 * @brief Pure cryptographic operations service for vault management
 *
 * This class provides stateless cryptographic operations. All methods
 * are [[nodiscard]] to ensure results are used. No side effects on
 * external systems (no file I/O, no hardware access).
 *
 * Thread-safety: All methods are thread-safe (no shared mutable state)
 * FIPS-compliance: Uses OpenSSL FIPS-approved algorithms when available
 */
class VaultCryptoService {
public:
    // ========================================================================
    // Result Types
    // ========================================================================

    /**
     * @brief Result of DEK generation with memory lock status
     */
    struct DEKResult {
        std::array<uint8_t, 32> dek;  ///< 256-bit Data Encryption Key
        bool memory_locked;            ///< True if mlock() succeeded
    };

    /**
     * @brief Result of KEK derivation including the generated salt
     */
    struct KEKResult {
        std::array<uint8_t, 32> kek;   ///< 256-bit Key Encryption Key
        std::array<uint8_t, 32> salt;  ///< 256-bit random salt used
    };

    /**
     * @brief Result of data encryption with IV
     */
    struct EncryptionResult {
        std::vector<uint8_t> ciphertext;  ///< Encrypted data + auth tag
        std::vector<uint8_t> iv;          ///< 12-byte IV used
    };

    /**
     * @brief Result of PIN encryption with storage format (IV + ciphertext)
     */
    struct PINEncryptionResult {
        std::vector<uint8_t> encrypted_pin;  ///< [IV(12) || ciphertext+tag]
    };

    // ========================================================================
    // Public Interface
    // ========================================================================

    /**
     * @brief Constructor (default)
     */
    VaultCryptoService() = default;

    /**
     * @brief Generate a random 256-bit DEK for vault encryption
     *
     * Uses OpenSSL's FIPS-approved random number generator.
     * Attempts to lock the DEK in memory with mlock() to prevent
     * swapping to disk (best effort, not guaranteed).
     *
     * @return DEKResult with generated DEK and lock status, or VaultError
     *
     * @note FIPS-140-3 compliant when OpenSSL FIPS module enabled
     * @note Memory locking may fail on systems with restrictions
     */
    [[nodiscard]] VaultResult<DEKResult> generate_dek();

    /**
     * @brief Derive KEK from password using PBKDF2-HMAC-SHA256
     *
     * Generates a random salt and derives a 256-bit KEK using PBKDF2.
     * This is an expensive operation (takes ~100-500ms depending on iterations).
     *
     * @param password User's password (UTF-8 encoded)
     * @param iterations PBKDF2 iteration count (100,000+ recommended)
     * @return KEKResult with derived KEK and generated salt, or VaultError
     *
     * @note FIPS-140-3 compliant when OpenSSL FIPS module enabled
     * @note Higher iterations = better security but slower (tune for UX)
     */
    [[nodiscard]] VaultResult<KEKResult> derive_kek_from_password(
        const Glib::ustring& password,
        uint32_t iterations);

    /**
     * @brief Derive KEK from password with provided salt
     *
     * Same as derive_kek_from_password() but uses an existing salt.
     * Used when opening vaults (salt stored in key slot).
     *
     * @param password User's password (UTF-8 encoded)
     * @param salt 256-bit salt from key slot
     * @param iterations PBKDF2 iteration count
     * @return 256-bit KEK or VaultError
     *
     * @note FIPS-140-3 compliant when OpenSSL FIPS module enabled
     */
    [[nodiscard]] VaultResult<std::array<uint8_t, 32>> derive_kek_with_salt(
        const Glib::ustring& password,
        const std::array<uint8_t, 32>& salt,
        uint32_t iterations);

    /**
     * @brief Wrap (encrypt) DEK with KEK using AES-256-KeyWrap
     *
     * Uses RFC 3394 AES Key Wrap algorithm with 256-bit KEK.
     * The wrapped key includes integrity protection.
     *
     * @param kek 256-bit Key Encryption Key
     * @param dek 256-bit Data Encryption Key to wrap
     * @return Wrapped key bytes (40 bytes) or VaultError
     *
     * @note FIPS-140-3 compliant when OpenSSL FIPS module enabled
     * @note Output is 40 bytes (32-byte key + 8-byte integrity)
     */
    [[nodiscard]] VaultResult<std::vector<uint8_t>> wrap_dek(
        const std::array<uint8_t, 32>& kek,
        const std::array<uint8_t, 32>& dek);

    /**
     * @brief Unwrap (decrypt) DEK using KEK via AES-256-KeyWrap
     *
     * Reverses wrap_dek() operation. Verifies integrity before returning.
     *
     * @param kek 256-bit Key Encryption Key
     * @param wrapped_dek Wrapped key from wrap_dek() (40 bytes)
     * @return Unwrapped 256-bit DEK or VaultError
     *
     * @note FIPS-140-3 compliant when OpenSSL FIPS module enabled
     * @note Returns error if integrity check fails
     */
    [[nodiscard]] VaultResult<std::array<uint8_t, 32>> unwrap_dek(
        const std::array<uint8_t, 32>& kek,
        const std::vector<uint8_t>& wrapped_dek);

    /**
     * @brief Encrypt vault data using AES-256-GCM
     *
     * Encrypts plaintext with DEK using AES-256-GCM authenticated encryption.
     * Generates random 12-byte IV. Output includes authentication tag.
     *
     * @param plaintext Serialized vault data to encrypt
     * @param dek 256-bit Data Encryption Key
     * @return EncryptionResult with ciphertext+tag and IV, or VaultError
     *
     * @note FIPS-140-3 compliant when OpenSSL FIPS module enabled
     * @note Ciphertext includes 16-byte GCM authentication tag
     */
    [[nodiscard]] VaultResult<EncryptionResult> encrypt_vault_data(
        const std::vector<uint8_t>& plaintext,
        const std::array<uint8_t, 32>& dek);

    /**
     * @brief Decrypt vault data using AES-256-GCM
     *
     * Reverses encrypt_vault_data() operation. Verifies authentication tag.
     *
     * @param ciphertext Encrypted data with auth tag
     * @param dek 256-bit Data Encryption Key
     * @param iv 12-byte IV used during encryption
     * @return Decrypted plaintext or VaultError
     *
     * @note FIPS-140-3 compliant when OpenSSL FIPS module enabled
     * @note Returns error if authentication tag verification fails
     */
    [[nodiscard]] VaultResult<std::vector<uint8_t>> decrypt_vault_data(
        const std::vector<uint8_t>& ciphertext,
        const std::array<uint8_t, 32>& dek,
        const std::vector<uint8_t>& iv);

    /**
     * @brief Encrypt YubiKey PIN with KEK for secure storage
     *
     * Encrypts PIN using AES-256-GCM with password-derived KEK.
     * Returns storage-ready format: [IV(12) || ciphertext+tag].
     *
     * @param pin YubiKey PIN to encrypt (4-63 characters)
     * @param kek Password-derived KEK (NOT combined with YubiKey yet)
     * @return PINEncryptionResult with [IV || encrypted_pin] or VaultError
     *
     * @note PIN must be encrypted with password-only KEK to avoid circular
     *       dependency (need PIN to get YubiKey response to derive final KEK)
     * @note FIPS-140-3 compliant when OpenSSL FIPS module enabled
     */
    [[nodiscard]] VaultResult<PINEncryptionResult> encrypt_pin(
        const std::string& pin,
        const std::array<uint8_t, 32>& kek);

    /**
     * @brief Decrypt YubiKey PIN from storage format
     *
     * Reverses encrypt_pin() operation. Extracts IV and decrypts.
     *
     * @param encrypted_pin Storage format [IV(12) || ciphertext+tag]
     * @param kek Password-derived KEK used during encryption
     * @return Decrypted PIN string or VaultError
     *
     * @note FIPS-140-3 compliant when OpenSSL FIPS module enabled
     */
    [[nodiscard]] VaultResult<std::string> decrypt_pin(
        const std::vector<uint8_t>& encrypted_pin,
        const std::array<uint8_t, 32>& kek);

    /**
     * @brief Combine password-derived KEK with YubiKey response
     *
     * XORs password KEK with YubiKey HMAC response to create final KEK.
     * This implements two-factor authentication at the cryptographic level.
     *
     * @param password_kek KEK derived from user's password
     * @param yubikey_response HMAC response from YubiKey (20-64 bytes)
     * @return Combined KEK (256 bits) or VaultError
     *
     * @note If YubiKey response < 32 bytes, it's repeated to fill 256 bits
     * @note Formula: final_kek = password_kek XOR extend(yubikey_response)
     */
    [[nodiscard]] VaultResult<std::array<uint8_t, 32>> combine_kek_with_yubikey(
        const std::array<uint8_t, 32>& password_kek,
        const std::vector<uint8_t>& yubikey_response);

    // ========================================================================
    // Utility Methods
    // ========================================================================

    /**
     * @brief Securely clear sensitive data from memory
     *
     * Uses OPENSSL_cleanse() to prevent compiler optimization from
     * removing the memory clearing operation.
     *
     * @param data Buffer to clear
     * @param size Size of buffer in bytes
     *
     * @note Always use this instead of memset() for sensitive data
     */
    static void secure_clear(void* data, size_t size);

    /**
     * @brief Securely clear sensitive data from vector
     *
     * Convenience overload for std::vector.
     *
     * @param data Vector to clear
     */
    static void secure_clear(std::vector<uint8_t>& data);

private:
    // No state - all methods are stateless
};

} // namespace KeepTower
