// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file PasswordHistory.h
 * @brief Password history management for user password reuse prevention
 *
 * This module provides PBKDF2-HMAC-SHA512-based password hashing and
 * constant-time comparison for preventing users from reusing recent passwords.
 *
 * @section security_features Security Features
 * - **PBKDF2-HMAC-SHA512 hashing**: FIPS 140-3 approved algorithm
 * - **Random salts**: Each entry has unique 32-byte salt (FIPS-approved DRBG)
 * - **Constant-time comparison**: Prevents timing side-channel attacks
 * - **Ring buffer**: FIFO eviction when depth limit reached
 * - **Secure memory**: All computed hashes are cleared with OPENSSL_cleanse
 *
 * @section implementation Implementation Details
 * - Hash parameters: 600,000 iterations (OWASP 2023 for PBKDF2-SHA512)
 * - Output length: 48 bytes
 * - Salt length: 32 bytes (cryptographically random via RAND_bytes)
 * - Comparison: Constant-time to prevent timing attacks
 * - Memory security: Computed hashes cleared immediately after use
 * - FIPS compliance: All operations use FIPS-approved primitives
 *
 * @section memory_safety Memory Safety
 * - Computed password hashes are securely cleared after comparison
 * - PasswordHistoryEntry destructor clears hash with OPENSSL_cleanse
 * - Failure paths clear partial hashes before returning
 * - No sensitive data left in memory after operations complete
 */

#ifndef PASSWORDHISTORY_H
#define PASSWORDHISTORY_H

#include "MultiUserTypes.h"
#include <glibmm/ustring.h>
#include <string>
#include <vector>
#include <optional>

namespace KeepTower {

/**
 * @brief Password history manager for reuse prevention
 *
 * Handles all password history operations including hashing,
 * comparison, and history management.
 */
class PasswordHistory {
public:
    /**
     * @brief Hash a password using PBKDF2-HMAC-SHA512
     *
     * Creates a password history entry with current timestamp,
     * random salt, and PBKDF2-HMAC-SHA512 hash.
     *
     * @param password The password to hash (UTF-8 string)
     * @return PasswordHistoryEntry on success, empty optional on failure
     *
     * @note Uses OWASP-recommended 600,000 iterations for PBKDF2-SHA512
     * @note Generates cryptographically random 32-byte salt (FIPS-approved DRBG)
     * @note FIPS 140-3 compliant when OpenSSL FIPS provider is enabled
     */
    static std::optional<PasswordHistoryEntry> hash_password(const Glib::ustring& password);

    /**
     * @brief Check if password matches any entry in history
     *
     * Performs constant-time comparison against all history entries.
     * Returns true if password matches any previous password.
     *
     * @param password The password to check
     * @param history Vector of previous password hashes
     * @return true if password was used previously, false otherwise
     *
     * @note Uses constant-time comparison to prevent timing attacks
     * @note Checks all entries even after finding a match (constant-time)
     */
    static bool is_password_reused(
        const Glib::ustring& password,
        const std::vector<PasswordHistoryEntry>& history);

    /**
     * @brief Add password to history with ring buffer behavior
     *
     * Adds new password hash to history. If history size exceeds depth,
     * removes oldest entry (FIFO).
     *
     * @param history Vector of password history entries (modified in-place)
     * @param new_entry New password entry to add
     * @param max_depth Maximum history depth (from VaultSecurityPolicy)
     *
     * @note Trims history to max_depth after adding
     * @note Oldest entries removed first (FIFO)
     */
    static void add_to_history(
        std::vector<PasswordHistoryEntry>& history,
        const PasswordHistoryEntry& new_entry,
        uint32_t max_depth);

    /**
     * @brief Trim history to specified depth
     *
     * Removes oldest entries if history exceeds max_depth.
     * Used when admin decreases password_history_depth policy.
     *
     * @param history Vector of password history entries (modified in-place)
     * @param max_depth Maximum history depth
     *
     * @note Preserves most recent entries
     * @note Does nothing if history.size() <= max_depth
     */
    static void trim_history(
        std::vector<PasswordHistoryEntry>& history,
        uint32_t max_depth);

private:
    /**
     * @brief PBKDF2-HMAC-SHA512 iteration count
     *
     * OWASP 2023 recommendation: 600,000 iterations for PBKDF2-SHA512.
     * Higher than the KEK derivation iterations since this is for storage,
     * not real-time authentication.
     *
     * @note FIPS 140-3 compliant (PBKDF2 is approved in FIPS SP 800-132)
     */
    static constexpr uint32_t PBKDF2_ITERATIONS = 600000;

    /**
     * @brief Iteration count override for testing (0 = use default)
     * @note Only for unit tests - do not use in production code
     */
    static inline uint32_t s_test_iterations = 0;

public:
    /**
     * @brief Set custom iteration count for testing
     * @param iterations Iteration count (0 to restore default)
     * @note Only for unit tests - resets to default after test
     */
    static void set_test_iterations(uint32_t iterations) {
        s_test_iterations = iterations;
    }

private:
    /**
     * @brief PBKDF2-HMAC-SHA512 output length in bytes
     *
     * 48 bytes provides 384 bits of security.
     * Matches SHA-512 output but truncated to reasonable storage size.
     */
    static constexpr uint32_t HASH_LENGTH = 48;

    /**
     * @brief Salt length in bytes
     *
     * 32 bytes (256 bits) provides sufficient entropy.
     */
    static constexpr uint32_t SALT_LENGTH = 32;
};

} // namespace KeepTower

#endif // PASSWORDHISTORY_H
