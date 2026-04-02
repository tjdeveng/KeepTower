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

} // namespace KeepTower