// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#pragma once

#include "../VaultError.h"
#include "../MultiUserTypes.h"

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace KeepTower {

class V2AuthService {
public:
    [[nodiscard]] static VaultResult<KeySlot*> resolve_user_slot_for_open(
        std::vector<KeySlot>& slots,
        std::string_view username,
        const VaultSecurityPolicy& policy);

    [[nodiscard]] static VaultResult<std::array<uint8_t, 32>> derive_password_kek_for_slot(
        const KeySlot& slot,
        std::string_view password,
        uint32_t pbkdf2_iterations,
        const VaultSecurityPolicy& policy);

    [[nodiscard]] static VaultResult<std::string> decrypt_yubikey_pin_for_open(
        const KeySlot& slot,
        const std::array<uint8_t, 32>& password_kek);
};

} // namespace KeepTower
