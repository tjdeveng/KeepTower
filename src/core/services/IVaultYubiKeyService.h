// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#pragma once

#include "../VaultError.h"
#include "lib/yubikey/YubiKeyAlgorithm.h"

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <functional>
#include <format>

namespace KeepTower {

/**
 * @brief Seam interface for YubiKey hardware operations
 *
 * Abstracts all YubiKey interactions (device detection, challenge-response,
 * and enrollment) behind a testable boundary. Production code uses
 * VaultYubiKeyService; tests inject FakeVaultYubiKeyService.
 */
class IVaultYubiKeyService {
public:
    /// @brief Controls how a serial number mismatch between expected and actual YubiKey is handled
    enum class SerialMismatchPolicy {
        StrictError, ///< Abort operation and return an error
        WarnOnly,    ///< Log a warning but continue
    };

    /// @brief Information about a connected YubiKey device
    struct DeviceInfo {
        std::string serial;       ///< Device serial number
        std::string manufacturer; ///< Manufacturer string
        std::string product;      ///< Product / model string
        uint8_t slot = 0;         ///< HMAC slot number (1 or 2)
        bool is_fips = false;     ///< True if device is operating in FIPS mode

        int version_major = 0;       ///< Firmware version major component
        int version_minor = 0;       ///< Firmware version minor component
        int version_build = 0;       ///< Firmware version build component
        bool slot2_configured = false; ///< True if HMAC slot 2 has a credential configured
        bool is_fips_capable = false;  ///< True if the device hardware supports FIPS operation
        bool is_fips_mode = false;     ///< True if the device is currently in FIPS mode

        /// @brief Format firmware version as "major.minor.build" string
        /// @return Dot-separated firmware version string
        [[nodiscard]] std::string version_string() const noexcept {
            return std::format("{}.{}.{}", version_major, version_minor, version_build);
        }
    };

    /// @brief Result of a successful challenge-response operation
    struct ChallengeResult {
        std::vector<uint8_t> response; ///< HMAC response bytes from the YubiKey
        DeviceInfo device_info;        ///< Information about the device that responded
    };

    /// @brief Result of a successful YubiKey enrollment operation
    struct EnrollmentResult {
        std::vector<uint8_t> policy_response; ///< Policy challenge response
        std::vector<uint8_t> user_response;   ///< User challenge response (combine with KEK)
        std::vector<uint8_t> credential_id;  ///< FIDO2 credential ID for storage
        DeviceInfo device_info;               ///< Device used for enrollment
    };

    virtual ~IVaultYubiKeyService() = default;

    /**
     * @brief Detect available YubiKey devices
     *
     * Enumerates all connected FIDO2/WebAuthn devices that are YubiKeys.
     * May be called from UI thread for simple presence check.
     *
     * @return Vector of device information, or VaultError if detection fails
     */
    [[nodiscard]] virtual VaultResult<std::vector<DeviceInfo>> detect_devices() = 0;

    /**
     * @brief Perform authenticated challenge-response
     *
     * Executes YubiKey challenge with configurable serial verification behavior.
     *
     * @param challenge Challenge bytes (1-64 bytes)
     * @param credential_id FIDO2 credential ID from enrollment
     * @param pin Optional PIN for protected credentials
     * @param expected_serial Expected device serial
     * @param algorithm YubiKey algorithm (HMAC-SHA1 or HMAC-SHA256)
     * @param require_touch Whether to require user touch confirmation
     * @param timeout_ms Timeout in milliseconds
     * @param enforce_fips Whether to enforce FIPS mode
     * @param serial_mismatch_policy How to handle serial mismatch (strict error vs warning-only)
     * @return ChallengeResult with response and device info, or error
     */
    [[nodiscard]] virtual VaultResult<ChallengeResult> perform_authenticated_challenge(
        const std::vector<uint8_t>& challenge,
        const std::vector<uint8_t>& credential_id,
        const std::string& pin,
        const std::string& expected_serial,
        YubiKeyAlgorithm algorithm,
        bool require_touch,
        int timeout_ms,
        bool enforce_fips = false,
        SerialMismatchPolicy serial_mismatch_policy = SerialMismatchPolicy::StrictError) = 0;

    /**
     * @brief Perform two-step YubiKey enrollment
     *
     * Creates a FIDO2 credential and performs challenge-response to bind the
     * YubiKey to a vault user slot.
     *
     * @param user_id User identifier for FIDO2 credential creation
     * @param policy_challenge Fixed challenge bytes (32 bytes, may be unused)
     * @param user_challenge Random per-user challenge bytes (32 bytes)
     * @param pin YubiKey PIN (4-63 characters)
     * @param slot HMAC slot to use (1 or 2)
     * @param enforce_fips Enforce FIPS-approved operation only
     * @param progress_callback Optional callback for touch progress messages
     * @return EnrollmentResult with response, credential ID and device info, or error
     */
    [[nodiscard]] virtual VaultResult<EnrollmentResult> enroll_yubikey(
        const std::string& user_id,
        const std::array<uint8_t, 32>& policy_challenge,
        const std::array<uint8_t, 32>& user_challenge,
        const std::string& pin,
        uint8_t slot = 1,
        bool enforce_fips = false,
        std::function<void(const std::string&)> progress_callback = nullptr) = 0;
};

} // namespace KeepTower