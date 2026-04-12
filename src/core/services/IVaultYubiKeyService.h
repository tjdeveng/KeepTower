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

namespace KeepTower {

class IVaultYubiKeyService {
public:
    enum class SerialMismatchPolicy {
        StrictError,
        WarnOnly,
    };

    struct DeviceInfo {
        std::string serial;
        std::string manufacturer;
        std::string product;
        uint8_t slot = 0;
        bool is_fips = false;
    };

    struct ChallengeResult {
        std::vector<uint8_t> response;
        DeviceInfo device_info;
    };

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