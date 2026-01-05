// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#ifndef YUBIKEY_ALGORITHM_H
#define YUBIKEY_ALGORITHM_H

#include <cstddef>
#include <cstdint>
#include <string_view>

/**
 * @brief YubiKey HMAC algorithm specifications for FIPS-140-3 compliance
 *
 * Defines FIPS-140-3 approved hash algorithms for YubiKey challenge-response.
 * SHA-1 support has been completely removed for FIPS-140-3 compliance.
 *
 * FIPS-Approved Hash Functions (NIST SP 800-140B):
 * - ✅ SHA-256 (minimum required, currently supported by YubiKey)
 * - ✅ SHA-512 (reserved for future YubiKey firmware)
 * - ✅ SHA3-256, SHA3-512 (reserved for future YubiKey firmware)
 * - ❌ SHA-1, MD5 (deprecated, prohibited, removed)
 *
 * YubiKey Compatibility:
 * - YubiKey 5 Series: Configure slot 2 for HMAC-SHA256
 * - YubiKey 5 FIPS: Native SHA-256 support in FIPS mode
 * - Minimum requirement: SHA-256 (no backward compatibility with SHA-1)
 *
 * @note Breaking change: SHA-1 vaults are not supported. Reconfigure YubiKey for SHA-256.
 * @see https://csrc.nist.gov/publications/detail/sp/800-140b/final
 * @see https://developers.yubico.com/YubiHSM2/Concepts/Algorithms.html
 */
enum class YubiKeyAlgorithm : uint8_t {
    /**
     * @brief HMAC-SHA256 (32-byte response)
     * @note FIPS-140-3 APPROVED. Default and minimum algorithm.
     * @note Supported by YubiKey 5 Series (firmware 5.0+)
     */
    HMAC_SHA256 = 0x02,

    /**
     * @brief HMAC-SHA512 (64-byte response)
     * @note FIPS-140-3 APPROVED. Maximum security.
     * @note Currently not supported by YubiKey firmware
     * @note Reserved for future use
     */
    HMAC_SHA512 = 0x03,

    /**
     * @brief HMAC-SHA3-256 (32-byte response)
     * @note FIPS-140-3 APPROVED. Future-proof quantum-resistant.
     * @note Currently not supported by YubiKey firmware
     * @note Reserved for future use when YubiKey adds SHA3
     */
    HMAC_SHA3_256 = 0x10,

    /**
     * @brief HMAC-SHA3-512 (64-byte response)
     * @note FIPS-140-3 APPROVED. Maximum future security.
     * @note Currently not supported by YubiKey firmware
     * @note Reserved for future use
     */
    HMAC_SHA3_512 = 0x11
};

/**
 * @brief Get the response size for a YubiKey HMAC algorithm
 * @param algorithm The HMAC algorithm
 * @return Response size in bytes, or 0 if unknown
 */
[[nodiscard]] constexpr size_t yubikey_algorithm_response_size(YubiKeyAlgorithm algorithm) noexcept {
    switch (algorithm) {
        case YubiKeyAlgorithm::HMAC_SHA256:    return 32;
        case YubiKeyAlgorithm::HMAC_SHA512:    return 64;
        case YubiKeyAlgorithm::HMAC_SHA3_256:  return 32;
        case YubiKeyAlgorithm::HMAC_SHA3_512:  return 64;
        default:                                return 0;
    }
}

/**
 * @brief Get human-readable name for an algorithm
 * @param algorithm The HMAC algorithm
 * @return Algorithm name (e.g., "HMAC-SHA256")
 */
[[nodiscard]] constexpr std::string_view yubikey_algorithm_name(YubiKeyAlgorithm algorithm) noexcept {
    switch (algorithm) {
        case YubiKeyAlgorithm::HMAC_SHA256:    return "HMAC-SHA256";
        case YubiKeyAlgorithm::HMAC_SHA512:    return "HMAC-SHA512";
        case YubiKeyAlgorithm::HMAC_SHA3_256:  return "HMAC-SHA3-256";
        case YubiKeyAlgorithm::HMAC_SHA3_512:  return "HMAC-SHA3-512";
        default:                                return "Unknown";
    }
}

/**
 * @brief Check if an algorithm is FIPS-140-3 approved
 * @param algorithm The HMAC algorithm
 * @return true if algorithm is FIPS-approved, false otherwise
 */
[[nodiscard]] constexpr bool yubikey_algorithm_is_fips_approved(YubiKeyAlgorithm algorithm) noexcept {
    switch (algorithm) {
        case YubiKeyAlgorithm::HMAC_SHA256:
        case YubiKeyAlgorithm::HMAC_SHA512:
        case YubiKeyAlgorithm::HMAC_SHA3_256:
        case YubiKeyAlgorithm::HMAC_SHA3_512:
            return true;
        default:
            return false;
    }
}

/**
 * @brief Get the default algorithm for FIPS mode
 * @return YubiKeyAlgorithm::HMAC_SHA256 (FIPS-approved default and minimum)
 */
[[nodiscard]] constexpr YubiKeyAlgorithm yubikey_algorithm_fips_default() noexcept {
    return YubiKeyAlgorithm::HMAC_SHA256;
}

/**
 * @brief Maximum response size across all algorithms
 */
inline constexpr size_t YUBIKEY_MAX_RESPONSE_SIZE = 64;

/**
 * @brief Challenge size (fixed for all algorithms)
 */
inline constexpr size_t YUBIKEY_CHALLENGE_SIZE = 64;

#endif // YUBIKEY_ALGORITHM_H
