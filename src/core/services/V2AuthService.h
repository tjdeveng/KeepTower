// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#pragma once

#include "../VaultError.h"
#include "../MultiUserTypes.h"

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

class YubiKeyManager;

namespace KeepTower {

/**
 * @brief Stateless helpers for V2 vault authentication flows.
 *
 * V2AuthService centralizes slot lookup, KEK derivation, PIN handling, and
 * YubiKey challenge execution so vault-open flows can reuse one policy-aware
 * implementation.
 */
class V2AuthService {
public:
    /**
     * @brief Resolve the mutable key slot for the supplied username.
     * @param slots Available V2 key slots.
     * @param username Username being authenticated.
     * @param policy Vault security policy describing username hashing.
     * @return Matching mutable slot or an authentication error.
     */
    [[nodiscard]] static VaultResult<KeySlot*> resolve_user_slot_for_open(
        std::vector<KeySlot>& slots,
        std::string_view username,
        const VaultSecurityPolicy& policy);

    /**
     * @brief Derive the password KEK needed for slot authentication.
     * @param slot Key slot being opened.
     * @param password User-supplied password.
     * @param pbkdf2_iterations PBKDF2 iteration fallback/legacy count.
     * @param policy Vault security policy describing derivation rules.
     * @return Derived 32-byte KEK or an error.
     */
    [[nodiscard]] static VaultResult<std::array<uint8_t, 32>> derive_password_kek_for_slot(
        const KeySlot& slot,
        std::string_view password,
        uint32_t pbkdf2_iterations,
        const VaultSecurityPolicy& policy);

    /**
     * @brief Decrypt the stored YubiKey PIN using the password-derived KEK.
     * @param slot Key slot containing encrypted PIN material.
     * @param password_kek KEK derived from the user's password.
     * @return Decrypted PIN string or an error.
     */
    [[nodiscard]] static VaultResult<std::string> decrypt_yubikey_pin_for_open(
        const KeySlot& slot,
        const std::array<uint8_t, 32>& password_kek);

    /**
     * @brief Load a required FIDO2 credential into the YubiKey manager.
     * @param slot Key slot containing the persisted credential.
     * @param yk_manager YubiKey manager used for hardware interaction.
     * @return Success or an error if the credential cannot be loaded.
     */
    [[nodiscard]] static VaultResult<> load_fido2_credential_for_open(
        const KeySlot& slot,
        ::YubiKeyManager& yk_manager);

    /**
     * @brief Load a FIDO2 credential when one is present on the slot.
     * @param slot Key slot containing optional credential data.
     * @param yk_manager YubiKey manager used for hardware interaction.
     * @return Success or an error if present credential data is invalid.
     */
    [[nodiscard]] static VaultResult<> load_fido2_credential_if_present(
        const KeySlot& slot,
        ::YubiKeyManager& yk_manager);

    /**
     * @brief Resolve the YubiKey PIN to use for authentication.
     * @param slot Key slot containing enrollment metadata.
     * @param password_kek KEK derived from the user's password.
     * @param fallback_pin Optional PIN supplied interactively.
     * @return Decrypted or fallback PIN, or an error.
     */
    [[nodiscard]] static VaultResult<std::string> resolve_yubikey_pin_for_auth(
        const KeySlot& slot,
        const std::array<uint8_t, 32>& password_kek,
        const std::optional<std::string>& fallback_pin);

    /**
     * @brief Run the user-slot YubiKey challenge during vault open.
     * @param slot Key slot containing challenge metadata.
     * @param policy Vault security policy for the challenge flow.
     * @param decrypted_pin PIN to use for hardware interaction.
     * @param yk_manager YubiKey manager used for hardware interaction.
     * @return Challenge response bytes or an error.
     */
    [[nodiscard]] static VaultResult<std::vector<uint8_t>> run_yubikey_challenge_for_open(
        const KeySlot& slot,
        const VaultSecurityPolicy& policy,
        std::string_view decrypted_pin,
        ::YubiKeyManager& yk_manager);

    /**
     * @brief Run a generic YubiKey challenge under the supplied policy.
     * @param challenge Challenge bytes to send to the device.
     * @param policy Vault security policy for the operation.
     * @param pin Optional PIN to provide to the hardware flow.
     * @param yk_manager YubiKey manager used for hardware interaction.
     * @param require_touch True when the device must require a touch.
     * @param timeout_ms Timeout for the challenge in milliseconds.
     * @return Challenge response bytes or an error.
     */
    [[nodiscard]] static VaultResult<std::vector<uint8_t>> run_yubikey_challenge_for_policy(
        std::span<const uint8_t> challenge,
        const VaultSecurityPolicy& policy,
        std::optional<std::string_view> pin,
        ::YubiKeyManager& yk_manager,
        bool require_touch = false,
        int timeout_ms = 5000);

    /**
     * @brief Combine password KEK material with a YubiKey response.
     * @param password_kek Password-derived KEK.
     * @param yubikey_response Raw YubiKey challenge response bytes.
     * @return Combined 32-byte KEK used to unwrap vault material.
     */
    [[nodiscard]] static std::array<uint8_t, 32> combine_kek_with_yubikey_response_for_open(
        const std::array<uint8_t, 32>& password_kek,
        std::span<const uint8_t> yubikey_response);
};

} // namespace KeepTower
