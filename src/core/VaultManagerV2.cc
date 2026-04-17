// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file VaultManagerV2.cc
 * @brief V2 multi-user vault authentication and management implementation
 *
 * Implements Phase 2 of multi-user architecture:
 * - V2 vault creation with security policy
 * - User authentication (username + password + optional YubiKey)
 * - User management (add/remove/change password)
 * - LUKS-style key slot operations
 */

#include "VaultManager.h"
#include "controllers/VaultCreationOrchestrator.h"
#include "lib/crypto/VaultCrypto.h"
#include "lib/crypto/KeyWrapping.h"
#include "PasswordHistory.h"
#include "managers/AccountManager.h"
#include "managers/GroupManager.h"
#include "lib/crypto/VaultCryptoService.h"
#include "services/VaultFileService.h"
#include "services/VaultDataService.h"
#include "lib/backup/VaultBackupPolicy.h"
#include "services/KeySlotManager.h"
#include "services/VaultYubiKeyService.h"
#include "services/YubiKeyEnrollmentService.h"
#include "services/PasswordManagementService.h"
#include "services/UserProvisioningService.h"
#include "services/SecurityPolicyService.h"
#include "lib/crypto/UsernameHashService.h"
#include "lib/crypto/KekDerivationService.h"
#include "services/V2AuthService.h"
#include "../utils/Log.h"
#include "../utils/SecureMemory.h"
#include <glibmm/main.h>
#include <algorithm>
#include <thread>
#include <chrono>
#include <filesystem>

// Using declarations for KeepTower types (VaultManager is global scope)
using KeepTower::VaultError;
using KeepTower::VaultHeaderV2;
using KeepTower::KeySlot;
using KeepTower::UserRole;
using KeepTower::UserSession;
using KeepTower::KekDerivationService;
using KeepTower::KeySlotManager;
using KeepTower::V2AuthService;
using KeepTower::VaultSecurityPolicy;
using KeepTower::KeyWrapping;
namespace Log = KeepTower::Log;

// ============================================================================
// Phase 2 Day 5: Orchestrator Factory
// ============================================================================

std::unique_ptr<KeepTower::VaultCreationOrchestrator> VaultManager::create_orchestrator() {
    // Lazy-initialize services on first use
    if (!m_crypto_service) {
        m_crypto_service = std::make_shared<KeepTower::VaultCryptoService>();
        Log::debug("VaultManager: Initialized VaultCryptoService");
    }
    if (!m_yubikey_service) {
        m_yubikey_service = std::make_shared<KeepTower::VaultYubiKeyService>();
        Log::debug("VaultManager: Initialized VaultYubiKeyService");
    }
    if (!m_file_service) {
        m_file_service = std::make_shared<KeepTower::VaultFileService>();
        Log::debug("VaultManager: Initialized VaultFileService");
    }

    auto concrete_yubikey_service = std::dynamic_pointer_cast<KeepTower::VaultYubiKeyService>(m_yubikey_service);
    if (!concrete_yubikey_service) {
        concrete_yubikey_service = std::make_shared<KeepTower::VaultYubiKeyService>();
        m_yubikey_service = concrete_yubikey_service;
        Log::debug("VaultManager: Replaced injected YubiKey service with concrete VaultYubiKeyService for orchestrator");
    }

    // Create orchestrator with injected services
    return std::make_unique<KeepTower::VaultCreationOrchestrator>(
        m_crypto_service,
        concrete_yubikey_service,
        m_file_service
    );
}

// ============================================================================
// V2 Vault Creation (Refactored to use Orchestrator)
// ============================================================================

KeepTower::VaultResult<> VaultManager::create_vault_v2(
    const std::string& path,
    const Glib::ustring& admin_username,
    const Glib::ustring& admin_password,
    const KeepTower::VaultSecurityPolicy& policy,
    const std::optional<std::string>& yubikey_pin) {

    Log::info("VaultManager: Creating V2 vault at: {} (using orchestrator)", path);

    // Close any open vault
    if (m_vault_open) {
        if (!close_vault()) {
            Log::error("VaultManager: Failed to close existing vault");
            return std::unexpected(VaultError::VaultAlreadyOpen);
        }
    }

    // Create orchestrator and delegate vault creation
    auto orchestrator = create_orchestrator();

    KeepTower::VaultCreationOrchestrator::CreationParams params;
    params.path = path;
    params.admin_username = admin_username;
    params.admin_password = admin_password;
    params.policy = policy;
    params.yubikey_pin = yubikey_pin;
    params.enforce_fips = is_fips_enabled();  // Pass FIPS mode setting
    params.progress_callback = nullptr;  // No progress for sync operation

    auto result = orchestrator->create_vault_v2_sync(params);

    if (!result.has_value()) {
        Log::error("VaultManager: Orchestrator failed to create vault");
        return std::unexpected(result.error());
    }

    // Extract results from orchestrator
    const auto& creation_result = result.value();

    // Initialize VaultManager state with orchestrator results
    m_v2_dek = creation_result.dek;
    m_v2_header = creation_result.header;
    m_vault_open = true;
    m_is_v2_vault = true;
    m_current_vault_path = path;
    m_modified = false;

    // FIPS-140-3: Lock DEK in memory to prevent swap exposure
    if (lock_memory(m_v2_dek.data(), m_v2_dek.size())) {
        Log::debug("VaultManager: Locked V2 DEK in memory");
    } else {
        Log::warning("VaultManager: Failed to lock V2 DEK - continuing without memory lock");
    }

    // Set current user session (admin)
    m_current_session = UserSession{
        .username = admin_username.raw(),
        .role = UserRole::ADMINISTRATOR,
        .password_change_required = false
    };

    // Initialize empty vault data and managers
    m_vault_data->Clear();  // Empty protobuf structure
    m_account_manager = std::make_unique<KeepTower::AccountManager>(*m_vault_data, m_modified);
    m_group_manager = std::make_unique<KeepTower::GroupManager>(*m_vault_data, m_modified);

    Log::info("VaultManager: V2 vault created successfully with admin user");
    return {};
}

// ============================================================================
// V2 Vault Creation - Asynchronous (Phase 3)
// ============================================================================

void VaultManager::create_vault_v2_async(
    const std::string& path,
    const Glib::ustring& admin_username,
    const Glib::ustring& admin_password,
    const KeepTower::VaultSecurityPolicy& policy,
    V2VaultCreationProgressCallback progress_callback,
    std::function<void(KeepTower::VaultResult<>)> completion_callback,
    const std::optional<std::string>& yubikey_pin) {

    Log::info("VaultManager: Creating V2 vault asynchronously at: {}", path);

    // Close any open vault first (synchronously, before spawning thread)
    if (m_vault_open) {
        if (!close_vault()) {
            Log::error("VaultManager: Failed to close existing vault");
            // Invoke completion callback with error on GTK thread
            Glib::signal_idle().connect_once([completion_callback]() {
                completion_callback(std::unexpected(VaultError::VaultAlreadyOpen));
            });
            return;
        }
    }

    // Create orchestrator
    auto orchestrator = create_orchestrator();

    // Setup parameters
    KeepTower::VaultCreationOrchestrator::CreationParams params;
    params.path = path;
    params.admin_username = admin_username;
    params.admin_password = admin_password;
    params.policy = policy;
    params.yubikey_pin = yubikey_pin;
    params.enforce_fips = is_fips_enabled();
    params.progress_callback = progress_callback;

    // Wrap the orchestrator's completion callback to initialize VaultManager state
    auto wrapped_completion = [this, path, admin_username, completion_callback](
        KeepTower::VaultResult<KeepTower::VaultCreationOrchestrator::CreationResult> result)
    {
        if (!result.has_value()) {
            // Vault creation failed
            Log::error("VaultManager: Async vault creation failed");
            completion_callback(std::unexpected(result.error()));
            return;
        }

        // Success: Initialize VaultManager state
        const auto& creation_result = result.value();

        m_v2_dek = creation_result.dek;
        m_v2_header = creation_result.header;
        m_vault_open = true;
        m_is_v2_vault = true;
        m_current_vault_path = path;
        m_modified = false;

        // FIPS-140-3: Lock DEK in memory
        if (lock_memory(m_v2_dek.data(), m_v2_dek.size())) {
            Log::debug("VaultManager: Locked V2 DEK in memory");
        } else {
            Log::warning("VaultManager: Failed to lock V2 DEK - continuing without memory lock");
        }

        // Set current user session (admin)
        m_current_session = UserSession{
            .username = admin_username.raw(),
            .role = UserRole::ADMINISTRATOR,
            .password_change_required = false
        };

        // Initialize empty vault data and managers
        m_vault_data->Clear();
        m_account_manager = std::make_unique<KeepTower::AccountManager>(*m_vault_data, m_modified);
        m_group_manager = std::make_unique<KeepTower::GroupManager>(*m_vault_data, m_modified);

        Log::info("VaultManager: Async V2 vault created successfully with admin user");

        // Notify caller of success (empty VaultResult means success)
        completion_callback({});
    };

    // Delegate to orchestrator's async method
    orchestrator->create_vault_v2_async(params, wrapped_completion);
}

// ============================================================================
// V2 Vault Authentication
// ============================================================================

KeepTower::VaultResult<KeepTower::UserSession> VaultManager::open_vault_v2(
    const std::string& path,
    const Glib::ustring& username,
    const Glib::ustring& password,
    const std::string& yubikey_serial) {

    (void)yubikey_serial;

    Log::info("VaultManager: Opening V2 vault: {}", path);

    // Close any open vault
    if (m_vault_open) {
        if (!close_vault()) {
            Log::error("VaultManager: Failed to close existing vault");
            return std::unexpected(VaultError::VaultAlreadyOpen);
        }
    }

    // Read vault file from disk
    std::vector<uint8_t> file_data;
    int iterations_from_file = 0;
    auto read_result = KeepTower::VaultFileService::read_vault_file(path, file_data, iterations_from_file);
    if (!read_result) {
        Log::error("VaultManager: Failed to read V2 vault file: {}", path);
        return std::unexpected(read_result.error());
    }

    // Parse V2 header
    auto metadata_result = KeepTower::VaultFileService::read_v2_metadata(file_data);
    if (!metadata_result) {
        Log::error("VaultManager: Failed to parse V2 vault header");
        return std::unexpected(metadata_result.error());
    }

    auto metadata = std::move(metadata_result.value());

    auto user_slot_result = V2AuthService::resolve_user_slot_for_open(
        metadata.vault_header.key_slots,
        username.raw(),
        metadata.vault_header.security_policy);
    if (!user_slot_result) {
        return std::unexpected(user_slot_result.error());
    }
    KeySlot* user_slot = user_slot_result.value();

    Log::debug("VaultManager: Deriving KEK (password length: {} bytes, {} chars, algorithm: 0x{:02x})",
               password.bytes(), password.length(), user_slot->kek_derivation_algorithm);

    auto kek_result = V2AuthService::derive_password_kek_for_slot(
        *user_slot,
        password.raw(),
        metadata.pbkdf2_iterations,
        metadata.vault_header.security_policy);
    if (!kek_result) {
        return std::unexpected(kek_result.error());
    }
    std::array<uint8_t, 32> final_kek = kek_result.value();

    // Check if this user has YubiKey enrolled
#ifdef HAVE_YUBIKEY_SUPPORT
    if (user_slot->yubikey_enrolled) {
        Log::info("VaultManager: User has YubiKey enrolled, requiring device");

        // Decrypt stored PIN first (encrypted with password-derived KEK only)
        // This must happen BEFORE getting YubiKey response to avoid circular dependency
        auto pin_result = V2AuthService::decrypt_yubikey_pin_for_open(*user_slot, final_kek);
        if (!pin_result) {
            return std::unexpected(pin_result.error());
        }
        std::string decrypted_pin = std::move(pin_result.value());

        // Initialize seam service if needed
        if (!m_yubikey_service) {
            m_yubikey_service = std::make_shared<KeepTower::VaultYubiKeyService>();
            Log::debug("VaultManager: Initialized VaultYubiKeyService for vault open");
        }

        // Use injected service for YubiKey challenge-response
        // The seam handles device initialization, presence checking, serial verification (warning-only),
        // credential loading, and challenge execution
        const YubiKeyAlgorithm yk_algorithm = static_cast<YubiKeyAlgorithm>(
            metadata.vault_header.security_policy.yubikey_algorithm);

        // Convert challenge array to vector for seam interface
        std::vector<uint8_t> challenge_vec(
            user_slot->yubikey_challenge.begin(),
            user_slot->yubikey_challenge.end());

        auto challenge_result = m_yubikey_service->perform_authenticated_challenge(
            challenge_vec,
            user_slot->yubikey_credential_id,
            decrypted_pin,
            user_slot->yubikey_serial,
            yk_algorithm,
            true,  // require_touch
            15000, // 15 second timeout
            is_fips_enabled(),
            KeepTower::IVaultYubiKeyService::SerialMismatchPolicy::WarnOnly);

        if (!challenge_result) {
            return std::unexpected(challenge_result.error());
        }

        // Combine KEK with YubiKey response (use v2 for variable-length responses)
        final_kek = V2AuthService::combine_kek_with_yubikey_response_for_open(
            final_kek,
            std::span<const uint8_t>(challenge_result.value().response));

        Log::info("VaultManager: YubiKey authentication successful (serial: {})",
                 challenge_result.value().device_info.serial);
    }
#endif

    // Unwrap DEK (verifies password correctness, and YubiKey if enrolled)
    Log::info("VaultManager: Attempting to unwrap DEK");
    auto unwrap_result = KeyWrapping::unwrap_key(final_kek, user_slot->wrapped_dek);
    if (!unwrap_result) {
        if (user_slot->yubikey_enrolled) {
            Log::error("VaultManager: Failed to unwrap DEK - incorrect password or YubiKey");
        } else {
            Log::error("VaultManager: Failed to unwrap DEK - incorrect password");
        }
        return std::unexpected(VaultError::AuthenticationFailed);
    }

    m_v2_dek = unwrap_result.value().dek;

    // FIPS-140-3: Lock DEK in memory to prevent swap exposure
    if (lock_memory(m_v2_dek.data(), m_v2_dek.size())) {
        Log::debug("VaultManager: Locked V2 DEK in memory");
    } else {
        Log::warning("VaultManager: Failed to lock V2 DEK - continuing without memory lock");
    }

    // FIPS-140-3: Lock policy-level YubiKey challenge (shared by all users)
    if (metadata.vault_header.security_policy.require_yubikey) {
        auto& policy_challenge = metadata.vault_header.security_policy.yubikey_challenge;
        if (lock_memory(policy_challenge.data(), policy_challenge.size())) {
            Log::debug("VaultManager: Locked V2 policy YubiKey challenge in memory");
        }
    }

    // FIPS-140-3: Lock authenticated user's YubiKey challenge
    if (user_slot->yubikey_enrolled) {
        if (lock_memory(user_slot->yubikey_challenge.data(),
                       user_slot->yubikey_challenge.size())) {
            Log::debug("VaultManager: Locked user YubiKey challenge in memory");
        }
    }

    // Extract encrypted data (after header)
    if (metadata.data_offset >= file_data.size()) {
        Log::error("VaultManager: Invalid data offset: {}", metadata.data_offset);
        return std::unexpected(VaultError::CorruptedFile);
    }

    std::vector<uint8_t> ciphertext(
        file_data.begin() + static_cast<std::ptrdiff_t>(metadata.data_offset),
        file_data.end());

    // Decrypt vault data
    std::vector<uint8_t> plaintext;
    std::span<const uint8_t> iv_span(metadata.data_iv);
    if (!KeepTower::VaultCrypto::decrypt_data(ciphertext, m_v2_dek, iv_span, plaintext)) {
        Log::error("VaultManager: Failed to decrypt vault data");
        return std::unexpected(VaultError::DecryptionFailed);
    }

    // Parse protobuf
    auto vault_data_result = KeepTower::VaultDataService::deserialize_vault_data(plaintext);
    if (!vault_data_result) {
        Log::error("VaultManager: Failed to parse vault data");
        secure_clear(plaintext);
        return std::unexpected(VaultError::CorruptedFile);
    }
    keeptower::VaultData vault_data = std::move(vault_data_result.value());
    secure_clear(plaintext);

    // Update last login timestamp
    user_slot->last_login_at = std::chrono::system_clock::now().time_since_epoch().count();

    // Initialize vault state BEFORE migration (migrate_user_hash needs these set)
    m_vault_open = true;
    m_is_v2_vault = true;
    m_current_vault_path = path;
    m_v2_header = metadata.vault_header;
    auto* v2_header = m_v2_header ? &*m_v2_header : nullptr;
    if (!v2_header) {
        Log::error("VaultManager: Failed to initialize V2 header");
        return std::unexpected(VaultError::InvalidData);
    }

    // Re-bind user_slot to the slot inside m_v2_header so modifications persist.
    auto slot_in_header_it = std::find_if(v2_header->key_slots.begin(), v2_header->key_slots.end(),
        [&](const KeySlot& s) {
            return s.active &&
                   s.username_hash == user_slot->username_hash &&
                   s.username_hash_size == user_slot->username_hash_size;
        });
    KeySlot* user_slot_in_header = (slot_in_header_it != v2_header->key_slots.end()) ? &*slot_in_header_it : nullptr;

    if (!user_slot_in_header) {
        Log::error("VaultManager: Failed to find user slot in m_v2_header after copy");
        return std::unexpected(VaultError::InvalidData);
    }

    // Check if user needs username hash migration
    // Status 0xFF = authenticated via old algorithm, must migrate to new
    bool migration_active = (metadata.vault_header.security_policy.migration_flags & 0x01) != 0;
    if (migration_active && user_slot_in_header->migration_status == 0xFF) {
        Log::info("VaultManager: Authenticated user requires username hash migration");

        // Perform migration (now using the slot from m_v2_header)
        auto migrate_result = migrate_user_hash(user_slot_in_header, username.raw(), password.raw());
        if (!migrate_result) {
            Log::error("VaultManager: Username hash migration failed: {}",
                       to_string(migrate_result.error()));
            // Don't fail authentication - user can try again later
            // Migration will be retried on next login
        } else {
            Log::info("VaultManager: Username hash migration completed");
        }
    }
    *m_vault_data = vault_data;
    m_modified = true;  // Mark modified to save updated last_login_at

    // Load vault-persisted backup settings (enabled/count). Path remains runtime-local.
    if (m_backup_policy && m_backup_policy->load_from_vault_data(*m_vault_data)) {
        const VaultManager::BackupSettings backup_settings = get_backup_settings();
        Log::debug(
            "VaultManager: Loaded backup settings from V2 vault: enabled={}, count={}",
            backup_settings.enabled,
            backup_settings.count
        );
    }

    // Extract FEC settings from V2 header
    // Note: Header always has FEC enabled (per spec), so check data FEC setting instead
    m_use_reed_solomon = metadata.fec_redundancy_percent > 0;
    if (m_use_reed_solomon) {
        m_rs_redundancy_percent = metadata.fec_redundancy_percent;
        Log::info("VaultManager: V2 vault has data FEC enabled (redundancy: {}%)", m_rs_redundancy_percent);
    } else {
        Log::info("VaultManager: V2 vault has data FEC disabled (header FEC still enabled at 20% per spec)");
    }

    // Initialize managers after vault data is loaded
    m_account_manager = std::make_unique<KeepTower::AccountManager>(*m_vault_data, m_modified);
    m_group_manager = std::make_unique<KeepTower::GroupManager>(*m_vault_data, m_modified);

    // Create session
    UserSession session{
        .username = username.raw(),
        .role = user_slot_in_header->role,
        .password_change_required = user_slot_in_header->must_change_password
    };

    // Check if vault policy requires YubiKey but user doesn't have one enrolled
    if (v2_header->security_policy.require_yubikey && !user_slot_in_header->yubikey_enrolled) {
        session.requires_yubikey_enrollment = true;
        Log::warning("VaultManager: User must enroll YubiKey (required by policy)");
    } else {
        session.requires_yubikey_enrollment = false;
    }

    m_current_session = session;

    Log::info("VaultManager: User authenticated successfully");
    return session;
}

// ============================================================================
// User Management
// ============================================================================

KeepTower::VaultResult<> VaultManager::add_user(
    const Glib::ustring& username,
    const Glib::ustring& temporary_password,
    KeepTower::UserRole role,
    bool must_change_password,
    const std::optional<std::string>& yubikey_pin) {

    Log::info("VaultManager: Adding user");

    if (!m_vault_open || !m_is_v2_vault) {
        Log::error("VaultManager: No V2 vault open");
        return std::unexpected(VaultError::VaultNotOpen);
    }

    // Check current user permissions
    if (!m_current_session || m_current_session->role != UserRole::ADMINISTRATOR) {
        Log::error("VaultManager: Only administrators can add users");
        return std::unexpected(VaultError::PermissionDenied);
    }

    auto header_result = require_open_v2_header("add_user");
    if (!header_result) {
        return std::unexpected(header_result.error());
    }

    KeepTower::UserProvisioningContext ctx{
        *header_result.value(), m_v2_dek, m_yubikey_service,
        m_modified, is_fips_enabled()};
    return KeepTower::UserProvisioningService::add_user(
        ctx, username, temporary_password, role, must_change_password, yubikey_pin);
}

KeepTower::VaultResult<> VaultManager::remove_user(const Glib::ustring& username) {
    Log::info("VaultManager: Removing user");

    // Validate vault state
    if (!m_vault_open || !m_is_v2_vault) {
        Log::error("VaultManager: No V2 vault open");
        return std::unexpected(VaultError::VaultNotOpen);
    }

    // Check current user permissions
    if (!m_current_session || m_current_session->role != UserRole::ADMINISTRATOR) {
        Log::error("VaultManager: Only administrators can remove users");
        return std::unexpected(VaultError::PermissionDenied);
    }

    // Prevent self-removal
    if (username.raw() == m_current_session->username) {
        Log::error("VaultManager: Cannot remove yourself");
        return std::unexpected(VaultError::SelfRemovalNotAllowed);
    }

    auto header_result = require_open_v2_header("remove_user");
    if (!header_result) {
        return std::unexpected(header_result.error());
    }
    auto* v2_header = header_result.value();

    auto deactivate_result = KeySlotManager::deactivate_user(
        v2_header->key_slots,
        username.raw(),
        v2_header->security_policy);
    if (!deactivate_result) {
        return std::unexpected(deactivate_result.error());
    }

    m_modified = true;

    Log::info("VaultManager: User removed successfully");
    return {};
}

KeepTower::VaultResult<> VaultManager::validate_new_password(
    const Glib::ustring& username,
    const Glib::ustring& new_password) {

    Log::debug("VaultManager: Validating new password for user: {}", username.raw());

    if (!m_vault_open || !m_is_v2_vault) {
        Log::error("VaultManager: No V2 vault open");
        return std::unexpected(VaultError::VaultNotOpen);
    }

    auto header_result = require_open_v2_header("validate_new_password");
    if (!header_result) {
        return std::unexpected(header_result.error());
    }

    KeepTower::PasswordManagementContext ctx{
        *header_result.value(), m_v2_dek, m_current_session,
        m_yubikey_service, m_modified, is_fips_enabled()};
    return KeepTower::PasswordManagementService::validate_new_password(ctx, username, new_password);
}

KeepTower::VaultResult<> VaultManager::change_user_password(
    const Glib::ustring& username,
    const Glib::ustring& old_password,
    const Glib::ustring& new_password,
    const std::optional<std::string>& yubikey_pin,
    std::function<void(const std::string&)> progress_callback) {

    Log::info("VaultManager: Changing password for user");

    if (!m_vault_open || !m_is_v2_vault) {
        Log::error("VaultManager: No V2 vault open");
        return std::unexpected(VaultError::VaultNotOpen);
    }

    auto header_result = require_open_v2_header("change_user_password");
    if (!header_result) {
        return std::unexpected(header_result.error());
    }

    KeepTower::PasswordManagementContext ctx{
        *header_result.value(), m_v2_dek, m_current_session,
        m_yubikey_service, m_modified, is_fips_enabled()};
    return KeepTower::PasswordManagementService::change_user_password(
        ctx, username, old_password, new_password, yubikey_pin, std::move(progress_callback));
}

// ============================================================================
// Username Hash Migration (Phase 1)
// ============================================================================

KeepTower::VaultResult<> VaultManager::migrate_user_hash(
    KeySlot* user_slot,
    const std::string& username,
    const std::string& password) {

    if (!user_slot || !m_v2_header) {
        Log::error("VaultManager: migrate_user_hash called with null parameters");
        return std::unexpected(VaultError::InvalidData);
    }

    if (!m_vault_open || !m_is_v2_vault) {
        Log::error("VaultManager: migrate_user_hash called but no V2 vault open");
        return std::unexpected(VaultError::VaultNotOpen);
    }

    KeepTower::PasswordManagementContext ctx{
        *m_v2_header, m_v2_dek, m_current_session,
        m_yubikey_service, m_modified, is_fips_enabled()};
    return KeepTower::PasswordManagementService::migrate_user_hash(
        ctx, user_slot, username, password, [this]() { return save_vault(true); });
}

// ============================================================================
// Phase 3: Async Password Change (Non-blocking YubiKey Touches)
// ============================================================================

void VaultManager::change_user_password_async(
    const Glib::ustring& username,
    const Glib::ustring& old_password,
    const Glib::ustring& new_password,
    std::function<void(int step, int total, const std::string& description)> progress_callback,
    std::function<void(KeepTower::VaultResult<>)> completion_callback,
    const std::optional<std::string>& yubikey_pin) {

    Log::info("VaultManager: Starting async password change for user");

    // Determine if YubiKey is enrolled for this user (affects step count)
    bool yubikey_enrolled = false;
    int total_steps = 1;  // Minimum: password change without YubiKey

    const auto* v2_header = (m_vault_open && m_is_v2_vault && m_v2_header)
        ? &*m_v2_header
        : nullptr;
    if (v2_header) {
        if (KeySlotManager::is_yubikey_enrolled_for_user(
                v2_header->key_slots, username.raw(), v2_header->security_policy)) {
            yubikey_enrolled = true;
            total_steps = 2;  // With YubiKey: verify old + combine new
        }
    }

    // Wrap progress callback for GTK thread
    auto wrapped_progress = [progress_callback](int step, int total, const std::string& desc) {
        if (progress_callback) {
            Glib::signal_idle().connect_once([progress_callback, step, total, desc]() {
                progress_callback(step, total, desc);
            });
        }
    };

    // Wrap completion callback for GTK thread
    auto wrapped_completion = [this, completion_callback, username](KeepTower::VaultResult<> result) {
        Glib::signal_idle().connect_once([this, completion_callback, username, result]() {
            // Update session if password changed successfully and user changed own password
            if (result && m_current_session && m_current_session->username == username.raw()) {
                m_current_session->password_change_required = false;
            }
            completion_callback(result);
        });
    };

    // Create progress callback for sync method
    auto sync_progress_callback = [progress_callback](const std::string& message) {
        if (progress_callback) {
            // Call the original callback via GTK idle (thread-safe)
            Glib::signal_idle().connect_once([progress_callback, message]() {
                progress_callback(0, 2, message);  // Always 2 steps for YubiKey touches
            });
        }
    };

    // Launch background thread for password change
    std::thread([this, username, old_password, new_password, yubikey_pin, yubikey_enrolled, total_steps,
                 wrapped_completion, sync_progress_callback]() {

        // Execute synchronous password change on background thread
        // Progress callback will report:
        // Touch 1: Verify old password with YubiKey challenge-response
        // Touch 2: Combine new KEK with YubiKey challenge-response
        auto result = change_user_password(username, old_password, new_password, yubikey_pin, sync_progress_callback);

        // Report completion on GTK thread
        wrapped_completion(result);
    }).detach();

    Log::debug("VaultManager: Async password change thread launched");
}

// ============================================================================
// YubiKey Enrollment - Async Wrapper
// ============================================================================

void VaultManager::enroll_yubikey_for_user_async(
    const Glib::ustring& username,
    const Glib::ustring& password,
    const std::string& yubikey_pin,
    std::function<void(const std::string&)> progress_callback,
    std::function<void(const KeepTower::VaultResult<>&)> completion_callback) {

    Log::info("VaultManager: Starting async YubiKey enrollment for user");

    // Wrap progress callback for GTK thread
    auto wrapped_progress = [progress_callback](const std::string& message) {
        if (progress_callback) {
            Glib::signal_idle().connect_once([progress_callback, message]() {
                progress_callback(message);
            });
        }
    };

    // Wrap completion callback for GTK thread
    auto wrapped_completion = [completion_callback](KeepTower::VaultResult<> result) {
        if (completion_callback) {
            Glib::signal_idle().connect_once([completion_callback, result]() {
                completion_callback(result);
            });
        }
    };

    // Launch background thread for YubiKey enrollment
    std::thread([this, username, password, yubikey_pin, wrapped_progress, wrapped_completion]() {
        // Execute synchronous enrollment on background thread
        // Progress callback will report before each YubiKey touch:
        // Touch 1: Create FIDO2 credential
        // Touch 2: Challenge-response for authentication
        auto result = enroll_yubikey_for_user(username, password, yubikey_pin, wrapped_progress);

        // Report completion on GTK thread
        wrapped_completion(result);
    }).detach();

    Log::debug("VaultManager: Async YubiKey enrollment thread launched");
}

KeepTower::VaultResult<> VaultManager::clear_user_password_history(
    const Glib::ustring& username) {

    Log::info("VaultManager: Clearing password history for user");

    if (!m_vault_open || !m_is_v2_vault) {
        Log::error("VaultManager: No V2 vault open");
        return std::unexpected(VaultError::VaultNotOpen);
    }

    auto header_result = require_open_v2_header("clear_user_password_history");
    if (!header_result) {
        return std::unexpected(header_result.error());
    }

    KeepTower::PasswordManagementContext ctx{
        *header_result.value(), m_v2_dek, m_current_session,
        m_yubikey_service, m_modified, is_fips_enabled()};
    return KeepTower::PasswordManagementService::clear_user_password_history(ctx, username);
}

// ============================================================================
// Phase 5: Admin Password Reset
// ============================================================================

KeepTower::VaultResult<> VaultManager::admin_reset_user_password(
    const Glib::ustring& username,
    const Glib::ustring& new_temporary_password) {

    Log::info("VaultManager: Admin resetting password for user");

    if (!m_vault_open || !m_is_v2_vault) {
        Log::error("VaultManager: No V2 vault open");
        return std::unexpected(VaultError::VaultNotOpen);
    }

    auto header_result = require_open_v2_header("admin_reset_user_password");
    if (!header_result) {
        return std::unexpected(header_result.error());
    }

    KeepTower::PasswordManagementContext ctx{
        *header_result.value(), m_v2_dek, m_current_session,
        m_yubikey_service, m_modified, is_fips_enabled()};
    return KeepTower::PasswordManagementService::admin_reset_user_password(
        ctx, username, new_temporary_password);
}

// ============================================================================
// YubiKey Enrollment/Unenrollment (Phase 2) — delegated to YubiKeyEnrollmentService
// ============================================================================

KeepTower::VaultResult<> VaultManager::enroll_yubikey_for_user(
    const Glib::ustring& username,
    const Glib::ustring& password,
    const std::string& yubikey_pin,
    std::function<void(const std::string&)> progress_callback) {

    // Validate vault state
    if (!m_vault_open || !m_is_v2_vault) {
        Log::error("VaultManager: No V2 vault open");
        return std::unexpected(VaultError::VaultNotOpen);
    }

    auto header_result = require_open_v2_header("enroll_yubikey_for_user");
    if (!header_result) {
        return std::unexpected(header_result.error());
    }

    KeepTower::YubiKeyEnrollmentContext ctx{
        *header_result.value(),
        m_v2_dek,
        m_current_session,
        m_yubikey_service,
        m_modified,
        is_fips_enabled()};

    return KeepTower::YubiKeyEnrollmentService::enroll_yubikey_for_user(
        ctx, username, password, yubikey_pin, progress_callback);
}

KeepTower::VaultResult<> VaultManager::unenroll_yubikey_for_user(
    const Glib::ustring& username,
    const Glib::ustring& password,
    std::function<void(const std::string&)> progress_callback) {

    // Validate vault state
    if (!m_vault_open || !m_is_v2_vault) {
        Log::error("VaultManager: No V2 vault open");
        return std::unexpected(VaultError::VaultNotOpen);
    }

    auto header_result = require_open_v2_header("unenroll_yubikey_for_user");
    if (!header_result) {
        return std::unexpected(header_result.error());
    }

    KeepTower::YubiKeyEnrollmentContext ctx{
        *header_result.value(),
        m_v2_dek,
        m_current_session,
        m_yubikey_service,
        m_modified,
        is_fips_enabled()};

    return KeepTower::YubiKeyEnrollmentService::unenroll_yubikey_for_user(
        ctx, username, password, progress_callback);
}

// ============================================================================
// YubiKey Unenrollment - Async Wrapper
// ============================================================================

void VaultManager::unenroll_yubikey_for_user_async(
    const Glib::ustring& username,
    const Glib::ustring& password,
    std::function<void(const std::string&)> progress_callback,
    std::function<void(const KeepTower::VaultResult<>&)> completion_callback) {

    Log::info("VaultManager: Starting async YubiKey unenrollment for user");

    // Wrap progress callback for GTK thread safety
    auto wrapped_progress = [progress_callback](const std::string& message) {
        if (progress_callback) {
            Glib::signal_idle().connect_once([progress_callback, message]() {
                progress_callback(message);
            });
        }
    };

    // Wrap completion callback for GTK thread safety
    auto wrapped_completion = [completion_callback](KeepTower::VaultResult<> result) {
        if (completion_callback) {
            Glib::signal_idle().connect_once([completion_callback, result]() {
                completion_callback(result);
            });
        }
    };

    // Launch background thread for YubiKey unenrollment
    std::thread([this, username, password, wrapped_progress, wrapped_completion]() {
        // Execute synchronous unenrollment on background thread
        // Progress callback will report before YubiKey verification touch
        auto result = unenroll_yubikey_for_user(username, password, wrapped_progress);

        // Report completion on GTK thread
        wrapped_completion(result);
    }).detach();

    Log::debug("VaultManager: Async YubiKey unenrollment thread launched");
}

// ============================================================================
// Session and User Info
// ============================================================================

std::optional<UserSession> VaultManager::get_current_user_session() const {
    if (!m_vault_open || !m_is_v2_vault) {
        return std::nullopt;
    }
    return m_current_session;
}

std::vector<KeepTower::KeySlot> VaultManager::list_users() const {
    if (!m_vault_open || !m_is_v2_vault || !m_v2_header) {
        return {};
    }
    return KeySlotManager::list_active_users(m_v2_header->key_slots);
}

std::optional<KeepTower::VaultSecurityPolicy> VaultManager::get_vault_security_policy() const noexcept {
    if (!m_vault_open || !m_is_v2_vault || !m_v2_header) {
        return std::nullopt;
    }
    return m_v2_header->security_policy;
}

KeepTower::VaultResult<> VaultManager::update_security_policy(
    const KeepTower::VaultSecurityPolicy& new_policy) {

    Log::info("VaultManager: Updating security policy");

    if (!m_vault_open || !m_is_v2_vault) {
        Log::error("VaultManager: No V2 vault open");
        return std::unexpected(VaultError::VaultNotOpen);
    }

    auto header_result = require_open_v2_header("update_security_policy");
    if (!header_result) {
        return std::unexpected(header_result.error());
    }

    if (!m_current_session.has_value()) {
        Log::error("VaultManager: No active session");
        return std::unexpected(VaultError::PermissionDenied);
    }

    if (m_current_session->role != UserRole::ADMINISTRATOR) {
        Log::error("VaultManager: User '{}' is not an administrator (role: {})",
                   m_current_session->username,
                   static_cast<int>(m_current_session->role));
        return std::unexpected(VaultError::PermissionDenied);
    }

    KeepTower::SecurityPolicyContext ctx{ *header_result.value(), m_modified };
    return KeepTower::SecurityPolicyService::update_security_policy(ctx, new_policy);
}

bool VaultManager::can_view_account(size_t account_index) const noexcept {
    // Check vault is open and index is valid first
    if (!m_vault_open) {
        return false;
    }

    if (!m_account_manager) {
        return false;
    }

    const auto accounts = m_account_manager->get_all_accounts();
    if (account_index >= accounts.size()) {
        return false;
    }

    // Administrators can view all accounts
    if (m_current_session && m_current_session->role == UserRole::ADMINISTRATOR) {
        return true;
    }

    // Standard users cannot view admin-only accounts
    const auto& account = accounts[account_index];
    return !account.is_admin_only_viewable();
}

bool VaultManager::can_delete_account(size_t account_index) const noexcept {
    if (!m_vault_open) {
        // Safe permissive default: no security context means no restriction on delete.
        // The actual delete operation will fail gracefully if the vault is still closed.
        return true;
    }

    // Invalid index
    if (!m_account_manager) {
        return false;
    }

    const auto accounts = m_account_manager->get_all_accounts();
    if (account_index >= accounts.size()) {
        return false;
    }

    // Administrators can delete all accounts
    if (m_current_session && m_current_session->role == UserRole::ADMINISTRATOR) {
        return true;
    }

    // Standard users cannot delete admin-only-deletable accounts
    const auto& account = accounts[account_index];
    return !account.is_admin_only_deletable();
}
