// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

/**
 * @file UsernameHashService.h
 * @brief Service for cryptographic hashing of usernames
 *
 * This service handles username hashing operations for vault security,
 * following the Single Responsibility Principle.
 *
 * Responsibilities:
 * - Compute cryptographic hashes of usernames
 * - Verify username hashes
 * - Support multiple FIPS-approved algorithms
 *
 * NOT responsible for:
 * - Vault operations (see VaultManager)
 * - User authentication (see VaultManager)
 * - Preferences management (see SettingsManager)
 * - UI operations (see PreferencesDialog)
 *
 * FIPS Compliance:
 * - SHA3-256, SHA3-384, SHA3-512: FIPS-approved (FIPS 202)
 * - PBKDF2-HMAC-SHA256: FIPS-approved (SP 800-132)
 * - Argon2id: NOT FIPS-approved (blocked in FIPS mode)
 */

#pragma once

#include "../VaultError.h"
#include <array>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <vector>

namespace KeepTower {

/**
 * @class UsernameHashService
 * @brief Pure cryptographic hashing service for usernames
 *
 * This class provides stateless username hashing operations. All methods
 * are static and [[nodiscard]] to ensure results are used. No side effects.
 *
 * Thread-safety: All methods are thread-safe (no shared mutable state)
 * FIPS-compliance: Uses OpenSSL FIPS-approved algorithms (SHA-3, PBKDF2)
 *
 * @section usage Usage Example
 * @code
 * // Generate random salt
 * std::array<uint8_t, 16> salt = generate_random_salt();
 *
 * // Hash username with SHA3-256 (recommended)
 * auto hash_result = UsernameHashService::hash_username(
 *     "alice",
 *     UsernameHashService::Algorithm::SHA3_256,
 *     salt
 * );
 *
 * if (hash_result.has_value()) {
 *     std::vector<uint8_t> hash = hash_result.value();
 *     // Store hash in vault's KeySlot
 * }
 *
 * // Later: Verify username during authentication
 * bool verified = UsernameHashService::verify_username(
 *     "alice",
 *     stored_hash,
 *     UsernameHashService::Algorithm::SHA3_256,
 *     salt
 * );
 * @endcode
 */
class UsernameHashService {
public:
    /**
     * @brief Username hashing algorithms
     *
     * All algorithms (except PLAINTEXT_LEGACY) produce unique hashes from
     * username + salt combination. Values match VaultSecurityPolicy encoding.
     */
    enum class Algorithm : uint8_t {
        PLAINTEXT_LEGACY = 0x00,  ///< No hashing (legacy, NOT RECOMMENDED)
        SHA3_256 = 0x01,          ///< SHA3-256 (32 bytes, FIPS-approved, DEFAULT)
        SHA3_384 = 0x02,          ///< SHA3-384 (48 bytes, FIPS-approved)
        SHA3_512 = 0x03,          ///< SHA3-512 (64 bytes, FIPS-approved)
        PBKDF2_SHA256 = 0x04,     ///< PBKDF2-HMAC-SHA256 (32 bytes, FIPS-approved)
        ARGON2ID = 0x05           ///< Argon2id (32 bytes, NOT FIPS-approved)
    };

    // ========================================================================
    // Hashing Operations
    // ========================================================================

    /**
     * @brief Compute cryptographic hash of username
     *
     * Hashes the given username using the specified algorithm and salt.
     * Combines username + salt before hashing to prevent rainbow table attacks.
     *
     * FIPS Compliance:
     * - SHA3-256, SHA3-384, SHA3-512: FIPS-approved (FIPS 202)
     * - PBKDF2-SHA256: FIPS-approved (SP 800-132)
     * - Argon2id: NOT FIPS-approved (returns error in FIPS mode)
     *
     * @param username Plaintext username to hash (UTF-8 encoded)
     * @param algorithm Hash algorithm to use
     * @param salt 16-byte random salt (unique per user)
     * @param iterations Iteration count for PBKDF2/Argon2 (ignored for SHA-3)
     * @return Hash bytes on success (size depends on algorithm), VaultError on failure
     *
     * @note Thread-safe, no side effects
     * @note Performance: SHA3-256 ~5ms, PBKDF2 ~50ms (10k iterations)
     * @note Username is case-sensitive (no normalization)
     *
     * @throws Never throws, uses std::expected for error handling
     */
    [[nodiscard]] static std::expected<std::vector<uint8_t>, VaultError>
    hash_username(std::string_view username,
                  Algorithm algorithm,
                  std::span<const uint8_t, 16> salt,
                  uint32_t iterations = 10000);

    /**
     * @brief Verify username against stored hash
     *
     * Computes hash of provided username and compares with stored hash
     * using constant-time comparison to prevent timing attacks.
     *
     * @param username Plaintext username to verify
     * @param stored_hash Hash to compare against (from KeySlot)
     * @param algorithm Hash algorithm used to create stored_hash
     * @param salt Salt used during original hashing
     * @param iterations Iteration count (PBKDF2/Argon2 only)
     * @return true if username matches hash, false otherwise
     *
     * @note Uses constant-time comparison (timing-attack resistant)
     * @note Returns false if hashing fails (prevents error-based attacks)
     * @note Thread-safe
     */
    [[nodiscard]] static bool
    verify_username(std::string_view username,
                    std::span<const uint8_t> stored_hash,
                    Algorithm algorithm,
                    std::span<const uint8_t, 16> salt,
                    uint32_t iterations = 10000);

    // ========================================================================
    // Utility Functions
    // ========================================================================

    /**
     * @brief Get expected hash size for algorithm
     *
     * @param algorithm Hash algorithm
     * @return Hash size in bytes (32, 48, or 64)
     *
     * @note constexpr for compile-time evaluation
     * @note Returns 0 for PLAINTEXT_LEGACY
     */
    [[nodiscard]] static constexpr size_t
    get_hash_size(Algorithm algorithm) noexcept {
        switch (algorithm) {
            case Algorithm::PLAINTEXT_LEGACY:
                return 0;
            case Algorithm::SHA3_256:
            case Algorithm::PBKDF2_SHA256:
            case Algorithm::ARGON2ID:
                return 32;  // 256 bits
            case Algorithm::SHA3_384:
                return 48;  // 384 bits
            case Algorithm::SHA3_512:
                return 64;  // 512 bits
            default:
                return 0;
        }
    }

    /**
     * @brief Get human-readable algorithm name
     *
     * @param algorithm Hash algorithm
     * @return Algorithm name for UI display
     */
    [[nodiscard]] static constexpr std::string_view
    get_algorithm_name(Algorithm algorithm) noexcept {
        switch (algorithm) {
            case Algorithm::PLAINTEXT_LEGACY:
                return "Plaintext (Legacy)";
            case Algorithm::SHA3_256:
                return "SHA3-256";
            case Algorithm::SHA3_384:
                return "SHA3-384";
            case Algorithm::SHA3_512:
                return "SHA3-512";
            case Algorithm::PBKDF2_SHA256:
                return "PBKDF2-HMAC-SHA256";
            case Algorithm::ARGON2ID:
                return "Argon2id";
            default:
                return "Unknown";
        }
    }

    /**
     * @brief Check if algorithm is FIPS-approved
     *
     * @param algorithm Hash algorithm to check
     * @return true if FIPS-approved, false otherwise
     */
    [[nodiscard]] static constexpr bool
    is_fips_approved(Algorithm algorithm) noexcept {
        switch (algorithm) {
            case Algorithm::SHA3_256:
            case Algorithm::SHA3_384:
            case Algorithm::SHA3_512:
            case Algorithm::PBKDF2_SHA256:
                return true;
            case Algorithm::PLAINTEXT_LEGACY:
            case Algorithm::ARGON2ID:
                return false;
            default:
                return false;
        }
    }

private:
    // ========================================================================
    // Private Implementation Methods (One Per Algorithm)
    // ========================================================================

    /**
     * @brief Hash username using SHA3-256
     * @param username Plaintext username
     * @param salt 16-byte salt
     * @return 32-byte hash or error
     */
    [[nodiscard]] static std::expected<std::vector<uint8_t>, VaultError>
    hash_sha3_256(std::string_view username,
                  std::span<const uint8_t, 16> salt);

    /**
     * @brief Hash username using SHA3-384
     * @param username Plaintext username
     * @param salt 16-byte salt
     * @return 48-byte hash or error
     */
    [[nodiscard]] static std::expected<std::vector<uint8_t>, VaultError>
    hash_sha3_384(std::string_view username,
                  std::span<const uint8_t, 16> salt);

    /**
     * @brief Hash username using SHA3-512
     * @param username Plaintext username
     * @param salt 16-byte salt
     * @return 64-byte hash or error
     */
    [[nodiscard]] static std::expected<std::vector<uint8_t>, VaultError>
    hash_sha3_512(std::string_view username,
                  std::span<const uint8_t, 16> salt);

    /**
     * @brief Hash username using PBKDF2-HMAC-SHA256
     * @param username Plaintext username
     * @param salt 16-byte salt
     * @param iterations Iteration count (default: 10000)
     * @return 32-byte hash or error
     */
    [[nodiscard]] static std::expected<std::vector<uint8_t>, VaultError>
    hash_pbkdf2_sha256(std::string_view username,
                       std::span<const uint8_t, 16> salt,
                       uint32_t iterations);

#ifdef ENABLE_ARGON2
    /**
     * @brief Hash username using Argon2id
     * @param username Plaintext username
     * @param salt 16-byte salt
     * @param iterations Time cost (default: 3)
     * @return 32-byte hash or error
     * @note Only available if ENABLE_ARGON2 build flag is set
     * @note Returns error in FIPS mode
     */
    [[nodiscard]] static std::expected<std::vector<uint8_t>, VaultError>
    hash_argon2id(std::string_view username,
                  std::span<const uint8_t, 16> salt,
                  uint32_t iterations);
#endif  // ENABLE_ARGON2

    /**
     * @brief Constant-time memory comparison (timing-attack resistant)
     * @param a First buffer
     * @param b Second buffer
     * @return true if buffers are equal, false otherwise
     */
    [[nodiscard]] static bool
    constant_time_compare(std::span<const uint8_t> a,
                          std::span<const uint8_t> b) noexcept;
};

}  // namespace KeepTower
