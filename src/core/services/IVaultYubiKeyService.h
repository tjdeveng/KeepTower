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

    virtual ~IVaultYubiKeyService() = default;

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
};

} // namespace KeepTower