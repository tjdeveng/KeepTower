// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2024 Travis E. Hansen

#ifndef KEEPTOWER_VAULT_CRYPTO_H
#define KEEPTOWER_VAULT_CRYPTO_H

#include <vector>
#include <span>
#include <glibmm/ustring.h>
#include <cstdint>

// Forward declare SecureVector to avoid pulling in OpenSSL headers
namespace KeepTower {
    template<typename T> class SecureAllocator;
    template<typename T> using SecureVector = std::vector<T, SecureAllocator<T>>;
}

namespace KeepTower {

/**
 * @brief Cryptographic operations for vault encryption
 *
 * Provides NIST-compliant cryptographic primitives for vault data protection:
 * - PBKDF2-HMAC-SHA256 key derivation
 * - AES-256-GCM authenticated encryption
 * - Cryptographically secure random generation
 *
 * This class is stateless and thread-safe. All methods are static.
 *
 * @section security Security Features
 * - NIST SP 800-132 compliant key derivation
 * - NIST SP 800-38D compliant AEAD encryption (GCM mode)
 * - Configurable PBKDF2 iterations (default: 600,000)
 * - 256-bit key length
 * - 96-bit IV for GCM (recommended size)
 * - 128-bit authentication tag
 *
 * @section usage Usage Example
 * @code
 * // Key derivation
 * std::vector<uint8_t> salt = VaultCrypto::generate_random_bytes(16);
 * std::vector<uint8_t> key(32);
 * if (!VaultCrypto::derive_key("password", salt, key, 600000)) {
 *     // Handle error
 * }
 *
 * // Encryption
 * std::vector<uint8_t> plaintext = {...};
 * std::vector<uint8_t> iv = VaultCrypto::generate_random_bytes(12);
 * std::vector<uint8_t> ciphertext;
 * if (!VaultCrypto::encrypt_data(plaintext, key, ciphertext, iv)) {
 *     // Handle error
 * }
 *
 * // Decryption
 * std::vector<uint8_t> decrypted;
 * if (!VaultCrypto::decrypt_data(ciphertext, key, iv, decrypted)) {
 *     // Handle error
 * }
 * @endcode
 */
class VaultCrypto {
public:
    // Cryptographic constants
    static constexpr size_t KEY_LENGTH = 32;        ///< AES-256 key length (256 bits)
    static constexpr size_t SALT_LENGTH = 16;       ///< Salt length (128 bits)
    static constexpr size_t IV_LENGTH = 12;         ///< GCM IV length (96 bits, recommended)
    static constexpr size_t TAG_LENGTH = 16;        ///< GCM authentication tag length (128 bits)
    static constexpr int DEFAULT_PBKDF2_ITERATIONS = 600000;  ///< NIST recommended minimum (2023)

    /**
     * @brief Derive encryption key from password using PBKDF2-HMAC-SHA256
     *
     * @param password User password (UTF-8 encoded)
     * @param salt Cryptographic salt (minimum 128 bits recommended)
     * @param key Output buffer for derived key (must be pre-allocated to KEY_LENGTH)
     * @param iterations Number of PBKDF2 iterations (default: 600,000)
     * @return true if successful, false on error
     *
     * @note Implements NIST SP 800-132 key derivation
     * @note Higher iteration counts increase resistance to brute-force attacks
     */
    [[nodiscard]] static bool derive_key(
        const Glib::ustring& password,
        std::span<const uint8_t> salt,
        std::vector<uint8_t>& key,
        int iterations = DEFAULT_PBKDF2_ITERATIONS);

    /**
     * @brief Encrypt data using AES-256-GCM
     *
     * @param plaintext Data to encrypt
     * @param key Encryption key (must be KEY_LENGTH bytes)
     * @param ciphertext Output encrypted data (includes authentication tag)
     * @param iv Initialization vector (must be IV_LENGTH bytes, must be unique per encryption)
     * @return true if successful, false on error
     *
     * @note GCM provides authenticated encryption (AEAD)
     * @note Output ciphertext includes 16-byte authentication tag appended
     * @note IV must be unique for each encryption with the same key
     *
     * @warning Never reuse the same IV with the same key - this breaks GCM security!
     */
    [[nodiscard]] static bool encrypt_data(
        std::span<const uint8_t> plaintext,
        std::span<const uint8_t> key,
        std::vector<uint8_t>& ciphertext,
        std::span<const uint8_t> iv);

    /**
     * @brief Decrypt and authenticate data using AES-256-GCM
     *
     * @param ciphertext Encrypted data (includes authentication tag)
     * @param key Decryption key (must be KEY_LENGTH bytes)
     * @param iv Initialization vector (must be IV_LENGTH bytes)
     * @param plaintext Output decrypted data
     * @return true if successful and authenticated, false if authentication fails or error
     *
     * @note Verifies authentication tag before returning plaintext
     * @note Returns false if ciphertext has been tampered with
     * @note Plaintext is only valid if function returns true
     */
    [[nodiscard]] static bool decrypt_data(
        std::span<const uint8_t> ciphertext,
        std::span<const uint8_t> key,
        std::span<const uint8_t> iv,
        std::vector<uint8_t>& plaintext);

    /**
     * @brief Generate cryptographically secure random bytes
     *
     * @param length Number of bytes to generate
     * @return Vector of random bytes
     *
     * @note Uses OpenSSL RAND_bytes() for CSPRNG
     * @note Suitable for salts, IVs, and keys
     */
    [[nodiscard]] static std::vector<uint8_t> generate_random_bytes(size_t length);

    // VaultCrypto is a utility class - no instances needed
    VaultCrypto() = delete;
    ~VaultCrypto() = delete;
    VaultCrypto(const VaultCrypto&) = delete;
    VaultCrypto& operator=(const VaultCrypto&) = delete;
    VaultCrypto(VaultCrypto&&) = delete;
    VaultCrypto& operator=(VaultCrypto&&) = delete;
};

}  // namespace KeepTower

#endif  // KEEPTOWER_VAULT_CRYPTO_H
