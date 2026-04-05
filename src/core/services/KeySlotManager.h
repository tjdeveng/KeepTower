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

/**
 * @brief Stateless helpers for manipulating V2 user key-slot records.
 *
 * KeySlotManager centralizes slot lookup, mutation, and enrollment helpers so
 * authentication code can reuse the same policy-aware behavior without pushing
 * low-level vector and record manipulation into callers.
 */
class KeySlotManager {
public:
    /**
     * @brief Build a new active user slot from derived credential material.
     * @param username Username stored in the slot metadata.
     * @param kek_derivation_algorithm Encoded KEK derivation algorithm identifier.
     * @param username_hash Derived username hash used for lookup.
     * @param username_salt Salt used to derive the username hash.
     * @param salt Salt used for password KEK derivation.
     * @param wrapped_dek Wrapped data-encryption key bytes.
     * @param role User role assigned to the slot.
     * @param must_change_password True when the user must rotate password on next login.
     * @return Fully initialized active key slot.
     */
    [[nodiscard]] static KeySlot create_user_slot(
        std::string_view username,
        uint8_t kek_derivation_algorithm,
        std::span<const uint8_t> username_hash,
        const std::array<uint8_t, 16>& username_salt,
        const std::array<uint8_t, 32>& salt,
        const std::array<uint8_t, 40>& wrapped_dek,
        UserRole role,
        bool must_change_password);

    /**
     * @brief Find the mutable slot matching a username under the active policy.
     * @param slots Slot collection to search.
     * @param username Username to resolve.
     * @param policy Security policy describing hashing rules.
     * @return Matching mutable slot, or `nullptr` if not found.
     */
    [[nodiscard]] static KeySlot* find_slot_by_username_hash(
        std::vector<KeySlot>& slots,
        std::string_view username,
        const VaultSecurityPolicy& policy);

    /**
     * @brief Find the read-only slot matching a username under the active policy.
     * @param slots Slot collection to search.
     * @param username Username to resolve.
     * @param policy Security policy describing hashing rules.
     * @return Matching read-only slot, or `nullptr` if not found.
     */
    [[nodiscard]] static const KeySlot* find_slot_by_username_hash(
        const std::vector<KeySlot>& slots,
        std::string_view username,
        const VaultSecurityPolicy& policy);

    /**
     * @brief Check whether a user has YubiKey enrollment data stored.
     * @param slots Slot collection to search.
     * @param username Username to resolve.
     * @param policy Security policy describing hashing rules.
     * @return True when the resolved slot is YubiKey-enrolled.
     */
    [[nodiscard]] static bool is_yubikey_enrolled_for_user(
        const std::vector<KeySlot>& slots,
        std::string_view username,
        const VaultSecurityPolicy& policy);

    /**
     * @brief Return only active user slots.
     * @param slots Source slot collection.
     * @return Copy of slots whose state is active.
     */
    [[nodiscard]] static std::vector<KeySlot> list_active_users(
        const std::vector<KeySlot>& slots);

    /**
     * @brief Count active administrator slots.
     * @param slots Source slot collection.
     * @return Number of active administrator users.
     */
    [[nodiscard]] static int count_active_administrators(
        const std::vector<KeySlot>& slots) noexcept;

    /**
     * @brief Find an available slot index for reuse or append.
     * @param slots Source slot collection.
     * @return Index of an inactive/reusable slot or `slots.size()` for append.
     */
    [[nodiscard]] static size_t find_available_slot_index(
        const std::vector<KeySlot>& slots) noexcept;

    /**
     * @brief Check whether a username already exists in the slot set.
     * @param slots Slot collection to search.
     * @param username Username to resolve.
     * @param policy Security policy describing hashing rules.
     * @return True when a matching active slot exists.
     */
    [[nodiscard]] static bool user_exists(
        const std::vector<KeySlot>& slots,
        std::string_view username,
        const VaultSecurityPolicy& policy);

    /**
     * @brief Require a mutable slot for a username.
     * @param slots Slot collection to search.
     * @param username Username to resolve.
     * @param policy Security policy describing hashing rules.
     * @return Matching mutable slot or an error when absent.
     */
    [[nodiscard]] static VaultResult<KeySlot*> require_user_slot(
        std::vector<KeySlot>& slots,
        std::string_view username,
        const VaultSecurityPolicy& policy);

    /**
     * @brief Require a read-only slot for a username.
     * @param slots Slot collection to search.
     * @param username Username to resolve.
     * @param policy Security policy describing hashing rules.
     * @return Matching read-only slot or an error when absent.
     */
    [[nodiscard]] static VaultResult<const KeySlot*> require_user_slot(
        const std::vector<KeySlot>& slots,
        std::string_view username,
        const VaultSecurityPolicy& policy);

    /**
     * @brief Store a slot in the first available position or append it.
     * @param slots Slot collection to mutate.
     * @param slot Slot value to store.
     * @param max_slots Maximum allowed slot count.
     * @return Index where the slot was stored or an error on capacity failure.
     */
    [[nodiscard]] static VaultResult<size_t> store_user_slot(
        std::vector<KeySlot>& slots,
        KeySlot slot,
        size_t max_slots);

    /**
     * @brief Deactivate a user slot while preserving record structure.
     * @param slots Slot collection to mutate.
     * @param username Username to deactivate.
     * @param policy Security policy describing hashing rules.
     * @return Success or an error when the user cannot be resolved.
     */
    [[nodiscard]] static VaultResult<> deactivate_user(
        std::vector<KeySlot>& slots,
        std::string_view username,
        const VaultSecurityPolicy& policy);

    /**
     * @brief Apply YubiKey enrollment metadata to an existing slot.
     * @param slot Slot to mutate.
     * @param enrolled True when the slot should be marked enrolled.
     * @param challenge Stored YubiKey challenge bytes.
     * @param serial Device serial captured during enrollment.
     * @param enrolled_at Unix timestamp of enrollment.
     * @param encrypted_pin Encrypted PIN blob to persist.
     * @param credential_id Stored FIDO2 credential identifier.
     */
    static void apply_yubikey_enrollment(
        KeySlot& slot,
        bool enrolled,
        const std::array<uint8_t, 32>& challenge,
        std::string serial,
        int64_t enrolled_at,
        std::vector<uint8_t> encrypted_pin,
        std::vector<uint8_t> credential_id) noexcept;

    /**
     * @brief Append a password history entry and trim to policy depth.
     * @param slot Slot to mutate.
     * @param entry Password history entry to add.
     * @param max_depth Maximum number of entries to retain.
     */
    static void add_password_history_entry(
        KeySlot& slot,
        const PasswordHistoryEntry& entry,
        uint32_t max_depth);

    /**
     * @brief Clear all stored password history entries.
     * @param slot Slot to mutate.
     * @return Number of entries removed.
     */
    [[nodiscard]] static size_t clear_password_history(KeySlot& slot) noexcept;

    /**
     * @brief Replace password-related slot material after a password change.
     * @param slot Slot to mutate.
     * @param salt New password salt.
     * @param wrapped_dek Newly wrapped DEK.
     * @param must_change_password True when another password rotation is still required.
     * @param password_changed_at Unix timestamp of the password change.
     */
    static void update_password_material(
        KeySlot& slot,
        const std::array<uint8_t, 32>& salt,
        const std::array<uint8_t, 40>& wrapped_dek,
        bool must_change_password,
        int64_t password_changed_at) noexcept;

    /**
     * @brief Remove stored YubiKey enrollment state from a slot.
     * @param slot Slot to mutate.
     */
    static void clear_yubikey_enrollment(KeySlot& slot) noexcept;

    /**
     * @brief Replace the stored encrypted YubiKey PIN blob.
     * @param slot Slot to mutate.
     * @param encrypted_pin New encrypted PIN blob.
     */
    static void update_yubikey_encrypted_pin(
        KeySlot& slot,
        std::vector<uint8_t> encrypted_pin) noexcept;

    /**
     * @brief Mark a slot as enrolled and replace wrapped DEK and challenge data.
     * @param slot Slot to mutate.
     * @param wrapped_dek Newly wrapped DEK protected for YubiKey flow.
     * @param challenge New YubiKey challenge bytes.
     * @param serial Device serial captured during enrollment.
     * @param enrolled_at Unix timestamp of enrollment.
     * @param encrypted_pin Encrypted PIN blob to persist.
     * @param credential_id Stored FIDO2 credential identifier.
     */
    static void enroll_yubikey(
        KeySlot& slot,
        const std::array<uint8_t, 40>& wrapped_dek,
        const std::array<uint8_t, 32>& challenge,
        std::string serial,
        int64_t enrolled_at,
        std::vector<uint8_t> encrypted_pin,
        std::vector<uint8_t> credential_id) noexcept;

    /**
     * @brief Remove YubiKey enrollment while restoring password-only material.
     * @param slot Slot to mutate.
     * @param salt Password salt to keep for password auth.
     * @param wrapped_dek Password-wrapped DEK to restore.
     */
    static void unenroll_yubikey(
        KeySlot& slot,
        const std::array<uint8_t, 32>& salt,
        const std::array<uint8_t, 40>& wrapped_dek) noexcept;
};

} // namespace KeepTower