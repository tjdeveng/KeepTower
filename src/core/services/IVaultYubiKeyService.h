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

    [[nodiscard]] virtual VaultResult<ChallengeResult> perform_authenticated_challenge(
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