// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#ifndef YUBIKEY_ALGORITHM_H
#define YUBIKEY_ALGORITHM_H

#include <cstddef>
#include <cstdint>
#include <string_view>

/**
 * @brief YubiKey HMAC algorithm specifications for FIPS compliance
 *
 * Defines FIPS-140-3 approved hash algorithms for YubiKey challenge-response.
 * SHA-1 is explicitly prohibited per NIST SP 800-140B (deprecated since 2011).
 *
 * FIPS-Approved Hash Functions (NIST SP 800-140B):
 * - ✅ SHA-256, SHA-384, SHA-512 (SHA-2 family)
 * - ✅ SHA3-256, SHA3-384, SHA3-512 (SHA-3 family)
 * - ❌ MD5, SHA-1 (deprecated, prohibited)
 *
 * YubiKey Compatibility:
 * - YubiKey 5 Series (firmware 5.0+): HMAC-SHA1, HMAC-SHA256
 * - YubiKey 5 FIPS (firmware 5.4+): HMAC-SHA256 (FIPS mode)
 * - Future firmware: HMAC-SHA3 support planned
 *
 * @see https://csrc.nist.gov/publications/detail/sp/800-140b/final
 * @see https://developers.yubico.com/YubiHSM2/Concepts/Algorithms.html
 */
enum class YubiKeyAlgorithm : uint8_t {
    /**
     * @brief HMAC-SHA1 (20-byte response)
     * @deprecated NOT FIPS-140-3 approved. Legacy support only.
     * @warning SHA-1 is cryptographically broken (collision attacks)
     * @note Only use for backward compatibility with existing vaults
     */
    HMAC_SHA1 = 0x01,

    /**
     * @brief HMAC-SHA256 (32-byte response)
     * @note FIPS-140-3 APPROVED. Default for new vaults.
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
        case YubiKeyAlgorithm::HMAC_SHA1:      return 20;
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
        case YubiKeyAlgorithm::HMAC_SHA1:      return "HMAC-SHA1";
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
        case YubiKeyAlgorithm::HMAC_SHA1:
        default:
            return false;
    }
}

/**
 * @brief Get the default algorithm for FIPS mode
 * @return YubiKeyAlgorithm::HMAC_SHA256 (FIPS-approved default)
 */
[[nodiscard]] constexpr YubiKeyAlgorithm yubikey_algorithm_fips_default() noexcept {
    return YubiKeyAlgorithm::HMAC_SHA256;
}

/**
 * @brief Get the legacy algorithm for backward compatibility
 * @return YubiKeyAlgorithm::HMAC_SHA1 (deprecated, not FIPS-approved)
 */
[[nodiscard]] constexpr YubiKeyAlgorithm yubikey_algorithm_legacy() noexcept {
    return YubiKeyAlgorithm::HMAC_SHA1;
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
