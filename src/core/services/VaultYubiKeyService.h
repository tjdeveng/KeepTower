// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

/**
 * @file VaultYubiKeyService.h
 * @brief Service class for YubiKey hardware operations
 *
 * This service handles ALL YubiKey hardware interactions for vault management,
 * following the Single Responsibility Principle.
 *
 * Responsibilities:
 * - YubiKey device detection and enumeration
 * - Two-step enrollment (policy challenge + user challenge)
 * - Challenge-response operations via HMAC-SHA1
 * - Device information retrieval
 * - Error handling and validation
 *
 * NOT responsible for:
 * - Cryptographic operations (see VaultCryptoService)
 * - File I/O operations (see VaultFileService)
 * - Vault state management (see VaultManager)
 * - Key derivation/combination (see VaultCryptoService)
 */

#pragma once

#include "../VaultError.h"
#include <array>
#include <vector>
#include <string>
#include <cstdint>
#include <functional>
#include <glibmm/ustring.h>
#include <expected>

namespace KeepTower {

/**
 * @class VaultYubiKeyService
 * @brief YubiKey hardware operations service for vault management
 *
 * This class provides YubiKey-specific operations. All methods are
 * [[nodiscard]] to ensure results are used. Hardware operations may
 * fail due to device removal, user cancellation, or timeout.
 *
 * Thread-safety: Methods are NOT thread-safe due to libfido2 limitations.
 *                Caller must serialize access to YubiKey hardware.
 * FIPS-compliance: Uses FIPS-approved HMAC-SHA1 algorithm
 */
class VaultYubiKeyService {
public:
    // ========================================================================
    // Result Types
    // ========================================================================

    /**
     * @brief YubiKey device information
     */
    struct DeviceInfo {
        std::string serial;           ///< Device serial number
        std::string manufacturer;     ///< Manufacturer name
        std::string product;          ///< Product name
        uint8_t slot;                 ///< HMAC slot (1 or 2)
        bool is_fips;                 ///< True if FIPS-capable device
    };

    /**
     * @brief Result of two-step enrollment
     */
    struct EnrollmentResult {
        std::vector<uint8_t> policy_response;  ///< Policy challenge response (20 bytes)
        std::vector<uint8_t> user_response;    ///< User challenge response (20 bytes)
        std::vector<uint8_t> credential_id;    ///< FIDO2 credential ID
        DeviceInfo device_info;                ///< Device used for enrollment
    };

    /**
     * @brief Result of challenge-response operation
     */
    struct ChallengeResult {
        std::vector<uint8_t> response;  ///< HMAC-SHA1 response (20 bytes)
        DeviceInfo device_info;         ///< Device used
    };

    // ========================================================================
    // Public Interface
    // ========================================================================

    /**
     * @brief Constructor (default)
     */
    VaultYubiKeyService() = default;

    /**
     * @brief Detect available YubiKey devices
     *
     * Enumerates all connected FIDO2/WebAuthn devices and filters for
     * YubiKeys with HMAC-SHA1 capability.
     *
     * @return Vector of device information, or VaultError
     *
     * @note May return empty vector if no YubiKeys are connected
     * @note Thread-unsafe - do not call concurrently
     */
    [[nodiscard]] VaultResult<std::vector<DeviceInfo>> detect_devices();

    /**
     * @brief Perform two-step YubiKey enrollment
     *
     * Enrolls a YubiKey for vault access using two separate challenges:
     * 1. Policy challenge (fixed, same for all users in vault)
     * 2. User challenge (random, unique per user key slot)
     *
     * This implements defense-in-depth: compromising one challenge
     * response is insufficient to unlock the vault.
     *
     * @param user_id User identifier for FIDO2 credential
     * @param policy_challenge Fixed challenge for vault policy (32 bytes)
     * @param user_challenge Random challenge for user slot (32 bytes)
     * @param pin YubiKey PIN for authentication (4-63 chars)
     * @param slot HMAC slot to use (1 or 2)
     * @param enforce_fips Enable FIPS mode enforcement
     * @param progress_callback Optional callback for touch progress (touch 1 of 2, touch 2 of 2)
     * @return EnrollmentResult with both responses, or VaultError
     *
     * @note Requires user to touch YubiKey twice (once per challenge)
     * @note May fail if device is removed or user cancels
     * @note PIN is required for FIDO2/WebAuthn YubiKeys
     * @note Progress callback is invoked before each touch operation
     */
    [[nodiscard]] VaultResult<EnrollmentResult> enroll_yubikey(
        const std::string& user_id,
        const std::array<uint8_t, 32>& policy_challenge,
        const std::array<uint8_t, 32>& user_challenge,
        const std::string& pin,
        uint8_t slot = 1,
        bool enforce_fips = false,
        std::function<void(const std::string&)> progress_callback = nullptr);

    /**
     * @brief Perform single challenge-response operation
     *
     * Sends a challenge to YubiKey and receives HMAC-SHA1 response.
     * Used during vault opening to derive the YubiKey component of KEK.
     *
     * @param challenge Challenge bytes (1-64 bytes, typically 32)
     * @param pin YubiKey PIN for authentication
     * @param slot HMAC slot to use (1 or 2)
     * @return ChallengeResult with HMAC response, or VaultError
     *
     * @note Requires user to touch YubiKey
     * @note Response is deterministic (same challenge → same response)
     * @note May time out if user doesn't touch within ~15 seconds
     */
    [[nodiscard]] VaultResult<ChallengeResult> challenge_response(
        const std::vector<uint8_t>& challenge,
        const std::string& pin,
        uint8_t slot = 1,
        bool enforce_fips = false);

    /**
     * @brief Get information about a specific YubiKey
     *
     * Retrieves device information without performing any operations.
     * Useful for displaying device selection UI.
     *
     * @param device_path Device path from detect_devices()
     * @return DeviceInfo or VaultError
     *
     * @note Device must still be connected
     */
    [[nodiscard]] VaultResult<DeviceInfo> get_device_info(
        const std::string& device_path,
        bool enforce_fips = false);

    /**
     * @brief Validate YubiKey PIN format
     *
     * Checks if PIN meets YubiKey requirements:
     * - Length: 4-63 characters
     * - No other restrictions (any UTF-8 allowed)
     *
     * @param pin PIN to validate
     * @return true if valid, false otherwise
     *
     * @note Does NOT verify PIN correctness with device
     * @note Only validates format
     */
    [[nodiscard]] static bool validate_pin_format(const std::string& pin);

    /**
     * @brief Check if a device is a FIPS-capable YubiKey
     *
     * FIPS YubiKeys have additional security features and restrictions.
     *
     * @param device_info Device info from detect_devices()
     * @return true if FIPS-capable
     */
    [[nodiscard]] static bool is_fips_device(const DeviceInfo& device_info);

    /**
     * @brief Generate random challenge for YubiKey enrollment
     *
     * Creates cryptographically secure random challenge suitable for
     * YubiKey HMAC-SHA1 operations.
     *
     * @param size Challenge size in bytes (1-64, default 32)
     * @return Random challenge bytes or VaultError
     *
     * @note Uses OpenSSL FIPS-approved RNG
     */
    [[nodiscard]] static VaultResult<std::vector<uint8_t>> generate_challenge(
        size_t size = 32);

private:
    // No state - all methods are stateless
    // YubiKey operations go through libfido2 (stateless from our perspective)
};

} // namespace KeepTower
