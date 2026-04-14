// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#include "SecurityPolicyService.h"
#include "../../utils/Log.h"

namespace KeepTower {

VaultResult<> SecurityPolicyService::update_security_policy(
    SecurityPolicyContext&     ctx,
    const VaultSecurityPolicy& new_policy) {

    // Validate the new policy
    // Minimum password length should be reasonable (at least 8)
    if (new_policy.min_password_length < 8) {
        Log::warning("SecurityPolicyService: min_password_length too low ({}), using 8",
                    new_policy.min_password_length);
    }

    // PBKDF2 iterations should be at least 100,000 (NIST recommendation)
    if (new_policy.pbkdf2_iterations < 100000) {
        Log::warning("SecurityPolicyService: pbkdf2_iterations too low ({}), recommend >= 100000",
                    new_policy.pbkdf2_iterations);
    }

    // Validate username hash algorithm
    if (new_policy.username_hash_algorithm > 0x05) {
        Log::error("SecurityPolicyService: Invalid username_hash_algorithm: 0x{:02x}",
                  new_policy.username_hash_algorithm);
        return std::unexpected(VaultError::InvalidData);
    }

    // Warn about migration risks
    bool migration_enabled = (new_policy.migration_flags & 0x01) != 0;
    bool algorithm_changed = (new_policy.username_hash_algorithm !=
                             ctx.v2_header.security_policy.username_hash_algorithm);

    if (algorithm_changed && !migration_enabled) {
        Log::warning("SecurityPolicyService: username_hash_algorithm changed without migration enabled!");
        Log::warning("SecurityPolicyService:   Old: 0x{:02x}, New: 0x{:02x}",
                    ctx.v2_header.security_policy.username_hash_algorithm,
                    new_policy.username_hash_algorithm);
        Log::warning("SecurityPolicyService:   This will lock out ALL users! Enable migration_flags to avoid this.");
    }

    if (migration_enabled) {
        Log::info("SecurityPolicyService: Migration enabled - two-phase authentication will be used");
        Log::info("SecurityPolicyService:   Old algorithm: 0x{:02x}", new_policy.username_hash_algorithm_previous);
        Log::info("SecurityPolicyService:   New algorithm: 0x{:02x}", new_policy.username_hash_algorithm);
    }

    // Reset migration status if targeting a new algorithm
    if (algorithm_changed) {
        Log::info("SecurityPolicyService: Algorithm changed (0x{:02x} -> 0x{:02x}) - resetting migration status for all users",
                 ctx.v2_header.security_policy.username_hash_algorithm,
                 new_policy.username_hash_algorithm);

        for (auto& slot : ctx.v2_header.key_slots) {
            if (slot.active && slot.migration_status != 0x00) {
                slot.migration_status = 0x00; // Reset to not-yet-migrated relative to NEW target
            }
        }
    }

    // Update the policy
    ctx.v2_header.security_policy = new_policy;

    // Mark vault as modified
    ctx.modified = true;

    Log::info("SecurityPolicyService: Security policy updated successfully");
    Log::info("SecurityPolicyService:   min_password_length: {}", new_policy.min_password_length);
    Log::info("SecurityPolicyService:   pbkdf2_iterations: {}", new_policy.pbkdf2_iterations);
    Log::info("SecurityPolicyService:   username_hash_algorithm: 0x{:02x}", new_policy.username_hash_algorithm);
    Log::info("SecurityPolicyService:   migration_flags: 0x{:02x}", new_policy.migration_flags);
    Log::info("SecurityPolicyService:   require_yubikey: {}", new_policy.require_yubikey);

    return {};
}

}  // namespace KeepTower
