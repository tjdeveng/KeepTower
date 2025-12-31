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
#include "KeyWrapping.h"
#include "PasswordHistory.h"
#include "../utils/Log.h"
#include "../utils/SecureMemory.h"
#include <chrono>
#include <filesystem>
#include <sys/stat.h>  // for chmod

// Using declarations for KeepTower types (VaultManager is global scope)
using KeepTower::VaultError;
using KeepTower::VaultHeaderV2;
using KeepTower::KeySlot;
using KeepTower::UserRole;
using KeepTower::UserSession;
using KeepTower::VaultSecurityPolicy;
using KeepTower::VaultFormatV2;
using KeepTower::KeyWrapping;
namespace Log = KeepTower::Log;

// ============================================================================
// V2 Vault Creation
// ============================================================================

KeepTower::VaultResult<> VaultManager::create_vault_v2(
    const std::string& path,
    const Glib::ustring& admin_username,
    const Glib::ustring& admin_password,
    const KeepTower::VaultSecurityPolicy& policy) {

    Log::info("VaultManager: Creating V2 vault at: {}", path);

    // Close any open vault
    if (m_vault_open) {
        if (!close_vault()) {
            Log::error("VaultManager: Failed to close existing vault");
            return std::unexpected(VaultError::VaultAlreadyOpen);
        }
    }

    // Validate inputs
    if (admin_username.empty()) {
        Log::error("VaultManager: Admin username cannot be empty");
        return std::unexpected(VaultError::InvalidUsername);
    }

    if (admin_password.length() < policy.min_password_length) {
        Log::error("VaultManager: Admin password too short (min: {} chars)",
                   policy.min_password_length);
        return std::unexpected(VaultError::WeakPassword);
    }

    // Generate Data Encryption Key (DEK) for vault
    auto dek_result = KeyWrapping::generate_random_dek();
    if (!dek_result) {
        Log::error("VaultManager: Failed to generate DEK");
        return std::unexpected(VaultError::CryptoError);
    }
    m_v2_dek = dek_result.value();

    // FIPS-140-3: Lock DEK in memory to prevent swap exposure
    if (lock_memory(m_v2_dek.data(), m_v2_dek.size())) {
        Log::debug("VaultManager: Locked V2 DEK in memory");
    } else {
        Log::warning("VaultManager: Failed to lock V2 DEK - continuing without memory lock");
    }

    // Generate unique salt for admin user
    auto salt_result = KeyWrapping::generate_random_salt();
    if (!salt_result) {
        Log::error("VaultManager: Failed to generate salt");
        return std::unexpected(VaultError::CryptoError);
    }

    // Derive KEK from admin password
    Log::info("VaultManager: Deriving KEK for admin user (password length: {} bytes, {} chars)",
              admin_password.bytes(), admin_password.length());
    auto kek_result = KeyWrapping::derive_kek_from_password(
        admin_password,
        salt_result.value(),
        policy.pbkdf2_iterations);
    if (!kek_result) {
        Log::error("VaultManager: Failed to derive KEK from admin password");
        return std::unexpected(VaultError::CryptoError);
    }

    std::array<uint8_t, 32> final_kek = kek_result.value();

    // YubiKey enrollment for admin (if required by policy)
    bool admin_yubikey_enrolled = false;
    std::array<uint8_t, 20> admin_yubikey_challenge = {};
    std::string admin_yubikey_serial;

#ifdef HAVE_YUBIKEY_SUPPORT
    if (policy.require_yubikey) {
        Log::info("VaultManager: YubiKey required by policy, enrolling admin's YubiKey");

        // Generate unique challenge for admin
        auto challenge_salt = KeyWrapping::generate_random_salt();
        if (!challenge_salt) {
            Log::error("VaultManager: Failed to generate YubiKey challenge");
            return std::unexpected(VaultError::CryptoError);
        }

        // Use first 20 bytes as challenge (HMAC-SHA1 challenge-response size)
        std::copy_n(challenge_salt.value().begin(), 20, admin_yubikey_challenge.begin());

        // Initialize YubiKey manager
        YubiKeyManager yk_manager;
        if (!yk_manager.initialize()) {
            Log::error("VaultManager: Failed to initialize YubiKey");
            return std::unexpected(VaultError::YubiKeyError);
        }

        if (!yk_manager.is_yubikey_present()) {
            Log::error("VaultManager: YubiKey not present but required by policy");
            return std::unexpected(VaultError::YubiKeyNotPresent);
        }

        // Perform challenge-response
        auto response = yk_manager.challenge_response(
            std::span<const unsigned char>(admin_yubikey_challenge.data(),
                                          admin_yubikey_challenge.size()),
            false,  // don't require touch for vault operations (usability)
            5000    // 5 second timeout
        );

        if (!response.success) {
            Log::error("VaultManager: YubiKey challenge-response failed: {}",
                       response.error_message);
            return std::unexpected(VaultError::YubiKeyError);
        }

        // Get device serial for audit trail
        auto device_info = yk_manager.get_device_info();
        if (device_info) {
            admin_yubikey_serial = device_info->serial_number;
            Log::info("VaultManager: Admin YubiKey enrolled with serial: {}",
                      admin_yubikey_serial);
        }

        // Combine KEK with YubiKey response (XOR first 20 bytes)
        std::array<uint8_t, 20> yk_response_array;
        std::copy_n(response.response.begin(), 20, yk_response_array.begin());
        final_kek = KeyWrapping::combine_with_yubikey(final_kek, yk_response_array);

        admin_yubikey_enrolled = true;
        Log::info("VaultManager: Admin KEK combined with YubiKey response");
    }
#else
    if (policy.require_yubikey) {
        Log::error("VaultManager: YubiKey support not compiled in");
        return std::unexpected(VaultError::YubiKeyError);
    }
#endif

    // Wrap DEK with admin's final KEK (password or password+YubiKey)
    auto wrapped_result = KeyWrapping::wrap_key(final_kek, m_v2_dek);
    if (!wrapped_result) {
        Log::error("VaultManager: Failed to wrap DEK with admin KEK");
        return std::unexpected(VaultError::CryptoError);
    }

    // Create admin key slot
    KeySlot admin_slot;
    admin_slot.active = true;
    admin_slot.username = admin_username.raw();
    admin_slot.salt = salt_result.value();
    admin_slot.wrapped_dek = wrapped_result.value().wrapped_key;
    admin_slot.role = UserRole::ADMINISTRATOR;
    admin_slot.must_change_password = false;  // Admin sets own password
    admin_slot.password_changed_at = std::chrono::system_clock::now().time_since_epoch().count();
    admin_slot.last_login_at = 0;
    // YubiKey enrollment data
    admin_slot.yubikey_enrolled = admin_yubikey_enrolled;
    admin_slot.yubikey_challenge = admin_yubikey_challenge;
    admin_slot.yubikey_serial = admin_yubikey_serial;
    admin_slot.yubikey_enrolled_at = admin_yubikey_enrolled ?
        std::chrono::system_clock::now().time_since_epoch().count() : 0;

    // Add admin password to history if enabled
    if (policy.password_history_depth > 0) {
        auto history_entry = KeepTower::PasswordHistory::hash_password(admin_password);
        if (history_entry) {
            KeepTower::PasswordHistory::add_to_history(
                admin_slot.password_history,
                history_entry.value(),
                policy.password_history_depth);
            Log::debug("VaultManager: Added admin password to initial history");
        } else {
            Log::warning("VaultManager: Failed to hash admin password for history");
        }
    }

    // Create vault header
    VaultHeaderV2 header;
    header.security_policy = policy;
    header.key_slots.push_back(admin_slot);

    // Create empty vault data
    keeptower::VaultData vault_data;
    // No accounts yet, just empty structure

    // Serialize vault data
    std::string serialized_data;
    if (!vault_data.SerializeToString(&serialized_data)) {
        Log::error("VaultManager: Failed to serialize vault data");
        return std::unexpected(VaultError::SerializationFailed);
    }

    // Encrypt vault data with DEK
    std::vector<uint8_t> plaintext(serialized_data.begin(), serialized_data.end());
    std::vector<uint8_t> ciphertext;
    std::vector<uint8_t> data_iv = KeepTower::VaultCrypto::generate_random_bytes(KeepTower::VaultCrypto::IV_LENGTH);

    if (!KeepTower::VaultCrypto::encrypt_data(plaintext, m_v2_dek, ciphertext, data_iv)) {
        Log::error("VaultManager: Failed to encrypt vault data");
        secure_clear(plaintext);
        return std::unexpected(VaultError::CryptoError);
    }
    secure_clear(plaintext);

    // Write V2 file format
    VaultFormatV2::V2FileHeader file_header;
    file_header.pbkdf2_iterations = policy.pbkdf2_iterations;
    file_header.vault_header = header;
    std::copy(data_iv.begin(), std::min(data_iv.begin() + 32, data_iv.end()), file_header.data_salt.begin());
    std::copy(data_iv.begin(), std::min(data_iv.begin() + 12, data_iv.end()), file_header.data_iv.begin());

    // Write header with FEC (use default user FEC preference, or 0% for new vault)
    auto write_result = VaultFormatV2::write_header(file_header, 0);  // 0% = use 20% minimum
    if (!write_result) {
        Log::error("VaultManager: Failed to write V2 header");
        return std::unexpected(write_result.error());
    }

    // Combine header + encrypted data
    std::vector<uint8_t> file_data = write_result.value();
    file_data.insert(file_data.end(), ciphertext.begin(), ciphertext.end());

    // Write to file
    if (!KeepTower::VaultIO::write_file(path, file_data, true, 0)) {
        Log::error("VaultManager: Failed to write vault file");
        return std::unexpected(VaultError::FileWriteError);
    }

    // Set secure file permissions (owner read/write only)
#ifdef __linux__
    chmod(path.c_str(), 0600);
#endif

    // Initialize vault state
    m_vault_open = true;
    m_is_v2_vault = true;
    m_current_vault_path = path;
    m_v2_header = header;
    m_current_session = UserSession{
        .username = admin_username.raw(),
        .role = UserRole::ADMINISTRATOR,
        .password_change_required = false
    };
    m_modified = false;

    // Initialize managers after vault data is loaded
    m_account_manager = std::make_unique<KeepTower::AccountManager>(m_vault_data, m_modified);
    m_group_manager = std::make_unique<KeepTower::GroupManager>(m_vault_data, m_modified);

    Log::info("VaultManager: V2 vault created successfully with admin user: {}",
              admin_username.raw());
    return {};
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

    // Read vault file
    std::vector<uint8_t> file_data;
    if (!KeepTower::VaultIO::read_file(path, file_data, true, m_pbkdf2_iterations)) {
        Log::error("VaultManager: Failed to read vault file");
        return std::unexpected(VaultError::FileReadError);
    }

    // Detect format version
    auto version_result = VaultFormatV2::detect_version(file_data);
    if (!version_result) {
        Log::error("VaultManager: Failed to detect vault version");
        return std::unexpected(version_result.error());
    }

    if (version_result.value() != 2) {
        Log::error("VaultManager: Not a V2 vault (version: {})", version_result.value());
        return std::unexpected(VaultError::UnsupportedVersion);
    }

    // Read V2 header with FEC recovery
    auto header_result = VaultFormatV2::read_header(file_data);
    if (!header_result) {
        Log::error("VaultManager: Failed to read V2 header");
        return std::unexpected(header_result.error());
    }

    auto [file_header, data_offset] = header_result.value();

    // Find key slot for username
    KeySlot* user_slot = nullptr;
    for (auto& slot : file_header.vault_header.key_slots) {
        if (slot.active && slot.username == username.raw()) {
            user_slot = &slot;
            break;
        }
    }

    if (!user_slot) {
        Log::error("VaultManager: No active key slot found for user: {}", username.raw());
        return std::unexpected(VaultError::AuthenticationFailed);
    }

    // Derive KEK from password
    Log::info("VaultManager: Deriving KEK for user: {} (password length: {} bytes, {} chars)",
              username.raw(), password.bytes(), password.length());
    auto kek_result = KeyWrapping::derive_kek_from_password(
        password,
        user_slot->salt,
        file_header.pbkdf2_iterations);
    if (!kek_result) {
        Log::error("VaultManager: Failed to derive KEK");
        return std::unexpected(VaultError::CryptoError);
    }

    std::array<uint8_t, 32> final_kek = kek_result.value();

    // Check if this user has YubiKey enrolled
#ifdef HAVE_YUBIKEY_SUPPORT
    if (user_slot->yubikey_enrolled) {
        Log::info("VaultManager: User {} has YubiKey enrolled, requiring device",
                  username.raw());

        YubiKeyManager yk_manager;
        if (!yk_manager.initialize()) {
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

        // Use user's unique challenge
        const auto& challenge = user_slot->yubikey_challenge;

        auto response = yk_manager.challenge_response(
            std::span<const unsigned char>(challenge.data(), challenge.size()),
            false,  // don't require touch for vault access (usability)
            5000    // 5 second timeout
        );

        if (!response.success) {
            Log::error("VaultManager: YubiKey challenge-response failed: {}",
                       response.error_message);
            return std::unexpected(VaultError::YubiKeyError);
        }

        // Combine KEK with YubiKey response
        std::array<uint8_t, 20> yk_response_array;
        std::copy_n(response.response.begin(), 20, yk_response_array.begin());
        final_kek = KeyWrapping::combine_with_yubikey(final_kek, yk_response_array);

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
    bool must_change_password) {

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

    // Check for duplicate username
    for (const auto& slot : m_v2_header->key_slots) {
        if (slot.active && slot.username == username.raw()) {
            Log::error("VaultManager: Username already exists: {}", username.raw());
            return std::unexpected(VaultError::UserAlreadyExists);
        }
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

    // Derive KEK from temporary password
    auto kek_result = KeyWrapping::derive_kek_from_password(
        temporary_password,
        salt_result.value(),
        m_v2_header->security_policy.pbkdf2_iterations);
    if (!kek_result) {
        Log::error("VaultManager: Failed to derive KEK");
        return std::unexpected(VaultError::CryptoError);
    }

    // Wrap vault DEK with new user's KEK
    auto wrapped_result = KeyWrapping::wrap_key(kek_result.value(), m_v2_dek);
    if (!wrapped_result) {
        Log::error("VaultManager: Failed to wrap DEK");
        return std::unexpected(VaultError::CryptoError);
    }

    // Create new key slot
    KeySlot new_slot;
    new_slot.active = true;
    new_slot.username = username.raw();
    new_slot.salt = salt_result.value();
    new_slot.wrapped_dek = wrapped_result.value().wrapped_key;
    new_slot.role = role;
    new_slot.must_change_password = must_change_password;
    new_slot.password_changed_at = 0;  // Not yet changed
    new_slot.last_login_at = 0;

    // YubiKey fields: Explicitly NOT enrolled
    // Admin cannot enroll YubiKey for user (doesn't have physical device)
    // User must enroll own YubiKey during first login if policy requires
    new_slot.yubikey_enrolled = false;
    new_slot.yubikey_challenge = {};
    new_slot.yubikey_serial.clear();
    new_slot.yubikey_enrolled_at = 0;

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

    // Find user slot
    KeySlot* user_slot = nullptr;
    for (auto& slot : m_v2_header->key_slots) {
        if (slot.active && slot.username == username.raw()) {
            user_slot = &slot;
            break;
        }
    }

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

    // Find user slot
    KeySlot* user_slot = nullptr;
    for (auto& slot : m_v2_header->key_slots) {
        if (slot.active && slot.username == username.raw()) {
            user_slot = &slot;
            break;
        }
    }

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
    const Glib::ustring& new_password) {

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

    // Find user slot
    KeySlot* user_slot = nullptr;
    for (auto& slot : m_v2_header->key_slots) {
        if (slot.active && slot.username == username.raw()) {
            user_slot = &slot;
            break;
        }
    }

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

    // Verify old password by unwrapping DEK
    auto old_kek_result = KeyWrapping::derive_kek_from_password(
        old_password,
        user_slot->salt,
        m_v2_header->security_policy.pbkdf2_iterations);
    if (!old_kek_result) {
        Log::error("VaultManager: Failed to derive old KEK");
        return std::unexpected(VaultError::CryptoError);
    }

    std::array<uint8_t, 32> old_final_kek = old_kek_result.value();

#ifdef HAVE_YUBIKEY_SUPPORT
    // If user has YubiKey enrolled, verify with YubiKey
    if (user_slot->yubikey_enrolled) {
        Log::info("VaultManager: User has YubiKey enrolled, verifying with YubiKey");

        YubiKeyManager yk_manager;
        if (!yk_manager.initialize()) {
            Log::error("VaultManager: Failed to initialize YubiKey subsystem");
            return std::unexpected(VaultError::YubiKeyError);
        }

        if (!yk_manager.is_yubikey_present()) {
            Log::error("VaultManager: YubiKey required but not detected");
            return std::unexpected(VaultError::YubiKeyNotPresent);
        }

        // Use user's enrolled challenge
        std::array<uint8_t, 20> user_challenge;
        std::copy_n(user_slot->yubikey_challenge.begin(), 20, user_challenge.begin());

        auto response = yk_manager.challenge_response(user_challenge, false, 5000);
        if (!response.success) {
            Log::error("VaultManager: YubiKey challenge-response failed: {}",
                       response.error_message);
            return std::unexpected(VaultError::YubiKeyError);
        }

        // Combine KEK with YubiKey response
        std::array<uint8_t, 20> yk_response_array;
        std::copy_n(response.response.begin(), 20, yk_response_array.begin());
        old_final_kek = KeyWrapping::combine_with_yubikey(old_final_kek, yk_response_array);

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

    // Derive new KEK
    auto new_kek_result = KeyWrapping::derive_kek_from_password(
        new_password,
        new_salt_result.value(),
        m_v2_header->security_policy.pbkdf2_iterations);
    if (!new_kek_result) {
        Log::error("VaultManager: Failed to derive new KEK");
        return std::unexpected(VaultError::CryptoError);
    }

    std::array<uint8_t, 32> new_final_kek = new_kek_result.value();

#ifdef HAVE_YUBIKEY_SUPPORT
    // If user has YubiKey enrolled, combine new KEK with SAME YubiKey challenge
    if (user_slot->yubikey_enrolled) {
        Log::info("VaultManager: Preserving YubiKey enrollment with new password");

        YubiKeyManager yk_manager;
        if (!yk_manager.initialize()) {
            Log::error("VaultManager: Failed to initialize YubiKey subsystem");
            return std::unexpected(VaultError::YubiKeyError);
        }

        if (!yk_manager.is_yubikey_present()) {
            Log::error("VaultManager: YubiKey required but not detected");
            return std::unexpected(VaultError::YubiKeyNotPresent);
        }

        // Use SAME challenge as before (don't regenerate!)
        std::array<uint8_t, 20> user_challenge;
        std::copy_n(user_slot->yubikey_challenge.begin(), 20, user_challenge.begin());

        auto response = yk_manager.challenge_response(user_challenge, false, 5000);
        if (!response.success) {
            Log::error("VaultManager: YubiKey challenge-response failed: {}",
                       response.error_message);
            return std::unexpected(VaultError::YubiKeyError);
        }

        // Combine new KEK with YubiKey response
        std::array<uint8_t, 20> yk_response_array;
        std::copy_n(response.response.begin(), 20, yk_response_array.begin());
        new_final_kek = KeyWrapping::combine_with_yubikey(new_final_kek, yk_response_array);

        Log::info("VaultManager: YubiKey enrollment preserved with new password");
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

    // Find user slot
    KeySlot* user_slot = nullptr;
    for (auto& slot : m_v2_header->key_slots) {
        if (slot.active && slot.username == username.raw()) {
            user_slot = &slot;
            break;
        }
    }

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

    // Find user slot
    KeySlot* user_slot = nullptr;
    for (auto& slot : m_v2_header->key_slots) {
        if (slot.active && slot.username == username.raw()) {
            user_slot = &slot;
            break;
        }
    }

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

    // Derive new KEK from temporary password
    auto new_kek_result = KeyWrapping::derive_kek_from_password(
        new_temporary_password,
        new_salt_result.value(),
        m_v2_header->security_policy.pbkdf2_iterations);
    if (!new_kek_result) {
        Log::error("VaultManager: Failed to derive new KEK");
        return std::unexpected(VaultError::CryptoError);
    }

    // Wrap DEK with new KEK (password-only, no YubiKey)
    auto new_wrapped_result = KeyWrapping::wrap_key(new_kek_result.value(), m_v2_dek);
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
    const Glib::ustring& password) {

    Log::info("VaultManager: Enrolling YubiKey for user: {}", username.raw());

    // Validate vault state
    if (!m_vault_open || !m_is_v2_vault) {
        Log::error("VaultManager: No V2 vault open");
        return std::unexpected(VaultError::VaultNotOpen);
    }

    // Check permissions: user enrolling own YubiKey OR admin enrolling for any user
    bool is_self = (m_current_session && m_current_session->username == username.raw());
    bool is_admin = (m_current_session && m_current_session->role == UserRole::ADMINISTRATOR);
    if (!is_self && !is_admin) {
        Log::error("VaultManager: Permission denied for YubiKey enrollment");
        return std::unexpected(VaultError::PermissionDenied);
    }

    // Find user slot
    KeySlot* user_slot = nullptr;
    for (auto& slot : m_v2_header->key_slots) {
        if (slot.active && slot.username == username.raw()) {
            user_slot = &slot;
            break;
        }
    }

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
    if (!yk_manager.initialize()) {
        Log::error("VaultManager: Failed to initialize YubiKey subsystem");
        return std::unexpected(VaultError::YubiKeyError);
    }

    // Verify YubiKey present
    if (!yk_manager.is_yubikey_present()) {
        Log::error("VaultManager: No YubiKey detected");
        return std::unexpected(VaultError::YubiKeyNotPresent);
    }

    // Verify password by unwrapping DEK with password-only KEK
    auto kek_result = KeyWrapping::derive_kek_from_password(
        password,
        user_slot->salt,
        m_v2_header->security_policy.pbkdf2_iterations);
    if (!kek_result) {
        Log::error("VaultManager: Failed to derive KEK");
        return std::unexpected(VaultError::CryptoError);
    }

    auto verify_unwrap = KeyWrapping::unwrap_key(
        kek_result.value(),
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
    auto response = yk_manager.challenge_response(user_challenge, true, 15000);
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

    // Combine KEK with YubiKey response
    std::array<uint8_t, 20> yk_response_array;
    std::copy_n(response.response.begin(), 20, yk_response_array.begin());
    auto final_kek = KeyWrapping::combine_with_yubikey(kek_result.value(), yk_response_array);

    // Re-wrap DEK with password+YubiKey combined KEK
    auto new_wrapped_result = KeyWrapping::wrap_key(final_kek, m_v2_dek);
    if (!new_wrapped_result) {
        Log::error("VaultManager: Failed to wrap DEK with combined KEK");
        return std::unexpected(VaultError::CryptoError);
    }

    // Update slot with YubiKey enrollment data
    user_slot->wrapped_dek = new_wrapped_result.value().wrapped_key;
    user_slot->yubikey_enrolled = true;
    std::copy_n(user_challenge.begin(), 20, user_slot->yubikey_challenge.begin());
    user_slot->yubikey_serial = device_serial;
    user_slot->yubikey_enrolled_at = std::chrono::system_clock::now().time_since_epoch().count();
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
    const Glib::ustring& password) {

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

    // Find user slot
    KeySlot* user_slot = nullptr;
    for (auto& slot : m_v2_header->key_slots) {
        if (slot.active && slot.username == username.raw()) {
            user_slot = &slot;
            break;
        }
    }

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
    if (!yk_manager.initialize()) {
        Log::error("VaultManager: Failed to initialize YubiKey subsystem");
        return std::unexpected(VaultError::YubiKeyError);
    }

    // Verify YubiKey present
    if (!yk_manager.is_yubikey_present()) {
        Log::error("VaultManager: YubiKey required but not detected");
        return std::unexpected(VaultError::YubiKeyNotPresent);
    }

    // Verify password+YubiKey by unwrapping DEK
    auto kek_result = KeyWrapping::derive_kek_from_password(
        password,
        user_slot->salt,
        m_v2_header->security_policy.pbkdf2_iterations);
    if (!kek_result) {
        Log::error("VaultManager: Failed to derive KEK");
        return std::unexpected(VaultError::CryptoError);
    }

    // Use user's enrolled challenge
    std::array<uint8_t, 20> user_challenge;
    std::copy_n(user_slot->yubikey_challenge.begin(), 20, user_challenge.begin());

    auto response = yk_manager.challenge_response(user_challenge, false, 5000);
    if (!response.success) {
        Log::error("VaultManager: YubiKey challenge-response failed: {}",
                   response.error_message);
        return std::unexpected(VaultError::YubiKeyError);
    }

    // Combine KEK with YubiKey response for verification
    std::array<uint8_t, 20> yk_response_array;
    std::copy_n(response.response.begin(), 20, yk_response_array.begin());
    auto current_kek = KeyWrapping::combine_with_yubikey(kek_result.value(), yk_response_array);

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

    // Derive password-only KEK (no YubiKey combination)
    auto new_kek_result = KeyWrapping::derive_kek_from_password(
        password,
        new_salt_result.value(),
        m_v2_header->security_policy.pbkdf2_iterations);
    if (!new_kek_result) {
        Log::error("VaultManager: Failed to derive new KEK");
        return std::unexpected(VaultError::CryptoError);
    }

    // Re-wrap DEK with password-only KEK
    auto new_wrapped_result = KeyWrapping::wrap_key(new_kek_result.value(), m_v2_dek);
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
