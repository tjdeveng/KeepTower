// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#pragma once

/**
 * @file YubiKeyEnrollmentService.h
 * @brief YubiKey enrollment and unenrollment logic extracted from VaultManager.
 *
 * Responsibilities:
 * - Validate permissions and PIN length before hardware operations
 * - Drive enroll_yubikey_for_user: KEK derivation → credential creation →
 *   DEK re-wrap → PIN encryption → KeySlot mutation
 * - Drive unenroll_yubikey_for_user: challenge-response verification →
 *   DEK re-wrap with password-only KEK → KeySlot mutation
 *
 * NOT responsible for:
 * - Vault open/close state management (VaultManager)
 * - Async dispatching or GTK thread marshalling (VaultManagerV2.cc shim)
 * - Challenge-response during vault open (V2AuthService)
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
 *        enrollment/unenrollment operations. Passed by the VaultManager at
 *        call time so the service carries no back-pointer to the manager.
 */
struct YubiKeyEnrollmentContext {
    VaultHeaderV2&                              v2_header;       ///< Mutable V2 header for slot updates
    std::array<uint8_t, 32>&                    v2_dek;          ///< DEK currently held in memory
    std::optional<UserSession>&                 current_session; ///< Active session (may be modified)
    std::shared_ptr<IVaultYubiKeyService>&      yubikey_service; ///< Hardware operation interface
    bool&                                       modified;        ///< Set to true on successful mutation
    bool                                        fips_enabled;    ///< Snapshot of current FIPS state
};

/**
 * @class YubiKeyEnrollmentService
 * @brief Stateless helper that drives YubiKey enrollment and unenrollment.
 *
 * All public methods are free functions grouped in a class for namespacing.
 * The caller (VaultManager) owns all mutable state and supplies it via
 * YubiKeyEnrollmentContext on each call.
 */
class YubiKeyEnrollmentService {
public:
    YubiKeyEnrollmentService() = delete;

    /**
     * @brief Enroll a YubiKey for a vault user.
     *
     * Validates the PIN, checks permissions, derives the current KEK,
     * creates a FIDO2 credential via the YubiKey service, re-wraps the DEK
     * with the combined (password + YubiKey) KEK, encrypts the PIN in the
     * key slot, and updates slot metadata.
     *
     * @param ctx         Live vault state from VaultManager.
     * @param username    User to enroll.
     * @param password    User's current password.
     * @param yubikey_pin YubiKey PIN (4-63 characters).
     * @param progress_callback Optional progress messages before hardware touches.
     * @return VaultResult<> — success or a VaultError.
     */
    [[nodiscard]] static VaultResult<> enroll_yubikey_for_user(
        YubiKeyEnrollmentContext&                     ctx,
        const Glib::ustring&                          username,
        const Glib::ustring&                          password,
        const std::string&                            yubikey_pin,
        std::function<void(const std::string&)>       progress_callback);

    /**
     * @brief Remove YubiKey enrollment from a vault user.
     *
     * Verifies the current password + YubiKey by attempting to unwrap the
     * DEK with the combined KEK, derives a fresh password-only KEK with a
     * new salt, re-wraps the DEK, and clears YubiKey fields from the slot.
     *
     * @param ctx             Live vault state from VaultManager.
     * @param username        User to unenroll.
     * @param password        User's current password.
     * @param progress_callback Optional message emitted before the hardware touch.
     * @return VaultResult<> — success or a VaultError.
     */
    [[nodiscard]] static VaultResult<> unenroll_yubikey_for_user(
        YubiKeyEnrollmentContext&                     ctx,
        const Glib::ustring&                          username,
        const Glib::ustring&                          password,
        std::function<void(const std::string&)>       progress_callback);
};

}  // namespace KeepTower
