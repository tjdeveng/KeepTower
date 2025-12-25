// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "PasswordHistory.h"
#include "../utils/Log.h"
#include "../utils/SecureMemory.h"
#include <openssl/evp.h>
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

    // Generate random salt using OpenSSL's CSPRNG (FIPS-approved DRBG)
    if (RAND_bytes(entry.salt.data(), SALT_LENGTH) != 1) {
        Log::error("PasswordHistory: Failed to generate random salt");
        return std::nullopt;
    }

    // Use test iterations if set, otherwise use default
    uint32_t iterations = (s_test_iterations > 0) ? s_test_iterations : PBKDF2_ITERATIONS;

    // Hash password with PBKDF2-HMAC-SHA512 (FIPS 140-3 approved)
    // Use higher iterations than KEK derivation since this is for storage, not authentication
    int result = PKCS5_PBKDF2_HMAC(
        password.c_str(),           // password data
        password.bytes(),           // password length
        entry.salt.data(),          // salt data
        SALT_LENGTH,                // salt length
        iterations,                 // iteration count
        EVP_sha512(),               // FIPS-approved hash function
        HASH_LENGTH,                // output length (48 bytes)
        entry.hash.data()           // output hash
    );

    if (result != 1) {
        // Securely clear the partial hash on failure
        secure_clear(entry.hash);
        Log::error("PasswordHistory: PBKDF2-HMAC-SHA512 hashing failed");
        return std::nullopt;
    }

    Log::debug("PasswordHistory: Successfully hashed password (PBKDF2-SHA512, iterations={})",
               iterations);

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
        // Compute hash for this entry's salt using PBKDF2-HMAC-SHA512 (FIPS-approved)
        // Use SecureBuffer to ensure computed hash is cleared after comparison
        std::array<uint8_t, HASH_LENGTH> computed_hash{};

        // Use test iterations if set, otherwise use default
        uint32_t iterations = (s_test_iterations > 0) ? s_test_iterations : PBKDF2_ITERATIONS;

        int result = PKCS5_PBKDF2_HMAC(
            password.c_str(),
            password.bytes(),
            entry.salt.data(),
            SALT_LENGTH,
            iterations,
            EVP_sha512(),           // FIPS-approved hash function
            HASH_LENGTH,
            computed_hash.data()
        );

        if (result != 1) {
            // Securely clear the partial hash on failure
            secure_clear(computed_hash);
            Log::error("PasswordHistory: PBKDF2-HMAC-SHA512 hashing failed during reuse check");
            continue; // Skip this entry but continue checking others
        }

        // Constant-time comparison using OpenSSL (prevents timing attacks)
        int match = CRYPTO_memcmp(computed_hash.data(), entry.hash.data(), HASH_LENGTH);

        // Securely clear computed hash immediately after comparison
        secure_clear(computed_hash);

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
