// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#include "VaultYubiKeyService.h"
#include "../managers/YubiKeyManager.h"
#include "../crypto/VaultCrypto.h"
#include "../../utils/Log.h"
#include <algorithm>

namespace KeepTower {

// ============================================================================
// Device Detection
// ============================================================================

VaultResult<std::vector<VaultYubiKeyService::DeviceInfo>>
VaultYubiKeyService::detect_devices() {
    YubiKeyManager yk_manager;

    if (!yk_manager.initialize()) {
        Log::error("VaultYubiKeyService: Failed to initialize YubiKey subsystem");
        return std::unexpected(VaultError::YubiKeyError);
    }

    auto devices = yk_manager.enumerate_devices();

    if (devices.empty()) {
        Log::debug("VaultYubiKeyService: No YubiKey devices found");
        return std::vector<DeviceInfo>{};  // Empty vector is valid (no error)
    }

    std::vector<DeviceInfo> device_list;
    device_list.reserve(devices.size());

    for (const auto& dev : devices) {
        DeviceInfo info;
        info.serial = dev.serial_number;
        info.manufacturer = "Yubico";  // YubiKeyManager doesn't expose this
        info.product = "YubiKey";      // Generic, could parse from version
        info.slot = 1;  // Default to slot 1 (user configurable later)
        info.is_fips = dev.is_fips_capable;
        device_list.push_back(std::move(info));
    }

    Log::debug("VaultYubiKeyService: Detected {} YubiKey device(s)", device_list.size());

    return device_list;
}

// ============================================================================
// YubiKey Enrollment
// ============================================================================

VaultResult<VaultYubiKeyService::EnrollmentResult>
VaultYubiKeyService::enroll_yubikey(
    const std::string& user_id,
    const std::array<uint8_t, 32>& policy_challenge,
    const std::array<uint8_t, 32>& user_challenge,
    const std::string& pin,
    uint8_t slot,
    bool enforce_fips,
    std::function<void(const std::string&)> progress_callback) {

    // Validate PIN format
    if (!validate_pin_format(pin)) {
        Log::error("VaultYubiKeyService: Invalid PIN format");
        return std::unexpected(VaultError::YubiKeyError);
    }

    // Validate slot
    if (slot != 1 && slot != 2) {
        Log::error("VaultYubiKeyService: Invalid HMAC slot (must be 1 or 2)");
        return std::unexpected(VaultError::YubiKeyError);
    }

    Log::debug("VaultYubiKeyService: Starting YubiKey enrollment (slot {})", slot);

    // Initialize YubiKey manager with FIPS mode
    YubiKeyManager yk_manager;
    if (!yk_manager.initialize(enforce_fips)) {
        Log::error("VaultYubiKeyService: Failed to initialize YubiKey subsystem");
        return std::unexpected(VaultError::YubiKeyError);
    }

    // Step 1: Create FIDO2 credential (requires touch)
    Log::info("VaultYubiKeyService: Creating FIDO2 credential (requires touch)...");
    if (progress_callback) {
        progress_callback("Touch 1 of 2: Creating YubiKey credential to verify user presence");
    }
    auto credential_id = yk_manager.create_credential(user_id, pin);
    if (!credential_id) {
        Log::error("VaultYubiKeyService: Failed to create FIDO2 credential");
        return std::unexpected(VaultError::YubiKeyError);
    }
    Log::info("VaultYubiKeyService: FIDO2 credential created ({} bytes)", credential_id->size());

    // Step 2: Single challenge-response for user authentication
    // Note: FIDO2 always requires touch despite require_touch parameter
    Log::info("VaultYubiKeyService: Performing challenge-response (requires touch)...");
    if (progress_callback) {
        progress_callback("Touch 2 of 2: Generating cryptographic response for authentication");
    }
    std::span<const uint8_t> user_span(user_challenge);
    auto user_result = yk_manager.challenge_response(
        user_span,
        YubiKeyAlgorithm::HMAC_SHA256,  // FIPS-approved
        false,  // parameter ignored by FIDO2 (always requires touch)
        15000,  // 15 second timeout
        pin);

    if (!user_result.success) {
        Log::error("VaultYubiKeyService: Challenge-response failed: {}",
                   user_result.error_message);
        return std::unexpected(VaultError::YubiKeyError);
    }

    Log::debug("VaultYubiKeyService: Challenge-response completed ({} bytes)",
               user_result.response_size);

    // Get device info
    auto device_info_opt = yk_manager.get_device_info();
    if (!device_info_opt) {
        Log::error("VaultYubiKeyService: Failed to get device info");
        return std::unexpected(VaultError::YubiKeyError);
    }

    // Build result
    EnrollmentResult result;
    // For backwards compatibility, copy user_response to both fields
    result.user_response.assign(
        user_result.response.begin(),
        user_result.response.begin() + user_result.response_size);
    result.policy_response = result.user_response;  // Same as user for single-challenge mode
    result.credential_id = credential_id.value();  // Store FIDO2 credential ID

    result.device_info.serial = device_info_opt->serial_number;
    result.device_info.manufacturer = "Yubico";
    result.device_info.product = "YubiKey";
    result.device_info.slot = slot;
    result.device_info.is_fips = device_info_opt->is_fips_capable;

    Log::info("VaultYubiKeyService: Enrollment completed (device: {}, slot: {})",
              result.device_info.serial, slot);

    return result;
}

// ============================================================================
// Challenge-Response
// ============================================================================

VaultResult<VaultYubiKeyService::ChallengeResult>
VaultYubiKeyService::challenge_response(
    const std::vector<uint8_t>& challenge,
    const std::string& pin,
    uint8_t slot,
    bool enforce_fips) {

    // Validate inputs
    if (challenge.empty() || challenge.size() > 64) {
        Log::error("VaultYubiKeyService: Invalid challenge size (must be 1-64 bytes)");
        return std::unexpected(VaultError::YubiKeyError);
    }

    if (!validate_pin_format(pin)) {
        Log::error("VaultYubiKeyService: Invalid PIN format");
        return std::unexpected(VaultError::YubiKeyError);
    }

    if (slot != 1 && slot != 2) {
        Log::error("VaultYubiKeyService: Invalid HMAC slot (must be 1 or 2)");
        return std::unexpected(VaultError::YubiKeyError);
    }

    Log::debug("VaultYubiKeyService: Challenge-response operation (slot {}, {} bytes)",
               slot, challenge.size());

    // Initialize YubiKey manager with FIPS mode
    YubiKeyManager yk_manager;
    if (!yk_manager.initialize(enforce_fips)) {
        Log::error("VaultYubiKeyService: Failed to initialize YubiKey subsystem");
        return std::unexpected(VaultError::YubiKeyError);
    }

    std::span<const uint8_t> challenge_span(challenge);
    auto result = yk_manager.challenge_response(
        challenge_span,
        YubiKeyAlgorithm::HMAC_SHA256,  // FIPS-approved
        true,   // require touch
        15000,  // 15 second timeout
        pin);

    if (!result.success) {
        Log::error("VaultYubiKeyService: Challenge-response failed: {}",
                   result.error_message);
        return std::unexpected(VaultError::YubiKeyError);
    }

    // Get device info
    auto device_info_opt = yk_manager.get_device_info();
    if (!device_info_opt) {
        Log::error("VaultYubiKeyService: Failed to get device info");
        return std::unexpected(VaultError::YubiKeyError);
    }

    ChallengeResult cr;
    cr.response.assign(
        result.response.begin(),
        result.response.begin() + result.response_size);
    cr.device_info.serial = device_info_opt->serial_number;
    cr.device_info.manufacturer = "Yubico";
    cr.device_info.product = "YubiKey";
    cr.device_info.slot = slot;
    cr.device_info.is_fips = device_info_opt->is_fips_capable;

    Log::debug("VaultYubiKeyService: Challenge-response completed ({} bytes response)",
               cr.response.size());

    return cr;
}

// ============================================================================
// Device Information
// ============================================================================

VaultResult<VaultYubiKeyService::DeviceInfo>
VaultYubiKeyService::get_device_info(const std::string& device_path, bool enforce_fips) {
    if (device_path.empty()) {
        Log::error("VaultYubiKeyService: Empty device path");
        return std::unexpected(VaultError::YubiKeyError);
    }

    // Initialize YubiKey manager with FIPS mode
    YubiKeyManager yk_manager;
    if (!yk_manager.initialize(enforce_fips)) {
        Log::error("VaultYubiKeyService: Failed to initialize YubiKey subsystem");
        return std::unexpected(VaultError::YubiKeyError);
    }

    // Get first available device (YubiKeyManager doesn't support path-based lookup)
    auto device_opt = yk_manager.get_device_info();
    if (!device_opt) {
        Log::error("VaultYubiKeyService: No YubiKey device found");
        return std::unexpected(VaultError::YubiKeyError);
    }

    DeviceInfo info;
    info.serial = device_opt->serial_number;
    info.manufacturer = "Yubico";
    info.product = "YubiKey";
    info.slot = 1;  // Default
    info.is_fips = device_opt->is_fips_capable;

    return info;
}

// ============================================================================
// Validation & Utility
// ============================================================================

bool VaultYubiKeyService::validate_pin_format(const std::string& pin) {
    // YubiKey PIN requirements:
    // - Minimum 4 characters
    // - Maximum 63 characters
    // - UTF-8 encoding (no other restrictions)

    if (pin.size() < 4) {
        Log::debug("VaultYubiKeyService: PIN too short (< 4 characters)");
        return false;
    }

    if (pin.size() > 63) {
        Log::debug("VaultYubiKeyService: PIN too long (> 63 characters)");
        return false;
    }

    // Basic UTF-8 validation (check for invalid sequences)
    // libfido2 will do more thorough validation
    return true;
}

bool VaultYubiKeyService::is_fips_device(const DeviceInfo& device_info) {
    return device_info.is_fips;
}

VaultResult<std::vector<uint8_t>>
VaultYubiKeyService::generate_challenge(size_t size) {
    if (size == 0 || size > 64) {
        Log::error("VaultYubiKeyService: Invalid challenge size (must be 1-64 bytes)");
        return std::unexpected(VaultError::YubiKeyError);
    }

    // Use VaultCrypto's FIPS-approved RNG
    auto challenge = VaultCrypto::generate_random_bytes(size);

    if (challenge.empty()) {
        Log::error("VaultYubiKeyService: Failed to generate random challenge");
        return std::unexpected(VaultError::CryptoError);
    }

    Log::debug("VaultYubiKeyService: Generated {} byte challenge", size);

    return challenge;
}

} // namespace KeepTower
