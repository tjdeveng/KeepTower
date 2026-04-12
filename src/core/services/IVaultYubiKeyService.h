// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#pragma once

#include "../VaultError.h"
#include "lib/yubikey/YubiKeyAlgorithm.h"

#include <cstdint>
#include <string>
#include <vector>

namespace KeepTower {

class IVaultYubiKeyService {
public:
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

    virtual ~IVaultYubiKeyService() = default;

    /**
     * @brief Perform authenticated challenge-response (V1/general purpose)
     *
     * Executes YubiKey challenge with strict serial verification.
     * Serial mismatch is treated as a hard error.
     *
     * @param challenge Challenge bytes (1-64 bytes)
     * @param credential_id FIDO2 credential ID from enrollment
     * @param pin Optional PIN for protected credentials
     * @param expected_serial Expected device serial (error if mismatch)
     * @param algorithm YubiKey algorithm (HMAC-SHA1 or HMAC-SHA256)
     * @param require_touch Whether to require user touch confirmation
     * @param timeout_ms Timeout in milliseconds
     * @param enforce_fips Whether to enforce FIPS mode
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
        bool enforce_fips = false) = 0;

    /**
     * @brief Perform authenticated challenge-response (V2 vault specific)
     *
     * Executes YubiKey challenge with warning-only serial verification.
     * Serial mismatch is logged as warning but does not fail the operation.
     *
     * @param challenge Challenge bytes (1-64 bytes) from vault header
     * @param credential_id FIDO2 credential ID from user key slot
     * @param pin User's decrypted YubiKey PIN
     * @param expected_serial Expected device serial (warning if mismatch)
     * @param algorithm YubiKey algorithm from vault security policy
     * @param require_touch Whether to require user touch confirmation
     * @param timeout_ms Timeout in milliseconds
     * @param enforce_fips Whether to enforce FIPS mode
     * @return ChallengeResult with response and device info, or error
     */
    [[nodiscard]] virtual VaultResult<ChallengeResult> perform_v2_authenticated_challenge(
        const std::vector<uint8_t>& challenge,
        const std::vector<uint8_t>& credential_id,
        const std::string& pin,
        const std::string& expected_serial,
        YubiKeyAlgorithm algorithm,
        bool require_touch,
        int timeout_ms,
        bool enforce_fips = false) = 0;
};

} // namespace KeepTower