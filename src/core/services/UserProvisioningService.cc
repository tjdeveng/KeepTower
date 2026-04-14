// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

/**
 * @file UserProvisioningService.cc
 * @brief User creation logic extracted from VaultManagerV2.cc.
 *
 * This method was formerly VaultManager::add_user. The mutable vault state
 * required by the operation is supplied via UserProvisioningContext so the
 * service carries no back-reference to VaultManager.
 */

#include "UserProvisioningService.h"

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
// add_user
// ============================================================================

VaultResult<> UserProvisioningService::add_user(
    UserProvisioningContext&              ctx,
    const Glib::ustring&                  username,
    const Glib::ustring&                  temporary_password,
    UserRole                              role,
    bool                                  must_change_password,
    const std::optional<std::string>&     yubikey_pin) {

    Log::info("UserProvisioningService: Adding user");

    auto& v2_header = ctx.v2_header;
    auto& policy = v2_header.security_policy;
    auto& key_slots = v2_header.key_slots;

    // Validate username
    if (username.empty()) {
        Log::error("UserProvisioningService: Username cannot be empty");
        return std::unexpected(VaultError::InvalidUsername);
    }

    // Check for duplicate username using hash verification
    if (KeySlotManager::user_exists(key_slots, username.raw(), policy)) {
        Log::error("UserProvisioningService: Username already exists");
        return std::unexpected(VaultError::UserAlreadyExists);
    }

    // Validate password meets policy
    if (temporary_password.length() < policy.min_password_length) {
        Log::error("UserProvisioningService: Password too short (min: {} chars)",
                   policy.min_password_length);
        return std::unexpected(VaultError::WeakPassword);
    }

    // Generate unique salt for new user
    auto salt_result = KeyWrapping::generate_random_salt();
    if (!salt_result) {
        Log::error("UserProvisioningService: Failed to generate salt");
        return std::unexpected(VaultError::CryptoError);
    }

    // Derive KEK from temporary password
    auto algorithm = KekDerivationService::Algorithm::PBKDF2_HMAC_SHA256;

    KekDerivationService::AlgorithmParameters params;
    params.pbkdf2_iterations = policy.pbkdf2_iterations;
    params.argon2_memory_kb = policy.argon2_memory_kb;
    params.argon2_time_cost = policy.argon2_iterations;
    params.argon2_parallelism = policy.argon2_parallelism;

    auto kek_result = KekDerivationService::derive_kek(
        temporary_password.raw(),
        algorithm,
        std::span<const uint8_t>(salt_result->data(), salt_result->size()),
        params);
    if (!kek_result) {
        Log::error("UserProvisioningService: Failed to derive KEK");
        return std::unexpected(VaultError::CryptoError);
    }

    std::array<uint8_t, 32> kek_array{};
    std::copy(kek_result->begin(), kek_result->end(), kek_array.begin());

    // Wrap vault DEK with new user's KEK
    auto wrapped_result = KeyWrapping::wrap_key(kek_array, ctx.v2_dek);
    if (!wrapped_result) {
        Log::error("UserProvisioningService: Failed to wrap DEK");
        return std::unexpected(VaultError::CryptoError);
    }

    // Hash username for secure storage
    auto username_hash_algo = static_cast<KeepTower::UsernameHashService::Algorithm>(
        policy.username_hash_algorithm);

    std::vector<uint8_t> username_salt_vec = KeepTower::VaultCrypto::generate_random_bytes(16);
    std::array<uint8_t, 16> username_salt{};
    std::copy_n(username_salt_vec.begin(), 16, username_salt.begin());

    auto username_hash_result = KeepTower::UsernameHashService::hash_username(
        username.raw(), username_hash_algo, username_salt, policy.pbkdf2_iterations);
    if (!username_hash_result) {
        Log::error("UserProvisioningService: Failed to hash username");
        return std::unexpected(VaultError::CryptoError);
    }

    KeySlot new_slot = KeySlotManager::create_user_slot(
        username.raw(),
        static_cast<uint8_t>(algorithm),
        username_hash_result.value(),
        username_salt,
        salt_result.value(),
        wrapped_result.value().wrapped_key,
        role,
        must_change_password);

    // YubiKey enrollment if PIN provided and policy requires it
    bool yubikey_enrolled = false;
    std::array<uint8_t, 32> yubikey_challenge = {};  // HMAC-SHA256 (32 bytes)
    std::string yubikey_serial;
    std::vector<uint8_t> encrypted_pin;
    std::vector<uint8_t> credential_id;

#ifdef HAVE_YUBIKEY_SUPPORT
    if (yubikey_pin.has_value() && policy.require_yubikey) {
        Log::info("UserProvisioningService: Enrolling YubiKey for new user");

        // Generate unique challenge for this user
        auto challenge_salt = KeyWrapping::generate_random_salt();
        if (!challenge_salt) {
            Log::error("UserProvisioningService: Failed to generate YubiKey challenge");
            return std::unexpected(VaultError::CryptoError);
        }
        std::copy_n(challenge_salt.value().begin(), 32, yubikey_challenge.begin());

        if (!ctx.yubikey_service) {
            ctx.yubikey_service = std::make_shared<KeepTower::VaultYubiKeyService>();
            Log::debug("UserProvisioningService: Initialized VaultYubiKeyService for user enrollment");
        }

        const std::array<uint8_t, 32> policy_challenge{};  // unused by service implementation
        auto enroll_result = ctx.yubikey_service->enroll_yubikey(
            username.raw(),
            policy_challenge,
            yubikey_challenge,
            yubikey_pin.value(),
            1,                 // slot
            (policy.yubikey_algorithm != 0x01),  // enforce_fips
            nullptr);          // no progress callback for sync add_user
        if (!enroll_result) {
            return std::unexpected(enroll_result.error());
        }

        credential_id = std::move(enroll_result->credential_id);
        yubikey_serial = enroll_result->device_info.serial;
        Log::info("UserProvisioningService: YubiKey enrolled for user {} (FIPS: {})",
                 username.raw(), enroll_result->device_info.is_fips ? "YES" : "NO");

        // Encrypt PIN with user's KEK
        std::vector<uint8_t> pin_iv = KeepTower::VaultCrypto::generate_random_bytes(
            KeepTower::VaultCrypto::IV_LENGTH);
        std::vector<uint8_t> pin_bytes(yubikey_pin->begin(), yubikey_pin->end());
        std::vector<uint8_t> pin_ciphertext;

        if (!KeepTower::VaultCrypto::encrypt_data(pin_bytes, kek_array,
                                                   pin_ciphertext, pin_iv)) {
            Log::error("UserProvisioningService: Failed to encrypt YubiKey PIN");
            return std::unexpected(VaultError::CryptoError);
        }

        // Store IV + ciphertext
        encrypted_pin.reserve(pin_iv.size() + pin_ciphertext.size());
        encrypted_pin.insert(encrypted_pin.end(), pin_iv.begin(), pin_iv.end());
        encrypted_pin.insert(encrypted_pin.end(), pin_ciphertext.begin(), pin_ciphertext.end());

        // Re-wrap DEK with YubiKey-enhanced KEK
        auto final_kek = V2AuthService::combine_kek_with_yubikey_response_for_open(
            kek_array,
            std::span<const uint8_t>(enroll_result->user_response));

        auto wrapped_result_yk = KeyWrapping::wrap_key(final_kek, ctx.v2_dek);
        if (!wrapped_result_yk) {
            Log::error("UserProvisioningService: Failed to wrap DEK with YubiKey-enhanced KEK");
            return std::unexpected(VaultError::CryptoError);
        }
        wrapped_result = wrapped_result_yk;

        yubikey_enrolled = true;
    }
#endif

    KeySlotManager::apply_yubikey_enrollment(
        new_slot,
        yubikey_enrolled,
        yubikey_challenge,
        std::move(yubikey_serial),
        std::chrono::system_clock::now().time_since_epoch().count(),
        std::move(encrypted_pin),
        std::move(credential_id));

    // Add initial password to history if enabled
    if (policy.password_history_depth > 0) {
        auto history_entry = KeepTower::PasswordHistory::hash_password(temporary_password);
        if (history_entry) {
            KeySlotManager::add_password_history_entry(
                new_slot,
                history_entry.value(),
                policy.password_history_depth);
            Log::debug("UserProvisioningService: Added initial password to new user's history");
        } else {
            Log::warning("UserProvisioningService: Failed to hash initial password for history");
        }
    }

    auto slot_store_result = KeySlotManager::store_user_slot(
        key_slots,
        std::move(new_slot),
        VaultHeaderV2::MAX_KEY_SLOTS);
    if (!slot_store_result) {
        return std::unexpected(slot_store_result.error());
    }

    const size_t slot_index = slot_store_result.value();
    ctx.modified = true;

    Log::info("UserProvisioningService: User added successfully (role: {}, slot: {})",
              role == UserRole::ADMINISTRATOR ? "admin" : "standard",
              slot_index);
    return {};
}

}  // namespace KeepTower
