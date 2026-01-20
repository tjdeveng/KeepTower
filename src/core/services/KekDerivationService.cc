// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#include "KekDerivationService.h"
#include "../../utils/Log.h"
#include <openssl/evp.h>
#include <argon2.h>

namespace KeepTower {

std::expected<SecureVector<uint8_t>, VaultError>
KekDerivationService::derive_kek(
    std::string_view password,
    Algorithm algorithm,
    std::span<const uint8_t> salt,
    const AlgorithmParameters& params) noexcept {

    // Validate salt length
    if (salt.size() < 16) {
        Log::error("KekDerivationService: Salt too short ({} bytes, minimum 16)", salt.size());
        return std::unexpected(VaultError::InvalidSalt);
    }

    // Dispatch to algorithm-specific implementation
    switch (algorithm) {
        case Algorithm::PBKDF2_HMAC_SHA256:
            return derive_kek_pbkdf2(password, salt, params.pbkdf2_iterations);

        case Algorithm::ARGON2ID:
            return derive_kek_argon2id(
                password, salt,
                params.argon2_memory_kb,
                params.argon2_time_cost,
                params.argon2_parallelism);

        default:
            Log::error("KekDerivationService: Unsupported algorithm: {}",
                       static_cast<int>(algorithm));
            return std::unexpected(VaultError::UnsupportedAlgorithm);
    }
}

std::expected<SecureVector<uint8_t>, VaultError>
KekDerivationService::derive_kek_pbkdf2(
    std::string_view password,
    std::span<const uint8_t> salt,
    uint32_t iterations) noexcept {

    // Allocate secure memory for output
    SecureVector<uint8_t> kek(32);  // 256-bit key

    // Call OpenSSL PBKDF2
    int result = PKCS5_PBKDF2_HMAC(
        password.data(), password.size(),
        salt.data(), salt.size(),
        static_cast<int>(iterations),
        EVP_sha256(),
        32,  // output length
        kek.data()
    );

    if (result != 1) {
        Log::error("KekDerivationService: PBKDF2 failed");
        return std::unexpected(VaultError::CryptoError);
    }

    Log::debug("KekDerivationService: PBKDF2 KEK derived successfully ({} iterations)",
               iterations);
    return kek;
}

std::expected<SecureVector<uint8_t>, VaultError>
KekDerivationService::derive_kek_argon2id(
    std::string_view password,
    std::span<const uint8_t> salt,
    uint32_t memory_kb,
    uint32_t time_cost,
    uint8_t parallelism) noexcept {

    // Allocate secure memory for output
    SecureVector<uint8_t> kek(32);  // 256-bit key

    // Call libargon2 directly
    // Note: Argon2 is NOT part of OpenSSL - it's provided by libargon2
    int result = argon2id_hash_raw(
        time_cost,                    // t_cost (iterations)
        memory_kb,                    // m_cost (memory in KB)
        parallelism,                  // parallelism (threads)
        password.data(),              // pwd
        password.size(),              // pwdlen
        salt.data(),                  // salt
        salt.size(),                  // saltlen
        kek.data(),                   // hash output
        kek.size()                    // hashlen
    );

    if (result != ARGON2_OK) {
        Log::error("KekDerivationService: Argon2id derivation failed: {}",
                   argon2_error_message(result));
        return std::unexpected(VaultError::CryptoError);
    }

    Log::debug("KekDerivationService: Argon2id KEK derived successfully "
               "({} KB memory, {} iterations, {} threads)",
               memory_kb, time_cost, parallelism);
    return kek;
}

KekDerivationService::Algorithm
KekDerivationService::get_algorithm_from_settings(
    const Glib::RefPtr<Gio::Settings>& settings) noexcept {

    if (!settings) {
        Log::warning("KekDerivationService: null settings, defaulting to PBKDF2");
        return Algorithm::PBKDF2_HMAC_SHA256;
    }

    // Check FIPS mode first
    bool fips_mode = settings->get_boolean("fips-mode-enabled");
    if (fips_mode) {
        Log::debug("KekDerivationService: FIPS mode enabled, using PBKDF2");
        return Algorithm::PBKDF2_HMAC_SHA256;
    }

    // Get username hashing algorithm preference
    Glib::ustring pref = settings->get_string("username-hash-algorithm");

    // Map to KEK derivation algorithm
    // CRITICAL: SHA3 variants are NOT suitable for password-based key derivation!
    // They lack the computational work factor and automatically fall back to PBKDF2.
    if (pref == "argon2id") {
        Log::debug("KekDerivationService: Using Argon2id from settings");
        return Algorithm::ARGON2ID;
    } else if (pref == "pbkdf2") {
        Log::debug("KekDerivationService: Using PBKDF2 from settings");
        return Algorithm::PBKDF2_HMAC_SHA256;
    } else if (pref == "sha3-256" || pref == "sha3-384" || pref == "sha3-512") {
        // SHA3 is appropriate for username hashing but catastrophically weak
        // for password-based key derivation. Automatically fallback to PBKDF2.
        Log::warning("KekDerivationService: SHA3 unsuitable for KEK derivation, "
                     "using PBKDF2 fallback");
        return Algorithm::PBKDF2_HMAC_SHA256;
    } else {
        Log::warning("KekDerivationService: Unknown algorithm '{}', defaulting to PBKDF2",
                     pref);
        return Algorithm::PBKDF2_HMAC_SHA256;
    }
}

KekDerivationService::AlgorithmParameters
KekDerivationService::get_parameters_from_settings(
    const Glib::RefPtr<Gio::Settings>& settings) noexcept {

    AlgorithmParameters params;

    if (!settings) {
        Log::warning("KekDerivationService: null settings, using defaults");
        return params;
    }

    // Read PBKDF2 parameters
    params.pbkdf2_iterations = settings->get_uint("username-pbkdf2-iterations");

    // Read Argon2 parameters
    params.argon2_memory_kb = settings->get_uint("username-argon2-memory-kb");
    params.argon2_time_cost = settings->get_uint("username-argon2-iterations");

    // Parallelism is fixed at 4 threads (reasonable for most systems)
    params.argon2_parallelism = 4;

    Log::debug("KekDerivationService: Parameters from settings - "
               "PBKDF2: {} iterations, Argon2: {} KB / {} iterations / {} threads",
               params.pbkdf2_iterations, params.argon2_memory_kb,
               params.argon2_time_cost, params.argon2_parallelism);

    return params;
}

} // namespace KeepTower
