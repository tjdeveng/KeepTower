// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#pragma once

#include "../VaultError.h"
#include "../MultiUserTypes.h"

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace KeepTower {

class KeySlotManager {
public:
    [[nodiscard]] static KeySlot create_user_slot(
        std::string_view username,
        uint8_t kek_derivation_algorithm,
        std::span<const uint8_t> username_hash,
        const std::array<uint8_t, 16>& username_salt,
        const std::array<uint8_t, 32>& salt,
        const std::array<uint8_t, 40>& wrapped_dek,
        UserRole role,
        bool must_change_password);

    [[nodiscard]] static KeySlot* find_slot_by_username_hash(
        std::vector<KeySlot>& slots,
        std::string_view username,
        const VaultSecurityPolicy& policy);

    [[nodiscard]] static const KeySlot* find_slot_by_username_hash(
        const std::vector<KeySlot>& slots,
        std::string_view username,
        const VaultSecurityPolicy& policy);

    [[nodiscard]] static bool is_yubikey_enrolled_for_user(
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

    [[nodiscard]] static VaultResult<KeySlot*> require_user_slot(
        std::vector<KeySlot>& slots,
        std::string_view username,
        const VaultSecurityPolicy& policy);

    [[nodiscard]] static VaultResult<const KeySlot*> require_user_slot(
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

    static void apply_yubikey_enrollment(
        KeySlot& slot,
        bool enrolled,
        const std::array<uint8_t, 32>& challenge,
        std::string serial,
        int64_t enrolled_at,
        std::vector<uint8_t> encrypted_pin,
        std::vector<uint8_t> credential_id) noexcept;

    static void add_password_history_entry(
        KeySlot& slot,
        const PasswordHistoryEntry& entry,
        uint32_t max_depth);

    [[nodiscard]] static size_t clear_password_history(KeySlot& slot) noexcept;

    static void update_password_material(
        KeySlot& slot,
        const std::array<uint8_t, 32>& salt,
        const std::array<uint8_t, 40>& wrapped_dek,
        bool must_change_password,
        int64_t password_changed_at) noexcept;

    static void clear_yubikey_enrollment(KeySlot& slot) noexcept;

    static void update_yubikey_encrypted_pin(
        KeySlot& slot,
        std::vector<uint8_t> encrypted_pin) noexcept;

    static void enroll_yubikey(
        KeySlot& slot,
        const std::array<uint8_t, 40>& wrapped_dek,
        const std::array<uint8_t, 32>& challenge,
        std::string serial,
        int64_t enrolled_at,
        std::vector<uint8_t> encrypted_pin,
        std::vector<uint8_t> credential_id) noexcept;

    static void unenroll_yubikey(
        KeySlot& slot,
        const std::array<uint8_t, 32>& salt,
        const std::array<uint8_t, 40>& wrapped_dek) noexcept;
};

} // namespace KeepTower