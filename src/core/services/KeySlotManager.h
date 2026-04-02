// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#pragma once

#include "../VaultError.h"
#include "../MultiUserTypes.h"

#include <cstddef>
#include <string_view>
#include <vector>

namespace KeepTower {

class KeySlotManager {
public:
    [[nodiscard]] static KeySlot* find_slot_by_username_hash(
        std::vector<KeySlot>& slots,
        std::string_view username,
        const VaultSecurityPolicy& policy);

    [[nodiscard]] static const KeySlot* find_slot_by_username_hash(
        const std::vector<KeySlot>& slots,
        std::string_view username,
        const VaultSecurityPolicy& policy);

    [[nodiscard]] static std::vector<KeySlot> list_active_users(
        const std::vector<KeySlot>& slots);

    [[nodiscard]] static int count_active_administrators(
        const std::vector<KeySlot>& slots) noexcept;

    [[nodiscard]] static size_t find_available_slot_index(
        const std::vector<KeySlot>& slots) noexcept;

    [[nodiscard]] static bool user_exists(
        const std::vector<KeySlot>& slots,
        std::string_view username,
        const VaultSecurityPolicy& policy);

    [[nodiscard]] static VaultResult<size_t> store_user_slot(
        std::vector<KeySlot>& slots,
        KeySlot slot,
        size_t max_slots);

    [[nodiscard]] static VaultResult<> deactivate_user(
        std::vector<KeySlot>& slots,
        std::string_view username,
        const VaultSecurityPolicy& policy);
};

} // namespace KeepTower