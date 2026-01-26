// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

/**
 * @file UsernameHashService.cc
 * @brief Implementation of username hashing service
 */

#include "config.h"  // For ENABLE_ARGON2
#include "UsernameHashService.h"
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/crypto.h>
#include <cstring>
#include <memory>

#ifdef ENABLE_ARGON2
#include <argon2.h>
#endif

namespace KeepTower {

// ============================================================================
// Public API: Hashing
// ============================================================================

std::expected<std::vector<uint8_t>, VaultError>
UsernameHashService::hash_username(std::string_view username,
                                   Algorithm algorithm,
                                   std::span<const uint8_t, 16> salt,
                                   uint32_t iterations) {
    // Validate inputs
    if (username.empty()) {
        return std::unexpected(VaultError::InvalidUsername);
    }

    // Dispatch to algorithm-specific implementation
    switch (algorithm) {
        case Algorithm::PLAINTEXT_LEGACY:
            // No hashing for legacy mode
            return std::vector<uint8_t>(username.begin(), username.end());

        case Algorithm::SHA3_256:
            return hash_sha3_256(username, salt);

        case Algorithm::SHA3_384:
            return hash_sha3_384(username, salt);

        case Algorithm::SHA3_512:
            return hash_sha3_512(username, salt);

        case Algorithm::PBKDF2_SHA256:
            return hash_pbkdf2_sha256(username, salt, iterations);

        case Algorithm::ARGON2ID:
#ifdef ENABLE_ARGON2
            return hash_argon2id(username, salt, iterations);
#else
            return std::unexpected(VaultError::CryptoError);
#endif

        default:
            return std::unexpected(VaultError::CryptoError);
    }
}

bool UsernameHashService::verify_username(std::string_view username,
                                          std::span<const uint8_t> stored_hash,
                                          Algorithm algorithm,
                                          std::span<const uint8_t, 16> salt,
                                          uint32_t iterations) {
    // Compute hash of provided username
    auto computed_hash = hash_username(username, algorithm, salt, iterations);

    if (!computed_hash.has_value()) {
        return false;  // Hashing failed
    }

    // Verify sizes match
    if (computed_hash->size() != stored_hash.size()) {
        return false;
    }

    // Constant-time comparison (timing-attack resistant)
    return constant_time_compare(*computed_hash, stored_hash);
}

// ============================================================================
// Private Implementation: SHA-3 Variants
// ============================================================================

std::expected<std::vector<uint8_t>, VaultError>
UsernameHashService::hash_sha3_256(std::string_view username,
                                   std::span<const uint8_t, 16> salt) {
    // Combine username + salt for input
    std::vector<uint8_t> input;
    input.reserve(username.size() + salt.size());
    input.insert(input.end(), username.begin(), username.end());
    input.insert(input.end(), salt.begin(), salt.end());

    // Create OpenSSL digest context (FIPS-approved EVP API)
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        return std::unexpected(VaultError::CryptoError);
    }

    // RAII cleanup guard
    auto cleanup = [](EVP_MD_CTX* p) { if (p) EVP_MD_CTX_free(p); };
    std::unique_ptr<EVP_MD_CTX, decltype(cleanup)> ctx_guard(ctx, cleanup);

    // Initialize SHA3-256 (FIPS-approved: FIPS 202)
    if (EVP_DigestInit_ex(ctx, EVP_sha3_256(), nullptr) != 1) {
        return std::unexpected(VaultError::CryptoError);
    }

    // Hash the input
    if (EVP_DigestUpdate(ctx, input.data(), input.size()) != 1) {
        return std::unexpected(VaultError::CryptoError);
    }

    // Finalize hash
    std::vector<uint8_t> hash(32);  // SHA3-256 = 32 bytes
    unsigned int hash_len = 0;
    if (EVP_DigestFinal_ex(ctx, hash.data(), &hash_len) != 1) {
        return std::unexpected(VaultError::CryptoError);
    }

    // Verify expected size
    if (hash_len != 32) {
        return std::unexpected(VaultError::CryptoError);
    }

    return hash;
}

std::expected<std::vector<uint8_t>, VaultError>
UsernameHashService::hash_sha3_384(std::string_view username,
                                   std::span<const uint8_t, 16> salt) {
    // Combine username + salt
    std::vector<uint8_t> input;
    input.reserve(username.size() + salt.size());
    input.insert(input.end(), username.begin(), username.end());
    input.insert(input.end(), salt.begin(), salt.end());

    // Create OpenSSL digest context
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        return std::unexpected(VaultError::CryptoError);
    }

    auto cleanup = [](EVP_MD_CTX* p) { if (p) EVP_MD_CTX_free(p); };
    std::unique_ptr<EVP_MD_CTX, decltype(cleanup)> ctx_guard(ctx, cleanup);

    // Initialize SHA3-384 (FIPS-approved: FIPS 202)
    if (EVP_DigestInit_ex(ctx, EVP_sha3_384(), nullptr) != 1) {
        return std::unexpected(VaultError::CryptoError);
    }

    if (EVP_DigestUpdate(ctx, input.data(), input.size()) != 1) {
        return std::unexpected(VaultError::CryptoError);
    }

    std::vector<uint8_t> hash(48);  // SHA3-384 = 48 bytes
    unsigned int hash_len = 0;
    if (EVP_DigestFinal_ex(ctx, hash.data(), &hash_len) != 1) {
        return std::unexpected(VaultError::CryptoError);
    }

    if (hash_len != 48) {
        return std::unexpected(VaultError::CryptoError);
    }

    return hash;
}

std::expected<std::vector<uint8_t>, VaultError>
UsernameHashService::hash_sha3_512(std::string_view username,
                                   std::span<const uint8_t, 16> salt) {
    // Combine username + salt
    std::vector<uint8_t> input;
    input.reserve(username.size() + salt.size());
    input.insert(input.end(), username.begin(), username.end());
    input.insert(input.end(), salt.begin(), salt.end());

    // Create OpenSSL digest context
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        return std::unexpected(VaultError::CryptoError);
    }

    auto cleanup = [](EVP_MD_CTX* p) { if (p) EVP_MD_CTX_free(p); };
    std::unique_ptr<EVP_MD_CTX, decltype(cleanup)> ctx_guard(ctx, cleanup);

    // Initialize SHA3-512 (FIPS-approved: FIPS 202)
    if (EVP_DigestInit_ex(ctx, EVP_sha3_512(), nullptr) != 1) {
        return std::unexpected(VaultError::CryptoError);
    }

    if (EVP_DigestUpdate(ctx, input.data(), input.size()) != 1) {
        return std::unexpected(VaultError::CryptoError);
    }

    std::vector<uint8_t> hash(64);  // SHA3-512 = 64 bytes
    unsigned int hash_len = 0;
    if (EVP_DigestFinal_ex(ctx, hash.data(), &hash_len) != 1) {
        return std::unexpected(VaultError::CryptoError);
    }

    if (hash_len != 64) {
        return std::unexpected(VaultError::CryptoError);
    }

    return hash;
}

// ============================================================================
// Private Implementation: PBKDF2
// ============================================================================

std::expected<std::vector<uint8_t>, VaultError>
UsernameHashService::hash_pbkdf2_sha256(std::string_view username,
                                        std::span<const uint8_t, 16> salt,
                                        uint32_t iterations) {
    // Validate iteration count
    if (iterations < 1000) {
        // NIST SP 800-132 recommends minimum 1000 iterations
        iterations = 1000;
    }

    std::vector<uint8_t> hash(32);  // 256 bits output

    // PBKDF2-HMAC-SHA256 (FIPS-approved: SP 800-132)
    if (PKCS5_PBKDF2_HMAC(username.data(),
                          static_cast<int>(username.size()),
                          salt.data(),
                          static_cast<int>(salt.size()),
                          static_cast<int>(iterations),
                          EVP_sha256(),  // FIPS-approved hash function
                          static_cast<int>(hash.size()),
                          hash.data()) != 1) {
        return std::unexpected(VaultError::KeyDerivationFailed);
    }

    return hash;
}

// ============================================================================
// Private Implementation: Argon2id (Optional)
// ============================================================================

#ifdef ENABLE_ARGON2
std::expected<std::vector<uint8_t>, VaultError>
UsernameHashService::hash_argon2id(std::string_view username,
                                   std::span<const uint8_t, 16> salt,
                                   uint32_t iterations) {
    // Check FIPS mode: Argon2id is NOT FIPS-approved
    // Note: FIPS_mode() check would go here if OpenSSL FIPS module is loaded
    // For now, we allow Argon2id (user must disable FIPS mode explicitly)

    // Argon2id parameters (from GSchema preferences or defaults)
    const uint32_t memory_kb = 65536;  // 64 MB (TODO: read from settings)
    const uint32_t parallelism = 1;     // Single thread for username hashing
    const uint32_t hash_len = 32;       // 256 bits

    // Argon2id time cost (t_cost) - NOT the same as PBKDF2 iterations!
    // Typical values: 1-10 (not 100,000 like PBKDF2)
    // Ignore the 'iterations' parameter for Argon2id and use a fixed sensible default
    const uint32_t time_cost = 3;  // Default Argon2id time cost (iterations parameter is ignored)

    std::vector<uint8_t> hash(hash_len);

    // Hash using Argon2id
    int result = argon2id_hash_raw(
        time_cost,                          // t_cost (time iterations) - fixed at 3
        memory_kb,                          // m_cost (memory in KB)
        parallelism,                        // parallelism
        username.data(),                    // password (username in our case)
        username.size(),                    // password length
        salt.data(),                        // salt
        salt.size(),                        // salt length
        hash.data(),                        // output hash buffer
        hash_len                            // hash length
    );

    if (result != ARGON2_OK) {
        return std::unexpected(VaultError::KeyDerivationFailed);
    }

    return hash;
}
#endif  // ENABLE_ARGON2

// ============================================================================
// Private Utility: Constant-Time Comparison
// ============================================================================

bool UsernameHashService::constant_time_compare(std::span<const uint8_t> a,
                                                std::span<const uint8_t> b) noexcept {
    // Check sizes first (not timing-sensitive)
    if (a.size() != b.size()) {
        return false;
    }

    // Use OpenSSL's constant-time comparison (timing-attack resistant)
    return CRYPTO_memcmp(a.data(), b.data(), a.size()) == 0;
}

}  // namespace KeepTower
