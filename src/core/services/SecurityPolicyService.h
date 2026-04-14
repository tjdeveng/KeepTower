// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#pragma once

/**
 * @file SecurityPolicyService.h
 * @brief Vault security policy update logic extracted from VaultManager.
 *
 * Responsibilities:
 * - Validate the incoming VaultSecurityPolicy (password length, PBKDF2
 *   iterations, username hash algorithm)
 * - Warn when a hash-algorithm change would lock out users without migration
 * - Reset per-slot migration status when the username hash algorithm changes
 * - Apply the new policy and mark the vault as modified
 *
 * NOT responsible for:
 * - Vault open/close state management (VaultManager)
 * - Administrator permission checking (VaultManager shim)
 * - Persisting the vault to disk (VaultManager)
 */

#include "../VaultError.h"
#include "../MultiUserTypes.h"

namespace KeepTower {

/**
 * @brief Context struct that bundles mutable vault state required by the
 *        security policy update operation. Passed by VaultManager at call
 *        time so the service carries no back-pointer to the manager.
 */
struct SecurityPolicyContext {
    VaultHeaderV2&  v2_header;   ///< Mutable V2 header (key slots + current policy)
    bool&           modified;    ///< Set to true on successful policy update
};

/**
 * @class SecurityPolicyService
 * @brief Stateless helper that updates the security policy of a V2 vault.
 *
 * All public methods are static, grouped in a class for namespacing.
 * The caller (VaultManager) owns all mutable state and supplies it via
 * SecurityPolicyContext on each call.
 */
class SecurityPolicyService {
public:
    SecurityPolicyService() = delete;

    /**
     * @brief Apply a new security policy to the vault.
     *
     * Validates @p new_policy fields (minimum password length, PBKDF2
     * iteration count, username hash algorithm range), warns when an
     * algorithm change would lock out users without migration, resets
     * per-slot migration status on algorithm change, writes the policy
     * into @p ctx.v2_header, and sets @p ctx.modified = true.
     *
     * @param ctx        Live vault state from VaultManager.
     * @param new_policy The policy to apply.
     * @return VaultResult<> — success or VaultError::InvalidData on bad input.
     */
    [[nodiscard]] static VaultResult<> update_security_policy(
        SecurityPolicyContext&      ctx,
        const VaultSecurityPolicy&  new_policy);
};

}  // namespace KeepTower
