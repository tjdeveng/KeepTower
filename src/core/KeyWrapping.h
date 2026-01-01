// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file KeyWrapping.h
 * @brief AES-256-KW key wrapping for multi-user vault encryption
 *
 * Implements NIST SP 800-38F key wrapping (AES-KW) for protecting
 * Data Encryption Keys (DEKs) with user-specific Key Encryption Keys (KEKs).
 *
 * This is FIPS-140-3 approved and used in LUKS2, TPM 2.0, and PKCS#11.
 */

#ifndef KEYWRAPPING_H
#define KEYWRAPPING_H

#include <array>
#include <vector>
#include <cstdint>
#include <expected>
#include <string>
#include <glibmm/ustring.h>

namespace KeepTower {

/**
 * @brief AES-256-KW key wrapping and unwrapping operations
 *
 * Provides cryptographic key wrapping using AES-256 in Key Wrap mode
 * (RFC 3394, NIST SP 800-38F). This protects the vault's Data Encryption
 * Key (DEK) by encrypting it with each user's Key Encryption Key (KEK).
 *
 * @section algorithm Algorithm Details
 * - Key Encryption Key (KEK): 32 bytes (256 bits), derived from user password
 * - Data Encryption Key (DEK): 32 bytes (256 bits), encrypts vault data
 * - Wrapped output: 40 bytes (DEK + 8-byte integrity tag)
 * - Mode: AES-256-KW (RFC 3394)
 * - Integrity: Built-in verification (unwrap fails if KEK is wrong)
 *
 * @section security Security Properties
 * - FIPS-140-3 approved (NIST SP 800-38F)
 * - Authenticated encryption (integrity + confidentiality)
 * - Deterministic (same KEK + DEK = same wrapped output)
 * - No IV required (uses internal constant)
 * - Fails safely (unwrap returns error if tampered)
 *
 * @section usage Usage Example
 * @code
 * // Key wrapping (when adding user or changing password)
 * std::array<uint8_t, 32> kek = derive_kek_from_password(password, salt);
 * std::array<uint8_t, 32> dek = generate_random_dek();
 *
 * auto wrapped = KeyWrapping::wrap_key(kek, dek);
 * if (wrapped) {
 *     // Store wrapped->wrapped_key in key slot
 * }
 *
 * // Key unwrapping (during authentication)
 * auto unwrapped = KeyWrapping::unwrap_key(kek, wrapped_dek);
 * if (unwrapped) {
 *     // Use unwrapped DEK to decrypt vault
 * } else {
 *     // Wrong password (KEK incorrect)
 * }
 * @endcode
 */
class KeyWrapping {
public:
    /** @brief KEK (Key Encryption Key) size in bytes (256 bits) */
    static constexpr size_t KEK_SIZE = 32;

    /** @brief DEK (Data Encryption Key) size in bytes (256 bits) */
    static constexpr size_t DEK_SIZE = 32;

    /** @brief Wrapped key size in bytes (DEK + integrity tag) */
    static constexpr size_t WRAPPED_KEY_SIZE = 40;

    /** @brief Salt size for PBKDF2 key derivation */
    static constexpr size_t SALT_SIZE = 32;

    /** @brief YubiKey HMAC-SHA1 response size */
    static constexpr size_t YUBIKEY_RESPONSE_SIZE = 20;

    /**
     * @brief Error codes for key wrapping operations
     */
    enum class Error {
        INVALID_KEK_SIZE,      ///< KEK is not 32 bytes
        INVALID_DEK_SIZE,      ///< DEK is not 32 bytes
        INVALID_WRAPPED_SIZE,  ///< Wrapped key is not 40 bytes
        WRAP_FAILED,           ///< OpenSSL wrap operation failed
        UNWRAP_FAILED,         ///< OpenSSL unwrap operation failed (wrong KEK or corrupted data)
        PBKDF2_FAILED,         ///< Password-based key derivation failed
        OPENSSL_ERROR          ///< Generic OpenSSL error
    };

    /**
     * @brief Result of key wrapping operation
     */
    struct WrappedKey {
        std::array<uint8_t, WRAPPED_KEY_SIZE> wrapped_key; ///< Wrapped DEK with integrity tag
    };

    /**
     * @brief Result of key unwrapping operation
     */
    struct UnwrappedKey {
        std::array<uint8_t, DEK_SIZE> dek; ///< Unwrapped Data Encryption Key
    };

    /**
     * @brief Wrap a DEK with a KEK using AES-256-KW
     *
     * Encrypts the Data Encryption Key (DEK) with the Key Encryption Key (KEK)
     * using AES-256 in Key Wrap mode (RFC 3394). The output includes an 8-byte
     * integrity tag that will cause unwrapping to fail if the KEK is incorrect.
     *
     * @param kek Key Encryption Key (32 bytes, derived from user password)
     * @param dek Data Encryption Key (32 bytes, encrypts vault data)
     * @return Wrapped key (40 bytes), or error
     *
     * @note This is a deterministic operation (same inputs = same output)
     * @note FIPS-140-3 approved when FIPS mode is enabled
     */
    [[nodiscard]] static std::expected<WrappedKey, Error>
    wrap_key(const std::array<uint8_t, KEK_SIZE>& kek,
             const std::array<uint8_t, DEK_SIZE>& dek);

    /**
     * @brief Unwrap a DEK using a KEK with AES-256-KW
     *
     * Decrypts and verifies the wrapped Data Encryption Key using the
     * Key Encryption Key. The operation will fail if:
     * - KEK is incorrect (wrong password)
     * - Wrapped data is corrupted
     * - Integrity tag verification fails
     *
     * @param kek Key Encryption Key (32 bytes, derived from user password)
     * @param wrapped_dek Wrapped DEK (40 bytes, from key slot)
     * @return Unwrapped DEK (32 bytes), or error
     *
     * @note Failure indicates wrong password or corrupted key slot
     * @note FIPS-140-3 approved when FIPS mode is enabled
     */
    [[nodiscard]] static std::expected<UnwrappedKey, Error>
    unwrap_key(const std::array<uint8_t, KEK_SIZE>& kek,
               const std::array<uint8_t, WRAPPED_KEY_SIZE>& wrapped_dek);

    /**
     * @brief Derive KEK from password using PBKDF2-HMAC-SHA256
     *
     * Derives a Key Encryption Key from a user password using PBKDF2
     * with HMAC-SHA256. This is the standard NIST-approved method for
     * password-based key derivation.
     *
     * @param password User's password (UTF-8)
     * @param salt Random salt (32 bytes, unique per user)
     * @param iterations PBKDF2 iteration count (e.g., 100,000)
     * @return KEK (32 bytes), or error
     *
     * @note FIPS-140-3 approved when FIPS mode is enabled
     * @note Higher iterations = more secure but slower
     * @note NIST minimum: 100,000 iterations
     */
    [[nodiscard]] static std::expected<std::array<uint8_t, KEK_SIZE>, Error>
    derive_kek_from_password(const Glib::ustring& password,
                             const std::array<uint8_t, SALT_SIZE>& salt,
                             uint32_t iterations);

    /**
     * @brief Combine KEK with YubiKey response (legacy SHA-1, 20 bytes)
     *
     * XORs the KEK with the YubiKey HMAC-SHA1 response to create a
     * two-factor authentication key. This binds the password and YubiKey
     * together (both are required to unwrap the DEK).
     *
     * @param kek Key Encryption Key (from password, 32 bytes)
     * @param yubikey_response YubiKey HMAC-SHA1 response (20 bytes)
     * @return Combined KEK (32 bytes)
     *
     * @note yubikey_response is zero-padded to 32 bytes before XOR
     * @note This matches LUKS2 YubiKey integration approach
     * @deprecated Use combine_with_yubikey_v2() for FIPS compliance
     */
    [[nodiscard]] static std::array<uint8_t, KEK_SIZE>
    combine_with_yubikey(const std::array<uint8_t, KEK_SIZE>& kek,
                         const std::array<uint8_t, YUBIKEY_RESPONSE_SIZE>& yubikey_response);

    /**
     * @brief Combine KEK with YubiKey response (variable-length, FIPS-compliant)
     *
     * XORs the KEK with YubiKey challenge-response output. Supports:
     * - HMAC-SHA1: 20 bytes (legacy, NOT FIPS-approved)
     * - HMAC-SHA256: 32 bytes (FIPS-approved, recommended)
     * - HMAC-SHA512: 64 bytes (FIPS-approved, hashed to 32 bytes)
     *
     * @param kek Key Encryption Key (from password, 32 bytes)
     * @param yubikey_response YubiKey response (20-64 bytes)
     * @return Combined KEK (32 bytes)
     *
     * @note Response > 32 bytes is hashed with SHA-256 to 32 bytes
     * @note Response < 32 bytes is zero-padded to 32 bytes
     * @note For FIPS compliance, use SHA-256 (32-byte response)
     */
    [[nodiscard]] static std::array<uint8_t, KEK_SIZE>
    combine_with_yubikey_v2(const std::array<uint8_t, KEK_SIZE>& kek,
                            const std::vector<uint8_t>& yubikey_response);

    /**
     * @brief Generate random DEK for new vault
     *
     * Generates a cryptographically secure random Data Encryption Key
     * using OpenSSL's RAND_bytes (FIPS DRBG when FIPS mode is enabled).
     *
     * @return DEK (32 bytes), or error
     *
     * @note Use this when creating a new vault (only once)
     * @note All users' key slots will wrap this same DEK
     */
    [[nodiscard]] static std::expected<std::array<uint8_t, DEK_SIZE>, Error>
    generate_random_dek();

    /**
     * @brief Generate random salt for PBKDF2
     *
     * Generates a cryptographically secure random salt using OpenSSL's
     * RAND_bytes (FIPS DRBG when FIPS mode is enabled).
     *
     * @return Salt (32 bytes), or error
     *
     * @note Use this when creating a new user key slot
     * @note Each user must have a unique salt
     */
    [[nodiscard]] static std::expected<std::array<uint8_t, SALT_SIZE>, Error>
    generate_random_salt();

    /**
     * @brief Convert error code to human-readable string
     * @param error Error code
     * @return Error description
     */
    [[nodiscard]] static std::string error_to_string(Error error);
};

} // namespace KeepTower

#endif // KEYWRAPPING_H
