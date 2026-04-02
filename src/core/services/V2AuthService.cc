// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#include "V2AuthService.h"

#include "KeySlotManager.h"
#include "../../utils/Log.h"

namespace Log = KeepTower::Log;

namespace KeepTower {

VaultResult<KeySlot*> V2AuthService::resolve_user_slot_for_open(
    std::vector<KeySlot>& slots,
    std::string_view username,
    const VaultSecurityPolicy& policy) {

    KeySlot* user_slot = KeySlotManager::find_slot_by_username_hash(slots, username, policy);
    if (!user_slot) {
        Log::error("V2AuthService: No active key slot found for requested user");
        return std::unexpected(VaultError::AuthenticationFailed);
    }

    return user_slot;
}

} // namespace KeepTower
