// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#include "KekDerivationService.h"

#include "utils/Log.h"
#include <argon2.h>
#include <openssl/evp.h>

namespace KeepTower {

std::expected<SecureVector<uint8_t>, VaultError>
KekDerivationService::derive_kek(
    std::string_view password,
    Algorithm algorithm,
    std::span<const uint8_t> salt,
    const AlgorithmParameters& params) noexcept {

    if (salt.size() < 16) {
        Log::error("KekDerivationService: Salt too short ({} bytes, minimum 16)", salt.size());
        return std::unexpected(VaultError::InvalidSalt);
    }

    switch (algorithm) {
        case Algorithm::PBKDF2_HMAC_SHA256:
            return derive_kek_pbkdf2(password, salt, params.pbkdf2_iterations);

        case Algorithm::ARGON2ID:
            return derive_kek_argon2id(
                password,
                salt,
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

    SecureVector<uint8_t> kek(32);

    int result = PKCS5_PBKDF2_HMAC(
        password.data(), password.size(),
        salt.data(), salt.size(),
        static_cast<int>(iterations),
        EVP_sha256(),
        32,
        kek.data());

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

    SecureVector<uint8_t> kek(32);

    int result = argon2id_hash_raw(
        time_cost,
        memory_kb,
        parallelism,
        password.data(),
        password.size(),
        salt.data(),
        salt.size(),
        kek.data(),
        kek.size());

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

} // namespace KeepTower