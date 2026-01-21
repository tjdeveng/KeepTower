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
#include "crypto/VaultCrypto.h"
#include "io/VaultIO.h"
#include "KeyWrapping.h"
#include "PasswordHistory.h"
#include "services/UsernameHashService.h"
#include "services/KekDerivationService.h"
#include "../utils/Log.h"
#include "../utils/SecureMemory.h"
#include <glibmm/main.h>
#include <thread>
#include <chrono>
#include <filesystem>
#include <sys/stat.h>  // for chmod

// Using declarations for KeepTower types (VaultManager is global scope)
using KeepTower::VaultError;
using KeepTower::VaultHeaderV2;
using KeepTower::KeySlot;
using KeepTower::UserRole;
using KeepTower::UserSession;
using KeepTower::KekDerivationService;
using KeepTower::VaultSecurityPolicy;
using KeepTower::VaultFormatV2;
using KeepTower::VaultIO;
using KeepTower::KeyWrapping;
namespace Log = KeepTower::Log;

// ============================================================================
// Helper Functions for Username Hashing
// ============================================================================

/**
 * @brief Find key slot by verifying username hash
 *
 * Iterates through all active key slots and verifies the username against
 * the stored hash using constant-time comparison.
 *
 * @param slots Vector of key slots to search
 * @param username Plaintext username to find
 * @param policy Security policy containing hash algorithm setting
 * @return Pointer to matching slot (with username populated in memory), or nullptr
 */
static KeySlot* find_slot_by_username_hash(
    std::vector<KeySlot>& slots,
    const std::string& username,
    const VaultSecurityPolicy& policy) {

    auto algorithm = static_cast<KeepTower::UsernameHashService::Algorithm>(
        policy.username_hash_algorithm);

    for (auto& slot : slots) {
        if (!slot.active) {
            continue;
        }

        // Verify username hash using constant-time comparison
        std::span<const uint8_t> stored_hash(slot.username_hash.data(), slot.username_hash_size);
        bool matches = KeepTower::UsernameHashService::verify_username(
            username,
            stored_hash,
            algorithm,
            slot.username_salt);

        if (matches) {
            // Populate username in memory for UI display (NOT serialized to disk)
            slot.username = username;
            return &slot;
        }
    }

    return nullptr;
}

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

    // Create orchestrator with injected services
    return std::make_unique<KeepTower::VaultCreationOrchestrator>(
        m_crypto_service,
        m_yubikey_service,
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
    m_vault_data.Clear();  // Empty protobuf structure
    m_account_manager = std::make_unique<KeepTower::AccountManager>(m_vault_data, m_modified);
    m_group_manager = std::make_unique<KeepTower::GroupManager>(m_vault_data, m_modified);

    Log::info("VaultManager: V2 vault created successfully with admin user: {}",
              admin_username.raw());
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
    KeepTower::VaultCreationOrchestrator::ProgressCallback progress_callback,
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
        m_vault_data.Clear();
        m_account_manager = std::make_unique<KeepTower::AccountManager>(m_vault_data, m_modified);
        m_group_manager = std::make_unique<KeepTower::GroupManager>(m_vault_data, m_modified);

        Log::info("VaultManager: Async V2 vault created successfully with admin user: {}",
                  admin_username.raw());

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
    if (!VaultIO::read_file(path, file_data, true, iterations_from_file)) {
        Log::error("VaultManager: Failed to read V2 vault file: {}", path);
        return std::unexpected(VaultError::FileNotFound);
    }

    // Parse V2 header
    auto header_result = VaultFormatV2::read_header(file_data);
    if (!header_result) {
        Log::error("VaultManager: Failed to parse V2 vault header");
        return std::unexpected(header_result.error());
    }

    auto [file_header, data_offset] = header_result.value();

    // Find key slot for username using hash verification
    KeySlot* user_slot = find_slot_by_username_hash(
        file_header.vault_header.key_slots, username.raw(),
        file_header.vault_header.security_policy);

    if (!user_slot) {
        Log::error("VaultManager: No active key slot found for user: {}", username.raw());
        return std::unexpected(VaultError::AuthenticationFailed);
    }

    // Derive KEK from password using algorithm stored in KeySlot
    Log::info("VaultManager: Deriving KEK for user: {} (password length: {} bytes, {} chars, algorithm: 0x{:02x})",
              username.raw(), password.bytes(), password.length(), user_slot->kek_derivation_algorithm);

    // Convert algorithm byte to enum
    auto algorithm = static_cast<KekDerivationService::Algorithm>(user_slot->kek_derivation_algorithm);

    // Prepare algorithm parameters from security policy
    KekDerivationService::AlgorithmParameters params;
    params.pbkdf2_iterations = file_header.pbkdf2_iterations;
    params.argon2_memory_kb = file_header.vault_header.security_policy.argon2_memory_kb;
    params.argon2_time_cost = m_v2_header->security_policy.argon2_iterations;
    params.argon2_parallelism = file_header.vault_header.security_policy.argon2_parallelism;

    // Derive KEK using KekDerivationService
    auto kek_result = KekDerivationService::derive_kek(
        password.raw(),
        algorithm,
        std::span<const uint8_t>(user_slot->salt.data(), user_slot->salt.size()),
        params);
    if (!kek_result) {
        Log::error("VaultManager: Failed to derive KEK");
        return std::unexpected(VaultError::CryptoError);
    }

    std::array<uint8_t, 32> final_kek;
    std::copy(kek_result->begin(), kek_result->end(), final_kek.begin());

    // Check if this user has YubiKey enrolled
#ifdef HAVE_YUBIKEY_SUPPORT
    if (user_slot->yubikey_enrolled) {
        Log::info("VaultManager: User {} has YubiKey enrolled, requiring device",
                  username.raw());

        YubiKeyManager yk_manager;
        if (!yk_manager.initialize(is_fips_enabled())) {
            Log::error("VaultManager: Failed to initialize YubiKey");
            return std::unexpected(VaultError::YubiKeyError);
        }

        if (!yk_manager.is_yubikey_present()) {
            Log::error("VaultManager: YubiKey not present but required for user {}",
                       username.raw());
            return std::unexpected(VaultError::YubiKeyNotPresent);
        }

        // Optional: Verify YubiKey serial matches enrolled device (warning only)
        if (!user_slot->yubikey_serial.empty()) {
            auto device_info = yk_manager.get_device_info();
            if (device_info) {
                const std::string& current_serial = device_info->serial_number;
                if (current_serial != user_slot->yubikey_serial) {
                    Log::warning("VaultManager: YubiKey serial mismatch - expected: {}, got: {}",
                               user_slot->yubikey_serial, current_serial);
                    // Don't fail - serial is informational, challenge-response is the auth
                }
            }
        }

        // Decrypt stored PIN first (encrypted with password-derived KEK only)
        // This must happen BEFORE getting YubiKey response to avoid circular dependency
        std::string decrypted_pin;
        if (!user_slot->yubikey_encrypted_pin.empty()) {
            // Extract IV and ciphertext from storage (format: [IV(12) || ciphertext+tag])
            if (user_slot->yubikey_encrypted_pin.size() < KeepTower::VaultCrypto::IV_LENGTH) {
                Log::error("VaultManager: Invalid encrypted PIN format");
                return std::unexpected(VaultError::CryptoError);
            }

            std::vector<uint8_t> pin_iv(
                user_slot->yubikey_encrypted_pin.begin(),
                user_slot->yubikey_encrypted_pin.begin() + KeepTower::VaultCrypto::IV_LENGTH);

            std::vector<uint8_t> pin_ciphertext(
                user_slot->yubikey_encrypted_pin.begin() + KeepTower::VaultCrypto::IV_LENGTH,
                user_slot->yubikey_encrypted_pin.end());

            // Decrypt PIN using password-derived KEK (not yet combined with YubiKey)
            std::vector<uint8_t> pin_bytes;
            if (!KeepTower::VaultCrypto::decrypt_data(pin_ciphertext, final_kek, pin_iv, pin_bytes)) {
                Log::error("VaultManager: Failed to decrypt YubiKey PIN");
                return std::unexpected(VaultError::CryptoError);
            }

            decrypted_pin = std::string(reinterpret_cast<const char*>(pin_bytes.data()),
                                       pin_bytes.size());
            Log::info("VaultManager: Successfully decrypted YubiKey PIN from vault");
        } else {
            Log::error("VaultManager: No encrypted PIN stored in vault for user {}", username.raw());
            return std::unexpected(VaultError::YubiKeyError);
        }

        // Load credential ID if stored (required for FIDO2 assertions)
        if (!user_slot->yubikey_credential_id.empty()) {
            if (!yk_manager.set_credential(user_slot->yubikey_credential_id)) {
                Log::error("VaultManager: Failed to set FIDO2 credential ID");
                return std::unexpected(VaultError::YubiKeyError);
            }
            Log::info("VaultManager: Loaded FIDO2 credential ID ({} bytes)",
                     user_slot->yubikey_credential_id.size());
        } else {
            Log::error("VaultManager: No FIDO2 credential ID stored for user {}", username.raw());
            return std::unexpected(VaultError::YubiKeyError);
        }

        // Use user's unique challenge
        const auto& challenge = user_slot->yubikey_challenge;

        // Perform challenge-response with decrypted PIN
        const YubiKeyAlgorithm algorithm = static_cast<YubiKeyAlgorithm>(file_header.vault_header.security_policy.yubikey_algorithm);
        auto response = yk_manager.challenge_response(
            std::span<const unsigned char>(challenge.data(), challenge.size()),
            algorithm,
            false,  // don't require touch for vault access (usability)
            5000,   // 5 second timeout
            decrypted_pin  // Use decrypted PIN for authentication
        );

        if (!response.success) {
            Log::error("VaultManager: YubiKey challenge-response failed: {}",
                       response.error_message);
            return std::unexpected(VaultError::YubiKeyError);
        }

        // Combine KEK with YubiKey response (use v2 for variable-length responses)
        std::vector<uint8_t> yk_response_vec(response.get_response().begin(),
                                              response.get_response().end());
        final_kek = KeyWrapping::combine_with_yubikey_v2(final_kek, yk_response_vec);

        Log::info("VaultManager: YubiKey authentication successful for user {}",
                  username.raw());
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
    if (file_header.vault_header.security_policy.require_yubikey) {
        auto& policy_challenge = file_header.vault_header.security_policy.yubikey_challenge;
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
    if (data_offset >= file_data.size()) {
        Log::error("VaultManager: Invalid data offset: {}", data_offset);
        return std::unexpected(VaultError::CorruptedFile);
    }

    std::vector<uint8_t> ciphertext(
        file_data.begin() + data_offset,
        file_data.end());

    // Decrypt vault data
    std::vector<uint8_t> plaintext;
    std::span<const uint8_t> iv_span(file_header.data_iv);
    if (!KeepTower::VaultCrypto::decrypt_data(ciphertext, m_v2_dek, iv_span, plaintext)) {
        Log::error("VaultManager: Failed to decrypt vault data");
        return std::unexpected(VaultError::DecryptionFailed);
    }

    // Parse protobuf
    keeptower::VaultData vault_data;
    if (!vault_data.ParseFromArray(plaintext.data(), plaintext.size())) {
        Log::error("VaultManager: Failed to parse vault data");
        secure_clear(plaintext);
        return std::unexpected(VaultError::CorruptedFile);
    }
    secure_clear(plaintext);

    // Update last login timestamp
    user_slot->last_login_at = std::chrono::system_clock::now().time_since_epoch().count();

    // Initialize vault state
    m_vault_open = true;
    m_is_v2_vault = true;
    m_current_vault_path = path;
    m_v2_header = file_header.vault_header;
    m_vault_data = vault_data;
    m_modified = true;  // Mark modified to save updated last_login_at

    // Extract FEC settings from V2 header
    // Note: Header always has FEC enabled (per spec), so check data FEC setting instead
    m_use_reed_solomon = file_header.fec_redundancy_percent > 0;
    if (m_use_reed_solomon) {
        m_rs_redundancy_percent = file_header.fec_redundancy_percent;
        Log::info("VaultManager: V2 vault has data FEC enabled (redundancy: {}%)", m_rs_redundancy_percent);
    } else {
        Log::info("VaultManager: V2 vault has data FEC disabled (header FEC still enabled at 20% per spec)");
    }

    // Initialize managers after vault data is loaded
    m_account_manager = std::make_unique<KeepTower::AccountManager>(m_vault_data, m_modified);
    m_group_manager = std::make_unique<KeepTower::GroupManager>(m_vault_data, m_modified);

    // Create session
    UserSession session{
        .username = username.raw(),
        .role = user_slot->role,
        .password_change_required = user_slot->must_change_password
    };

    // Check if vault policy requires YubiKey but user doesn't have one enrolled
    if (m_v2_header->security_policy.require_yubikey && !user_slot->yubikey_enrolled) {
        session.requires_yubikey_enrollment = true;
        Log::warning("VaultManager: User '{}' must enroll YubiKey (required by policy)",
                     username.raw());
    } else {
        session.requires_yubikey_enrollment = false;
    }

    m_current_session = session;

    Log::info("VaultManager: User authenticated successfully: {}", username.raw());
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

    Log::info("VaultManager: Adding user: {}", username.raw());

    // Validate vault state
    if (!m_vault_open || !m_is_v2_vault) {
        Log::error("VaultManager: No V2 vault open");
        return std::unexpected(VaultError::VaultNotOpen);
    }

    // Check current user permissions
    if (!m_current_session || m_current_session->role != UserRole::ADMINISTRATOR) {
        Log::error("VaultManager: Only administrators can add users");
        return std::unexpected(VaultError::PermissionDenied);
    }

    // Validate username
    if (username.empty()) {
        Log::error("VaultManager: Username cannot be empty");
        return std::unexpected(VaultError::InvalidUsername);
    }

    // Check for duplicate username using hash verification
    if (find_slot_by_username_hash(m_v2_header->key_slots, username.raw(),
                                    m_v2_header->security_policy)) {
        Log::error("VaultManager: Username already exists: {}", username.raw());
        return std::unexpected(VaultError::UserAlreadyExists);
    }

    // Validate password meets policy
    if (temporary_password.length() < m_v2_header->security_policy.min_password_length) {
        Log::error("VaultManager: Password too short (min: {} chars)",
                   m_v2_header->security_policy.min_password_length);
        return std::unexpected(VaultError::WeakPassword);
    }

    // Find empty slot or add new one
    size_t slot_index = m_v2_header->key_slots.size();
    for (size_t i = 0; i < m_v2_header->key_slots.size(); ++i) {
        if (!m_v2_header->key_slots[i].active) {
            slot_index = i;
            break;
        }
    }

    if (slot_index >= VaultHeaderV2::MAX_KEY_SLOTS) {
        Log::error("VaultManager: No available key slots (max: 32)");
        return std::unexpected(VaultError::MaxUsersReached);
    }

    // Generate unique salt for new user
    auto salt_result = KeyWrapping::generate_random_salt();
    if (!salt_result) {
        Log::error("VaultManager: Failed to generate salt");
        return std::unexpected(VaultError::CryptoError);
    }

    // Derive KEK from temporary password (use vault's default algorithm - PBKDF2 for now)
    // TODO: Allow per-user algorithm selection when UI is implemented
    auto algorithm = KekDerivationService::Algorithm::PBKDF2_HMAC_SHA256;

    KekDerivationService::AlgorithmParameters params;
    params.pbkdf2_iterations = m_v2_header->security_policy.pbkdf2_iterations;
    params.argon2_memory_kb = m_v2_header->security_policy.argon2_memory_kb;
    params.argon2_time_cost = m_v2_header->security_policy.argon2_iterations;
    params.argon2_parallelism = m_v2_header->security_policy.argon2_parallelism;

    auto kek_result = KekDerivationService::derive_kek(
        temporary_password.raw(),
        algorithm,
        std::span<const uint8_t>(salt_result->data(), salt_result->size()),
        params);
    if (!kek_result) {
        Log::error("VaultManager: Failed to derive KEK");
        return std::unexpected(VaultError::CryptoError);
    }

    std::array<uint8_t, 32> kek_array;
    std::copy(kek_result->begin(), kek_result->end(), kek_array.begin());

    // Wrap vault DEK with new user's KEK
    auto wrapped_result = KeyWrapping::wrap_key(kek_array, m_v2_dek);
    if (!wrapped_result) {
        Log::error("VaultManager: Failed to wrap DEK");
        return std::unexpected(VaultError::CryptoError);
    }

    // Hash username for secure storage (following USERNAME_HASHING_SECURITY_PLAN.md)
    auto username_hash_algo = static_cast<KeepTower::UsernameHashService::Algorithm>(
        m_v2_header->security_policy.username_hash_algorithm);

    std::vector<uint8_t> username_salt_vec = KeepTower::VaultCrypto::generate_random_bytes(16);
    std::array<uint8_t, 16> username_salt{};
    std::copy_n(username_salt_vec.begin(), 16, username_salt.begin());

    auto username_hash_result = KeepTower::UsernameHashService::hash_username(
        username.raw(), username_hash_algo, username_salt);
    if (!username_hash_result) {
        Log::error("VaultManager: Failed to hash username");
        return std::unexpected(VaultError::CryptoError);
    }

    // Create new key slot
    KeySlot new_slot;
    new_slot.active = true;
    new_slot.username = username.raw();  // Keep in memory for UI (NOT serialized to disk)
    new_slot.kek_derivation_algorithm = static_cast<uint8_t>(algorithm);  // Store algorithm used

    // Copy hash from vector to array
    const auto& hash_vec = username_hash_result.value();
    std::copy_n(hash_vec.begin(), std::min(hash_vec.size(), size_t(64)), new_slot.username_hash.begin());
    new_slot.username_hash_size = static_cast<uint8_t>(hash_vec.size());
    new_slot.username_salt = username_salt;
    new_slot.salt = salt_result.value();
    new_slot.wrapped_dek = wrapped_result.value().wrapped_key;
    new_slot.role = role;
    new_slot.must_change_password = must_change_password;
    new_slot.password_changed_at = 0;  // Not yet changed
    new_slot.last_login_at = 0;

    // YubiKey enrollment if PIN provided and policy requires it
    bool yubikey_enrolled = false;
    std::array<uint8_t, 32> yubikey_challenge = {};  // HMAC-SHA256 (32 bytes)
    std::string yubikey_serial;
    std::vector<uint8_t> encrypted_pin;
    std::vector<uint8_t> credential_id;

#ifdef HAVE_YUBIKEY_SUPPORT
    if (yubikey_pin.has_value() && m_v2_header->security_policy.require_yubikey) {
        Log::info("VaultManager: Enrolling YubiKey for new user {}", username.raw());

        // Generate unique challenge for this user
        auto challenge_salt = KeyWrapping::generate_random_salt();
        if (!challenge_salt) {
            Log::error("VaultManager: Failed to generate YubiKey challenge");
            return std::unexpected(VaultError::CryptoError);
        }
        std::copy_n(challenge_salt.value().begin(), 32, yubikey_challenge.begin());  // Use all 32 bytes

        // Initialize YubiKey manager
        YubiKeyManager yk_manager;
        const bool enforce_fips = (m_v2_header->security_policy.yubikey_algorithm != 0x01);
        if (!yk_manager.initialize(enforce_fips)) {
            Log::error("VaultManager: Failed to initialize YubiKey");
            return std::unexpected(VaultError::YubiKeyError);
        }

        if (!yk_manager.is_yubikey_present()) {
            Log::error("VaultManager: YubiKey not present");
            return std::unexpected(VaultError::YubiKeyNotPresent);
        }

        // Create credential for this user (use username as identifier)
        const std::string& pin_str = yubikey_pin.value();
        auto cred_result = yk_manager.create_credential(username.raw(), pin_str.c_str());
        if (!cred_result) {
            Log::error("VaultManager: Failed to create FIDO2 credential: {}",
                      yk_manager.get_last_error());
            return std::unexpected(VaultError::YubiKeyError);
        }
        credential_id = std::move(cred_result.value());

        // Test challenge-response
        const YubiKeyAlgorithm algorithm = static_cast<YubiKeyAlgorithm>(
            m_v2_header->security_policy.yubikey_algorithm);
        auto response = yk_manager.challenge_response(
            std::span<const unsigned char>(yubikey_challenge.data(), yubikey_challenge.size()),
            algorithm, false, 5000);

        if (!response.success) {
            Log::error("VaultManager: YubiKey challenge-response failed: {}",
                      response.error_message);
            return std::unexpected(VaultError::YubiKeyError);
        }

        // Get device serial
        auto device_info = yk_manager.get_device_info();
        if (device_info) {
            yubikey_serial = device_info->serial_number;
        }

        // Encrypt PIN with user's KEK
        std::vector<uint8_t> pin_iv = KeepTower::VaultCrypto::generate_random_bytes(
            KeepTower::VaultCrypto::IV_LENGTH);
        std::vector<uint8_t> pin_bytes(pin_str.begin(), pin_str.end());
        std::vector<uint8_t> pin_ciphertext;

        if (!KeepTower::VaultCrypto::encrypt_data(pin_bytes, kek_array,
                                                   pin_ciphertext, pin_iv)) {
            Log::error("VaultManager: Failed to encrypt YubiKey PIN");
            return std::unexpected(VaultError::CryptoError);
        }

        // Store IV + ciphertext
        encrypted_pin.reserve(pin_iv.size() + pin_ciphertext.size());
        encrypted_pin.insert(encrypted_pin.end(), pin_iv.begin(), pin_iv.end());
        encrypted_pin.insert(encrypted_pin.end(), pin_ciphertext.begin(), pin_ciphertext.end());

        // Re-wrap DEK with YubiKey-enhanced KEK
        std::vector<uint8_t> yk_response_vec(response.get_response().begin(),
                                             response.get_response().end());
        auto final_kek = KeyWrapping::combine_with_yubikey_v2(kek_array, yk_response_vec);

        auto wrapped_result_yk = KeyWrapping::wrap_key(final_kek, m_v2_dek);
        if (!wrapped_result_yk) {
            Log::error("VaultManager: Failed to wrap DEK with YubiKey-enhanced KEK");
            return std::unexpected(VaultError::CryptoError);
        }
        wrapped_result = wrapped_result_yk;

        yubikey_enrolled = true;
        Log::info("VaultManager: YubiKey enrolled for user {} (FIPS: {})",
                 username.raw(), device_info && device_info->is_fips_mode ? "YES" : "NO");
    }
#endif

    // YubiKey fields: Use enrollment data if available
    new_slot.yubikey_enrolled = yubikey_enrolled;
    new_slot.yubikey_challenge = yubikey_challenge;
    new_slot.yubikey_serial = yubikey_serial;
    new_slot.yubikey_enrolled_at = yubikey_enrolled ?
        std::chrono::system_clock::now().time_since_epoch().count() : 0;
    new_slot.yubikey_encrypted_pin = std::move(encrypted_pin);
    new_slot.yubikey_credential_id = std::move(credential_id);

    // Add initial password to history if enabled
    if (m_v2_header->security_policy.password_history_depth > 0) {
        auto history_entry = KeepTower::PasswordHistory::hash_password(temporary_password);
        if (history_entry) {
            KeepTower::PasswordHistory::add_to_history(
                new_slot.password_history,
                history_entry.value(),
                m_v2_header->security_policy.password_history_depth);
            Log::debug("VaultManager: Added initial password to new user's history");
        } else {
            Log::warning("VaultManager: Failed to hash initial password for history");
        }
    }

    // Add to header
    if (slot_index < m_v2_header->key_slots.size()) {
        m_v2_header->key_slots[slot_index] = new_slot;
    } else {
        m_v2_header->key_slots.push_back(new_slot);
    }
    m_modified = true;

    Log::info("VaultManager: User added successfully: {} (role: {}, slot: {})",
              username.raw(),
              role == UserRole::ADMINISTRATOR ? "admin" : "standard",
              slot_index);
    return {};
}

KeepTower::VaultResult<> VaultManager::remove_user(const Glib::ustring& username) {
    Log::info("VaultManager: Removing user: {}", username.raw());

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

    // Find user slot using hash verification
    KeySlot* user_slot = find_slot_by_username_hash(
        m_v2_header->key_slots, username.raw(), m_v2_header->security_policy);

    if (!user_slot) {
        Log::error("VaultManager: User not found: {}", username.raw());
        return std::unexpected(VaultError::UserNotFound);
    }

    // Check if removing last administrator
    if (user_slot->role == UserRole::ADMINISTRATOR) {
        int admin_count = 0;
        for (const auto& slot : m_v2_header->key_slots) {
            if (slot.active && slot.role == UserRole::ADMINISTRATOR) {
                admin_count++;
            }
        }
        if (admin_count <= 1) {
            Log::error("VaultManager: Cannot remove last administrator");
            return std::unexpected(VaultError::LastAdministrator);
        }
    }

    // Deactivate slot (don't delete, preserve structure)
    user_slot->active = false;
    m_modified = true;

    Log::info("VaultManager: User removed successfully: {}", username.raw());
    return {};
}

KeepTower::VaultResult<> VaultManager::validate_new_password(
    const Glib::ustring& username,
    const Glib::ustring& new_password) {

    Log::debug("VaultManager: Validating new password for user: {}", username.raw());

    // Validate vault state
    if (!m_vault_open || !m_is_v2_vault) {
        Log::error("VaultManager: No V2 vault open");
        return std::unexpected(VaultError::VaultNotOpen);
    }

    // Find user slot using hash verification
    KeySlot* user_slot = find_slot_by_username_hash(
        m_v2_header->key_slots, username.raw(), m_v2_header->security_policy);

    if (!user_slot) {
        Log::error("VaultManager: User not found: {}", username.raw());
        return std::unexpected(VaultError::UserNotFound);
    }

    // Validate new password meets minimum length
    if (new_password.length() < m_v2_header->security_policy.min_password_length) {
        Log::error("VaultManager: New password too short - actual: {} chars, min: {} chars",
                   new_password.length(), m_v2_header->security_policy.min_password_length);
        return std::unexpected(VaultError::WeakPassword);
    }

    // Check password history if enabled (depth > 0)
    if (m_v2_header->security_policy.password_history_depth > 0) {
        Log::debug("VaultManager: Checking password history (depth: {})",
                   m_v2_header->security_policy.password_history_depth);

        if (KeepTower::PasswordHistory::is_password_reused(new_password, user_slot->password_history)) {
            Log::error("VaultManager: Password was used previously (reuse detected)");
            return std::unexpected(VaultError::PasswordReused);
        }

        Log::debug("VaultManager: Password not found in history (OK)");
    }

    Log::debug("VaultManager: New password validation passed");
    return {};
}

KeepTower::VaultResult<> VaultManager::change_user_password(
    const Glib::ustring& username,
    const Glib::ustring& old_password,
    const Glib::ustring& new_password,
    const std::optional<std::string>& yubikey_pin,
    std::function<void(const std::string&)> progress_callback) {

    Log::info("VaultManager: Changing password for user: {}", username.raw());

    // Validate vault state
    if (!m_vault_open || !m_is_v2_vault) {
        Log::error("VaultManager: No V2 vault open");
        return std::unexpected(VaultError::VaultNotOpen);
    }

    // Check permissions: user changing own password OR admin changing any
    bool is_self = (m_current_session && m_current_session->username == username.raw());
    bool is_admin = (m_current_session && m_current_session->role == UserRole::ADMINISTRATOR);
    if (!is_self && !is_admin) {
        Log::error("VaultManager: Permission denied for password change");
        return std::unexpected(VaultError::PermissionDenied);
    }

    // Find user slot using hash verification
    KeySlot* user_slot = find_slot_by_username_hash(
        m_v2_header->key_slots, username.raw(), m_v2_header->security_policy);

    if (!user_slot) {
        Log::error("VaultManager: User not found: {}", username.raw());
        return std::unexpected(VaultError::UserNotFound);
    }

    // Validate new password meets policy
    Log::info("VaultManager: Password length check - length: {}, bytes: {}, required: {}",
              new_password.length(), new_password.bytes(), m_v2_header->security_policy.min_password_length);
    if (new_password.length() < m_v2_header->security_policy.min_password_length) {
        Log::error("VaultManager: New password too short - actual: {} chars, min: {} chars",
                   new_password.length(), m_v2_header->security_policy.min_password_length);
        return std::unexpected(VaultError::WeakPassword);
    }

    // Check password history if enabled (depth > 0)
    if (m_v2_header->security_policy.password_history_depth > 0) {
        Log::debug("VaultManager: Checking password history (depth: {})",
                   m_v2_header->security_policy.password_history_depth);

        if (KeepTower::PasswordHistory::is_password_reused(new_password, user_slot->password_history)) {
            Log::error("VaultManager: Password was used previously (reuse detected)");
            return std::unexpected(VaultError::PasswordReused);
        }

        Log::debug("VaultManager: Password not found in history (OK)");
    }

    // Verify old password by unwrapping DEK (use algorithm from KeySlot)
    auto old_algorithm = static_cast<KekDerivationService::Algorithm>(user_slot->kek_derivation_algorithm);

    KekDerivationService::AlgorithmParameters params;
    params.pbkdf2_iterations = m_v2_header->security_policy.pbkdf2_iterations;
    params.argon2_memory_kb = m_v2_header->security_policy.argon2_memory_kb;
    params.argon2_time_cost = m_v2_header->security_policy.argon2_iterations;
    params.argon2_parallelism = m_v2_header->security_policy.argon2_parallelism;

    auto old_kek_result = KekDerivationService::derive_kek(
        old_password.raw(),
        old_algorithm,
        std::span<const uint8_t>(user_slot->salt.data(), user_slot->salt.size()),
        params);
    if (!old_kek_result) {
        Log::error("VaultManager: Failed to derive old KEK");
        return std::unexpected(VaultError::CryptoError);
    }

    std::array<uint8_t, 32> old_kek_array;
    std::copy(old_kek_result->begin(), old_kek_result->end(), old_kek_array.begin());

    std::array<uint8_t, 32> old_final_kek = old_kek_array;

#ifdef HAVE_YUBIKEY_SUPPORT
    // If user has YubiKey enrolled, verify with YubiKey
    if (user_slot->yubikey_enrolled) {
        Log::info("VaultManager: User has YubiKey enrolled, verifying with YubiKey");

        YubiKeyManager yk_manager;
        if (!yk_manager.initialize(is_fips_enabled())) {
            Log::error("VaultManager: Failed to initialize YubiKey subsystem");
            return std::unexpected(VaultError::YubiKeyError);
        }

        if (!yk_manager.is_yubikey_present()) {
            Log::error("VaultManager: YubiKey required but not detected");
            return std::unexpected(VaultError::YubiKeyNotPresent);
        }

        // Decrypt stored PIN using old password-derived KEK
        std::string decrypted_pin;
        if (!user_slot->yubikey_encrypted_pin.empty()) {
            if (user_slot->yubikey_encrypted_pin.size() < KeepTower::VaultCrypto::IV_LENGTH) {
                Log::error("VaultManager: Invalid encrypted PIN format");
                return std::unexpected(VaultError::CryptoError);
            }

            std::vector<uint8_t> pin_iv(
                user_slot->yubikey_encrypted_pin.begin(),
                user_slot->yubikey_encrypted_pin.begin() + KeepTower::VaultCrypto::IV_LENGTH);

            std::vector<uint8_t> pin_ciphertext(
                user_slot->yubikey_encrypted_pin.begin() + KeepTower::VaultCrypto::IV_LENGTH,
                user_slot->yubikey_encrypted_pin.end());

            std::vector<uint8_t> pin_bytes;
            if (!KeepTower::VaultCrypto::decrypt_data(pin_ciphertext, old_final_kek, pin_iv, pin_bytes)) {
                Log::error("VaultManager: Failed to decrypt stored PIN with old password");
                return std::unexpected(VaultError::CryptoError);
            }

            decrypted_pin = std::string(reinterpret_cast<const char*>(pin_bytes.data()), pin_bytes.size());
            Log::info("VaultManager: Successfully decrypted stored PIN");
        } else if (yubikey_pin.has_value()) {
            // User provided PIN (first password change after vault creation)
            decrypted_pin = yubikey_pin.value();
            Log::info("VaultManager: Using provided PIN");
        } else {
            Log::error("VaultManager: YubiKey enrolled but no PIN available");
            return std::unexpected(VaultError::YubiKeyError);
        }

        // Load credential ID
        if (!user_slot->yubikey_credential_id.empty()) {
            if (!yk_manager.set_credential(user_slot->yubikey_credential_id)) {
                Log::error("VaultManager: Failed to set FIDO2 credential ID");
                return std::unexpected(VaultError::YubiKeyError);
            }
        }

        // Report progress before first touch
        if (progress_callback) {
            progress_callback("Touch 1 of 2: Verifying old password with YubiKey...");
        }

        // Use user's enrolled challenge
        const auto& challenge = user_slot->yubikey_challenge;
        const YubiKeyAlgorithm algorithm = static_cast<YubiKeyAlgorithm>(m_v2_header->security_policy.yubikey_algorithm);
        auto response = yk_manager.challenge_response(
            std::span<const unsigned char>(challenge.data(), challenge.size()),
            algorithm,
            false,
            5000,
            decrypted_pin);  // Pass decrypted PIN

        if (!response.success) {
            Log::error("VaultManager: YubiKey challenge-response failed: {}",
                       response.error_message);
            return std::unexpected(VaultError::YubiKeyError);
        }

        // Combine KEK with YubiKey response (use v2 for variable-length responses)
        std::vector<uint8_t> yk_response_vec(response.get_response().begin(),
                                              response.get_response().end());
        old_final_kek = KeyWrapping::combine_with_yubikey_v2(old_final_kek, yk_response_vec);

        Log::info("VaultManager: Old password verified with YubiKey");
    }
#endif

    auto verify_unwrap = KeyWrapping::unwrap_key(
        old_final_kek,
        user_slot->wrapped_dek);
    if (!verify_unwrap) {
        Log::error("VaultManager: Old password verification failed");
        return std::unexpected(VaultError::AuthenticationFailed);
    }

    // Generate new salt for new password
    auto new_salt_result = KeyWrapping::generate_random_salt();
    if (!new_salt_result) {
        Log::error("VaultManager: Failed to generate new salt");
        return std::unexpected(VaultError::CryptoError);
    }

    // Derive new KEK (keep same algorithm as old KEK for consistency)
    auto new_kek_result = KekDerivationService::derive_kek(
        new_password.raw(),
        old_algorithm,  // Use same algorithm
        std::span<const uint8_t>(new_salt_result->data(), new_salt_result->size()),
        params);
    if (!new_kek_result) {
        Log::error("VaultManager: Failed to derive new KEK");
        return std::unexpected(VaultError::CryptoError);
    }

    std::array<uint8_t, 32> new_final_kek;
    std::copy(new_kek_result->begin(), new_kek_result->end(), new_final_kek.begin());

#ifdef HAVE_YUBIKEY_SUPPORT
    // If user has YubiKey enrolled, combine new KEK with SAME YubiKey challenge and re-encrypt PIN
    if (user_slot->yubikey_enrolled) {
        Log::info("VaultManager: Preserving YubiKey enrollment with new password");

        YubiKeyManager yk_manager;
        if (!yk_manager.initialize(is_fips_enabled())) {
            Log::error("VaultManager: Failed to initialize YubiKey subsystem");
            return std::unexpected(VaultError::YubiKeyError);
        }

        if (!yk_manager.is_yubikey_present()) {
            Log::error("VaultManager: YubiKey required but not detected");
            return std::unexpected(VaultError::YubiKeyNotPresent);
        }

        // Get PIN (either decrypted from old or provided by user)
        std::string pin_to_use;
        if (!user_slot->yubikey_encrypted_pin.empty()) {
            // Decrypt with OLD KEK (before combining with YubiKey)
            std::vector<uint8_t> pin_iv(
                user_slot->yubikey_encrypted_pin.begin(),
                user_slot->yubikey_encrypted_pin.begin() + KeepTower::VaultCrypto::IV_LENGTH);

            std::vector<uint8_t> pin_ciphertext(
                user_slot->yubikey_encrypted_pin.begin() + KeepTower::VaultCrypto::IV_LENGTH,
                user_slot->yubikey_encrypted_pin.end());

            std::vector<uint8_t> pin_bytes;
            if (!KeepTower::VaultCrypto::decrypt_data(pin_ciphertext, old_kek_array, pin_iv, pin_bytes)) {
                Log::error("VaultManager: Failed to decrypt PIN");
                return std::unexpected(VaultError::CryptoError);
            }

            pin_to_use = std::string(reinterpret_cast<const char*>(pin_bytes.data()), pin_bytes.size());
        } else if (yubikey_pin.has_value()) {
            pin_to_use = yubikey_pin.value();
        } else {
            Log::error("VaultManager: YubiKey enrolled but no PIN available");
            return std::unexpected(VaultError::YubiKeyError);
        }

        // Load credential ID
        if (!user_slot->yubikey_credential_id.empty()) {
            if (!yk_manager.set_credential(user_slot->yubikey_credential_id)) {
                Log::error("VaultManager: Failed to set FIDO2 credential ID");
                return std::unexpected(VaultError::YubiKeyError);
            }
        }

        // Report progress before second touch
        if (progress_callback) {
            progress_callback("Touch 2 of 2: Combining new password with YubiKey...");
        }

        // Use SAME challenge as before (don't regenerate!)
        const auto& challenge = user_slot->yubikey_challenge;
        const YubiKeyAlgorithm algorithm = static_cast<YubiKeyAlgorithm>(m_v2_header->security_policy.yubikey_algorithm);
        auto response = yk_manager.challenge_response(
            std::span<const unsigned char>(challenge.data(), challenge.size()),
            algorithm,
            false,
            5000,
            pin_to_use);  // Pass PIN

        if (!response.success) {
            Log::error("VaultManager: YubiKey challenge-response failed: {}",
                       response.error_message);
            return std::unexpected(VaultError::YubiKeyError);
        }

        // Combine new KEK with YubiKey response (use v2 for variable-length)
        std::vector<uint8_t> yk_response_vec(response.get_response().begin(),
                                              response.get_response().end());
        new_final_kek = KeyWrapping::combine_with_yubikey_v2(new_final_kek, yk_response_vec);

        // Re-encrypt PIN with NEW password-derived KEK (before YubiKey combination)
        std::vector<uint8_t> new_encrypted_pin;
        std::vector<uint8_t> new_pin_iv = KeepTower::VaultCrypto::generate_random_bytes(
            KeepTower::VaultCrypto::IV_LENGTH);

        std::vector<uint8_t> pin_bytes(pin_to_use.begin(), pin_to_use.end());
        if (!KeepTower::VaultCrypto::encrypt_data(pin_bytes, new_final_kek, new_encrypted_pin, new_pin_iv)) {
            Log::error("VaultManager: Failed to re-encrypt PIN with new password");
            return std::unexpected(VaultError::CryptoError);
        }

        // Store re-encrypted PIN
        std::vector<uint8_t> new_pin_storage;
        new_pin_storage.reserve(new_pin_iv.size() + new_encrypted_pin.size());
        new_pin_storage.insert(new_pin_storage.end(), new_pin_iv.begin(), new_pin_iv.end());
        new_pin_storage.insert(new_pin_storage.end(), new_encrypted_pin.begin(), new_encrypted_pin.end());
        user_slot->yubikey_encrypted_pin = std::move(new_pin_storage);

        Log::info("VaultManager: YubiKey enrollment preserved and PIN re-encrypted with new password");
    }
#endif

    // Wrap DEK with new KEK (with optional YubiKey)
    auto new_wrapped_result = KeyWrapping::wrap_key(new_final_kek, m_v2_dek);
    if (!new_wrapped_result) {
        Log::error("VaultManager: Failed to wrap DEK with new KEK");
        return std::unexpected(VaultError::CryptoError);
    }

    // Update slot
    user_slot->salt = new_salt_result.value();
    user_slot->wrapped_dek = new_wrapped_result.value().wrapped_key;
    user_slot->must_change_password = false;
    user_slot->password_changed_at = std::chrono::system_clock::now().time_since_epoch().count();

    // Add new password to history if enabled
    if (m_v2_header->security_policy.password_history_depth > 0) {
        auto history_entry = KeepTower::PasswordHistory::hash_password(new_password);
        if (history_entry) {
            KeepTower::PasswordHistory::add_to_history(
                user_slot->password_history,
                history_entry.value(),
                m_v2_header->security_policy.password_history_depth);
            Log::debug("VaultManager: Added password to history (size: {})",
                       user_slot->password_history.size());
        } else {
            Log::warning("VaultManager: Failed to hash password for history");
        }
    }

    m_modified = true;

    // Update session if user changed own password
    if (is_self && m_current_session) {
        m_current_session->password_change_required = false;
    }

    Log::info("VaultManager: Password changed successfully for user: {}", username.raw());
    return {};
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

    Log::info("VaultManager: Starting async password change for user: {}", username.raw());

    // Determine if YubiKey is enrolled for this user (affects step count)
    bool yubikey_enrolled = false;
    int total_steps = 1;  // Minimum: password change without YubiKey

    if (m_vault_open && m_is_v2_vault) {
        KeySlot* slot = find_slot_by_username_hash(
            m_v2_header->key_slots, username.raw(), m_v2_header->security_policy);
        if (slot && slot->yubikey_enrolled) {
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

    Log::info("VaultManager: Starting async YubiKey enrollment for user: {}", username.raw());

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

    Log::info("VaultManager: Clearing password history for user: {}", username.raw());

    // Validate vault state
    if (!m_vault_open || !m_is_v2_vault) {
        Log::error("VaultManager: No V2 vault open");
        return std::unexpected(VaultError::VaultNotOpen);
    }

    // Check permissions: user clearing own history OR admin clearing any
    bool is_self = (m_current_session && m_current_session->username == username.raw());
    bool is_admin = (m_current_session && m_current_session->role == UserRole::ADMINISTRATOR);
    if (!is_self && !is_admin) {
        Log::error("VaultManager: Permission denied for clearing password history");
        return std::unexpected(VaultError::PermissionDenied);
    }

    // Find user slot using hash verification
    KeySlot* user_slot = find_slot_by_username_hash(
        m_v2_header->key_slots, username.raw(), m_v2_header->security_policy);

    if (!user_slot) {
        Log::error("VaultManager: User not found: {}", username.raw());
        return std::unexpected(VaultError::UserNotFound);
    }

    // Clear password history
    size_t old_size = user_slot->password_history.size();
    user_slot->password_history.clear();

    m_modified = true;

    Log::info("VaultManager: Cleared {} password history entries for user: {}",
              old_size, username.raw());
    return {};
}

// ============================================================================
// Phase 5: Admin Password Reset
// ============================================================================

KeepTower::VaultResult<> VaultManager::admin_reset_user_password(
    const Glib::ustring& username,
    const Glib::ustring& new_temporary_password) {

    Log::info("VaultManager: Admin resetting password for user: {}", username.raw());

    // Validate vault state
    if (!m_vault_open || !m_is_v2_vault) {
        Log::error("VaultManager: No V2 vault open");
        return std::unexpected(VaultError::VaultNotOpen);
    }

    // Check admin permissions
    if (!m_current_session || m_current_session->role != UserRole::ADMINISTRATOR) {
        Log::error("VaultManager: Admin permission required for password reset");
        return std::unexpected(VaultError::PermissionDenied);
    }

    // Prevent admin from resetting own password (must use change_user_password)
    if (m_current_session->username == username.raw()) {
        Log::error("VaultManager: Cannot reset own password (use change password instead)");
        return std::unexpected(VaultError::PermissionDenied);
    }

    // Find user slot using hash verification
    KeySlot* user_slot = find_slot_by_username_hash(
        m_v2_header->key_slots, username.raw(), m_v2_header->security_policy);

    if (!user_slot) {
        Log::error("VaultManager: User not found: {}", username.raw());
        return std::unexpected(VaultError::UserNotFound);
    }

    // Validate new password meets policy
    if (new_temporary_password.length() < m_v2_header->security_policy.min_password_length) {
        Log::error("VaultManager: New password too short (min: {} chars)",
                   m_v2_header->security_policy.min_password_length);
        return std::unexpected(VaultError::WeakPassword);
    }

    // Generate new salt for new password
    auto new_salt_result = KeyWrapping::generate_random_salt();
    if (!new_salt_result) {
        Log::error("VaultManager: Failed to generate new salt");
        return std::unexpected(VaultError::CryptoError);
    }

    // Derive new KEK from temporary password (use same algorithm as user's current algorithm)
    auto user_algorithm = static_cast<KekDerivationService::Algorithm>(user_slot->kek_derivation_algorithm);

    KekDerivationService::AlgorithmParameters params;
    params.pbkdf2_iterations = m_v2_header->security_policy.pbkdf2_iterations;
    params.argon2_memory_kb = m_v2_header->security_policy.argon2_memory_kb;
    params.argon2_time_cost = m_v2_header->security_policy.argon2_iterations;
    params.argon2_parallelism = m_v2_header->security_policy.argon2_parallelism;

    auto new_kek_result = KekDerivationService::derive_kek(
        new_temporary_password.raw(),
        user_algorithm,
        std::span<const uint8_t>(new_salt_result->data(), new_salt_result->size()),
        params);
    if (!new_kek_result) {
        Log::error("VaultManager: Failed to derive new KEK");
        return std::unexpected(VaultError::CryptoError);
    }

    std::array<uint8_t, 32> new_kek_array;
    std::copy(new_kek_result->begin(), new_kek_result->end(), new_kek_array.begin());

    // Wrap DEK with new KEK (password-only, no YubiKey)
    auto new_wrapped_result = KeyWrapping::wrap_key(new_kek_array, m_v2_dek);
    if (!new_wrapped_result) {
        Log::error("VaultManager: Failed to wrap DEK with new KEK");
        return std::unexpected(VaultError::CryptoError);
    }

    // Update slot with new wrapped key and force password change
    user_slot->salt = new_salt_result.value();
    user_slot->wrapped_dek = new_wrapped_result.value().wrapped_key;
    user_slot->must_change_password = true;  // Force password change on next login
    user_slot->password_changed_at = 0;  // Reset to indicate temporary password

    // Clear password history (admin reset = fresh start)
    user_slot->password_history.clear();
    Log::debug("VaultManager: Cleared password history for reset user");

    // IMPORTANT: Unenroll YubiKey if enrolled
    // Admin doesn't have user's YubiKey device, so reset to password-only
    if (user_slot->yubikey_enrolled) {
        Log::info("VaultManager: Unenrolling YubiKey for user '{}' (admin reset)", username.raw());
        user_slot->yubikey_enrolled = false;
        user_slot->yubikey_challenge = {};
        user_slot->yubikey_serial.clear();
        user_slot->yubikey_enrolled_at = 0;

        // If vault policy requires YubiKey, user will need to re-enroll after password change
        if (m_v2_header->security_policy.require_yubikey) {
            Log::info("VaultManager: User will need to re-enroll YubiKey (required by policy)");
        }
    }

    m_modified = true;

    Log::info("VaultManager: Password reset successfully for user: {}", username.raw());
    Log::info("VaultManager: User will be required to change password on next login");
    return {};
}

// ============================================================================
// YubiKey Enrollment/Unenrollment (Phase 2)
// ============================================================================

KeepTower::VaultResult<> VaultManager::enroll_yubikey_for_user(
    const Glib::ustring& username,
    const Glib::ustring& password,
    const std::string& yubikey_pin,
    std::function<void(const std::string&)> progress_callback) {

    Log::info("VaultManager: Enrolling YubiKey for user: {}", username.raw());

    // Validate vault state
    if (!m_vault_open || !m_is_v2_vault) {
        Log::error("VaultManager: No V2 vault open");
        return std::unexpected(VaultError::VaultNotOpen);
    }

    // Validate YubiKey PIN (4-63 characters as per YubiKey spec)
    if (yubikey_pin.empty() || yubikey_pin.length() < 4 || yubikey_pin.length() > 63) {
        Log::error("VaultManager: Invalid YubiKey PIN length (must be 4-63 characters)");
        return std::unexpected(VaultError::YubiKeyError);
    }

    // Check permissions: user enrolling own YubiKey OR admin enrolling for any user
    bool is_self = (m_current_session && m_current_session->username == username.raw());
    bool is_admin = (m_current_session && m_current_session->role == UserRole::ADMINISTRATOR);
    if (!is_self && !is_admin) {
        Log::error("VaultManager: Permission denied for YubiKey enrollment");
        return std::unexpected(VaultError::PermissionDenied);
    }

    // Find user slot using hash verification
    KeySlot* user_slot = find_slot_by_username_hash(
        m_v2_header->key_slots, username.raw(), m_v2_header->security_policy);

    if (!user_slot) {
        Log::error("VaultManager: User not found: {}", username.raw());
        return std::unexpected(VaultError::UserNotFound);
    }

    // Check if already enrolled
    if (user_slot->yubikey_enrolled) {
        Log::error("VaultManager: User already has YubiKey enrolled");
        return std::unexpected(VaultError::YubiKeyError);
    }

#ifdef HAVE_YUBIKEY_SUPPORT
    // Initialize YubiKey subsystem
    YubiKeyManager yk_manager;
    if (!yk_manager.initialize(is_fips_enabled())) {
        Log::error("VaultManager: Failed to initialize YubiKey subsystem");
        return std::unexpected(VaultError::YubiKeyError);
    }

    // Verify YubiKey present
    if (!yk_manager.is_yubikey_present()) {
        Log::error("VaultManager: No YubiKey detected");
        return std::unexpected(VaultError::YubiKeyNotPresent);
    }

    // Verify password by unwrapping DEK with password-only KEK (use user's algorithm)
    auto user_algorithm = static_cast<KekDerivationService::Algorithm>(user_slot->kek_derivation_algorithm);

    KekDerivationService::AlgorithmParameters params;
    params.pbkdf2_iterations = m_v2_header->security_policy.pbkdf2_iterations;
    params.argon2_memory_kb = m_v2_header->security_policy.argon2_memory_kb;
    params.argon2_time_cost = m_v2_header->security_policy.argon2_iterations;
    params.argon2_parallelism = m_v2_header->security_policy.argon2_parallelism;

    auto kek_result = KekDerivationService::derive_kek(
        password.raw(),
        user_algorithm,
        std::span<const uint8_t>(user_slot->salt.data(), user_slot->salt.size()),
        params);
    if (!kek_result) {
        Log::error("VaultManager: Failed to derive KEK");
        return std::unexpected(VaultError::CryptoError);
    }

    std::array<uint8_t, 32> kek_array;
    std::copy(kek_result->begin(), kek_result->end(), kek_array.begin());

    auto verify_unwrap = KeyWrapping::unwrap_key(
        kek_array,
        user_slot->wrapped_dek);
    if (!verify_unwrap) {
        Log::error("VaultManager: Password verification failed");
        return std::unexpected(VaultError::AuthenticationFailed);
    }

    // Generate unique 20-byte challenge for this user (from first 20 bytes of new salt)
    auto challenge_salt = KeyWrapping::generate_random_salt();
    if (!challenge_salt) {
        Log::error("VaultManager: Failed to generate challenge salt");
        return std::unexpected(VaultError::CryptoError);
    }

    std::array<uint8_t, 20> user_challenge;
    std::copy_n(challenge_salt->begin(), 20, user_challenge.begin());

    // Perform YubiKey challenge-response (require touch = true for enrollment security)
    Log::info("VaultManager: Performing YubiKey challenge-response (touch required)");
    const YubiKeyAlgorithm algorithm = static_cast<YubiKeyAlgorithm>(m_v2_header->security_policy.yubikey_algorithm);

    // Create FIDO2 credential for enrollment (required for FIDO2 hmac-secret extension)
    Log::info("VaultManager: Creating FIDO2 credential for enrollment");
    if (progress_callback) {
        progress_callback("Touch 1 of 2: Creating YubiKey credential to verify user presence");
    }
    auto credential_id = yk_manager.create_credential(username.raw(), yubikey_pin);
    if (!credential_id.has_value() || credential_id->empty()) {
        Log::error("VaultManager: Failed to create FIDO2 credential");
        return std::unexpected(VaultError::YubiKeyError);
    }
    Log::info("VaultManager: FIDO2 credential created (ID length: {})", credential_id->size());

    // Perform challenge-response with the newly created credential
    Log::info("VaultManager: Performing challenge-response for user authentication");
    if (progress_callback) {
        progress_callback("Touch 2 of 2: Generating cryptographic response for authentication");
    }
    auto response = yk_manager.challenge_response(
        user_challenge,
        algorithm,
        true,
        15000,
        yubikey_pin  // Pass PIN for challenge-response
    );
    if (!response.success) {
        Log::error("VaultManager: YubiKey challenge-response failed: {}",
                   response.error_message);
        return std::unexpected(VaultError::YubiKeyError);
    }

    // Get device serial for audit trail
    std::string device_serial;
    auto device_info = yk_manager.get_device_info();
    if (device_info) {
        device_serial = device_info->serial_number;
        Log::info("VaultManager: YubiKey serial: {}", device_serial);
    }

    // Combine KEK with YubiKey response (use v2 for variable-length responses)
    std::vector<uint8_t> yk_response_vec(response.get_response().begin(),
                                          response.get_response().end());
    auto final_kek = KeyWrapping::combine_with_yubikey_v2(kek_array, yk_response_vec);

    // Re-wrap DEK with password+YubiKey combined KEK
    auto new_wrapped_result = KeyWrapping::wrap_key(final_kek, m_v2_dek);
    if (!new_wrapped_result) {
        Log::error("VaultManager: Failed to wrap DEK with combined KEK");
        return std::unexpected(VaultError::CryptoError);
    }

    // Encrypt YubiKey PIN with password-derived KEK (NOT combined KEK)
    // This allows us to decrypt the PIN with password alone during vault opening
    Log::info("VaultManager: Encrypting YubiKey PIN");
    std::vector<uint8_t> pin_bytes(yubikey_pin.begin(), yubikey_pin.end());
    std::vector<uint8_t> encrypted_pin;
    std::array<uint8_t, 12> pin_iv;

    if (!KeepTower::VaultCrypto::encrypt_data(pin_bytes, kek_array, encrypted_pin, pin_iv)) {
        Log::error("VaultManager: Failed to encrypt YubiKey PIN");
        return std::unexpected(VaultError::CryptoError);
    }

    // Store IV + ciphertext in KeySlot (format: [IV(12) || ciphertext+tag])
    std::vector<uint8_t> pin_storage;
    pin_storage.reserve(pin_iv.size() + encrypted_pin.size());
    pin_storage.insert(pin_storage.end(), pin_iv.begin(), pin_iv.end());
    pin_storage.insert(pin_storage.end(), encrypted_pin.begin(), encrypted_pin.end());

    Log::info("VaultManager: YubiKey PIN encrypted ({} bytes)", pin_storage.size());

    // Update slot with YubiKey enrollment data
    user_slot->wrapped_dek = new_wrapped_result.value().wrapped_key;
    user_slot->yubikey_enrolled = true;
    std::copy_n(user_challenge.begin(), 20, user_slot->yubikey_challenge.begin());
    user_slot->yubikey_serial = device_serial;
    user_slot->yubikey_enrolled_at = std::chrono::system_clock::now().time_since_epoch().count();
    user_slot->yubikey_encrypted_pin = pin_storage;

    // Store credential ID from FIDO2 enrollment
    user_slot->yubikey_credential_id = *credential_id;
    Log::info("VaultManager: Stored FIDO2 credential ID ({} bytes)", credential_id->size());

    // Mark vault as modified so the new wrapped_dek gets saved
    m_modified = true;

    // Update current session if user enrolled their own YubiKey
    if (m_current_session && m_current_session->username == username.raw()) {
        m_current_session->requires_yubikey_enrollment = false;
        Log::info("VaultManager: Updated session for user '{}' - YubiKey enrollment complete", username.raw());
    }

    Log::info("VaultManager: YubiKey enrolled successfully for user: {}", username.raw());
    return {};
#else
    Log::error("VaultManager: YubiKey support not compiled in");
    return std::unexpected(VaultError::YubiKeyError);
#endif
}

KeepTower::VaultResult<> VaultManager::unenroll_yubikey_for_user(
    const Glib::ustring& username,
    const Glib::ustring& password,
    std::function<void(const std::string&)> progress_callback) {

    Log::info("VaultManager: Unenrolling YubiKey for user: {}", username.raw());

    // Validate vault state
    if (!m_vault_open || !m_is_v2_vault) {
        Log::error("VaultManager: No V2 vault open");
        return std::unexpected(VaultError::VaultNotOpen);
    }

    // Check permissions: user unenrolling own YubiKey OR admin unenrolling for any user
    bool is_self = (m_current_session && m_current_session->username == username.raw());
    bool is_admin = (m_current_session && m_current_session->role == UserRole::ADMINISTRATOR);
    if (!is_self && !is_admin) {
        Log::error("VaultManager: Permission denied for YubiKey unenrollment");
        return std::unexpected(VaultError::PermissionDenied);
    }

    // Find user slot using hash verification
    KeySlot* user_slot = find_slot_by_username_hash(
        m_v2_header->key_slots, username.raw(), m_v2_header->security_policy);

    if (!user_slot) {
        Log::error("VaultManager: User not found: {}", username.raw());
        return std::unexpected(VaultError::UserNotFound);
    }

    // Check if YubiKey is enrolled
    if (!user_slot->yubikey_enrolled) {
        Log::error("VaultManager: User does not have YubiKey enrolled");
        return std::unexpected(VaultError::YubiKeyError);
    }

#ifdef HAVE_YUBIKEY_SUPPORT
    // Initialize YubiKey subsystem
    YubiKeyManager yk_manager;
    if (!yk_manager.initialize(is_fips_enabled())) {
        Log::error("VaultManager: Failed to initialize YubiKey subsystem");
        return std::unexpected(VaultError::YubiKeyError);
    }

    // Verify YubiKey present
    if (!yk_manager.is_yubikey_present()) {
        Log::error("VaultManager: YubiKey required but not detected");
        return std::unexpected(VaultError::YubiKeyNotPresent);
    }

    // Report progress before YubiKey verification touch
    if (progress_callback) {
        progress_callback("Verifying current password with YubiKey (touch required)...");
    }

    // Verify password+YubiKey by unwrapping DEK (use user's algorithm)
    auto user_algorithm = static_cast<KekDerivationService::Algorithm>(user_slot->kek_derivation_algorithm);

    KekDerivationService::AlgorithmParameters params;
    params.pbkdf2_iterations = m_v2_header->security_policy.pbkdf2_iterations;
    params.argon2_memory_kb = m_v2_header->security_policy.argon2_memory_kb;
    params.argon2_time_cost = m_v2_header->security_policy.argon2_iterations;
    params.argon2_parallelism = m_v2_header->security_policy.argon2_parallelism;

    auto kek_result = KekDerivationService::derive_kek(
        password.raw(),
        user_algorithm,
        std::span<const uint8_t>(user_slot->salt.data(), user_slot->salt.size()),
        params);
    if (!kek_result) {
        Log::error("VaultManager: Failed to derive KEK");
        return std::unexpected(VaultError::CryptoError);
    }

    // Use user's enrolled challenge
    std::array<uint8_t, 20> user_challenge;
    std::copy_n(user_slot->yubikey_challenge.begin(), 20, user_challenge.begin());

    const YubiKeyAlgorithm algorithm = static_cast<YubiKeyAlgorithm>(m_v2_header->security_policy.yubikey_algorithm);
    auto response = yk_manager.challenge_response(user_challenge, algorithm, false, 5000);
    if (!response.success) {
        Log::error("VaultManager: YubiKey challenge-response failed: {}",
                   response.error_message);
        return std::unexpected(VaultError::YubiKeyError);
    }

    // Combine KEK with YubiKey response for verification
    std::array<uint8_t, 32> kek_array;
    std::copy(kek_result->begin(), kek_result->end(), kek_array.begin());

    std::array<uint8_t, 20> yk_response_array;
    std::copy_n(response.response.begin(), 20, yk_response_array.begin());
    auto current_kek = KeyWrapping::combine_with_yubikey(kek_array, yk_response_array);

    auto verify_unwrap = KeyWrapping::unwrap_key(current_kek, user_slot->wrapped_dek);
    if (!verify_unwrap) {
        Log::error("VaultManager: Password+YubiKey verification failed");
        return std::unexpected(VaultError::AuthenticationFailed);
    }

    // Generate new salt for password-only KEK
    auto new_salt_result = KeyWrapping::generate_random_salt();
    if (!new_salt_result) {
        Log::error("VaultManager: Failed to generate new salt");
        return std::unexpected(VaultError::CryptoError);
    }

    // Derive password-only KEK (no YubiKey combination, use same algorithm as before)
    auto new_kek_result = KekDerivationService::derive_kek(
        password.raw(),
        user_algorithm,
        std::span<const uint8_t>(new_salt_result->data(), new_salt_result->size()),
        params);
    if (!new_kek_result) {
        Log::error("VaultManager: Failed to derive new KEK");
        return std::unexpected(VaultError::CryptoError);
    }

    std::array<uint8_t, 32> new_kek_array;
    std::copy(new_kek_result->begin(), new_kek_result->end(), new_kek_array.begin());

    // Re-wrap DEK with password-only KEK
    auto new_wrapped_result = KeyWrapping::wrap_key(new_kek_array, m_v2_dek);
    if (!new_wrapped_result) {
        Log::error("VaultManager: Failed to wrap DEK with new KEK");
        return std::unexpected(VaultError::CryptoError);
    }

    // Update slot: remove YubiKey enrollment, use password-only
    user_slot->salt = new_salt_result.value();
    user_slot->wrapped_dek = new_wrapped_result.value().wrapped_key;
    user_slot->yubikey_enrolled = false;
    user_slot->yubikey_challenge = {};  // Clear challenge
    user_slot->yubikey_serial.clear();
    user_slot->yubikey_enrolled_at = 0;
    m_modified = true;

    // Update current session if user unenrolled their own YubiKey
    if (m_current_session && m_current_session->username == username.raw()) {
        // Check if re-enrollment will be required by policy
        if (m_v2_header->security_policy.require_yubikey) {
            m_current_session->requires_yubikey_enrollment = true;
            Log::info("VaultManager: Updated session for user '{}' - YubiKey re-enrollment required by policy", username.raw());
        }
    }

    Log::info("VaultManager: YubiKey unenrolled successfully for user: {}", username.raw());
    return {};
#else
    Log::error("VaultManager: YubiKey support not compiled in");
    return std::unexpected(VaultError::YubiKeyError);
#endif
}

// ============================================================================
// YubiKey Unenrollment - Async Wrapper
// ============================================================================

void VaultManager::unenroll_yubikey_for_user_async(
    const Glib::ustring& username,
    const Glib::ustring& password,
    std::function<void(const std::string&)> progress_callback,
    std::function<void(const KeepTower::VaultResult<>&)> completion_callback) {

    Log::info("VaultManager: Starting async YubiKey unenrollment for user: {}", username.raw());

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
    std::vector<KeySlot> active_users;
    if (!m_vault_open || !m_is_v2_vault || !m_v2_header) {
        return active_users;
    }

    for (const auto& slot : m_v2_header->key_slots) {
        if (slot.active) {
            active_users.push_back(slot);
        }
    }

    return active_users;
}

std::optional<KeepTower::VaultSecurityPolicy> VaultManager::get_vault_security_policy() const noexcept {
    if (!m_vault_open || !m_is_v2_vault || !m_v2_header) {
        return std::nullopt;
    }
    return m_v2_header->security_policy;
}

bool VaultManager::can_view_account(size_t account_index) const noexcept {
    // Check vault is open and index is valid first
    if (!m_vault_open) {
        return false;
    }

    const auto& accounts = get_all_accounts();
    if (account_index >= accounts.size()) {
        return false;
    }

    // V1 vaults have no access control beyond bounds checking
    if (!m_is_v2_vault) {
        return true;
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
    // V1 vaults have no access control
    if (!m_is_v2_vault || !m_vault_open) {
        return true;
    }

    // Invalid index
    const auto& accounts = get_all_accounts();
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

KeepTower::VaultResult<> VaultManager::convert_v1_to_v2(
    const Glib::ustring& admin_username,
    const Glib::ustring& admin_password,
    const KeepTower::VaultSecurityPolicy& policy)
{
    // Validation: Must have V1 vault open
    if (!m_vault_open) {
        return std::unexpected(KeepTower::VaultError::VaultNotOpen);
    }

    if (m_is_v2_vault) {
        return std::unexpected(KeepTower::VaultError::PermissionDenied);
    }

    // Validate admin credentials
    if (admin_username.empty() || admin_username.length() < 3 || admin_username.length() > 32) {
        return std::unexpected(KeepTower::VaultError::InvalidUsername);
    }

    if (admin_password.empty() || admin_password.length() < policy.min_password_length) {
        return std::unexpected(KeepTower::VaultError::WeakPassword);
    }

    // Save current vault path and extract all accounts
    std::string old_vault_path = m_current_vault_path;
    std::vector<keeptower::AccountRecord> v1_accounts = get_all_accounts();

    KeepTower::Log::info("Migrating V1 vault: {} accounts", v1_accounts.size());

    // Create backup before migration
    std::string backup_path = old_vault_path + ".v1.backup";
    try {
        std::filesystem::copy_file(
            old_vault_path,
            backup_path,
            std::filesystem::copy_options::overwrite_existing
        );
        KeepTower::Log::info("Created V1 backup: {}", backup_path);
    } catch (const std::exception& e) {
        KeepTower::Log::error("Failed to create backup: {}", e.what());
        return std::unexpected(KeepTower::VaultError::FileWriteError);
    }

    // Close V1 vault
    close_vault();

    // Create new V2 vault with same path (overwrites V1)
    auto create_result = create_vault_v2(old_vault_path, admin_username, admin_password, policy);
    if (!create_result) {
        KeepTower::Log::error("Failed to create V2 vault during migration");
        // Restore from backup
        try {
            std::filesystem::copy_file(
                backup_path,
                old_vault_path,
                std::filesystem::copy_options::overwrite_existing
            );
            KeepTower::Log::info("Restored V1 vault from backup after failed migration");
        } catch (const std::exception& e) {
            KeepTower::Log::error("Failed to restore backup: {}", e.what());
        }
        return std::unexpected(create_result.error());
    }

    // Open newly created V2 vault
    auto open_result = open_vault_v2(old_vault_path, admin_username, admin_password);
    if (!open_result) {
        KeepTower::Log::error("Failed to open V2 vault after migration");
        return std::unexpected(open_result.error());
    }

    // Import all V1 accounts into V2 vault
    for (auto& account : v1_accounts) {
        // Preserve all account data including IDs and timestamps
        if (!add_account(account)) {
            KeepTower::Log::warning("Failed to add account during migration: {}", account.account_name());
        }
    }

    // Save V2 vault with migrated data
    if (!save_vault()) {
        KeepTower::Log::error("Failed to save V2 vault after importing accounts");
        return std::unexpected(KeepTower::VaultError::FileWriteError);
    }

    KeepTower::Log::info("Successfully migrated V1 vault to V2 format");
    KeepTower::Log::info("Administrator account: {}", admin_username.raw());
    KeepTower::Log::info("Migrated {} accounts", v1_accounts.size());

    return {};
}
