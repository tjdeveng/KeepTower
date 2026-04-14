// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

/**
 * @file PasswordManagementService.cc
 * @brief Password management logic extracted from VaultManagerV2.cc.
 *
 * These methods were formerly VaultManager::validate_new_password,
 * VaultManager::change_user_password, VaultManager::migrate_user_hash,
 * VaultManager::clear_user_password_history, and
 * VaultManager::admin_reset_user_password.  The mutable vault state required
 * by each operation is supplied via PasswordManagementContext so the service
 * carries no back-reference to VaultManager.
 */

#include "PasswordManagementService.h"

#include "KeySlotManager.h"
#include "V2AuthService.h"
#include "VaultYubiKeyService.h"
#include "../lib/crypto/KeyWrapping.h"
#include "../lib/crypto/KekDerivationService.h"
#include "../lib/crypto/VaultCrypto.h"
#include "../PasswordHistory.h"
#include "../lib/crypto/UsernameHashService.h"

#include "../../utils/Log.h"

#if __has_include("config.h")
#include "config.h"
#endif

#include <chrono>
#include <ctime>
#include <span>

namespace Log = KeepTower::Log;

using KeepTower::VaultError;
using KeepTower::UserRole;
using KeepTower::KeySlot;
using KeepTower::KeySlotManager;
using KeepTower::V2AuthService;
using KeepTower::KekDerivationService;
using KeepTower::KeyWrapping;

namespace KeepTower {

// ============================================================================
// validate_new_password
// ============================================================================

VaultResult<> PasswordManagementService::validate_new_password(
    PasswordManagementContext&        ctx,
    const Glib::ustring&              username,
    const Glib::ustring&              new_password) {

    Log::debug("PasswordManagementService: Validating new password for user: {}", username.raw());

    auto& v2_header = ctx.v2_header;
    auto& policy = v2_header.security_policy;

    // Find user slot using hash verification
    auto user_slot_result = KeySlotManager::require_user_slot(
        v2_header.key_slots, username.raw(), policy);
    if (!user_slot_result) {
        return std::unexpected(user_slot_result.error());
    }
    const KeySlot* user_slot = user_slot_result.value();

    // Validate new password meets minimum length
    if (new_password.length() < policy.min_password_length) {
        Log::error("PasswordManagementService: New password too short - actual: {} chars, min: {} chars",
                   new_password.length(), policy.min_password_length);
        return std::unexpected(VaultError::WeakPassword);
    }

    // Check password history if enabled (depth > 0)
    if (policy.password_history_depth > 0) {
        Log::debug("PasswordManagementService: Checking password history (depth: {})",
                   policy.password_history_depth);

        if (KeepTower::PasswordHistory::is_password_reused(new_password, user_slot->password_history)) {
            Log::error("PasswordManagementService: Password was used previously (reuse detected)");
            return std::unexpected(VaultError::PasswordReused);
        }

        Log::debug("PasswordManagementService: Password not found in history (OK)");
    }

    Log::debug("PasswordManagementService: New password validation passed");
    return {};
}

// ============================================================================
// change_user_password
// ============================================================================

VaultResult<> PasswordManagementService::change_user_password(
    PasswordManagementContext&                    ctx,
    const Glib::ustring&                          username,
    const Glib::ustring&                          old_password,
    const Glib::ustring&                          new_password,
    const std::optional<std::string>&             yubikey_pin,
    std::function<void(const std::string&)>       progress_callback) {

    Log::info("PasswordManagementService: Changing password for user");

    auto& v2_header = ctx.v2_header;
    auto& policy = v2_header.security_policy;

    // Check permissions: user changing own password OR admin changing any
    bool is_self = (ctx.current_session && ctx.current_session->username == username.raw());
    bool is_admin = (ctx.current_session && ctx.current_session->role == UserRole::ADMINISTRATOR);
    if (!is_self && !is_admin) {
        Log::error("PasswordManagementService: Permission denied for password change");
        return std::unexpected(VaultError::PermissionDenied);
    }

    // Find user slot using hash verification
    auto user_slot_result = KeySlotManager::require_user_slot(
        v2_header.key_slots, username.raw(), policy);
    if (!user_slot_result) {
        return std::unexpected(user_slot_result.error());
    }
    KeySlot* user_slot = user_slot_result.value();

    // Validate new password meets policy
    Log::info("PasswordManagementService: Password length check - length: {}, bytes: {}, required: {}",
              new_password.length(), new_password.bytes(), policy.min_password_length);
    if (new_password.length() < policy.min_password_length) {
        Log::error("PasswordManagementService: New password too short - actual: {} chars, min: {} chars",
                   new_password.length(), policy.min_password_length);
        return std::unexpected(VaultError::WeakPassword);
    }

    // Check password history if enabled (depth > 0)
    if (policy.password_history_depth > 0) {
        Log::debug("PasswordManagementService: Checking password history (depth: {})",
                   policy.password_history_depth);

        if (KeepTower::PasswordHistory::is_password_reused(new_password, user_slot->password_history)) {
            Log::error("PasswordManagementService: Password was used previously (reuse detected)");
            return std::unexpected(VaultError::PasswordReused);
        }

        Log::debug("PasswordManagementService: Password not found in history (OK)");
    }

    // Verify old password by unwrapping DEK (use algorithm from KeySlot)
    auto old_algorithm = static_cast<KekDerivationService::Algorithm>(user_slot->kek_derivation_algorithm);

    KekDerivationService::AlgorithmParameters params;
    params.pbkdf2_iterations = policy.pbkdf2_iterations;
    params.argon2_memory_kb = policy.argon2_memory_kb;
    params.argon2_time_cost = policy.argon2_iterations;
    params.argon2_parallelism = policy.argon2_parallelism;

    auto old_kek_result = V2AuthService::derive_password_kek_for_slot(
        *user_slot,
        old_password.raw(),
        policy.pbkdf2_iterations,
        policy);
    if (!old_kek_result) {
        Log::error("PasswordManagementService: Failed to derive old KEK");
        return std::unexpected(old_kek_result.error());
    }

    std::array<uint8_t, 32> old_kek_array = old_kek_result.value();

    std::array<uint8_t, 32> old_final_kek = old_kek_array;

#ifdef HAVE_YUBIKEY_SUPPORT
    // If user has YubiKey enrolled, verify with YubiKey
    if (user_slot->yubikey_enrolled) {
        Log::info("PasswordManagementService: User has YubiKey enrolled, verifying with YubiKey");

        auto pin_result = V2AuthService::resolve_yubikey_pin_for_auth(
            *user_slot,
            old_kek_array,
            yubikey_pin);
        if (!pin_result) {
            return std::unexpected(pin_result.error());
        }
        std::string decrypted_pin = std::move(pin_result.value());

        if (!ctx.yubikey_service) {
            ctx.yubikey_service = std::make_shared<KeepTower::VaultYubiKeyService>();
            Log::debug("PasswordManagementService: Initialized VaultYubiKeyService for password change");
        }

        const YubiKeyAlgorithm yk_algorithm = static_cast<YubiKeyAlgorithm>(policy.yubikey_algorithm);
        std::vector<uint8_t> challenge_vec(
            user_slot->yubikey_challenge.begin(),
            user_slot->yubikey_challenge.end());

        // Report progress before first touch
        if (progress_callback) {
            progress_callback("Touch 1 of 2: Verifying old password with YubiKey...");
        }

        auto old_challenge_result = ctx.yubikey_service->perform_authenticated_challenge(
            challenge_vec,
            user_slot->yubikey_credential_id,
            decrypted_pin,
            user_slot->yubikey_serial,
            yk_algorithm,
            true,  // require_touch
            15000, // 15 second timeout
            ctx.fips_enabled,
            KeepTower::IVaultYubiKeyService::SerialMismatchPolicy::WarnOnly);
        if (!old_challenge_result) {
            return std::unexpected(old_challenge_result.error());
        }

        // Combine KEK with YubiKey response (use v2 for variable-length responses)
        old_final_kek = V2AuthService::combine_kek_with_yubikey_response_for_open(
            old_final_kek,
            std::span<const uint8_t>(old_challenge_result.value().response));

        Log::info("PasswordManagementService: Old password verified with YubiKey");
    }
#endif

    auto verify_unwrap = KeyWrapping::unwrap_key(
        old_final_kek,
        user_slot->wrapped_dek);
    if (!verify_unwrap) {
        Log::error("PasswordManagementService: Old password verification failed");
        return std::unexpected(VaultError::AuthenticationFailed);
    }

    // Generate new salt for new password
    auto new_salt_result = KeyWrapping::generate_random_salt();
    if (!new_salt_result) {
        Log::error("PasswordManagementService: Failed to generate new salt");
        return std::unexpected(VaultError::CryptoError);
    }

    // Derive new KEK (keep same algorithm as old KEK for consistency)
    auto new_kek_result = KekDerivationService::derive_kek(
        new_password.raw(),
        old_algorithm,  // Use same algorithm
        std::span<const uint8_t>(new_salt_result->data(), new_salt_result->size()),
        params);
    if (!new_kek_result) {
        Log::error("PasswordManagementService: Failed to derive new KEK");
        return std::unexpected(VaultError::CryptoError);
    }

    std::array<uint8_t, 32> new_final_kek{};
    std::copy(new_kek_result->begin(), new_kek_result->end(), new_final_kek.begin());

#ifdef HAVE_YUBIKEY_SUPPORT
    // If user has YubiKey enrolled, combine new KEK with SAME YubiKey challenge and re-encrypt PIN
    if (user_slot->yubikey_enrolled) {
        Log::info("PasswordManagementService: Preserving YubiKey enrollment with new password");

        auto pin_result2 = V2AuthService::resolve_yubikey_pin_for_auth(
            *user_slot,
            old_kek_array,
            yubikey_pin);
        if (!pin_result2) {
            return std::unexpected(pin_result2.error());
        }
        std::string pin_to_use = std::move(pin_result2.value());

        const YubiKeyAlgorithm yk_algorithm2 = static_cast<YubiKeyAlgorithm>(policy.yubikey_algorithm);
        std::vector<uint8_t> challenge_vec2(
            user_slot->yubikey_challenge.begin(),
            user_slot->yubikey_challenge.end());

        // Report progress before second touch
        if (progress_callback) {
            progress_callback("Touch 2 of 2: Combining new password with YubiKey...");
        }

        auto new_challenge_result = ctx.yubikey_service->perform_authenticated_challenge(
            challenge_vec2,
            user_slot->yubikey_credential_id,
            pin_to_use,
            user_slot->yubikey_serial,
            yk_algorithm2,
            true,  // require_touch
            15000, // 15 second timeout
            ctx.fips_enabled,
            KeepTower::IVaultYubiKeyService::SerialMismatchPolicy::WarnOnly);
        if (!new_challenge_result) {
            return std::unexpected(new_challenge_result.error());
        }

        // Combine new KEK with YubiKey response (use v2 for variable-length)
        new_final_kek = V2AuthService::combine_kek_with_yubikey_response_for_open(
            new_final_kek,
            std::span<const uint8_t>(new_challenge_result.value().response));

        // Re-encrypt PIN with NEW password-derived KEK (before YubiKey combination)
        std::vector<uint8_t> new_encrypted_pin;
        std::vector<uint8_t> new_pin_iv = KeepTower::VaultCrypto::generate_random_bytes(
            KeepTower::VaultCrypto::IV_LENGTH);

        std::vector<uint8_t> pin_bytes(pin_to_use.begin(), pin_to_use.end());
        if (!KeepTower::VaultCrypto::encrypt_data(pin_bytes, new_final_kek, new_encrypted_pin, new_pin_iv)) {
            Log::error("PasswordManagementService: Failed to re-encrypt PIN with new password");
            return std::unexpected(VaultError::CryptoError);
        }

        // Store re-encrypted PIN
        std::vector<uint8_t> new_pin_storage;
        new_pin_storage.reserve(new_pin_iv.size() + new_encrypted_pin.size());
        new_pin_storage.insert(new_pin_storage.end(), new_pin_iv.begin(), new_pin_iv.end());
        new_pin_storage.insert(new_pin_storage.end(), new_encrypted_pin.begin(), new_encrypted_pin.end());
        KeySlotManager::update_yubikey_encrypted_pin(*user_slot, std::move(new_pin_storage));

        Log::info("PasswordManagementService: YubiKey enrollment preserved and PIN re-encrypted with new password");
    }
#endif

    // Wrap DEK with new KEK (with optional YubiKey)
    auto new_wrapped_result = KeyWrapping::wrap_key(new_final_kek, ctx.v2_dek);
    if (!new_wrapped_result) {
        Log::error("PasswordManagementService: Failed to wrap DEK with new KEK");
        return std::unexpected(VaultError::CryptoError);
    }

    KeySlotManager::update_password_material(
        *user_slot,
        new_salt_result.value(),
        new_wrapped_result.value().wrapped_key,
        false,
        std::chrono::system_clock::now().time_since_epoch().count());

    // Add new password to history if enabled
    if (policy.password_history_depth > 0) {
        auto history_entry = KeepTower::PasswordHistory::hash_password(new_password);
        if (history_entry) {
            KeySlotManager::add_password_history_entry(
                *user_slot,
                history_entry.value(),
                policy.password_history_depth);
            Log::debug("PasswordManagementService: Added password to history (size: {})",
                       user_slot->password_history.size());
        } else {
            Log::warning("PasswordManagementService: Failed to hash password for history");
        }
    }

    ctx.modified = true;

    // Update session if user changed own password
    if (is_self && ctx.current_session) {
        ctx.current_session->password_change_required = false;
    }

    Log::info("PasswordManagementService: Password changed successfully for user");
    return {};
}

// ============================================================================
// migrate_user_hash
// ============================================================================

VaultResult<> PasswordManagementService::migrate_user_hash(
    PasswordManagementContext&        ctx,
    KeySlot*                          user_slot,
    const std::string&                username,
    const std::string&                password,
    std::function<bool()>             save_fn) {

    (void)password;

    if (!user_slot) {
        Log::error("PasswordManagementService: migrate_user_hash called with null user_slot");
        return std::unexpected(VaultError::InvalidData);
    }

    auto& v2_header = ctx.v2_header;
    auto& policy = v2_header.security_policy;

    // Verify migration is actually active
    bool migration_active = (policy.migration_flags & 0x01) != 0;
    if (!migration_active) {
        Log::warning("PasswordManagementService: migrate_user_hash called but migration not active (flags=0x{:02x})",
                    policy.migration_flags);
        return std::unexpected(VaultError::InvalidData);
    }

    Log::info("PasswordManagementService: Starting username hash migration for user: {}", username);
    Log::info("PasswordManagementService:   Old algorithm: 0x{:02x}", policy.username_hash_algorithm_previous);
    Log::info("PasswordManagementService:   New algorithm: 0x{:02x}", policy.username_hash_algorithm);

    // Get new algorithm from policy
    auto new_algo = static_cast<KeepTower::UsernameHashService::Algorithm>(
        policy.username_hash_algorithm);

    // Generate new salt for username (best practice: don't reuse salt across algorithms)
    std::vector<uint8_t> new_username_salt_vec = KeepTower::VaultCrypto::generate_random_bytes(16);
    std::array<uint8_t, 16> new_username_salt{};
    std::copy_n(new_username_salt_vec.begin(), 16, new_username_salt.begin());

    // Compute new username hash with new algorithm
    // Use policy's pbkdf2_iterations (critical for PBKDF2/Argon2 algorithms)
    auto new_hash_result = KeepTower::UsernameHashService::hash_username(
        username, new_algo, new_username_salt, policy.pbkdf2_iterations);

    if (!new_hash_result) {
        Log::error("PasswordManagementService: Failed to compute new username hash");
        return std::unexpected(VaultError::CryptoError);
    }

    // Update KeySlot with new hash
    const auto& new_hash_vec = new_hash_result.value();
    std::fill(user_slot->username_hash.begin(), user_slot->username_hash.end(), 0);
    std::copy_n(new_hash_vec.begin(),
                std::min(new_hash_vec.size(), size_t(64)),
                user_slot->username_hash.begin());
    user_slot->username_hash_size = static_cast<uint8_t>(new_hash_vec.size());
    user_slot->username_salt = new_username_salt; // cppcheck-suppress autoVariables -- std::array value copy, not address escape

    // Mark as migrated with timestamp
    user_slot->migration_status = 0x01;  // Migrated
    user_slot->migrated_at = static_cast<uint64_t>(std::time(nullptr));

    // Mark vault as modified
    ctx.modified = true;

    // Save vault immediately (critical! migration must persist)
    Log::info("PasswordManagementService: Saving vault after migration (creating backup)");
    if (!save_fn()) {
        Log::error("PasswordManagementService: Failed to save vault after migration");
        return std::unexpected(VaultError::FileWriteError);
    }

    Log::info("PasswordManagementService: Successfully migrated user to algorithm 0x{:02x} (hash_size={}, timestamp={})",
              policy.username_hash_algorithm,
              user_slot->username_hash_size, user_slot->migrated_at);

    return {};  // Success
}

// ============================================================================
// clear_user_password_history
// ============================================================================

VaultResult<> PasswordManagementService::clear_user_password_history(
    PasswordManagementContext&        ctx,
    const Glib::ustring&              username) {

    Log::info("PasswordManagementService: Clearing password history for user");

    auto& v2_header = ctx.v2_header;

    // Check permissions: user clearing own history OR admin clearing any
    bool is_self = (ctx.current_session && ctx.current_session->username == username.raw());
    bool is_admin = (ctx.current_session && ctx.current_session->role == UserRole::ADMINISTRATOR);
    if (!is_self && !is_admin) {
        Log::error("PasswordManagementService: Permission denied for clearing password history");
        return std::unexpected(VaultError::PermissionDenied);
    }

    // Find user slot using hash verification
    auto user_slot_result = KeySlotManager::require_user_slot(
        v2_header.key_slots, username.raw(), v2_header.security_policy);
    if (!user_slot_result) {
        return std::unexpected(user_slot_result.error());
    }
    KeySlot* user_slot = user_slot_result.value();

    // Clear password history
    const size_t old_size = KeySlotManager::clear_password_history(*user_slot);

    ctx.modified = true;

    Log::info("PasswordManagementService: Cleared {} password history entries for user",
              old_size);
    return {};
}

// ============================================================================
// admin_reset_user_password
// ============================================================================

VaultResult<> PasswordManagementService::admin_reset_user_password(
    PasswordManagementContext&        ctx,
    const Glib::ustring&              username,
    const Glib::ustring&              new_temporary_password) {

    Log::info("PasswordManagementService: Admin resetting password for user");

    auto& v2_header = ctx.v2_header;
    auto& policy = v2_header.security_policy;

    // Check admin permissions
    if (!ctx.current_session || ctx.current_session->role != UserRole::ADMINISTRATOR) {
        Log::error("PasswordManagementService: Admin permission required for password reset");
        return std::unexpected(VaultError::PermissionDenied);
    }

    // Prevent admin from resetting own password (must use change_user_password)
    if (ctx.current_session->username == username.raw()) {
        Log::error("PasswordManagementService: Cannot reset own password (use change password instead)");
        return std::unexpected(VaultError::PermissionDenied);
    }

    // Find user slot using hash verification
    auto user_slot_result = KeySlotManager::require_user_slot(
        v2_header.key_slots, username.raw(), policy);
    if (!user_slot_result) {
        return std::unexpected(user_slot_result.error());
    }
    KeySlot* user_slot = user_slot_result.value();

    // Validate new password meets policy
    if (new_temporary_password.length() < policy.min_password_length) {
        Log::error("PasswordManagementService: New password too short (min: {} chars)",
                   policy.min_password_length);
        return std::unexpected(VaultError::WeakPassword);
    }

    // Generate new salt for new password
    auto new_salt_result = KeyWrapping::generate_random_salt();
    if (!new_salt_result) {
        Log::error("PasswordManagementService: Failed to generate new salt");
        return std::unexpected(VaultError::CryptoError);
    }

    // Derive new KEK from temporary password (use same algorithm as user's current algorithm)
    auto user_algorithm = static_cast<KekDerivationService::Algorithm>(user_slot->kek_derivation_algorithm);

    KekDerivationService::AlgorithmParameters params;
    params.pbkdf2_iterations = policy.pbkdf2_iterations;
    params.argon2_memory_kb = policy.argon2_memory_kb;
    params.argon2_time_cost = policy.argon2_iterations;
    params.argon2_parallelism = policy.argon2_parallelism;

    auto new_kek_result = KekDerivationService::derive_kek(
        new_temporary_password.raw(),
        user_algorithm,
        std::span<const uint8_t>(new_salt_result->data(), new_salt_result->size()),
        params);
    if (!new_kek_result) {
        Log::error("PasswordManagementService: Failed to derive new KEK");
        return std::unexpected(VaultError::CryptoError);
    }

    std::array<uint8_t, 32> new_kek_array{};
    std::copy(new_kek_result->begin(), new_kek_result->end(), new_kek_array.begin());

    // Wrap DEK with new KEK (password-only, no YubiKey)
    auto new_wrapped_result = KeyWrapping::wrap_key(new_kek_array, ctx.v2_dek);
    if (!new_wrapped_result) {
        Log::error("PasswordManagementService: Failed to wrap DEK with new KEK");
        return std::unexpected(VaultError::CryptoError);
    }

    KeySlotManager::update_password_material(
        *user_slot,
        new_salt_result.value(),
        new_wrapped_result.value().wrapped_key,
        true,
        0);

    // Clear password history (admin reset = fresh start)
    (void)KeySlotManager::clear_password_history(*user_slot);
    Log::debug("PasswordManagementService: Cleared password history for reset user");

    // IMPORTANT: Unenroll YubiKey if enrolled
    // Admin doesn't have user's YubiKey device, so reset to password-only
    if (user_slot->yubikey_enrolled) {
        Log::info("PasswordManagementService: Unenrolling YubiKey for user (admin reset)");
        KeySlotManager::clear_yubikey_enrollment(*user_slot);

        // If vault policy requires YubiKey, user will need to re-enroll after password change
        if (policy.require_yubikey) {
            Log::info("PasswordManagementService: User will need to re-enroll YubiKey (required by policy)");
        }
    }

    ctx.modified = true;

    Log::info("PasswordManagementService: Password reset successfully for user");
    Log::info("PasswordManagementService: User will be required to change password on next login");
    return {};
}

}  // namespace KeepTower
