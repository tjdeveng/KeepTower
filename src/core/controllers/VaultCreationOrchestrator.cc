// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2024 KeepTower Contributors
// File: src/core/controllers/VaultCreationOrchestrator.cc

#include "VaultCreationOrchestrator.h"
#include "../services/VaultCryptoService.h"
#include "../services/VaultYubiKeyService.h"
#include "../services/VaultFileService.h"
#include "../VaultFormatV2.h"
#include "../PasswordHistory.h"
#include "../../utils/Log.h"
#include "../record.pb.h"
#include <ctime>

namespace KeepTower {

// ============================================================================
// Constructor
// ============================================================================

VaultCreationOrchestrator::VaultCreationOrchestrator(
    std::shared_ptr<VaultCryptoService> crypto,
    std::shared_ptr<VaultYubiKeyService> yubikey,
    std::shared_ptr<VaultFileService> file)
    : m_crypto(crypto)
    , m_yubikey(yubikey)
    , m_file(file)
{
    if (!m_crypto) {
        throw std::invalid_argument("VaultCryptoService cannot be null");
    }
    if (!m_yubikey) {
        throw std::invalid_argument("VaultYubiKeyService cannot be null");
    }
    if (!m_file) {
        throw std::invalid_argument("VaultFileService cannot be null");
    }
}

// ============================================================================
// Public Interface - Synchronous Creation
// ============================================================================

VaultResult<VaultCreationOrchestrator::CreationResult>
VaultCreationOrchestrator::create_vault_v2_sync(const CreationParams& params)
{
    Log::info("VaultCreationOrchestrator: Starting vault creation: {}", params.path);

    // Step 1: Validate parameters
    report_progress(params, CreationStep::Validation, "Validating parameters...");
    if (auto result = validate_params(params); !result) {
        return std::unexpected(result.error());
    }

    // Step 2: Generate Data Encryption Key
    report_progress(params, CreationStep::GenerateDEK, "Generating encryption key...");
    auto dek_result = generate_dek();
    if (!dek_result) {
        return std::unexpected(dek_result.error());
    }

    // Step 3: Derive admin KEK from password
    report_progress(params, CreationStep::DeriveAdminKEK, "Deriving key from password...");
    auto kek_result = derive_admin_kek(params);
    if (!kek_result) {
        return std::unexpected(kek_result.error());
    }

    // Step 4: Enroll YubiKey (if enabled)
    report_progress(params, CreationStep::EnrollYubiKey, "Enrolling YubiKey...");
    auto yubikey_result = enroll_yubikey(params, kek_result->kek);
    if (!yubikey_result) {
        return std::unexpected(yubikey_result.error());
    }

    // Step 5: Create admin key slot
    report_progress(params, CreationStep::CreateKeySlot, "Creating admin key slot...");
    auto slot_result = create_admin_key_slot(
        params, *kek_result, dek_result->dek, *yubikey_result);
    if (!slot_result) {
        return std::unexpected(slot_result.error());
    }

    // Step 6: Create vault header
    report_progress(params, CreationStep::CreateHeader, "Initializing vault header...");
    auto header_result = create_header(params, *slot_result);
    if (!header_result) {
        return std::unexpected(header_result.error());
    }

    // Step 7: Encrypt vault data
    report_progress(params, CreationStep::EncryptData, "Encrypting vault data...");
    auto encrypt_result = encrypt_vault_data(dek_result->dek);
    if (!encrypt_result) {
        return std::unexpected(encrypt_result.error());
    }

    // Step 8: Write to file
    report_progress(params, CreationStep::WriteFile, "Writing vault file...");
    auto write_result = write_vault_file(params, *header_result, *encrypt_result);
    if (!write_result) {
        return std::unexpected(write_result.error());
    }

    Log::info("VaultCreationOrchestrator: Vault creation completed successfully");

    // Build result
    CreationResult result;
    result.dek = dek_result->dek;
    result.memory_locked = dek_result->memory_locked;
    result.header = *header_result;
    result.file_path = params.path;

    return result;
}

// ============================================================================
// Public Interface - Asynchronous Creation
// ============================================================================

void VaultCreationOrchestrator::create_vault_v2_async(
    const CreationParams& params,
    CompletionCallback completion_callback)
{
    // TODO: Phase 3 - Implement async wrapper
    // For now, just call sync version and invoke callback
    Log::warning("VaultCreationOrchestrator: Async creation not yet implemented, using sync");

    auto result = create_vault_v2_sync(params);
    if (result) {
        completion_callback({});
    } else {
        completion_callback(std::unexpected(result.error()));
    }
}

// ============================================================================
// Step 1: Validate Parameters
// ============================================================================

VaultResult<> VaultCreationOrchestrator::validate_params(const CreationParams& params)
{
    // Validate path
    if (params.path.empty()) {
        return std::unexpected(VaultError::InvalidData);
    }

    // Validate username
    if (params.admin_username.empty() || params.admin_username.size() < 3) {
        return std::unexpected(VaultError::InvalidUsername);
    }
    if (params.admin_username.size() > 64) {
        return std::unexpected(VaultError::InvalidUsername);
    }

    // Validate password against policy
    if (params.admin_password.empty()) {
        return std::unexpected(VaultError::WeakPassword);
    }
    if (params.admin_password.size() < params.policy.min_password_length) {
        return std::unexpected(VaultError::WeakPassword);
    }

    // Validate policy
    if (params.policy.pbkdf2_iterations < 100000) {
        return std::unexpected(VaultError::InvalidData);
    }

    // Validate YubiKey PIN if provided
    if (params.policy.require_yubikey && params.yubikey_pin) {
        if (!VaultYubiKeyService::validate_pin_format(*params.yubikey_pin)) {
            return std::unexpected(VaultError::YubiKeyError);
        }
    }

    return {};
}

// ============================================================================
// Step 2: Generate Data Encryption Key
// ============================================================================

VaultResult<VaultCreationOrchestrator::DEKData>
VaultCreationOrchestrator::generate_dek()
{
    auto result = m_crypto->generate_dek();
    if (!result) {
        return std::unexpected(result.error());
    }

    DEKData data;
    data.dek = result->dek;
    data.memory_locked = result->memory_locked;

    return data;
}

// ============================================================================
// Step 3: Derive Admin KEK
// ============================================================================

VaultResult<VaultCreationOrchestrator::KEKResult>
VaultCreationOrchestrator::derive_admin_kek(const CreationParams& params)
{
    // Derive KEK from password (VaultCryptoService generates salt internally)
    auto kek_result = m_crypto->derive_kek_from_password(
        params.admin_password,
        params.policy.pbkdf2_iterations
    );
    if (!kek_result) {
        return std::unexpected(kek_result.error());
    }

    KEKResult result;
    result.kek = kek_result->kek;
    result.salt = kek_result->salt;

    return result;
}

// ============================================================================
// Step 4: Enroll YubiKey (if enabled)
// ============================================================================

VaultResult<std::optional<VaultCreationOrchestrator::EnrollmentData>>
VaultCreationOrchestrator::enroll_yubikey(
    const CreationParams& params,
    std::array<uint8_t, 32>& kek)
{
    // If YubiKey not enabled, skip
    if (!params.policy.require_yubikey) {
        Log::debug("VaultCreationOrchestrator: YubiKey not required, skipping enrollment");
        return std::optional<EnrollmentData>{};
    }

    // Generate challenges for enrollment
    auto policy_challenge_result = VaultYubiKeyService::generate_challenge(32);
    if (!policy_challenge_result) {
        return std::unexpected(policy_challenge_result.error());
    }
    std::array<uint8_t, 32> policy_challenge;
    std::copy_n(policy_challenge_result->begin(), 32, policy_challenge.begin());

    auto user_challenge_result = VaultYubiKeyService::generate_challenge(32);
    if (!user_challenge_result) {
        return std::unexpected(user_challenge_result.error());
    }
    std::array<uint8_t, 32> user_challenge;
    std::copy_n(user_challenge_result->begin(), 32, user_challenge.begin());

    // Enroll YubiKey with two challenges
    auto enroll_result = m_yubikey->enroll_yubikey(
        params.admin_username,  // Use admin username as FIDO2 user_id
        policy_challenge,
        user_challenge,
        params.yubikey_pin.value_or(""),
        1,  // slot 1
        params.enforce_fips  // Pass FIPS mode from creation parameters
    );
    if (!enroll_result) {
        return std::unexpected(enroll_result.error());
    }

    // Combine YubiKey user response with KEK via XOR
    // This provides hybrid authentication: password + YubiKey
    if (enroll_result->user_response.size() >= 32) {
        for (size_t i = 0; i < 32; ++i) {
            kek[i] ^= enroll_result->user_response[i];
        }
    } else {
        Log::warning("VaultCreationOrchestrator: YubiKey response too short for XOR");
    }

    // Store enrollment data for key slot
    EnrollmentData data;
    data.serial = enroll_result->device_info.serial;
    data.user_challenge = user_challenge;  // Store the challenge itself (not just response)
    data.policy_response = enroll_result->policy_response;
    data.user_response = enroll_result->user_response;
    data.slot = 1;  // hardcoded to slot 1 for now

    return data;
}

// ============================================================================
// Step 5: Create Admin Key Slot
// ============================================================================

VaultResult<KeySlot>
VaultCreationOrchestrator::create_admin_key_slot(
    const CreationParams& params,
    const KEKResult& kek,
    const std::array<uint8_t, 32>& dek,
    const std::optional<EnrollmentData>& yubikey_data)
{
    // Wrap DEK with KEK
    auto wrapped_result = m_crypto->wrap_dek(kek.kek, dek);
    if (!wrapped_result) {
        return std::unexpected(wrapped_result.error());
    }

    // Convert vector to array for wrapped_dek (40 bytes)
    if (wrapped_result->size() != 40) {
        return std::unexpected(VaultError::CryptoError);
    }
    std::array<uint8_t, 40> wrapped_array;
    std::copy_n(wrapped_result->begin(), 40, wrapped_array.begin());

    // Build key slot
    KeySlot slot;
    slot.active = true;
    slot.username = params.admin_username.raw();
    slot.role = UserRole::ADMINISTRATOR;
    slot.salt = kek.salt;
    slot.wrapped_dek = wrapped_array;
    slot.must_change_password = false;
    slot.password_changed_at = std::time(nullptr);

    // Add initial password to history if password history is enabled
    if (params.policy.password_history_depth > 0) {
        auto history_entry = KeepTower::PasswordHistory::hash_password(params.admin_password);
        if (history_entry) {
            KeepTower::PasswordHistory::add_to_history(
                slot.password_history,
                history_entry.value(),
                params.policy.password_history_depth
            );
            Log::debug("VaultCreationOrchestrator: Added initial admin password to history");
        } else {
            Log::warning("VaultCreationOrchestrator: Failed to hash initial password for history");
        }
    }

    // Add YubiKey data if enrolled
    if (yubikey_data.has_value()) {
        slot.yubikey_enrolled = true;
        slot.yubikey_serial = yubikey_data->serial;
        slot.yubikey_enrolled_at = std::time(nullptr);
        slot.yubikey_challenge = yubikey_data->user_challenge;  // Store 32-byte challenge
    }

    return slot;
}

// ============================================================================
// Step 6: Create Vault Header
// ============================================================================

VaultResult<VaultHeaderV2>
VaultCreationOrchestrator::create_header(
    const CreationParams& params,
    const KeySlot& admin_slot)
{
    VaultHeaderV2 header;
    header.security_policy = params.policy;
    header.key_slots.push_back(admin_slot);

    return header;
}

// ============================================================================
// Step 7: Encrypt Vault Data
// ============================================================================

VaultResult<VaultCreationOrchestrator::EncryptionResult>
VaultCreationOrchestrator::encrypt_vault_data(const std::array<uint8_t, 32>& dek)
{
    // Create initial empty VaultData protobuf
    keeptower::VaultData vault_data;

    // Initialize metadata
    auto* metadata = vault_data.mutable_metadata();
    metadata->set_schema_version(2);
    metadata->set_created_at(std::time(nullptr));
    metadata->set_last_modified(metadata->created_at());
    metadata->set_last_accessed(metadata->created_at());
    metadata->set_name("New Vault");
    metadata->set_access_count(0);

    // Set default security settings
    metadata->set_clipboard_timeout_seconds(30);
    metadata->set_auto_lock_timeout_seconds(300);  // 5 minutes
    metadata->set_auto_lock_enabled(true);
    metadata->set_undo_redo_enabled(true);
    metadata->set_undo_history_limit(50);
    metadata->set_account_password_history_enabled(true);
    metadata->set_account_password_history_limit(5);

    // Empty accounts list (no accounts yet)
    // Empty groups list (no groups yet)

    // Serialize to binary
    std::vector<uint8_t> plaintext(vault_data.ByteSizeLong());
    if (!vault_data.SerializeToArray(plaintext.data(), plaintext.size())) {
        return std::unexpected(VaultError::SerializationFailed);
    }

    // Encrypt with DEK
    auto encrypted = m_crypto->encrypt_vault_data(plaintext, dek);
    if (!encrypted) {
        return std::unexpected(encrypted.error());
    }

    EncryptionResult result;
    result.ciphertext = encrypted->ciphertext;
    result.iv = encrypted->iv;

    return result;
}

// ============================================================================
// Step 8: Write Vault File
// ============================================================================

VaultResult<>
VaultCreationOrchestrator::write_vault_file(
    const CreationParams& params,
    const VaultHeaderV2& header,
    const EncryptionResult& encrypted)
{
    // Build V2FileHeader
    VaultFormatV2::V2FileHeader file_header;
    file_header.magic = VaultFormatV2::VAULT_MAGIC;
    file_header.version = VaultFormatV2::VAULT_VERSION_V2;
    file_header.pbkdf2_iterations = params.policy.pbkdf2_iterations;
    file_header.vault_header = header;

    // Copy encryption metadata
    // Note: data_salt is generated during encryption (in encrypted.iv derivation)
    // For now, use zeros for salt (will be refactored when adding proper salt generation)
    file_header.data_salt = {};

    // Copy IV from encryption result
    if (encrypted.iv.size() != 12) {
        return std::unexpected(VaultError::CryptoError);
    }
    std::copy_n(encrypted.iv.begin(), 12, file_header.data_iv.begin());

    // FEC settings
    // TODO: Add FEC settings to VaultSecurityPolicy
    // For now, disable FEC (FEC percentage = 0)
    file_header.header_flags = 0;  // FEC disabled
    file_header.fec_redundancy_percent = 0;

    // Serialize V2FileHeader
    auto header_bytes_result = VaultFormatV2::write_header(file_header);
    if (!header_bytes_result) {
        return std::unexpected(header_bytes_result.error());
    }

    // Combine header + encrypted data
    std::vector<uint8_t> file_data = *header_bytes_result;
    file_data.insert(file_data.end(),
                     encrypted.ciphertext.begin(),
                     encrypted.ciphertext.end());

    // Write to file atomically
    return m_file->write_vault_file(
        params.path,
        file_data,
        true,  // is_v2 format
        0      // FEC percentage (already handled in header)
    );
}

// ============================================================================
// Helper Methods
// ============================================================================

void VaultCreationOrchestrator::report_progress(
    const CreationParams& params,
    CreationStep step,
    const std::string& description)
{
    if (params.progress_callback) {
        params.progress_callback(
            step_number(step),
            total_steps(),
            description
        );
    }
}

int VaultCreationOrchestrator::step_number(CreationStep step)
{
    switch (step) {
        case CreationStep::Validation:      return 1;
        case CreationStep::GenerateDEK:     return 2;
        case CreationStep::DeriveAdminKEK:  return 3;
        case CreationStep::EnrollYubiKey:   return 4;
        case CreationStep::CreateKeySlot:   return 5;
        case CreationStep::CreateHeader:    return 6;
        case CreationStep::EncryptData:     return 7;
        case CreationStep::WriteFile:       return 8;
    }
    return 0;
}

} // namespace KeepTower
