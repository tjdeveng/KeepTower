// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "PasswordHistory.h"
#include "../utils/Log.h"
#include <argon2.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include <ctime>
#include <algorithm>

namespace KeepTower {

std::optional<PasswordHistoryEntry> PasswordHistory::hash_password(const Glib::ustring& password) {
    // Validate password is not empty
    if (password.empty()) {
        Log::error("PasswordHistory: Cannot hash empty password");
        return std::nullopt;
    }

    PasswordHistoryEntry entry;

    // Set timestamp
    entry.timestamp = std::time(nullptr);

    // Generate random salt using OpenSSL's CSPRNG
    if (RAND_bytes(entry.salt.data(), SALT_LENGTH) != 1) {
        Log::error("PasswordHistory: Failed to generate random salt");
        return std::nullopt;
    }

    // Hash password with Argon2id
    // argon2id_hash_raw(t_cost, m_cost, parallelism, pwd, pwdlen, salt, saltlen, hash, hashlen)
    int result = argon2id_hash_raw(
        ARGON2_TIME_COST,        // t_cost (iterations)
        ARGON2_MEMORY_COST,      // m_cost (KiB)
        ARGON2_PARALLELISM,      // parallelism (threads)
        password.c_str(),        // password data
        password.bytes(),        // password length
        entry.salt.data(),       // salt data
        SALT_LENGTH,             // salt length
        entry.hash.data(),       // output hash
        ARGON2_HASH_LENGTH       // hash length
    );

    if (result != ARGON2_OK) {
        Log::error("PasswordHistory: Argon2id hashing failed: {}",
                   argon2_error_message(result));
        return std::nullopt;
    }

    Log::debug("PasswordHistory: Successfully hashed password (t={}, m={} KiB, p={})",
               ARGON2_TIME_COST, ARGON2_MEMORY_COST, ARGON2_PARALLELISM);

    return entry;
}

bool PasswordHistory::is_password_reused(
    const Glib::ustring& password,
    const std::vector<PasswordHistoryEntry>& history) {

    // Empty history means no reuse
    if (history.empty()) {
        return false;
    }

    // Validate password
    if (password.empty()) {
        Log::warning("PasswordHistory: Empty password provided for reuse check");
        return false;
    }

    bool found_match = false;

    // Check against each history entry
    // IMPORTANT: We check ALL entries to maintain constant time
    for (const auto& entry : history) {
        // Compute hash for this entry's salt
        std::array<uint8_t, ARGON2_HASH_LENGTH> computed_hash;

        int result = argon2id_hash_raw(
            ARGON2_TIME_COST,
            ARGON2_MEMORY_COST,
            ARGON2_PARALLELISM,
            password.c_str(),
            password.bytes(),
            entry.salt.data(),
            SALT_LENGTH,
            computed_hash.data(),
            ARGON2_HASH_LENGTH
        );

        if (result != ARGON2_OK) {
            Log::error("PasswordHistory: Argon2id hashing failed during reuse check: {}",
                       argon2_error_message(result));
            continue; // Skip this entry but continue checking others
        }

        // Constant-time comparison using OpenSSL
        int match = CRYPTO_memcmp(computed_hash.data(), entry.hash.data(), ARGON2_HASH_LENGTH);
        if (match == 0) {
            found_match = true;
            // IMPORTANT: Continue checking other entries for constant-time behavior
        }
    }

    if (found_match) {
        Log::debug("PasswordHistory: Password reuse detected");
    }

    return found_match;
}

void PasswordHistory::add_to_history(
    std::vector<PasswordHistoryEntry>& history,
    const PasswordHistoryEntry& new_entry,
    uint32_t max_depth) {

    // If depth is 0, password history is disabled
    if (max_depth == 0) {
        history.clear();
        return;
    }

    // Add new entry to the end (most recent)
    history.push_back(new_entry);

    // Trim to max_depth if necessary
    trim_history(history, max_depth);

    Log::debug("PasswordHistory: Added entry to history (size={}, max_depth={})",
               history.size(), max_depth);
}

void PasswordHistory::trim_history(
    std::vector<PasswordHistoryEntry>& history,
    uint32_t max_depth) {

    if (max_depth == 0) {
        history.clear();
        Log::debug("PasswordHistory: Cleared history (depth set to 0)");
        return;
    }

    // Remove oldest entries if history exceeds max_depth
    while (history.size() > max_depth) {
        // Remove first element (oldest)
        history.erase(history.begin());
    }

    if (history.size() == max_depth) {
        Log::debug("PasswordHistory: Trimmed history to max_depth={}", max_depth);
    }
}

} // namespace KeepTower
