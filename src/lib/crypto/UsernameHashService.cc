// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

/**
 * @file UsernameHashService.cc
 * @brief Implementation of username hashing service
 */

#include "config.h"
#include "UsernameHashService.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <cstring>
#include <memory>

#ifdef ENABLE_ARGON2
#include <argon2.h>
#endif

namespace KeepTower {

std::expected<std::vector<uint8_t>, VaultError>
UsernameHashService::hash_username(std::string_view username,
                                   Algorithm algorithm,
                                   std::span<const uint8_t, 16> salt,
                                   uint32_t iterations) {
    if (username.empty()) {
        return std::unexpected(VaultError::InvalidUsername);
    }

    switch (algorithm) {
        case Algorithm::PLAINTEXT_LEGACY:
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
    auto computed_hash = hash_username(username, algorithm, salt, iterations);
    if (!computed_hash.has_value()) {
        return false;
    }

    if (computed_hash->size() != stored_hash.size()) {
        return false;
    }

    return constant_time_compare(*computed_hash, stored_hash);
}

std::expected<std::vector<uint8_t>, VaultError>
UsernameHashService::hash_sha3_256(std::string_view username,
                                   std::span<const uint8_t, 16> salt) {
    std::vector<uint8_t> input;
    input.reserve(username.size() + salt.size());
    input.insert(input.end(), username.begin(), username.end());
    input.insert(input.end(), salt.begin(), salt.end());

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        return std::unexpected(VaultError::CryptoError);
    }

    auto cleanup = [](EVP_MD_CTX* pointer) {
        if (pointer) {
            EVP_MD_CTX_free(pointer);
        }
    };
    std::unique_ptr<EVP_MD_CTX, decltype(cleanup)> ctx_guard(ctx, cleanup);

    if (EVP_DigestInit_ex(ctx, EVP_sha3_256(), nullptr) != 1) {
        return std::unexpected(VaultError::CryptoError);
    }

    if (EVP_DigestUpdate(ctx, input.data(), input.size()) != 1) {
        return std::unexpected(VaultError::CryptoError);
    }

    std::vector<uint8_t> hash(32);
    unsigned int hash_len = 0;
    if (EVP_DigestFinal_ex(ctx, hash.data(), &hash_len) != 1) {
        return std::unexpected(VaultError::CryptoError);
    }

    if (hash_len != 32) {
        return std::unexpected(VaultError::CryptoError);
    }

    return hash;
}

std::expected<std::vector<uint8_t>, VaultError>
UsernameHashService::hash_sha3_384(std::string_view username,
                                   std::span<const uint8_t, 16> salt) {
    std::vector<uint8_t> input;
    input.reserve(username.size() + salt.size());
    input.insert(input.end(), username.begin(), username.end());
    input.insert(input.end(), salt.begin(), salt.end());

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        return std::unexpected(VaultError::CryptoError);
    }

    auto cleanup = [](EVP_MD_CTX* pointer) {
        if (pointer) {
            EVP_MD_CTX_free(pointer);
        }
    };
    std::unique_ptr<EVP_MD_CTX, decltype(cleanup)> ctx_guard(ctx, cleanup);

    if (EVP_DigestInit_ex(ctx, EVP_sha3_384(), nullptr) != 1) {
        return std::unexpected(VaultError::CryptoError);
    }

    if (EVP_DigestUpdate(ctx, input.data(), input.size()) != 1) {
        return std::unexpected(VaultError::CryptoError);
    }

    std::vector<uint8_t> hash(48);
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
    std::vector<uint8_t> input;
    input.reserve(username.size() + salt.size());
    input.insert(input.end(), username.begin(), username.end());
    input.insert(input.end(), salt.begin(), salt.end());

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        return std::unexpected(VaultError::CryptoError);
    }

    auto cleanup = [](EVP_MD_CTX* pointer) {
        if (pointer) {
            EVP_MD_CTX_free(pointer);
        }
    };
    std::unique_ptr<EVP_MD_CTX, decltype(cleanup)> ctx_guard(ctx, cleanup);

    if (EVP_DigestInit_ex(ctx, EVP_sha3_512(), nullptr) != 1) {
        return std::unexpected(VaultError::CryptoError);
    }

    if (EVP_DigestUpdate(ctx, input.data(), input.size()) != 1) {
        return std::unexpected(VaultError::CryptoError);
    }

    std::vector<uint8_t> hash(64);
    unsigned int hash_len = 0;
    if (EVP_DigestFinal_ex(ctx, hash.data(), &hash_len) != 1) {
        return std::unexpected(VaultError::CryptoError);
    }

    if (hash_len != 64) {
        return std::unexpected(VaultError::CryptoError);
    }

    return hash;
}

std::expected<std::vector<uint8_t>, VaultError>
UsernameHashService::hash_pbkdf2_sha256(std::string_view username,
                                        std::span<const uint8_t, 16> salt,
                                        uint32_t iterations) {
    if (iterations < 1000) {
        iterations = 1000;
    }

    std::vector<uint8_t> hash(32);
    if (PKCS5_PBKDF2_HMAC(username.data(),
                          static_cast<int>(username.size()),
                          salt.data(),
                          static_cast<int>(salt.size()),
                          static_cast<int>(iterations),
                          EVP_sha256(),
                          static_cast<int>(hash.size()),
                          hash.data()) != 1) {
        return std::unexpected(VaultError::KeyDerivationFailed);
    }

    return hash;
}

#ifdef ENABLE_ARGON2
std::expected<std::vector<uint8_t>, VaultError>
UsernameHashService::hash_argon2id(std::string_view username,
                                   std::span<const uint8_t, 16> salt,
                                   [[maybe_unused]] uint32_t iterations) {
    const uint32_t memory_kb = 65536;
    const uint32_t parallelism = 1;
    const uint32_t hash_len = 32;
    const uint32_t time_cost = 3;

    std::vector<uint8_t> hash(hash_len);
    int result = argon2id_hash_raw(
        time_cost,
        memory_kb,
        parallelism,
        username.data(),
        username.size(),
        salt.data(),
        salt.size(),
        hash.data(),
        hash_len);

    if (result != ARGON2_OK) {
        return std::unexpected(VaultError::KeyDerivationFailed);
    }

    return hash;
}
#endif

bool UsernameHashService::constant_time_compare(std::span<const uint8_t> a,
                                                std::span<const uint8_t> b) noexcept {
    if (a.size() != b.size()) {
        return false;
    }

    return CRYPTO_memcmp(a.data(), b.data(), a.size()) == 0;
}

}  // namespace KeepTower