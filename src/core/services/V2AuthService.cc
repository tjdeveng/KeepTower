// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#include "V2AuthService.h"

#include "KeySlotManager.h"
#include "KekDerivationService.h"
#include "lib/crypto/KeyWrapping.h"
#include "lib/crypto/VaultCrypto.h"
#include "managers/YubiKeyManager.h"
#include "../../utils/Log.h"

namespace Log = KeepTower::Log;

namespace KeepTower {

VaultResult<KeySlot*> V2AuthService::resolve_user_slot_for_open(
    std::vector<KeySlot>& slots,
    std::string_view username,
    const VaultSecurityPolicy& policy) {

    KeySlot* user_slot = KeySlotManager::find_slot_by_username_hash(slots, username, policy);
    if (!user_slot) {
        Log::error("V2AuthService: No active key slot found for requested user");
        return std::unexpected(VaultError::AuthenticationFailed);
    }

    return user_slot;
}

VaultResult<std::array<uint8_t, 32>> V2AuthService::derive_password_kek_for_slot(
    const KeySlot& slot,
    std::string_view password,
    uint32_t pbkdf2_iterations,
    const VaultSecurityPolicy& policy) {

    const auto algorithm = static_cast<KekDerivationService::Algorithm>(
        slot.kek_derivation_algorithm);

    KekDerivationService::AlgorithmParameters params;
    params.pbkdf2_iterations = pbkdf2_iterations;
    params.argon2_memory_kb = policy.argon2_memory_kb;
    params.argon2_time_cost = policy.argon2_iterations;
    params.argon2_parallelism = policy.argon2_parallelism;

    auto kek_result = KekDerivationService::derive_kek(
        password,
        algorithm,
        std::span<const uint8_t>(slot.salt.data(), slot.salt.size()),
        params);
    if (!kek_result) {
        Log::error("V2AuthService: Failed to derive KEK");
        return std::unexpected(VaultError::CryptoError);
    }

    std::array<uint8_t, 32> kek{};
    std::copy(kek_result->begin(), kek_result->end(), kek.begin());
    return kek;
}

VaultResult<std::string> V2AuthService::decrypt_yubikey_pin_for_open(
    const KeySlot& slot,
    const std::array<uint8_t, 32>& password_kek) {

    if (slot.yubikey_encrypted_pin.empty()) {
        Log::error("V2AuthService: No encrypted PIN stored in vault for user");
        return std::unexpected(VaultError::YubiKeyError);
    }

    if (slot.yubikey_encrypted_pin.size() < KeepTower::VaultCrypto::IV_LENGTH) {
        Log::error("V2AuthService: Invalid encrypted PIN format");
        return std::unexpected(VaultError::CryptoError);
    }

    std::vector<uint8_t> pin_iv(
        slot.yubikey_encrypted_pin.begin(),
        slot.yubikey_encrypted_pin.begin() + KeepTower::VaultCrypto::IV_LENGTH);

    std::vector<uint8_t> pin_ciphertext(
        slot.yubikey_encrypted_pin.begin() + KeepTower::VaultCrypto::IV_LENGTH,
        slot.yubikey_encrypted_pin.end());

    std::vector<uint8_t> pin_bytes;
    if (!KeepTower::VaultCrypto::decrypt_data(pin_ciphertext, password_kek, pin_iv, pin_bytes)) {
        Log::error("V2AuthService: Failed to decrypt YubiKey PIN");
        return std::unexpected(VaultError::CryptoError);
    }

    Log::info("V2AuthService: Successfully decrypted YubiKey PIN from vault");
    return std::string(reinterpret_cast<const char*>(pin_bytes.data()), pin_bytes.size());
}

VaultResult<> V2AuthService::load_fido2_credential_for_open(
    const KeySlot& slot,
    ::YubiKeyManager& yk_manager) {

    if (slot.yubikey_credential_id.empty()) {
        Log::error("V2AuthService: No FIDO2 credential ID stored for user");
        return std::unexpected(VaultError::YubiKeyError);
    }

    if (!yk_manager.set_credential(slot.yubikey_credential_id)) {
        Log::error("V2AuthService: Failed to set FIDO2 credential ID");
        return std::unexpected(VaultError::YubiKeyError);
    }

    Log::info("V2AuthService: Loaded FIDO2 credential ID ({} bytes)",
              slot.yubikey_credential_id.size());
    return {};
}

VaultResult<std::vector<uint8_t>> V2AuthService::run_yubikey_challenge_for_open(
    const KeySlot& slot,
    const VaultSecurityPolicy& policy,
    std::string_view decrypted_pin,
    ::YubiKeyManager& yk_manager) {

    return run_yubikey_challenge_for_policy(
        std::span<const uint8_t>(slot.yubikey_challenge.data(), slot.yubikey_challenge.size()),
        policy,
        decrypted_pin,
        yk_manager);
}

VaultResult<std::vector<uint8_t>> V2AuthService::run_yubikey_challenge_for_policy(
    std::span<const uint8_t> challenge,
    const VaultSecurityPolicy& policy,
    std::optional<std::string_view> pin,
    ::YubiKeyManager& yk_manager) {

    const YubiKeyAlgorithm yk_algorithm = static_cast<YubiKeyAlgorithm>(policy.yubikey_algorithm);

    auto response = yk_manager.challenge_response(
        std::span<const unsigned char>(challenge.data(), challenge.size()),
        yk_algorithm,
        false,
        5000,
        pin);

    if (!response.success) {
        Log::error("V2AuthService: YubiKey challenge-response failed: {}",
                   response.error_message);
        return std::unexpected(VaultError::YubiKeyError);
    }

    return std::vector<uint8_t>(response.get_response().begin(), response.get_response().end());
}

std::array<uint8_t, 32> V2AuthService::combine_kek_with_yubikey_response_for_open(
    const std::array<uint8_t, 32>& password_kek,
    std::span<const uint8_t> yubikey_response) {
    std::vector<uint8_t> yk_response_vec(yubikey_response.begin(), yubikey_response.end());
    return KeyWrapping::combine_with_yubikey_v2(password_kek, yk_response_vec);
}

} // namespace KeepTower
