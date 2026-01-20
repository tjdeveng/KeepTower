// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

/**
 * @file KekDerivationService.h
 * @brief Service for Key Encryption Key (KEK) derivation
 *
 * This service handles password-based key derivation for vault KEK generation,
 * following the Single Responsibility Principle.
 *
 * Responsibilities:
 * - Derive KEKs from master passwords
 * - Support multiple key derivation algorithms
 * - Provide secure, memory-hard derivation options
 *
 * NOT responsible for:
 * - Vault operations (see VaultManager)
 * - User authentication (see VaultManager)
 * - Preferences management (see SettingsManager)
 * - UI operations (see PreferencesDialog)
 *
 * FIPS Compliance:
 * - PBKDF2-HMAC-SHA256: FIPS-approved (NIST SP 800-132)
 * - Argon2id: NOT FIPS-approved (blocked in FIPS mode)
 *
 * Security Properties:
 * - NIST SP 800-132 compliant (PBKDF2)
 * - RFC 9106 compliant (Argon2id)
 * - GPU/ASIC resistant (Argon2id)
 * - Side-channel resistant (constant-time operations)
 * - Memory-hard (Argon2id prevents parallel attacks)
 */

#pragma once

#include "../VaultError.h"
#include "../../utils/SecureMemory.h"
#include <giomm/settings.h>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>

namespace KeepTower {

/**
 * @class KekDerivationService
 * @brief Pure cryptographic key derivation service
 *
 * This class provides stateless KEK derivation operations. All methods
 * are static and [[nodiscard]] to ensure results are used. No side effects.
 *
 * Thread-safety: All methods are thread-safe (no shared mutable state)
 * FIPS-compliance: PBKDF2 is FIPS-approved, Argon2id requires FIPS mode disabled
 *
 * Performance Characteristics:
 * | Algorithm       | Time  | Memory | FIPS |
 * |-----------------|-------|--------|------|
 * | PBKDF2 600K     | ~1.0s | <1 KB  | Yes  |
 * | PBKDF2 1M       | ~1.7s | <1 KB  | Yes  |
 * | Argon2id 64MB   | ~0.5s | 64 MB  | No   |
 * | Argon2id 256MB  | ~2.0s | 256 MB | No   |
 *
 * @section usage Usage Example
 * @code
 * // Generate random salt
 * std::array<uint8_t, 16> salt = VaultCrypto::generate_random_bytes(16);
 *
 * // Derive KEK with PBKDF2 (FIPS-compliant)
 * KekDerivationService::AlgorithmParameters params;
 * params.pbkdf2_iterations = 600000;
 *
 * auto kek_result = KekDerivationService::derive_kek(
 *     "master_password",
 *     KekDerivationService::Algorithm::PBKDF2_HMAC_SHA256,
 *     salt,
 *     params
 * );
 *
 * if (kek_result.has_value()) {
 *     SecureVector<uint8_t> kek = std::move(kek_result.value());
 *     // Use KEK to encrypt DEK
 * }
 * // KEK automatically zeroized on destruction
 * @endcode
 *
 * @warning SHA3 algorithms are NOT suitable for password-based key derivation!
 *          They lack the computational work factor needed to resist brute-force
 *          attacks. Always use PBKDF2 or Argon2id for passwords.
 */
class KekDerivationService {
public:
    /**
     * @brief Key derivation algorithm
     *
     * IMPORTANT: Only PBKDF2 and Argon2id are suitable for password-based
     * key derivation. SHA3 variants lack the work factor needed to resist
     * brute-force attacks and MUST NOT be used for KEK derivation.
     */
    enum class Algorithm : uint8_t {
        PBKDF2_HMAC_SHA256 = 0x04,  ///< FIPS-approved, default (600K iterations)
        ARGON2ID = 0x05              ///< Maximum security, memory-hard (not FIPS)
    };

    /**
     * @brief Algorithm-specific parameters
     *
     * Each algorithm uses different parameters:
     * - PBKDF2: iteration count (computational cost)
     * - Argon2id: memory cost, time cost, parallelism
     */
    struct AlgorithmParameters {
        uint32_t pbkdf2_iterations = 600000;  ///< PBKDF2 iteration count (10K-1M)
        uint32_t argon2_memory_kb = 65536;    ///< Argon2 memory cost in KB (8MB-1GB)
        uint32_t argon2_time_cost = 3;        ///< Argon2 time cost (1-10 iterations)
        uint8_t argon2_parallelism = 4;       ///< Argon2 thread count (1-16)
    };

    /**
     * @brief Derive KEK from master password
     *
     * Derives a 256-bit Key Encryption Key using the specified algorithm.
     * Output is stored in secure memory that is automatically zeroized.
     *
     * @param password Master password (UTF-8 encoded)
     * @param algorithm Key derivation algorithm (PBKDF2 or Argon2id)
     * @param salt Cryptographic salt (minimum 128 bits)
     * @param params Algorithm-specific parameters
     * @return 256-bit KEK in secure memory, or VaultError on failure
     *
     * @note Thread-safe, no side effects
     * @note Output automatically zeroized on destruction
     * @note Salt MUST be unique per KeySlot
     * @note Never throws (returns std::expected)
     *
     * @warning Passwords must have sufficient entropy for security
     *
     * Error conditions:
     * - VaultError::INVALID_SALT: salt too short (< 16 bytes)
     * - VaultError::UNSUPPORTED_ALGORITHM: unknown algorithm
     * - VaultError::CRYPTO_ERROR: OpenSSL internal error
     */
    [[nodiscard]] static std::expected<SecureVector<uint8_t>, VaultError>
    derive_kek(
        std::string_view password,
        Algorithm algorithm,
        std::span<const uint8_t> salt,
        const AlgorithmParameters& params) noexcept;

    /**
     * @brief Get algorithm from preference settings
     *
     * Maps the username-hash-algorithm preference to KEK derivation algorithm.
     * Automatically falls back to PBKDF2 if SHA3 is selected (SHA3 unsuitable
     * for password-based key derivation).
     *
     * @param settings GSettings instance
     * @return Algorithm enum value (PBKDF2 or Argon2id)
     *
     * @note SHA3 variants automatically map to PBKDF2
     * @note FIPS mode forces PBKDF2 regardless of preference
     * @note Thread-safe
     */
    [[nodiscard]] static Algorithm get_algorithm_from_settings(
        const Glib::RefPtr<Gio::Settings>& settings) noexcept;

    /**
     * @brief Get algorithm parameters from preference settings
     *
     * Reads username hashing parameters and applies them to KEK derivation.
     * This reuses the same parameter preferences for both username hashing
     * and KEK derivation, ensuring consistency.
     *
     * @param settings GSettings instance
     * @return AlgorithmParameters struct with values from preferences
     *
     * @note Clamps values to safe ranges
     * @note Thread-safe
     */
    [[nodiscard]] static AlgorithmParameters get_parameters_from_settings(
        const Glib::RefPtr<Gio::Settings>& settings) noexcept;

    /**
     * @brief Check if algorithm is FIPS-approved
     *
     * @param algorithm Algorithm to check
     * @return true if FIPS-140-3 approved
     *
     * @note Only PBKDF2-HMAC-SHA256 is FIPS-approved
     * @note Argon2id requires FIPS mode to be disabled
     */
    [[nodiscard]] static constexpr bool is_fips_approved(Algorithm algorithm) noexcept {
        return algorithm == Algorithm::PBKDF2_HMAC_SHA256;
    }

    /**
     * @brief Get expected output size for algorithm
     *
     * @param algorithm Key derivation algorithm
     * @return KEK size in bytes (always 32 for AES-256)
     *
     * @note All algorithms produce 256-bit (32-byte) keys
     */
    [[nodiscard]] static constexpr size_t get_output_size([[maybe_unused]] Algorithm algorithm) noexcept {
        return 32;  // AES-256 key size
    }

    /**
     * @brief Convert algorithm enum to string for logging
     *
     * @param algorithm Algorithm to convert
     * @return Human-readable algorithm name
     */
    [[nodiscard]] static constexpr std::string_view algorithm_to_string(Algorithm algorithm) noexcept {
        switch (algorithm) {
            case Algorithm::PBKDF2_HMAC_SHA256: return "PBKDF2-HMAC-SHA256";
            case Algorithm::ARGON2ID: return "Argon2id";
            default: return "Unknown";
        }
    }

private:
    /**
     * @brief Derive KEK using PBKDF2-HMAC-SHA256
     *
     * @param password Master password
     * @param salt Cryptographic salt
     * @param iterations Number of PBKDF2 iterations
     * @return 256-bit KEK or error
     */
    [[nodiscard]] static std::expected<SecureVector<uint8_t>, VaultError>
    derive_kek_pbkdf2(
        std::string_view password,
        std::span<const uint8_t> salt,
        uint32_t iterations) noexcept;

    /**
     * @brief Derive KEK using Argon2id
     *
     * @param password Master password
     * @param salt Cryptographic salt
     * @param memory_kb Memory cost in kilobytes
     * @param time_cost Time cost (iterations)
     * @param parallelism Thread count
     * @return 256-bit KEK or error
     */
    [[nodiscard]] static std::expected<SecureVector<uint8_t>, VaultError>
    derive_kek_argon2id(
        std::string_view password,
        std::span<const uint8_t> salt,
        uint32_t memory_kb,
        uint32_t time_cost,
        uint8_t parallelism) noexcept;
};

} // namespace KeepTower
