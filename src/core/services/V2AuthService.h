// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#pragma once

#include "../VaultError.h"
#include "../MultiUserTypes.h"

#include <string_view>
#include <vector>

namespace KeepTower {

class V2AuthService {
public:
    [[nodiscard]] static VaultResult<KeySlot*> resolve_user_slot_for_open(
        std::vector<KeySlot>& slots,
        std::string_view username,
        const VaultSecurityPolicy& policy);
};

} // namespace KeepTower
