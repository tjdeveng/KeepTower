// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file PasswordHistory.h
 * @brief Password history management for user password reuse prevention
 *
 * This module provides Argon2id-based password hashing and constant-time
 * comparison for preventing users from reusing recent passwords.
 *
 * @section security_features Security Features
 * - **Argon2id hashing**: Memory-hard algorithm resistant to GPU attacks
 * - **Random salts**: Each entry has unique 32-byte salt
 * - **Constant-time comparison**: Prevents timing side-channel attacks
 * - **Ring buffer**: FIFO eviction when depth limit reached
 *
 * @section implementation Implementation Details
 * - Hash parameters: m=65536 (64 MB), t=3, p=4 (OWASP 2023 recommendations)
 * - Output length: 48 bytes
 * - Salt length: 32 bytes (cryptographically random)
 * - Comparison: Constant-time to prevent timing attacks
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
     * @brief Hash a password using Argon2id
     *
     * Creates a password history entry with current timestamp,
     * random salt, and Argon2id hash.
     *
     * @param password The password to hash (UTF-8 string)
     * @return PasswordHistoryEntry on success, empty optional on failure
     *
     * @note Uses OWASP-recommended parameters: m=65536, t=3, p=4
     * @note Generates cryptographically random 32-byte salt
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
     * @brief Argon2id memory cost in KiB (64 MB)
     *
     * OWASP recommendation: 64 MB minimum for password hashing.
     */
    static constexpr uint32_t ARGON2_MEMORY_COST = 65536;

    /**
     * @brief Argon2id time cost (iterations)
     *
     * OWASP recommendation: 3 iterations minimum.
     */
    static constexpr uint32_t ARGON2_TIME_COST = 3;

    /**
     * @brief Argon2id parallelism factor
     *
     * OWASP recommendation: 4 threads.
     */
    static constexpr uint32_t ARGON2_PARALLELISM = 4;

    /**
     * @brief Argon2id output length in bytes
     *
     * 48 bytes provides 384 bits of security.
     */
    static constexpr uint32_t ARGON2_HASH_LENGTH = 48;

    /**
     * @brief Salt length in bytes
     *
     * 32 bytes (256 bits) provides sufficient entropy.
     */
    static constexpr uint32_t SALT_LENGTH = 32;
};

} // namespace KeepTower

#endif // PASSWORDHISTORY_H
