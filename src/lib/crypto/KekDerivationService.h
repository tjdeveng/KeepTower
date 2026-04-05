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

class KekDerivationService {
public:
    enum class Algorithm : uint8_t {
        PBKDF2_HMAC_SHA256 = 0x04,
        ARGON2ID = 0x05
    };

    struct AlgorithmParameters {
        uint32_t pbkdf2_iterations = 600000;
        uint32_t argon2_memory_kb = 65536;
        uint32_t argon2_time_cost = 3;
        uint8_t argon2_parallelism = 4;
    };

    [[nodiscard]] static std::expected<SecureVector<uint8_t>, VaultError>
    derive_kek(
        std::string_view password,
        Algorithm algorithm,
        std::span<const uint8_t> salt,
        const AlgorithmParameters& params) noexcept;

    [[nodiscard]] static constexpr bool is_fips_approved(Algorithm algorithm) noexcept {
        return algorithm == Algorithm::PBKDF2_HMAC_SHA256;
    }

    [[nodiscard]] static constexpr size_t get_output_size([[maybe_unused]] Algorithm algorithm) noexcept {
        return 32;
    }

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