// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

/**
 * @file YubiKeyEnrollmentService.cc
 * @brief YubiKey enrollment and unenrollment logic, extracted from VaultManagerV2.cc.
 *
 * These methods were formerly VaultManager::enroll_yubikey_for_user and
 * VaultManager::unenroll_yubikey_for_user. The mutable vault state required
 * by each operation is supplied via YubiKeyEnrollmentContext so the service
 * carries no back-reference to VaultManager.
 */

#include "YubiKeyEnrollmentService.h"

#include "KeySlotManager.h"
#include "V2AuthService.h"
#include "VaultYubiKeyService.h"
#include "../lib/crypto/KeyWrapping.h"
#include "../lib/crypto/KekDerivationService.h"
#include "../lib/crypto/VaultCrypto.h"

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
// enroll_yubikey_for_user
// ============================================================================

VaultResult<> YubiKeyEnrollmentService::enroll_yubikey_for_user(
    YubiKeyEnrollmentContext&                     ctx,
    const Glib::ustring&                          username,
    const Glib::ustring&                          password,
    const std::string&                            yubikey_pin,
    std::function<void(const std::string&)>       progress_callback) {

    Log::info("YubiKeyEnrollmentService: Enrolling YubiKey for user");

    // Validate YubiKey PIN (4-63 characters as per YubiKey spec)
    if (yubikey_pin.empty() || yubikey_pin.length() < 4 || yubikey_pin.length() > 63) {
        Log::error("YubiKeyEnrollmentService: Invalid YubiKey PIN length (must be 4-63 characters)");
        return std::unexpected(VaultError::YubiKeyError);
    }

    // Check permissions: user enrolling own YubiKey OR admin enrolling for any user
    bool is_self  = (ctx.current_session && ctx.current_session->username == username.raw());
    bool is_admin = (ctx.current_session && ctx.current_session->role == UserRole::ADMINISTRATOR);
    if (!is_self && !is_admin) {
        Log::error("YubiKeyEnrollmentService: Permission denied for YubiKey enrollment");
        return std::unexpected(VaultError::PermissionDenied);
    }

    auto& policy = ctx.v2_header.security_policy;

    // Find user slot using hash verification
#ifdef HAVE_YUBIKEY_SUPPORT
    auto user_slot_result = KeySlotManager::require_user_slot(
        ctx.v2_header.key_slots, username.raw(), policy);
    if (!user_slot_result) {
        return std::unexpected(user_slot_result.error());
    }
    KeySlot* user_slot = user_slot_result.value();
#else
    auto user_slot_result = KeySlotManager::require_user_slot(
        ctx.v2_header.key_slots, username.raw(), policy);
    if (!user_slot_result) {
        return std::unexpected(user_slot_result.error());
    }
    const KeySlot* user_slot = user_slot_result.value();
#endif

    // Check if already enrolled
    if (user_slot->yubikey_enrolled) {
        Log::error("YubiKeyEnrollmentService: User already has YubiKey enrolled");
        return std::unexpected(VaultError::YubiKeyError);
    }

#ifdef HAVE_YUBIKEY_SUPPORT
    KekDerivationService::AlgorithmParameters params;
    params.pbkdf2_iterations = policy.pbkdf2_iterations;
    params.argon2_memory_kb  = policy.argon2_memory_kb;
    params.argon2_time_cost  = policy.argon2_iterations;
    params.argon2_parallelism = policy.argon2_parallelism;

    auto kek_result = V2AuthService::derive_password_kek_for_slot(
        *user_slot,
        password.raw(),
        policy.pbkdf2_iterations,
        policy);
    if (!kek_result) {
        Log::error("YubiKeyEnrollmentService: Failed to derive KEK");
        return std::unexpected(kek_result.error());
    }

    std::array<uint8_t, 32> kek_array = kek_result.value();

    auto verify_unwrap = KeyWrapping::unwrap_key(kek_array, user_slot->wrapped_dek);
    if (!verify_unwrap) {
        Log::error("YubiKeyEnrollmentService: Password verification failed");
        return std::unexpected(VaultError::AuthenticationFailed);
    }

    // Generate unique 32-byte challenge for this user
    auto challenge_salt = KeyWrapping::generate_random_salt();
    if (!challenge_salt) {
        Log::error("YubiKeyEnrollmentService: Failed to generate challenge salt");
        return std::unexpected(VaultError::CryptoError);
    }

    std::array<uint8_t, 32> user_challenge{};
    std::copy_n(challenge_salt->begin(), 32, user_challenge.begin());

    if (!ctx.yubikey_service) {
        ctx.yubikey_service = std::make_shared<VaultYubiKeyService>();
        Log::debug("YubiKeyEnrollmentService: Initialized VaultYubiKeyService for enrollment");
    }

    const std::array<uint8_t, 32> policy_challenge{};  // unused by service implementation
    auto enroll_result = ctx.yubikey_service->enroll_yubikey(
        username.raw(),
        policy_challenge,
        user_challenge,
        yubikey_pin,
        1,                   // slot
        ctx.fips_enabled,
        progress_callback);
    if (!enroll_result) {
        return std::unexpected(enroll_result.error());
    }

    Log::info("YubiKeyEnrollmentService: FIDO2 credential created (ID length: {})",
              enroll_result->credential_id.size());

    std::vector<uint8_t> credential_id = std::move(enroll_result->credential_id);

    std::string device_serial = enroll_result->device_info.serial;
    if (!device_serial.empty()) {
        Log::info("YubiKeyEnrollmentService: YubiKey serial: {}", device_serial);
    }

    // Combine KEK with YubiKey response (v2 for variable-length responses)
    auto final_kek = V2AuthService::combine_kek_with_yubikey_response_for_open(
        kek_array,
        std::span<const uint8_t>(enroll_result->user_response));

    // Re-wrap DEK with password+YubiKey combined KEK
    auto new_wrapped_result = KeyWrapping::wrap_key(final_kek, ctx.v2_dek);
    if (!new_wrapped_result) {
        Log::error("YubiKeyEnrollmentService: Failed to wrap DEK with combined KEK");
        return std::unexpected(VaultError::CryptoError);
    }

    // Encrypt YubiKey PIN with password-derived KEK (NOT combined KEK)
    Log::info("YubiKeyEnrollmentService: Encrypting YubiKey PIN");
    std::vector<uint8_t> pin_bytes(yubikey_pin.begin(), yubikey_pin.end());
    std::vector<uint8_t> encrypted_pin;
    std::array<uint8_t, 12> pin_iv{};

    if (!VaultCrypto::encrypt_data(pin_bytes, kek_array, encrypted_pin, pin_iv)) {
        Log::error("YubiKeyEnrollmentService: Failed to encrypt YubiKey PIN");
        return std::unexpected(VaultError::CryptoError);
    }

    // Store IV + ciphertext in KeySlot (format: [IV(12) || ciphertext+tag])
    std::vector<uint8_t> pin_storage;
    pin_storage.reserve(pin_iv.size() + encrypted_pin.size());
    pin_storage.insert(pin_storage.end(), pin_iv.begin(), pin_iv.end());
    pin_storage.insert(pin_storage.end(), encrypted_pin.begin(), encrypted_pin.end());

    Log::info("YubiKeyEnrollmentService: YubiKey PIN encrypted ({} bytes)", pin_storage.size());

    KeySlotManager::enroll_yubikey(
        *user_slot,
        new_wrapped_result.value().wrapped_key,
        user_challenge,
        std::move(device_serial),
        std::chrono::system_clock::now().time_since_epoch().count(),
        std::move(pin_storage),
        std::move(credential_id));

    Log::info("YubiKeyEnrollmentService: YubiKey enrolled successfully for user");

    ctx.modified = true;

    // Update current session if user enrolled their own YubiKey
    if (ctx.current_session && ctx.current_session->username == username.raw()) {
        ctx.current_session->requires_yubikey_enrollment = false;
        Log::info("YubiKeyEnrollmentService: Updated session - YubiKey enrollment complete");
    }

    return {};
#else
    Log::error("YubiKeyEnrollmentService: YubiKey support not compiled in");
    return std::unexpected(VaultError::YubiKeyError);
#endif
}

// ============================================================================
// unenroll_yubikey_for_user
// ============================================================================

VaultResult<> YubiKeyEnrollmentService::unenroll_yubikey_for_user(
    YubiKeyEnrollmentContext&                     ctx,
    const Glib::ustring&                          username,
    const Glib::ustring&                          password,
    std::function<void(const std::string&)>       progress_callback) {

    Log::info("YubiKeyEnrollmentService: Unenrolling YubiKey for user");

    // Check permissions: user unenrolling own YubiKey OR admin unenrolling for any user
    bool is_self  = (ctx.current_session && ctx.current_session->username == username.raw());
    bool is_admin = (ctx.current_session && ctx.current_session->role == UserRole::ADMINISTRATOR);
    if (!is_self && !is_admin) {
        Log::error("YubiKeyEnrollmentService: Permission denied for YubiKey unenrollment");
        return std::unexpected(VaultError::PermissionDenied);
    }

    auto& policy = ctx.v2_header.security_policy;

    // Find user slot using hash verification
#ifdef HAVE_YUBIKEY_SUPPORT
    auto user_slot_result = KeySlotManager::require_user_slot(
        ctx.v2_header.key_slots, username.raw(), policy);
    if (!user_slot_result) {
        return std::unexpected(user_slot_result.error());
    }
    KeySlot* user_slot = user_slot_result.value();
#else
    auto user_slot_result = KeySlotManager::require_user_slot(
        ctx.v2_header.key_slots, username.raw(), policy);
    if (!user_slot_result) {
        return std::unexpected(user_slot_result.error());
    }
    const KeySlot* user_slot = user_slot_result.value();
#endif

    // Check if YubiKey is enrolled
    if (!user_slot->yubikey_enrolled) {
        Log::error("YubiKeyEnrollmentService: User does not have YubiKey enrolled");
        return std::unexpected(VaultError::YubiKeyError);
    }

#ifdef HAVE_YUBIKEY_SUPPORT
    // Verify password+YubiKey by unwrapping DEK (use user's algorithm)
    auto user_algorithm = static_cast<KekDerivationService::Algorithm>(user_slot->kek_derivation_algorithm);

    KekDerivationService::AlgorithmParameters params;
    params.pbkdf2_iterations = policy.pbkdf2_iterations;
    params.argon2_memory_kb  = policy.argon2_memory_kb;
    params.argon2_time_cost  = policy.argon2_iterations;
    params.argon2_parallelism = policy.argon2_parallelism;

    auto kek_result = KekDerivationService::derive_kek(
        password.raw(),
        user_algorithm,
        std::span<const uint8_t>(user_slot->salt.data(), user_slot->salt.size()),
        params);
    if (!kek_result) {
        Log::error("YubiKeyEnrollmentService: Failed to derive KEK");
        return std::unexpected(VaultError::CryptoError);
    }

    std::array<uint8_t, 32> kek_array{};
    std::copy(kek_result->begin(), kek_result->end(), kek_array.begin());

    // Decrypt stored PIN (required for FIDO2 UV-required assertion)
    auto pin_result = V2AuthService::decrypt_yubikey_pin_for_open(*user_slot, kek_array);
    if (!pin_result) {
        return std::unexpected(pin_result.error());
    }
    std::string decrypted_pin = std::move(pin_result.value());

    if (!ctx.yubikey_service) {
        ctx.yubikey_service = std::make_shared<VaultYubiKeyService>();
        Log::debug("YubiKeyEnrollmentService: Initialized VaultYubiKeyService for unenrollment");
    }

    const YubiKeyAlgorithm yk_algorithm =
        static_cast<YubiKeyAlgorithm>(policy.yubikey_algorithm);
    std::vector<uint8_t> challenge_vec(
        user_slot->yubikey_challenge.begin(),
        user_slot->yubikey_challenge.end());

    if (progress_callback) {
        progress_callback("Verifying current password with YubiKey (touch required)...");
    }

    auto challenge_result = ctx.yubikey_service->perform_authenticated_challenge(
        challenge_vec,
        user_slot->yubikey_credential_id,
        decrypted_pin,
        user_slot->yubikey_serial,
        yk_algorithm,
        true,      // require_touch
        15000,
        ctx.fips_enabled,
        IVaultYubiKeyService::SerialMismatchPolicy::WarnOnly);
    if (!challenge_result) {
        return std::unexpected(challenge_result.error());
    }

    // Combine KEK with YubiKey response (v2 — matches enroll path)
    auto current_kek = V2AuthService::combine_kek_with_yubikey_response_for_open(
        kek_array,
        std::span<const uint8_t>(challenge_result->response));

    auto verify_unwrap = KeyWrapping::unwrap_key(current_kek, user_slot->wrapped_dek);
    if (!verify_unwrap) {
        Log::error("YubiKeyEnrollmentService: Password+YubiKey verification failed");
        return std::unexpected(VaultError::AuthenticationFailed);
    }

    // Generate new salt for password-only KEK
    auto new_salt_result = KeyWrapping::generate_random_salt();
    if (!new_salt_result) {
        Log::error("YubiKeyEnrollmentService: Failed to generate new salt");
        return std::unexpected(VaultError::CryptoError);
    }

    // Derive password-only KEK (no YubiKey combination)
    auto new_kek_result = KekDerivationService::derive_kek(
        password.raw(),
        user_algorithm,
        std::span<const uint8_t>(new_salt_result->data(), new_salt_result->size()),
        params);
    if (!new_kek_result) {
        Log::error("YubiKeyEnrollmentService: Failed to derive new KEK");
        return std::unexpected(VaultError::CryptoError);
    }

    std::array<uint8_t, 32> new_kek_array{};
    std::copy(new_kek_result->begin(), new_kek_result->end(), new_kek_array.begin());

    // Re-wrap DEK with password-only KEK
    auto new_wrapped_result = KeyWrapping::wrap_key(new_kek_array, ctx.v2_dek);
    if (!new_wrapped_result) {
        Log::error("YubiKeyEnrollmentService: Failed to wrap DEK with new KEK");
        return std::unexpected(VaultError::CryptoError);
    }

    KeySlotManager::unenroll_yubikey(
        *user_slot,
        new_salt_result.value(),
        new_wrapped_result.value().wrapped_key);

    ctx.modified = true;

    // Update current session if user unenrolled their own YubiKey
    if (ctx.current_session && ctx.current_session->username == username.raw()) {
        if (policy.require_yubikey) {
            ctx.current_session->requires_yubikey_enrollment = true;
            Log::info("YubiKeyEnrollmentService: Updated session - YubiKey re-enrollment required by policy");
        }
    }

    Log::info("YubiKeyEnrollmentService: YubiKey unenrolled successfully for user");
    return {};
#else
    Log::error("YubiKeyEnrollmentService: YubiKey support not compiled in");
    return std::unexpected(VaultError::YubiKeyError);
#endif
}

}  // namespace KeepTower
