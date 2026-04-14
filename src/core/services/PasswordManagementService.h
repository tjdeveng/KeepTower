// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#pragma once

/**
 * @file PasswordManagementService.h
 * @brief Password management logic extracted from VaultManager.
 *
 * Responsibilities:
 * - Validate a new password against vault policy (length + history)
 * - Change a user's password: old KEK derivation → optional YubiKey
 *   verification → new KEK derivation → DEK re-wrap → key slot update
 * - Migrate a user's username hash from one algorithm to another
 * - Clear a user's password history
 * - Admin-reset a user's password without knowing the old password
 *
 * NOT responsible for:
 * - Vault open/close state management (VaultManager)
 * - Async dispatching or GTK thread marshalling (VaultManagerV2.cc shim)
 * - Vault persistence / save_vault (VaultManager)
 */

#include "../VaultError.h"
#include "../MultiUserTypes.h"

#include <array>
#include <functional>
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
 *        password management operations. Passed by VaultManager at call time
 *        so the service carries no back-pointer to the manager.
 */
struct PasswordManagementContext {
    VaultHeaderV2&                              v2_header;       ///< Mutable V2 header for slot updates
    std::array<uint8_t, 32>&                    v2_dek;          ///< DEK currently held in memory
    std::optional<UserSession>&                 current_session; ///< Active session (may be modified)
    std::shared_ptr<IVaultYubiKeyService>&      yubikey_service; ///< Hardware operation interface (may be null)
    bool&                                       modified;        ///< Set to true on successful mutation
    bool                                        fips_enabled;    ///< Snapshot of current FIPS state
};

/**
 * @class PasswordManagementService
 * @brief Stateless helper that drives password management operations.
 *
 * All public methods are static, grouped in a class for namespacing.
 * The caller (VaultManager) owns all mutable state and supplies it via
 * PasswordManagementContext on each call.
 */
class PasswordManagementService {
public:
    PasswordManagementService() = delete;

    /**
     * @brief Validate a new password against vault policy.
     *
     * Checks minimum length and password history (if enabled).
     * Does NOT verify the old password — call before the full change flow to
     * give early feedback without a YubiKey touch.
     *
     * @param ctx         Live vault state from VaultManager.
     * @param username    User whose policy and history to check.
     * @param new_password Candidate new password.
     * @return VaultResult<> — success or WeakPassword / PasswordReused.
     */
    [[nodiscard]] static VaultResult<> validate_new_password(
        PasswordManagementContext&        ctx,
        const Glib::ustring&              username,
        const Glib::ustring&              new_password);

    /**
     * @brief Change a user's password.
     *
     * Verifies the old password (with optional YubiKey), derives a new KEK,
     * re-wraps the DEK, and updates the key slot.  When YubiKey is enrolled
     * the operation performs two hardware touches (verify old, combine new).
     *
     * @param ctx               Live vault state from VaultManager.
     * @param username          User whose password is being changed.
     * @param old_password      Current password for verification.
     * @param new_password      New password to set.
     * @param yubikey_pin       Optional YubiKey PIN (required when YubiKey enrolled).
     * @param progress_callback Optional message emitted before each hardware touch.
     * @return VaultResult<> — success or a VaultError.
     */
    [[nodiscard]] static VaultResult<> change_user_password(
        PasswordManagementContext&                    ctx,
        const Glib::ustring&                          username,
        const Glib::ustring&                          old_password,
        const Glib::ustring&                          new_password,
        const std::optional<std::string>&             yubikey_pin,
        std::function<void(const std::string&)>       progress_callback);

    /**
     * @brief Migrate a user's username hash to the algorithm in the security policy.
     *
     * Generates a new salt and re-hashes the username with the new algorithm,
     * then saves the vault immediately (critical — migration must persist).
     *
     * @param ctx         Live vault state from VaultManager.
     * @param user_slot   Key slot to update (must be non-null).
     * @param username    Plain-text username to re-hash.
     * @param password    User's password (reserved for future use, currently unused).
     * @param save_fn     Functor that persists the vault; returns true on success.
     * @return VaultResult<> — success or a VaultError.
     */
    [[nodiscard]] static VaultResult<> migrate_user_hash(
        PasswordManagementContext&        ctx,
        KeySlot*                          user_slot,
        const std::string&                username,
        const std::string&                password,
        std::function<bool()>             save_fn);

    /**
     * @brief Clear all stored password history entries for a user.
     *
     * @param ctx         Live vault state from VaultManager.
     * @param username    User whose history to clear.
     * @return VaultResult<> — success or a VaultError.
     */
    [[nodiscard]] static VaultResult<> clear_user_password_history(
        PasswordManagementContext&        ctx,
        const Glib::ustring&              username);

    /**
     * @brief Reset a user's password without knowing the old password.
     *
     * Caller must hold ADMINISTRATOR role. Clears password history and
     * removes any existing YubiKey enrollment (admin cannot re-enroll for
     * the user). Sets the password_change_required flag on the slot.
     *
     * @param ctx                   Live vault state from VaultManager.
     * @param username              User whose password is being reset.
     * @param new_temporary_password Temporary password to set.
     * @return VaultResult<> — success or a VaultError.
     */
    [[nodiscard]] static VaultResult<> admin_reset_user_password(
        PasswordManagementContext&        ctx,
        const Glib::ustring&              username,
        const Glib::ustring&              new_temporary_password);
};

}  // namespace KeepTower
