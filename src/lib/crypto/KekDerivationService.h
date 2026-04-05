// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

/**
 * @file KekDerivationService.h
 * @brief Pure cryptographic KEK derivation service
 */

#pragma once

#include "core/VaultError.h"
#include "utils/SecureMemory.h"
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>

namespace KeepTower {

/**
 * @class KekDerivationService
 * @brief Stateless cryptographic Key Encryption Key (KEK) derivation.
 *
 * This service provides password-based KEK derivation primitives for vault
 * workflows. It intentionally contains only cryptographic behavior and no
 * settings/UI dependencies.
 */
class KekDerivationService {
public:
    /**
     * @brief Supported KEK derivation algorithms.
     */
    enum class Algorithm : uint8_t {
        PBKDF2_HMAC_SHA256 = 0x04,  ///< FIPS-approved PBKDF2-HMAC-SHA256.
        ARGON2ID = 0x05             ///< Memory-hard Argon2id (not FIPS-approved).
    };

    /**
     * @brief Tunable parameters for KEK derivation.
     */
    struct AlgorithmParameters {
        uint32_t pbkdf2_iterations = 600000;  ///< PBKDF2 iteration count.
        uint32_t argon2_memory_kb = 65536;    ///< Argon2 memory cost in KiB.
        uint32_t argon2_time_cost = 3;        ///< Argon2 time cost (iterations).
        uint8_t argon2_parallelism = 4;       ///< Argon2 lane/thread count.
    };

    /**
     * @brief Derive a 256-bit KEK from a password and salt.
     * @param password UTF-8 password input.
     * @param algorithm Derivation algorithm.
     * @param salt Unique salt, minimum 16 bytes.
     * @param params Algorithm parameters.
     * @return Derived key or error.
     */
    [[nodiscard]] static std::expected<SecureVector<uint8_t>, VaultError>
    derive_kek(
        std::string_view password,
        Algorithm algorithm,
        std::span<const uint8_t> salt,
        const AlgorithmParameters& params) noexcept;

    /**
     * @brief Check whether an algorithm is FIPS-140-3 approved.
     * @param algorithm Algorithm to inspect.
     * @return True when the algorithm is FIPS-approved.
     */
    [[nodiscard]] static constexpr bool is_fips_approved(Algorithm algorithm) noexcept {
        return algorithm == Algorithm::PBKDF2_HMAC_SHA256;
    }

    /**
     * @brief Return KEK output size in bytes.
     * @param algorithm Algorithm whose output size is requested.
     * @return Output size in bytes for the algorithm.
     */
    [[nodiscard]] static constexpr size_t get_output_size([[maybe_unused]] Algorithm algorithm) noexcept {
        return 32;
    }

    /**
     * @brief Convert algorithm enum to a human-readable name.
     * @param algorithm Algorithm to stringify.
     * @return Human-readable algorithm name.
     */
    [[nodiscard]] static constexpr std::string_view algorithm_to_string(Algorithm algorithm) noexcept {
        switch (algorithm) {
            case Algorithm::PBKDF2_HMAC_SHA256: return "PBKDF2-HMAC-SHA256";
            case Algorithm::ARGON2ID: return "Argon2id";
            default: return "Unknown";
        }
    }

private:
    [[nodiscard]] static std::expected<SecureVector<uint8_t>, VaultError>
    derive_kek_pbkdf2(
        std::string_view password,
        std::span<const uint8_t> salt,
        uint32_t iterations) noexcept;

    [[nodiscard]] static std::expected<SecureVector<uint8_t>, VaultError>
    derive_kek_argon2id(
        std::string_view password,
        std::span<const uint8_t> salt,
        uint32_t memory_kb,
        uint32_t time_cost,
        uint8_t parallelism) noexcept;
};

} // namespace KeepTower