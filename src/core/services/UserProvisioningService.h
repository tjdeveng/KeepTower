// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#pragma once

/**
 * @file UserProvisioningService.h
 * @brief User creation logic extracted from VaultManager.
 *
 * Responsibilities:
 * - Validate username uniqueness and password strength
 * - Derive the KEK from the temporary password
 * - Wrap the vault DEK with the new KEK
 * - Optionally enroll a YubiKey for the new user (challenge creation,
 *   credential enrolment, PIN encryption, DEK re-wrap)
 * - Populate and store the key slot
 *
 * NOT responsible for:
 * - Vault open/close state management (VaultManager)
 * - User removal (VaultManager)
 * - Session management (VaultManager)
 */

#include "../VaultError.h"
#include "../MultiUserTypes.h"

#include <array>
#include <memory>
#include <optional>
#include <string>

#include <glibmm/ustring.h>

namespace KeepTower {
class IVaultYubiKeyService;
}  // namespace KeepTower

namespace KeepTower {

/**
 * @brief Context struct that bundles mutable vault state required by the
 *        user provisioning operation. Passed by VaultManager at call time
 *        so the service carries no back-pointer to the manager.
 */
struct UserProvisioningContext {
    VaultHeaderV2&                              v2_header;       ///< Mutable V2 header for slot creation
    std::array<uint8_t, 32>&                    v2_dek;          ///< DEK currently held in memory
    std::shared_ptr<IVaultYubiKeyService>&      yubikey_service; ///< Hardware operation interface (may be null)
    bool&                                       modified;        ///< Set to true on successful mutation
    bool                                        fips_enabled;    ///< Snapshot of current FIPS state
};

/**
 * @class UserProvisioningService
 * @brief Stateless helper that drives user creation in a V2 vault.
 *
 * All public methods are static, grouped in a class for namespacing.
 * The caller (VaultManager) owns all mutable state and supplies it via
 * UserProvisioningContext on each call.
 */
class UserProvisioningService {
public:
    UserProvisioningService() = delete;

    /**
     * @brief Add a new user to the vault.
     *
     * Checks for duplicate usernames, derives a KEK from @p temporary_password,
     * wraps the DEK, optionally performs inline YubiKey enrolment when
     * @p yubikey_pin is supplied and the policy requires it, then stores the
     * resulting key slot and records the initial password-history entry.
     *
     * @param ctx                Live vault state from VaultManager.
     * @param username           New user's login name.
     * @param temporary_password Initial password (must satisfy policy).
     * @param role               UserRole::ADMINISTRATOR or UserRole::STANDARD.
     * @param must_change_password Mark the slot as requiring a password change on first login.
     * @param yubikey_pin        Optional YubiKey PIN for inline enrolment.
     * @return VaultResult<> — success or a VaultError.
     */
    [[nodiscard]] static VaultResult<> add_user(
        UserProvisioningContext&              ctx,
        const Glib::ustring&                  username,
        const Glib::ustring&                  temporary_password,
        UserRole                              role,
        bool                                  must_change_password,
        const std::optional<std::string>&     yubikey_pin);
};

}  // namespace KeepTower
