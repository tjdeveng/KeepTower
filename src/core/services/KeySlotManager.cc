// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#include "KeySlotManager.h"

#include "UsernameHashService.h"
#include "../../utils/Log.h"

#include <optional>
#include <span>

namespace Log = KeepTower::Log;

namespace KeepTower {

namespace {

KeySlot* find_slot_by_username_hash_mutable(
    std::vector<KeySlot>& slots,
    std::string_view username,
    const VaultSecurityPolicy& policy) {

    auto current_algo = static_cast<UsernameHashService::Algorithm>(
        policy.username_hash_algorithm);

    const bool migration_active = (policy.migration_flags & 0x01) != 0;

    std::optional<UsernameHashService::Algorithm> fallback_algo;
    if (migration_active && policy.username_hash_algorithm_previous != 0x00) {
        fallback_algo = static_cast<UsernameHashService::Algorithm>(
            policy.username_hash_algorithm_previous);
        Log::info(
            "KeySlotManager: Migration active - trying algorithm 0x{:02x} (new) then 0x{:02x} (old)",
            policy.username_hash_algorithm,
            policy.username_hash_algorithm_previous);
    }

    for (auto& slot : slots) {
        if (!slot.active) {
            continue;
        }

        if (migration_active && slot.migration_status == 0x00) {
            continue;
        }

        std::span<const uint8_t> stored_hash(slot.username_hash.data(), slot.username_hash_size);
        const bool matches = UsernameHashService::verify_username(
            username,
            stored_hash,
            current_algo,
            slot.username_salt,
            policy.pbkdf2_iterations);

        if (matches) {
            slot.username = std::string(username);
            Log::debug(
                "KeySlotManager: Match using current algorithm (migration_status=0x{:02x})",
                slot.migration_status);
            return &slot;
        }
    }

    if (migration_active && fallback_algo.has_value()) {
        for (auto& slot : slots) {
            if (!slot.active) {
                continue;
            }

            std::span<const uint8_t> stored_hash(slot.username_hash.data(), slot.username_hash_size);
            const bool matches = UsernameHashService::verify_username(
                username,
                stored_hash,
                *fallback_algo,
                slot.username_salt,
                policy.pbkdf2_iterations);

            if (matches) {
                slot.username = std::string(username);
                slot.migration_status = 0xFF;
                Log::debug("KeySlotManager: Match using fallback algorithm - marked for migration");
                return &slot;
            }
        }
    }

    static const std::vector<UsernameHashService::Algorithm> sweep_algos = {
        UsernameHashService::Algorithm::SHA3_256,
        UsernameHashService::Algorithm::SHA3_384,
        UsernameHashService::Algorithm::SHA3_512,
        UsernameHashService::Algorithm::PBKDF2_SHA256,
        UsernameHashService::Algorithm::ARGON2ID,
    };

    for (const auto algo : sweep_algos) {
        if (algo == current_algo) {
            continue;
        }
        if (migration_active && fallback_algo.has_value() && algo == *fallback_algo) {
            continue;
        }

        for (auto& slot : slots) {
            if (!slot.active) {
                continue;
            }

            std::span<const uint8_t> stored_hash(slot.username_hash.data(), slot.username_hash_size);
            const bool matches = UsernameHashService::verify_username(
                username,
                stored_hash,
                algo,
                slot.username_salt,
                policy.pbkdf2_iterations);

            if (matches) {
                slot.username = std::string(username);
                slot.migration_status = 0xFF;

                Log::warning(
                    "KeySlotManager: RESCUE! Match using algo 0x{:02x} (Status=0x{:02x}, Expected=0x{:02x}/0x{:02x})",
                    static_cast<int>(algo),
                    slot.migration_status,
                    static_cast<int>(current_algo),
                    fallback_algo.has_value() ? static_cast<int>(*fallback_algo) : 0);
                return &slot;
            }
        }
    }

    Log::warning(
        "KeySlotManager: User not found (migration_active={}, tried {} algorithm(s))",
        migration_active,
        fallback_algo.has_value() ? 2 : 1);
    return nullptr;
}

} // namespace

KeySlot* KeySlotManager::find_slot_by_username_hash(
    std::vector<KeySlot>& slots,
    std::string_view username,
    const VaultSecurityPolicy& policy) {
    return find_slot_by_username_hash_mutable(slots, username, policy);
}

const KeySlot* KeySlotManager::find_slot_by_username_hash(
    const std::vector<KeySlot>& slots,
    std::string_view username,
    const VaultSecurityPolicy& policy) {
    auto current_algo = static_cast<UsernameHashService::Algorithm>(
        policy.username_hash_algorithm);

    const bool migration_active = (policy.migration_flags & 0x01) != 0;

    std::optional<UsernameHashService::Algorithm> fallback_algo;
    if (migration_active && policy.username_hash_algorithm_previous != 0x00) {
        fallback_algo = static_cast<UsernameHashService::Algorithm>(
            policy.username_hash_algorithm_previous);
    }

    for (const auto& slot : slots) {
        if (!slot.active) {
            continue;
        }

        if (migration_active && slot.migration_status == 0x00) {
            continue;
        }

        std::span<const uint8_t> stored_hash(slot.username_hash.data(), slot.username_hash_size);
        const bool matches = UsernameHashService::verify_username(
            username,
            stored_hash,
            current_algo,
            slot.username_salt,
            policy.pbkdf2_iterations);

        if (matches) {
            return &slot;
        }
    }

    if (migration_active && fallback_algo.has_value()) {
        for (const auto& slot : slots) {
            if (!slot.active) {
                continue;
            }

            std::span<const uint8_t> stored_hash(slot.username_hash.data(), slot.username_hash_size);
            const bool matches = UsernameHashService::verify_username(
                username,
                stored_hash,
                *fallback_algo,
                slot.username_salt,
                policy.pbkdf2_iterations);

            if (matches) {
                return &slot;
            }
        }
    }

    static const std::vector<UsernameHashService::Algorithm> sweep_algos = {
        UsernameHashService::Algorithm::SHA3_256,
        UsernameHashService::Algorithm::SHA3_384,
        UsernameHashService::Algorithm::SHA3_512,
        UsernameHashService::Algorithm::PBKDF2_SHA256,
        UsernameHashService::Algorithm::ARGON2ID,
    };

    for (const auto algo : sweep_algos) {
        if (algo == current_algo) {
            continue;
        }
        if (migration_active && fallback_algo.has_value() && algo == *fallback_algo) {
            continue;
        }

        for (const auto& slot : slots) {
            if (!slot.active) {
                continue;
            }

            std::span<const uint8_t> stored_hash(slot.username_hash.data(), slot.username_hash_size);
            const bool matches = UsernameHashService::verify_username(
                username,
                stored_hash,
                algo,
                slot.username_salt,
                policy.pbkdf2_iterations);

            if (matches) {
                return &slot;
            }
        }
    }

    return nullptr;
}

std::vector<KeySlot> KeySlotManager::list_active_users(const std::vector<KeySlot>& slots) {
    std::vector<KeySlot> active_users;
    active_users.reserve(slots.size());

    for (const auto& slot : slots) {
        if (slot.active) {
            active_users.push_back(slot);
        }
    }

    return active_users;
}

int KeySlotManager::count_active_administrators(const std::vector<KeySlot>& slots) noexcept {
    int admin_count = 0;
    for (const auto& slot : slots) {
        if (slot.active && slot.role == UserRole::ADMINISTRATOR) {
            ++admin_count;
        }
    }
    return admin_count;
}

size_t KeySlotManager::find_available_slot_index(const std::vector<KeySlot>& slots) noexcept {
    for (size_t i = 0; i < slots.size(); ++i) {
        if (!slots[i].active) {
            return i;
        }
    }
    return slots.size();
}

bool KeySlotManager::user_exists(
    const std::vector<KeySlot>& slots,
    std::string_view username,
    const VaultSecurityPolicy& policy) {
    return find_slot_by_username_hash(slots, username, policy) != nullptr;
}

VaultResult<size_t> KeySlotManager::store_user_slot(
    std::vector<KeySlot>& slots,
    KeySlot slot,
    size_t max_slots) {
    const size_t slot_index = find_available_slot_index(slots);
    if (slot_index >= max_slots) {
        Log::error("KeySlotManager: No available key slots (max: {})", max_slots);
        return std::unexpected(VaultError::MaxUsersReached);
    }

    if (slot_index < slots.size()) {
        slots[slot_index] = std::move(slot);
    } else {
        slots.push_back(std::move(slot));
    }

    return slot_index;
}

VaultResult<> KeySlotManager::deactivate_user(
    std::vector<KeySlot>& slots,
    std::string_view username,
    const VaultSecurityPolicy& policy) {
    KeySlot* user_slot = find_slot_by_username_hash(slots, username, policy);
    if (!user_slot) {
        Log::error("KeySlotManager: User not found");
        return std::unexpected(VaultError::UserNotFound);
    }

    if (user_slot->role == UserRole::ADMINISTRATOR && count_active_administrators(slots) <= 1) {
        Log::error("KeySlotManager: Cannot remove last administrator");
        return std::unexpected(VaultError::LastAdministrator);
    }

    user_slot->active = false;
    return {};
}

} // namespace KeepTower