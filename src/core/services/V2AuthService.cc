// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#include "V2AuthService.h"

#include "KeySlotManager.h"
#include "KekDerivationService.h"
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

VaultResult<std::array<uint8_t, 32>> V2AuthService::derive_password_kek_for_slot(
    const KeySlot& slot,
    std::string_view password,
    uint32_t pbkdf2_iterations,
    const VaultSecurityPolicy& policy) {

    const auto algorithm = static_cast<KekDerivationService::Algorithm>(
        slot.kek_derivation_algorithm);

    KekDerivationService::AlgorithmParameters params;
    params.pbkdf2_iterations = pbkdf2_iterations;
    params.argon2_memory_kb = policy.argon2_memory_kb;
    params.argon2_time_cost = policy.argon2_iterations;
    params.argon2_parallelism = policy.argon2_parallelism;

    auto kek_result = KekDerivationService::derive_kek(
        password,
        algorithm,
        std::span<const uint8_t>(slot.salt.data(), slot.salt.size()),
        params);
    if (!kek_result) {
        Log::error("V2AuthService: Failed to derive KEK");
        return std::unexpected(VaultError::CryptoError);
    }

    std::array<uint8_t, 32> kek{};
    std::copy(kek_result->begin(), kek_result->end(), kek.begin());
    return kek;
}

} // namespace KeepTower
