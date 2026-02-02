// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file VaultManager.h
 * @brief Secure password vault management with AES-256-GCM encryption
 *
 * This file contains the VaultManager class which handles encrypted storage
 * of password records using modern cryptographic standards.
 */

#ifndef VAULTMANAGER_H
#define VAULTMANAGER_H

#include <string>
#include <string_view>
#include <vector>
#include <span>
#include <memory>
#include <expected>
#include <optional>
#include <mutex>
#include <atomic>
#include <glibmm.h>
#include "record.pb.h"
#include "VaultError.h"
#include "ReedSolomon.h"
#include "MultiUserTypes.h"
#include "VaultFormatV2.h"
#include "crypto/VaultCrypto.h"
#include "io/VaultIO.h"
#include "serialization/VaultSerialization.h"
#include "format/VaultFormat.h"
#include "managers/AccountManager.h"
#include "managers/GroupManager.h"

// Phase 2 Day 5: VaultCreationOrchestrator integration
#include "controllers/VaultCreationOrchestrator.h"
#include "services/VaultCryptoService.h"
#include "services/VaultYubiKeyService.h"
#include "services/VaultFileService.h"

// Forward declare for conditional compilation
#if __has_include("config.h")
#include "config.h"
#endif

#ifdef HAVE_YUBIKEY_SUPPORT
#include "managers/YubiKeyManager.h"
#endif

// Forward declarations for OpenSSL types
/** @brief OpenSSL cipher context structure (opaque type) */
typedef struct evp_cipher_ctx_st EVP_CIPHER_CTX;

/**
 * @brief RAII wrapper for OpenSSL cipher context
 *
 * Provides automatic resource management for EVP_CIPHER_CTX to prevent
 * memory leaks and ensure proper cleanup in exception scenarios.
 */
class EVPCipherContext {
public:
    EVPCipherContext();
    ~EVPCipherContext();

    // Delete copy and move operations
    EVPCipherContext(const EVPCipherContext&) = delete;
    EVPCipherContext& operator=(const EVPCipherContext&) = delete;
    EVPCipherContext(EVPCipherContext&&) = delete;
    EVPCipherContext& operator=(EVPCipherContext&&) = delete;

    /**
     * @brief Get the underlying OpenSSL context pointer
     * @return Raw pointer to EVP_CIPHER_CTX
     */
    EVP_CIPHER_CTX* get() noexcept { return ctx_; }

    /**
     * @brief Check if the context is valid
     * @return true if context was successfully created, false otherwise
     */
    [[nodiscard]] bool is_valid() const noexcept { return ctx_ != nullptr; }

private:
    EVP_CIPHER_CTX* ctx_;
};

/**
 * @brief Manages encrypted password vaults with AES-256-GCM encryption
 *
 * VaultManager provides secure storage and retrieval of password records
 * using industry-standard encryption and key derivation.
 *
 * @section features Features
 * - AES-256-GCM authenticated encryption
 * - PBKDF2-SHA256 key derivation (100,000 iterations default)
 * - Atomic file operations with automatic backups
 * - Memory protection with mlock() and secure erasure
 * - File format versioning for future compatibility
 *
 * @section security Security
 * - Encryption keys derived from user password using PBKDF2
 * - Random salt (32 bytes) per vault
 * - Random IV (12 bytes) per encryption operation
 * - Authentication tags verify data integrity
 * - Sensitive data cleared with OPENSSL_cleanse()
 *
 * @section usage Usage Example
 * @code
 * VaultManager vm;
 *
 * // Create new vault
 * if (!vm.create_vault("/path/to/vault.vault", "strong_password")) {
 *     // Handle error
 * }
 *
 * // Add account
 * keeptower::AccountRecord account;
 * account.set_account_name("Example");
 * account.set_user_name("user@example.com");
 * vm.add_account(account);
 *
 * // Save and close
 * vm.save_vault();
 * vm.close_vault();
 * @endcode
 */
class VaultManager {
public:
    /** @name Vault File Format Constants
     * @brief Protocol constants defining vault file structure
     * @{
     */

    /** @brief Reed-Solomon error correction enabled flag (bit 0) */
    static constexpr uint8_t FLAG_RS_ENABLED = 0x01;

    /** @brief YubiKey authentication required flag (bit 1) */
    static constexpr uint8_t FLAG_YUBIKEY_REQUIRED = 0x02;

    /** @brief PBKDF2 salt length in bytes (256 bits) */
    static constexpr size_t SALT_LENGTH = 32;

    /** @brief AES-256 key length in bytes (256 bits) */
    static constexpr size_t KEY_LENGTH = 32;

    /** @brief AES-GCM initialization vector length in bytes (96 bits, GCM recommended) */
    static constexpr size_t IV_LENGTH = 12;

    /** @brief Default PBKDF2 iteration count (NIST recommendation: ≥100,000) */
    static constexpr int DEFAULT_PBKDF2_ITERATIONS = 100000;

    /** @brief YubiKey HMAC-SHA1 challenge size in bytes */
    static constexpr size_t YUBIKEY_CHALLENGE_SIZE = 64;

    /** @brief YubiKey HMAC-SHA1 response size in bytes */
    static constexpr size_t YUBIKEY_RESPONSE_SIZE = 20;

    /** @brief YubiKey operation timeout in milliseconds (15 seconds) */
    static constexpr int YUBIKEY_TIMEOUT_MS = 15000;

    /** @brief Default number of backup files to maintain per vault */
    static constexpr int DEFAULT_BACKUP_COUNT = 5;

    /** @brief Default Reed-Solomon redundancy percentage */
    static constexpr int DEFAULT_RS_REDUNDANCY = 10;

    /** @brief Vault header size: flags(1) + redundancy(1) + original_size(4) bytes */
    static constexpr size_t VAULT_HEADER_SIZE = 6;

    /** @brief Minimum Reed-Solomon redundancy percentage (5%) */
    static constexpr uint8_t MIN_RS_REDUNDANCY = 5;

    /** @brief Maximum Reed-Solomon redundancy percentage (50%) */
    static constexpr uint8_t MAX_RS_REDUNDANCY = 50;

    /** @brief Maximum vault file size in bytes (100MB) */
    static constexpr size_t MAX_VAULT_SIZE = 100 * 1024 * 1024;

    /** @brief Bit shift for big-endian byte 0 (most significant byte) */
    static constexpr size_t BIGENDIAN_SHIFT_24 = 24;

    /** @brief Bit shift for big-endian byte 1 */
    static constexpr size_t BIGENDIAN_SHIFT_16 = 16;

    /** @brief Bit shift for big-endian byte 2 */
    static constexpr size_t BIGENDIAN_SHIFT_8 = 8;

    /** @} */ // end of Vault File Format Constants

    VaultManager();
    ~VaultManager() noexcept;

    // Vault operations

    /**
     * @brief Create a new encrypted vault
     * @param path Filesystem path where vault will be created
     * @param password Master password for vault encryption
     * @param require_yubikey Optional: require YubiKey for vault access
     * @param yubikey_serial Optional: specific YubiKey serial number
     * @return true if vault created successfully, false on error
     *
     * Creates a new vault file with:
     * - Magic header (0x5641554C / "VAUL")
     * - Version number (1)
     * - PBKDF2 iteration count
     * - Random salt (32 bytes)
     * - Optional: YubiKey challenge and serial
     * - Empty encrypted data
     *
     * @note File permissions set to 0600 (owner read/write only)
     * @warning Overwrites existing file at path
     */
    [[nodiscard]] bool create_vault(const std::string& path,
                                     const Glib::ustring& password,
                                     bool require_yubikey = false,
                                     std::string yubikey_serial = "");

    /**
     * @brief Check if a vault requires YubiKey authentication
     * @param path Filesystem path to vault file
     * @param serial Output parameter for YubiKey serial number (if available)
     * @return true if vault requires YubiKey, false otherwise or on error
     *
     * Peeks at vault file flags without opening or decrypting.
     */
    [[nodiscard]] bool check_vault_requires_yubikey(const std::string& path, std::string& serial);

    /**
     * @brief Open and decrypt an existing vault
     * @param path Filesystem path to vault file
     * @param password Master password for vault decryption
     * @return true if vault opened successfully, false on error
     *
     * Validates file format, verifies authentication tag, and loads accounts.
     *
     * @note Fails if vault is already open
     * @note Password verification is implicit through decryption success
     */
    [[nodiscard]] bool open_vault(const std::string& path, const Glib::ustring& password);

    /**
     * @brief Save vault to disk
     * @param explicit_save If true, backup is created; if false (auto-save), no backup
     * @return true if saved successfully, false on error
     *
     * Performs atomic save operation:
     * 1. Creates backup if explicit_save=true and backups enabled
     * 2. Writes to temporary file
     * 3. Renames temporary to actual vault
     *
     * @note Requires vault to be open
     * @note Backups only created on explicit saves (Ctrl+S, Close with save)
     */
    [[nodiscard]] bool save_vault(bool explicit_save = true);

    /**
     * @brief Close vault and clear sensitive data
     * @return true if closed successfully, false on error
     *
     * Securely erases encryption keys and other sensitive data from memory.
     */
    [[nodiscard]] bool close_vault();

    /** @name Multi-User Vault Operations (V2 Format)
     * @brief LUKS-style multi-user authentication and management
     *
     * V2 vaults support multiple users with individual passwords/YubiKeys,
     * each unlocking the same vault data. Uses NIST-approved key wrapping.
     * @{
     */

    /**
     * @brief Create a new V2 vault with multi-user support
     * @param path Filesystem path where vault will be created
     * @param admin_username Initial administrator username
     * @param admin_password Initial administrator password
     * @param policy Security policy for vault (YubiKey requirements, password rules)
     * @return Expected void or VaultError
     *
     * Creates V2 vault with:
     * - VaultSecurityPolicy (YubiKey, password requirements, PBKDF2 iterations)
     * - Initial administrator key slot
     * - FEC-protected header (20% minimum redundancy)
     * - Empty encrypted data section
     *
     * @note File permissions set to 0600 (owner read/write only)
     * @warning Overwrites existing file at path
     *
     * @code
     * VaultSecurityPolicy policy;
     * policy.require_yubikey = false;
     * policy.min_password_length = 12;
     * policy.pbkdf2_iterations = 100000;
     *
     * auto result = vm.create_vault_v2("/path/vault.v2", "admin", "password123", policy);
     * if (!result) {
     *     // Handle error: result.error()
     * }
     * @endcode
     */
    [[nodiscard]] KeepTower::VaultResult<> create_vault_v2(
        const std::string& path,
        const Glib::ustring& admin_username,
        const Glib::ustring& admin_password,
        const KeepTower::VaultSecurityPolicy& policy,
        const std::optional<std::string>& yubikey_pin = std::nullopt);

    /**
     * @brief Create V2 vault asynchronously (non-blocking)
     * @param path Filesystem path for new vault
     * @param admin_username Username for initial administrator account
     * @param admin_password Initial administrator password
     * @param policy Security policy for vault
     * @param progress_callback Optional callback for progress updates (on GTK thread)
     * @param completion_callback Callback invoked when creation completes (on GTK thread)
     * @param yubikey_pin Optional YubiKey PIN if YubiKey enrollment required
     *
     * Creates V2 vault in background thread without blocking UI.
     * Progress and completion callbacks are invoked on GTK main thread.
     *
     * Suitable for:
     * - GTK UI applications
     * - Long-running vault creation (10-15 seconds with YubiKey)
     * - Operations requiring user interaction (YubiKey touch)
     *
     * @note Thread-safe: can be called from any thread
     * @note Progress callbacks report 8 steps (validation, key generation, etc.)
     * @note YubiKey touches happen on background thread but are properly synchronized
     * @note If VaultManager is destroyed before completion, behavior is undefined
     *
     * @code
     * VaultSecurityPolicy policy;
     * policy.require_yubikey = true;
     * policy.min_password_length = 12;
     * policy.pbkdf2_iterations = 100000;
     *
     * auto progress_fn = [](int step, int total, const std::string& msg) {
     *     std::cout << "Step " << step << "/" << total << ": " << msg << std::endl;
     * };
     *
     * auto completion_fn = [](VaultResult<> result) {
     *     if (result) {
     *         std::cout << "Vault created successfully!" << std::endl;
     *     } else {
     *         std::cerr << "Failed: " << static_cast<int>(result.error()) << std::endl;
     *     }
     * };
     *
     * vm.create_vault_v2_async(
     *     "/path/vault.v2", "admin", "password123",
     *     policy, progress_fn, completion_fn, "123456");
     * @endcode
     *
     * @since Version 0.3.2 (Phase 3)
     */
    void create_vault_v2_async(
        const std::string& path,
        const Glib::ustring& admin_username,
        const Glib::ustring& admin_password,
        const KeepTower::VaultSecurityPolicy& policy,
        KeepTower::VaultCreationOrchestrator::ProgressCallback progress_callback,
        std::function<void(KeepTower::VaultResult<>)> completion_callback,
        const std::optional<std::string>& yubikey_pin = std::nullopt);

    /**
     * @brief Open V2 vault with user authentication
     * @param path Filesystem path to V2 vault
     * @param username Username for authentication
     * @param password User's password
     * @param yubikey_serial Optional YubiKey serial (if vault requires it)
     * @return Expected UserSession or VaultError
     *
     * Authentication process:
     * 1. Find active key slot for username
     * 2. Derive KEK from password (PBKDF2)
     * 3. Optionally combine with YubiKey response (XOR)
     * 4. Unwrap DEK using AES-256-KW
     * 5. Decrypt vault data with DEK
     *
     * Returns UserSession containing:
     * - username
     * - role (ADMINISTRATOR or STANDARD_USER)
     * - password_change_required flag
     *
     * @note If password_change_required is true, UI must enforce password change
     * @note Wrong password returns VaultError::AuthenticationFailed
     *
     * @code
     * auto session = vm.open_vault_v2("/path/vault.v2", "alice", "password");
     * if (!session) {
     *     // Handle auth failure
     *     return;
     * }
     *
     * if (session->password_change_required) {
     *     // Force password change dialog
     *     show_change_password_dialog();
     * }
     * @endcode
     */
    [[nodiscard]] KeepTower::VaultResult<KeepTower::UserSession> open_vault_v2(
        const std::string& path,
        const Glib::ustring& username,
        const Glib::ustring& password,
        const std::string& yubikey_serial = "");

    /**
     * @brief Add new user to open V2 vault
     * @param username New user's username (must be unique)
     * @param temporary_password Temporary password for first login
     * @param role User role (ADMINISTRATOR or STANDARD_USER)
     * @param must_change_password Force password change on first login (default: true)
     * @return Expected void or VaultError
     *
     * Requirements:
     * - Vault must be open
     * - Current user must have ADMINISTRATOR role
     * - Username must be unique (not already exist)
     * - Password must meet vault's minimum length requirement
     *
     * Creates new key slot:
     * - Unique salt generated
     * - Current vault DEK wrapped with new user's KEK
     * - Marked as must_change_password if temporary
     *
     * @note Call save_vault() after to persist changes
     *
     * @code
     * auto result = vm.add_user("bob", "temp1234", UserRole::STANDARD_USER, true);
     * if (!result) {
     *     show_error(result.error());
     *     return;
     * }
     * vm.save_vault();
     * @endcode
     */
    [[nodiscard]] KeepTower::VaultResult<> add_user(
        const Glib::ustring& username,
        const Glib::ustring& temporary_password,
        KeepTower::UserRole role = KeepTower::UserRole::STANDARD_USER,
        bool must_change_password = true,
        const std::optional<std::string>& yubikey_pin = std::nullopt);

    /**
     * @brief Remove user from open V2 vault
     * @param username Username to remove
     * @return Expected void or VaultError
     *
     * Requirements:
     * - Vault must be open
     * - Current user must have ADMINISTRATOR role
     * - Cannot remove yourself (self-removal prevention)
     * - Cannot remove last administrator (must have at least one)
     *
     * Marks key slot as inactive (doesn't delete, preserves slot position).
     *
     * @note Call save_vault() after to persist changes
     *
     * @code
     * auto result = vm.remove_user("bob");
     * if (!result) {
     *     show_error("Cannot remove user: " + to_string(result.error()));
     *     return;
     * }
     * vm.save_vault();
     * @endcode
     */
    [[nodiscard]] KeepTower::VaultResult<> remove_user(const Glib::ustring& username);

    /**
     * @brief Validate new password without performing the change
     * @param username Username whose password would be changed
     * @param new_password New password to validate
     * @return Expected void or VaultError (PasswordTooShort, PasswordReused)
     *
     * Requirements:
     * - Vault must be open
     * - User must exist
     *
     * Validates:
     * 1. Password meets minimum length requirement
     * 2. Password not in user's password history (if enabled)
     *
     * @note This allows UI to validate before showing YubiKey prompts
     *
     * @code
     * // Validate before showing YubiKey prompt
     * auto validation = vm.validate_new_password("alice", "newpass123");
     * if (!validation) {
     *     show_error(validation.error());
     *     return;
     * }
     * // Now show YubiKey prompt and proceed with actual change
     * @endcode
     */
    [[nodiscard]] KeepTower::VaultResult<> validate_new_password(
        const Glib::ustring& username,
        const Glib::ustring& new_password);

    /**
     * @brief Change user's password in open V2 vault
     * @param username Username whose password to change
     * @param old_password Current password (for verification)
     * @param new_password New password
     * @return Expected void or VaultError
     *
     * Requirements:
     * - Vault must be open
     * - Either: user changing own password OR current user is admin
     * - Old password must be correct (KEK unwrapping verification)
     * - New password must meet vault's minimum length requirement
     *
     * Process:
     * 1. Verify old password by unwrapping DEK
     * 2. Derive new KEK from new password
     * 3. Re-wrap DEK with new KEK
     * 4. Update timestamps and clear must_change_password flag
     *
     * @note Call save_vault() after to persist changes
     *
     * @code
     * // User changing own password
     * auto result = vm.change_user_password("alice", "oldpass", "newpass123");
     * if (!result) {
     *     show_error("Password change failed");
     *     return;
     * }
     * vm.save_vault();
     * @endcode
     */
    [[nodiscard]] KeepTower::VaultResult<> change_user_password(
        const Glib::ustring& username,
        const Glib::ustring& old_password,
        const Glib::ustring& new_password,
        const std::optional<std::string>& yubikey_pin = std::nullopt,
        std::function<void(const std::string&)> progress_callback = nullptr);

    /**
     * @brief Change user password asynchronously (non-blocking with YubiKey touch prompts)
     * @param username Username whose password to change
     * @param old_password Current password (for verification)
     * @param new_password New password to set
     * @param progress_callback Optional callback for YubiKey touch progress (on GTK thread)
     * @param completion_callback Callback invoked when operation completes (on GTK thread)
     * @param yubikey_pin Optional YubiKey PIN if not yet encrypted in vault
     *
     * Changes password in background thread without blocking UI.
     * If YubiKey is enrolled, shows non-blocking progress for 2 required touches.
     * Progress and completion callbacks are invoked on GTK main thread.
     *
     * Scenarios:
     * 1. **YubiKey Enrolled**: 2 touches required (verify old, combine with new)
     * 2. **No YubiKey**: Fast operation, minimal progress reporting
     *
     * Progress callback receives:
     * - Step 1: "Verifying old password with YubiKey (touch 1 of 2)"
     * - Step 2: "Combining new password with YubiKey (touch 2 of 2)"
     *
     * @note Thread-safe: can be called from any thread
     * @note If VaultManager is destroyed before completion, behavior is undefined
     * @note YubiKey touches happen on background thread but UI can update on progress
     *
     * @code
     * auto progress_fn = [](int step, int total, const std::string& msg) {
     *     // Update UI: show touch prompt dialog, spinner, etc.
     *     std::cout << "Step " << step << "/" << total << ": " << msg << std::endl;
     * };
     *
     * auto completion_fn = [](VaultResult<> result) {
     *     if (result) {
     *         show_success("Password changed successfully!");
     *         save_vault();
     *     } else {
     *         show_error("Failed to change password");
     *     }
     * };
     *
     * vm.change_user_password_async(
     *     "alice", "oldpass", "newpass123",
     *     progress_fn, completion_fn);
     * @endcode
     *
     * @since Version 0.3.2 (Phase 3)
     */
    void change_user_password_async(
        const Glib::ustring& username,
        const Glib::ustring& old_password,
        const Glib::ustring& new_password,
        std::function<void(int step, int total, const std::string& description)> progress_callback,
        std::function<void(KeepTower::VaultResult<>)> completion_callback,
        const std::optional<std::string>& yubikey_pin = std::nullopt);

    /**
     * @brief Clear password history for a user
     * @param username Username whose password history to clear
     * @return Expected void or VaultError
     *
     * Clears all stored password history entries for the specified user.
     * Used when admin disables password history or for manual cleanup.
     *
     * @note Requires admin privileges
     * @note Changes take effect immediately but vault must be saved
     */
    [[nodiscard]] KeepTower::VaultResult<> clear_password_history(
        const Glib::ustring& username);

    // ========== USERNAME HASH MIGRATION (Phase 1) ==========

    /**
     * @brief Migrate user's username hash to new algorithm (internal use only)
     *
     * Called automatically after successful authentication when migration is active.
     * Re-hashes the user's username with the new algorithm from policy and updates
     * the KeySlot with new hash and salt.
     *
     * @section when_called When Called
     * Invoked by open_vault_v2() when:
     * 1. Migration is active (policy.migration_flags bit 0 = 1)
     * 2. User authenticated successfully (password verified)
     * 3. User's migration_status = 0xFF (authenticated with old algorithm)
     *
     * @section security_guarantee Security Guarantee
     * This function is ONLY called after successful authentication. Failed
     * authentication attempts never reach this code. The plaintext password
     * is already validated, so we have cryptographic proof of user identity.
     *
     * @section what_changes What Changes
     * - Generates new random username_salt (16 bytes)
     * - Computes new username_hash using policy.username_hash_algorithm
     * - Updates KeySlot.username_hash and username_hash_size
     * - Sets KeySlot.migration_status = 0x01 (migrated)
     * - Sets KeySlot.migrated_at = current timestamp
     * - Saves vault immediately (critical for persistence)
     *
     * @section what_unchanged What Stays Unchanged
     * - User's password (not re-entered, not changed)
     * - KEK derivation (separate from username hashing)
     * - Wrapped DEK (no re-encryption needed)
     * - YubiKey credentials (FIDO2 uses plaintext username, not hash)
     * - User's role, permissions, and other metadata
     *
     * @param user_slot Pointer to user's KeySlot (must be non-null)
     * @param username Plaintext username (for hashing)
     * @param password Plaintext password (for logging only, not used for migration)
     * @return Expected void or VaultError
     *
     * @note Private/internal function - not exposed to UI layer
     * @note Logs all migration events for audit trail
     * @note Creates backup before saving (migration is critical operation)
     *
     * @code
     * // Called internally by open_vault_v2():
     * if (user_slot->migration_status == 0xFF) {
     *     auto result = migrate_user_hash(user_slot, username.raw(), password.raw());
     *     if (!result) {
     *         Log::error("Migration failed, but user can still access vault");
     *         // Don't fail login - migration can be retried next time
     *     }
     * }
     * @endcode
     */
    [[nodiscard]] KeepTower::VaultResult<> migrate_user_hash(
        KeepTower::KeySlot* user_slot,
        const std::string& username,
        const std::string& password);

    /**
     * @brief Clear password history for a user
     * @param username Username whose password history to clear
     * @return Expected void or VaultError
     *
     * Requirements:
     * - Vault must be open (V2 only)
     * - Either: current user is target user OR current user is admin
     *
     * Clears all password history entries for the specified user.
     * This does NOT affect the password_history_depth policy setting.
     *
     * @note Call save_vault() after to persist changes
     *
     * @code
     * auto result = vm.clear_user_password_history("alice");
     * if (result) {
     *     vm.save_vault();
     * }
     * @endcode
     */
    [[nodiscard]] KeepTower::VaultResult<> clear_user_password_history(
        const Glib::ustring& username);

    /**
     * @brief Admin-only: Reset user password without knowing current password
     * @param username Username whose password to reset
     * @param new_temporary_password New temporary password
     * @return Expected void or VaultError
     *
     * Requirements:
     * - Vault must be open
     * - Current user must have ADMINISTRATOR role
     * - New password must meet vault's minimum length requirement
     * - Sets must_change_password flag (user required to change on next login)
     *
     * Process:
     * 1. Verify admin permissions
     * 2. Derive new KEK from new temporary password
     * 3. Re-wrap DEK with new KEK
     * 4. Set must_change_password = true
     * 5. Update timestamps
     *
     * @note Call save_vault() after to persist changes
     * @note This bypasses old password verification (admin override)
     *
     * @code
     * // Admin resetting user password
     * auto result = vm.admin_reset_user_password("bob", "TempPass1234");
     * if (!result) {
     *     show_error("Failed to reset password");
     *     return;
     * }
     * vm.save_vault();
     * @endcode
     */
    [[nodiscard]] KeepTower::VaultResult<> admin_reset_user_password(
        const Glib::ustring& username,
        const Glib::ustring& new_temporary_password);

    /**
     * @brief Enroll YubiKey for a user account (two-factor authentication)
     * @param username Username to enroll YubiKey for
     * @param password User's current password (for verification)
     * @return Expected void or VaultError
     *
     * Requirements:
     * - Vault must be open (V2 only)
     * - Either: user enrolling own YubiKey OR current user is admin
     * - Password must be correct (KEK unwrapping verification)
     * - YubiKey must be present and functional
     * - User must not already have YubiKey enrolled
     *
     * Process:
     * 1. Verify password by unwrapping DEK
     * 2. Generate unique 20-byte challenge for user
     * 3. Perform YubiKey challenge-response (requires touch for security)
     * 4. Combine KEK with YubiKey response
     * 5. Re-wrap DEK with password+YubiKey combined KEK
     * 6. Encrypt and store YubiKey PIN with password-derived KEK
     * 7. Store challenge, serial, credential ID, and timestamp in user's KeySlot
     *
     * @note After enrollment, user MUST have YubiKey present for all future logins
     * @note Call save_vault() after to persist changes
     *
     * @code
     * // User enrolling their own YubiKey with PIN
     * auto result = vm.enroll_yubikey_for_user("alice", "alicepass123", "123456");
     * if (!result) {
     *     show_error("YubiKey enrollment failed");
     *     return;
     * }
     * vm.save_vault();
     * show_success("YubiKey enrolled! Required for future logins.");
     * @endcode
     */
    [[nodiscard]] KeepTower::VaultResult<> enroll_yubikey_for_user(
        const Glib::ustring& username,
        const Glib::ustring& password,
        const std::string& yubikey_pin,
        std::function<void(const std::string&)> progress_callback = nullptr);

    /**
     * @brief Async version of enroll_yubikey_for_user with progress reporting
     * @param username Username to enroll YubiKey for
     * @param password User's current password (for verification)
     * @param yubikey_pin YubiKey PIN (4-63 characters)
     * @param progress_callback Callback for touch progress ("Touch 1 of 2...", "Touch 2 of 2...")
     * @param completion_callback Called with result when enrollment completes
     *
     * Runs enrollment in background thread, reports progress for each YubiKey touch.
     * Callbacks are invoked on GTK main thread via Glib::signal_idle().
     */
    void enroll_yubikey_for_user_async(
        const Glib::ustring& username,
        const Glib::ustring& password,
        const std::string& yubikey_pin,
        std::function<void(const std::string&)> progress_callback,
        std::function<void(const KeepTower::VaultResult<>&)> completion_callback);

    /**
     * @brief Remove YubiKey enrollment from a user account
     * @param username Username to unenroll YubiKey from
     * @param password User's current password (for verification)
     * @return Expected void or VaultError
     *
     * Requirements:
     * - Vault must be open (V2 only)
     * - Either: user unenrolling own YubiKey OR current user is admin
     * - Password AND YubiKey must be correct (combined verification)
     * - User must currently have YubiKey enrolled
     *
     * Process:
     * 1. Verify password+YubiKey by unwrapping DEK
     * 2. Derive new KEK from password ONLY (no YubiKey)
     * 3. Re-wrap DEK with password-only KEK
     * 4. Clear YubiKey fields in user's KeySlot
     *
     * @note After unenrollment, user logs in with password only
     * @note Call save_vault() after to persist changes
     *
     * @code
     * // User removing their YubiKey requirement
     * auto result = vm.unenroll_yubikey_for_user("alice", "alicepass123");
     * if (!result) {
     *     show_error("YubiKey unenrollment failed");
     *     return;
     * }
     * vm.save_vault();
     * show_success("YubiKey removed. Password-only login enabled.");
     * @endcode
     */
    [[nodiscard]] KeepTower::VaultResult<> unenroll_yubikey_for_user(
        const Glib::ustring& username,
        const Glib::ustring& password,
        std::function<void(const std::string&)> progress_callback = nullptr);

    /**
     * @brief Async version of unenroll_yubikey_for_user with progress reporting
     * @param username Username to unenroll YubiKey from
     * @param password User's current password (for verification)
     * @param progress_callback Callback for YubiKey touch progress message
     * @param completion_callback Called with result when unenrollment completes
     *
     * Runs unenrollment in background thread, reports progress when YubiKey
     * verification touch is required. Callbacks are invoked on GTK main thread
     * via Glib::signal_idle().
     *
     * Progress message:
     * - "Verifying current password with YubiKey (touch required)..."
     *
     * @code
     * // Async unenrollment with progress dialog
     * vault_manager->unenroll_yubikey_for_user_async(
     *     "alice", "password123",
     *     [dialog](const std::string& msg) {
     *         dialog->update_message(msg);
     *         dialog->present();
     *     },
     *     [this, dialog](const auto& result) {
     *         dialog->hide();
     *         if (!result) {
     *             show_error("Unenrollment failed");
     *             return;
     *         }
     *         vault_manager->save_vault();
     *         show_success("YubiKey removed successfully");
     *     });
     * @endcode
     */
    void unenroll_yubikey_for_user_async(
        const Glib::ustring& username,
        const Glib::ustring& password,
        std::function<void(const std::string&)> progress_callback,
        std::function<void(const KeepTower::VaultResult<>&)> completion_callback);

    /**
     * @brief Get current user session info
     * @return Optional UserSession, empty if no V2 vault open
     *
     * Returns current session information including:
     * - username
     * - role (for permission checking)
     * - password_change_required flag
     */
    [[nodiscard]] std::optional<KeepTower::UserSession> get_current_user_session() const;

    /**
     * @brief List all users in open V2 vault
     * @return Vector of UserInfo structs, empty if not V2 vault or error
     *
     * Returns user information for all active key slots:
     * - username
     * - role
     * - must_change_password flag
     * - password_changed_at timestamp
     * - last_login_at timestamp
     *
     * @note Only returns active users (inactive slots excluded)
     * @note Requires vault to be open
     */
    [[nodiscard]] std::vector<KeepTower::KeySlot> list_users() const;

    /**
     * @brief Convert V1 vault to V2 multi-user format
     * @param admin_username Username for the administrator account
     * @param admin_password Password for the administrator account
     * @param policy Security policy for the new V2 vault
     * @return VaultResult with success or error
     *
     * Migrates a legacy single-user vault (V1) to the modern multi-user format (V2).
     * This operation:
     * 1. Creates automatic backup of V1 vault
     * 2. Converts vault structure to V2 format
     * 3. Creates first administrator account with provided credentials
     * 4. Applies security policy (min password length, iterations)
     * 5. Preserves all existing accounts and metadata
     *
     * Migration is irreversible - V1 clients cannot open migrated vaults.
     *
     * Requirements:
     * - Vault must be currently open (V1 format)
     * - Admin password must meet security policy requirements
     * - Sufficient disk space for backup
     *
     * @code
     * VaultManager vm;
     * vm.open_vault("/path/vault.v1", "old_password");
     *
     * KeepTower::VaultSecurityPolicy policy;
     * policy.min_password_length = 12;
     * policy.pbkdf2_iterations = 600000;
     *
     * auto result = vm.convert_v1_to_v2("admin", "secure_password", policy);
     * if (!result) {
     *     // Handle error: result.error()
     * }
     * @endcode
     *
     * @note Creates backup at {vault_path}.v1.backup before migration
     * @note Vault remains open after successful migration (as admin)
     * @note All accounts preserve their IDs, timestamps, and data
     *
     * @see create_vault_v2() for V2 vault creation from scratch
     * @see open_vault_v2() for opening existing V2 vaults
     */
    [[nodiscard]] KeepTower::VaultResult<> convert_v1_to_v2(
        const Glib::ustring& admin_username,
        const Glib::ustring& admin_password,
        const KeepTower::VaultSecurityPolicy& policy);

    /**
     * @brief Get vault security policy
     * @return Optional security policy, empty if no V2 vault open
     *
     * Returns vault-wide security settings:
     * - require_yubikey flag
     * - min_password_length (characters)
     * - pbkdf2_iterations (key derivation rounds)
     * - yubikey_challenge (if YubiKey enabled)
     *
     * Use this to:
     * - Validate password requirements before setting
     * - Generate temporary passwords meeting policy
     * - Display security requirements to users
     * - Enforce consistent policy across UI
     *
     * @code
     * auto policy = vault_manager.get_vault_security_policy();
     * if (policy) {
     *     if (new_password.length() < policy->min_password_length) {
     *         show_error("Password too short");
     *     }
     * }
     * @endcode
     *
     * @note Only available for V2 vaults
     * @note Returns copy of policy (modifications don't affect vault)
     */
    [[nodiscard]] std::optional<KeepTower::VaultSecurityPolicy> get_vault_security_policy() const noexcept;

    /**
     * @brief Update vault security policy (admin only)
     * @param new_policy New security policy to apply
     * @return Expected void or VaultError
     *
     * Updates the security policy for a V2 vault. This operation:
     * - Requires administrator permissions
     * - Marks vault as modified (must save)
     * - Can be used to enable/disable migration
     * - Can change password requirements
     * - Can change algorithm settings
     *
     * @note Changes take effect immediately for new operations
     * @note Existing users are not re-validated against new policy
     * @note Migration flag changes affect authentication behavior immediately
     *
     * @warning Changing username_hash_algorithm without proper migration setup
     *          will lock out all users! Use migration_flags to enable gradual migration.
     *
     * Example usage (enabling migration):
     * @code
     * auto policy_opt = vault_mgr.get_vault_security_policy();
     * if (policy_opt) {
     *     auto policy = *policy_opt;
     *     policy.username_hash_algorithm_previous = policy.username_hash_algorithm;
     *     policy.username_hash_algorithm = 0x04; // PBKDF2
     *     policy.migration_flags = 0x01; // Enable migration
     *     policy.migration_started_at = std::time(nullptr);
     *
     *     auto result = vault_mgr.update_security_policy(policy);
     *     if (result) {
     *         vault_mgr.save_vault();
     *     }
     * }
     * @endcode
     */
    [[nodiscard]] KeepTower::VaultResult<> update_security_policy(
        const KeepTower::VaultSecurityPolicy& new_policy);

    /**
     * @brief Check if current user can view an account
     * @param account_index Index of account to check
     * @return True if user can view account, false otherwise
     *
     * Access control rules:
     * - Administrators: Can view ALL accounts (including admin-only)
     * - Standard users: Can view non-admin-only accounts
     * - Returns false for invalid indices
     *
     * Use this to filter account list based on user role.
     *
     * @code
     * for (size_t i = 0; i < get_accounts().size(); ++i) {
     *     if (can_view_account(i)) {
     *         // Show account in UI
     *     }
     * }
     * @endcode
     *
     * @note Only relevant for V2 vaults with multi-user support
     * @note V1 vaults always return true (no access control)
     */
    [[nodiscard]] bool can_view_account(size_t account_index) const noexcept;

    /**
     * @brief Check if current user can delete an account
     * @param account_index Index of account to check
     * @return True if user can delete account, false otherwise
     *
     * Access control rules:
     * - Administrators: Can delete ALL accounts
     * - Standard users: Can delete non-admin-only-deletable accounts
     * - Returns false for invalid indices
     *
     * Use this to enable/disable delete button based on user role.
     *
     * @code
     * delete_button->set_sensitive(vault_manager.can_delete_account(index));
     * @endcode
     *
     * @note Only relevant for V2 vaults with multi-user support
     * @note V1 vaults always return true (no access control)
     */
    [[nodiscard]] bool can_delete_account(size_t account_index) const noexcept;

    /** @} */ // end of Multi-User Vault Operations

    /** @name FIPS-140-3 Cryptographic Mode Management
     * @brief OpenSSL FIPS provider initialization and control
     *
     * These methods manage FIPS-140-3 compliant cryptographic operations using
     * the OpenSSL 3.5+ FIPS provider. FIPS mode ensures all cryptographic
     * operations meet NIST FIPS 140-3 validation requirements.
     *
     * @section fips_usage Usage Pattern
     * @code
     * // At application startup (before any crypto operations)
     * bool enable_fips = settings->get_boolean("fips-mode-enabled");
     * if (!VaultManager::init_fips_mode(enable_fips)) {
     *     Log::error("Failed to initialize FIPS mode");
     * }
     *
     * // Check availability and status
     * if (VaultManager::is_fips_available()) {
     *     Log::info("FIPS mode: {}", VaultManager::is_fips_enabled() ? "enabled" : "disabled");
     * }
     * @endcode
     *
     * @section fips_requirements Requirements
     * - OpenSSL 3.5.0 or higher
     * - FIPS module installed and configured (fipsmodule.cnf)
     * - Valid FIPS provider configuration in openssl.cnf or via environment
     *
     * @section fips_compliance Compliance
     * All KeepTower cryptographic algorithms are FIPS-approved:
     * - AES-256-GCM (encryption)
     * - PBKDF2-HMAC-SHA256 (key derivation, 100K+ iterations)
     * - SHA-256 (hashing)
     * - RAND_bytes (DRBG random number generation)
     *
     * @{
     */

    /**
     * @brief Initialize OpenSSL FIPS provider and set cryptographic mode
     *
     * Initializes the OpenSSL provider system and optionally enables FIPS-140-3
     * validated cryptographic operations. This method must be called once at
     * application startup before any cryptographic operations are performed.
     *
     * **Initialization Behavior:**
     * - If `enable = true` and FIPS provider available: Loads FIPS provider
     * - If `enable = true` and FIPS provider unavailable: Falls back to default provider
     * - If `enable = false`: Uses default OpenSSL provider
     * - Subsequent calls are no-ops (thread-safe, single initialization guarantee)
     *
     * **Thread Safety:**
     * Uses atomic compare-and-exchange to ensure single initialization across
     * all threads. Safe to call from multiple threads simultaneously.
     *
     * @param enable If true, attempts to enable FIPS mode; if false, uses default provider
     *
     * @return true if initialization successful (provider loaded), false on error
     * @retval true FIPS provider loaded successfully (if requested and available)
     * @retval true Default provider loaded (fallback or when FIPS not requested)
     * @retval false Provider loading failed (rare, indicates OpenSSL corruption)
     *
     * @note **Process Lifetime:** Can only be called once per process. Subsequent
     *       calls return cached result without performing initialization.
     * @note **Global State:** Affects all VaultManager instances and all OpenSSL
     *       operations in the process.
     * @note **Restart Required:** Changing FIPS mode typically requires application
     *       restart for consistent behavior across all cryptographic contexts.
     *
     * @warning Do not call this method after performing any cryptographic operations.
     *          Provider initialization must occur before first crypto operation.
     *
     * @see is_fips_available() to check if FIPS provider was loaded
     * @see is_fips_enabled() to check current FIPS mode status
     * @see set_fips_mode() to toggle FIPS mode after initialization
     *
     * @par Example:
     * @code
     * // At application startup
     * bool fips_requested = config->get_fips_preference();
     * if (!VaultManager::init_fips_mode(fips_requested)) {
     *     Log::error("Cryptographic initialization failed");
     *     return EXIT_FAILURE;
     * }
     *
     * if (fips_requested && !VaultManager::is_fips_available()) {
     *     Log::warning("FIPS mode requested but not available - using default provider");
     * }
     * @endcode
     *
     * @security **FIPS-140-3 Compliance:** Applications requiring FIPS compliance
     *           must enable FIPS mode and verify `is_fips_enabled()` returns true.
     *           Operating without FIPS mode when required violates compliance.
     */
    [[nodiscard]] static bool init_fips_mode(bool enable = false);

    /**
     * @brief Check if OpenSSL FIPS provider is available
     *
     * Queries whether the FIPS cryptographic provider was successfully loaded
     * during initialization. Availability depends on OpenSSL configuration and
     * FIPS module installation.
     *
     * **FIPS Provider Availability Requirements:**
     * - OpenSSL 3.5.0+ installed
     * - FIPS module compiled and installed (libfips.so / fips.dll)
     * - Valid fipsmodule.cnf configuration file
     * - Proper openssl.cnf configuration OR OPENSSL_CONF environment variable
     * - FIPS module self-tests passed during provider load
     *
     * **Typical Unavailability Causes:**
     * - FIPS module not installed
     * - OpenSSL < 3.5.0 (FIPS 140-3 requires 3.5+)
     * - Missing or invalid fipsmodule.cnf
     * - FIPS self-test failures
     * - Insufficient permissions to load provider
     *
     * @return true if FIPS provider is available, false otherwise
     * @retval true FIPS provider loaded and operational
     * @retval false FIPS provider not available (using default provider)
     *
     * @pre init_fips_mode() must have been called first
     *
     * @note This method only checks availability, not whether FIPS mode is active.
     *       Use is_fips_enabled() to check active status.
     * @note Returns false if init_fips_mode() hasn't been called yet.
     *
     * @see init_fips_mode() to initialize provider system
     * @see is_fips_enabled() to check if FIPS mode is active
     *
     * @par Example:
     * @code
     * if (VaultManager::is_fips_available()) {
     *     ui->show_fips_toggle();  // Enable FIPS checkbox
     *     ui->set_fips_status("✓ FIPS module available");
     * } else {
     *     ui->disable_fips_toggle();  // Disable checkbox
     *     ui->set_fips_status("⚠️ FIPS module not available");
     * }
     * @endcode
     */
    [[nodiscard]] static bool is_fips_available();

    /**
     * @brief Check if FIPS-140-3 mode is currently enabled
     *
     * Queries the current operational status of FIPS mode. When enabled, all
     * cryptographic operations use FIPS-validated implementations and enforce
     * FIPS algorithm restrictions.
     *
     * **FIPS Enabled Status:**
     * - `true`: All crypto operations use FIPS provider (compliant mode)
     * - `false`: Crypto operations use default provider (standard mode)
     *
     * **Relationship with is_fips_available():**
     * - `available = false, enabled = false`: FIPS not installed
     * - `available = true, enabled = false`: FIPS installed but not active
     * - `available = true, enabled = true`: FIPS active (compliant)
     * - `available = false, enabled = true`: Invalid state (cannot occur)
     *
     * @return true if FIPS mode is currently active, false otherwise
     * @retval true All cryptographic operations use FIPS provider
     * @retval false Using default OpenSSL provider (non-FIPS)
     *
     * @pre init_fips_mode() must have been called first
     *
     * @note FIPS mode can only be enabled if the FIPS provider is available.
     * @note Returns false if init_fips_mode() hasn't been called yet.
     *
     * @see init_fips_mode() to set initial FIPS mode
     * @see is_fips_available() to check if FIPS provider is installed
     * @see set_fips_mode() to change FIPS mode at runtime
     *
     * @par Example:
     * @code
     * // Display FIPS status in About dialog
     * std::string status;
     * if (VaultManager::is_fips_available()) {
     *     if (VaultManager::is_fips_enabled()) {
     *         status = "FIPS-140-3: Enabled ✓";
     *     } else {
     *         status = "FIPS-140-3: Available (not enabled)";
     *     }
     * } else {
     *     status = "FIPS-140-3: Not available";
     * }
     * about_dialog->set_comments(status);
     * @endcode
     *
     * @security Applications under FIPS compliance requirements must verify
     *           this returns true before processing sensitive data.
     */
    [[nodiscard]] static bool is_fips_enabled();

    /**
     * @brief Enable or disable FIPS-140-3 mode at runtime
     *
     * Dynamically switches between FIPS and default cryptographic providers.
     * This allows toggling FIPS mode without reinitializing the entire
     * cryptographic subsystem.
     *
     * **Provider Switching:**
     * - `enable = true`: Activates FIPS provider for all subsequent operations
     * - `enable = false`: Activates default provider (standard OpenSSL algorithms)
     *
     * **Switching Behavior:**
     * - Idempotent: Setting same mode returns success without changes
     * - Thread-safe: Uses atomic operations for state management
     * - Immediate effect: Next cryptographic operation uses new provider
     * - Existing contexts: Active encryption/decryption contexts may continue
     *   using old provider until completion
     *
     * **Failure Conditions:**
     * - init_fips_mode() not called
     * - FIPS provider not available (when enabling)
     * - OpenSSL provider switching API failure
     *
     * @param enable If true, enable FIPS mode; if false, use default provider
     *
     * @return true if mode change successful, false on error
     * @retval true FIPS mode successfully changed to requested state
     * @retval true Already in requested state (no-op, success)
     * @retval false FIPS not initialized (call init_fips_mode() first)
     * @retval false FIPS provider unavailable (when trying to enable)
     * @retval false OpenSSL provider switch failed
     *
     * @pre init_fips_mode() must have been called
     * @pre FIPS provider must be available (for enable = true)
     *
     * @post All new cryptographic operations use the selected provider
     * @post is_fips_enabled() will return the new state
     *
     * @note **Application Restart Recommended:** While runtime switching is
     *       supported, some cryptographic contexts may not switch immediately.
     *       For consistent behavior, restart the application after changing mode.
     * @note **User Experience:** Display a restart warning when changing FIPS mode.
     *
     * @warning In some OpenSSL configurations, enabling FIPS mode may be
     *          irreversible without process restart. Always test runtime
     *          switching in your deployment environment.
     *
     * @see init_fips_mode() to perform initial FIPS initialization
     * @see is_fips_available() to verify FIPS provider is loaded
     * @see is_fips_enabled() to query current FIPS state
     *
     * @par Example:
     * @code
     * // In preferences dialog "Apply" handler
     * bool fips_enabled = fips_checkbox->get_active();
     *
     * if (VaultManager::set_fips_mode(fips_enabled)) {
     *     settings->set_boolean("fips-mode-enabled", fips_enabled);
     *     show_info_dialog(
     *         "FIPS mode will be fully active after restart.\n"
     *         "Please restart KeepTower now."
     *     );
     * } else {
     *     show_error_dialog("Failed to change FIPS mode");
     * }
     * @endcode
     *
     * @security **Compliance Impact:** Disabling FIPS mode in a compliance-required
     *           environment may violate security policy. Implement appropriate
     *           access controls and audit logging for FIPS mode changes.
     */
    [[nodiscard]] static bool set_fips_mode(bool enable);

    /** @} */ // end of FIPS-140-3 mode management

    // Account operations

    /**
     * @brief Add new account to vault
     * @param account Account record to add
     * @return true if added successfully, false on error
     *
     * @note Requires vault to be open
     * @note Call save_vault() to persist changes
     */
    [[nodiscard]] bool add_account(const keeptower::AccountRecord& account);

    /**
     * @brief Get all accounts from vault
     * @return Vector of all account records
     *
     * @note Returns empty vector if vault not open
     */
    [[nodiscard]] std::vector<keeptower::AccountRecord> get_all_accounts() const;

    /**
     * @brief Update existing account
     * @param index Zero-based index of account to update
     * @param account New account data
     * @return true if updated successfully, false on error
     *
     * @note Requires vault to be open
     * @note Call save_vault() to persist changes
     */
    [[nodiscard]] bool update_account(size_t index, const keeptower::AccountRecord& account);

    /**
     * @brief Delete account from vault
     * @param index Zero-based index of account to delete
     * @return true if deleted successfully, false on error
     *
     * @note Requires vault to be open
     * @note Call save_vault() to persist changes
     */
    [[nodiscard]] bool delete_account(size_t index);

    /**
     * @brief Get read-only pointer to account
     * @param index Zero-based index of account
     * @return Pointer to account or nullptr if invalid index
     */
    const keeptower::AccountRecord* get_account(size_t index) const;

    /**
     * @brief Get mutable pointer to account
     * @param index Zero-based index of account
     * @return Pointer to account or nullptr if invalid index
     *
     * @note Changes must be followed by save_vault() to persist
     */
    keeptower::AccountRecord* get_account_mutable(size_t index);

    /**
     * @brief Get number of accounts in vault
     * @return Account count, or 0 if vault not open
     */
    [[nodiscard]] size_t get_account_count() const;

    // Account reordering (drag-and-drop support)

    /**
     * @brief Reorder account by moving it from one position to another
     * @param old_index Current position of the account
     * @param new_index Target position for the account
     * @return true if reordered successfully, false on error
     *
     * Initializes global_display_order for all accounts if not already set.
     * Updates display order values to reflect the new arrangement.
     *
     * @note Requires vault to be open
     * @note Automatically calls save_vault() to persist changes
     */
    [[nodiscard]] bool reorder_account(size_t old_index, size_t new_index);

    /**
     * @brief Reset all accounts to automatic sorting
     * @return true if reset successfully, false on error
     *
     * Sets global_display_order to -1 for all accounts, restoring the
     * default sorting behavior (favorites first, then alphabetical).
     *
     * @note Requires vault to be open
     * @note Automatically calls save_vault() to persist changes
     */
    [[nodiscard]] bool reset_global_display_order();

    /**
     * @brief Check if any accounts have custom global ordering
     * @return true if custom ordering is active, false if automatic sorting
     *
     * Returns true if any account has global_display_order >= 0
     */
    [[nodiscard]] bool has_custom_global_ordering() const;

    // Group management (Phase 2 - prepared for future implementation)

    /**
     * @brief Create a new account group
     * @param name Display name for the group
     * @return Group ID (UUID) if created successfully, empty string on error
     *
     * @note Phase 2 feature - implementation pending
     */
    [[nodiscard]] std::string create_group(std::string_view name);

    /**
     * @brief Delete an account group
     * @param group_id UUID of the group to delete
     * @return true if deleted successfully, false on error
     *
     * @note Phase 2 feature - implementation pending
     * @note System groups (e.g., "Favorites") cannot be deleted
     */
    [[nodiscard]] bool delete_group(std::string_view group_id);

    /**
     * @brief Add an account to a group
     * @param account_index Index of the account
     * @param group_id UUID of the group
     * @return true if added successfully, false on error
     *
     * @note Phase 2 feature - implementation pending
     * @note Accounts can belong to multiple groups
     */
    [[nodiscard]] bool add_account_to_group(size_t account_index, std::string_view group_id);

    /**
     * @brief Remove an account from a group
     * @param account_index Index of the account
     * @param group_id UUID of the group
     * @return true if removed successfully, false on error
     *
     * @note Phase 2 feature - implementation pending
     */
    [[nodiscard]] bool remove_account_from_group(size_t account_index, std::string_view group_id);

    /**
     * @brief Reorder an account within a specific group
     * @param account_index Index of the account
     * @param group_id UUID of the group
     * @param new_order New display order within the group (0 = first, higher = later)
     * @return true if reordered successfully, false on error
     *
     * @note Phase 5 feature
     */
    [[nodiscard]] bool reorder_account_in_group(size_t account_index,
                                                 std::string_view group_id,
                                                 int new_order);

    /**
     * @brief Get or create the "Favorites" system group
     * @return Group ID of the Favorites group
     *
     * @note Phase 2 feature - implementation pending
     * @note Auto-creates the group if it doesn't exist
     */
    [[nodiscard]] std::string get_favorites_group_id();

    /**
     * @brief Check if an account belongs to a specific group
     * @param account_index Index of the account
     * @param group_id UUID of the group
     * @return true if the account is in the group, false otherwise
     *
     * @note Phase 2 feature - implementation pending
     */
    [[nodiscard]] bool is_account_in_group(size_t account_index, std::string_view group_id) const;

    /**
     * @brief Get all account groups
     * @return Vector of all groups in the vault
     */
    [[nodiscard]] std::vector<keeptower::AccountGroup> get_all_groups() const;

    /**
     * @brief Rename an existing account group
     * @param group_id UUID of the group to rename
     * @param new_name New display name for the group
     * @return true if renamed successfully, false on error
     *
     * @note Phase 5 feature
     * @note System groups (e.g., "Favorites") cannot be renamed
     * @security Validates new name (length, special characters)
     */
    [[nodiscard]] bool rename_group(std::string_view group_id, std::string_view new_name);

    /**
     * @brief Reorder groups in the UI display
     * @param group_id UUID of the group to move
     * @param new_order New display order (0 = first, higher = later)
     * @return true if reordered successfully, false on error
     *
     * @note Phase 5 feature
     * @note System groups maintain display_order = 0 (always first)
     */
    [[nodiscard]] bool reorder_group(std::string_view group_id, int new_order);

    // State queries

    /**
     * @brief Check if vault is currently open
     * @return true if vault is open, false otherwise
     */
    bool is_vault_open() const { return m_vault_open; }

    /**
     * @brief Check if currently open vault is V2 format
     * @return true if V2 vault is open, false if V1 or no vault open
     */
    bool is_v2_vault() const { return m_vault_open && m_is_v2_vault; }

    /** @brief Get path of currently open vault
     *  @return Vault file path (empty if no vault open) */
    std::string get_current_vault_path() const { return m_current_vault_path; }

    /** @brief Check if vault has unsaved modifications
     *  @return true if vault has been modified since last save */
    bool is_modified() const { return m_modified; }

    // Reed-Solomon error correction

    /**
     * @brief Enable or disable Reed-Solomon error correction for future saves
     * @param enable true to enable RS encoding, false to disable
     * @note This marks the settings as user-modified (not from file)
     */
    void set_reed_solomon_enabled(bool enable) {
        m_use_reed_solomon = enable;
        m_fec_loaded_from_file = false;  // User is explicitly changing the setting
    }

    /**
     * @brief Apply default FEC preferences (used for new vaults)
     * @param enable true to enable RS encoding, false to disable
     * @param redundancy_percent Redundancy level (5-50%)
     * @note This does NOT mark settings as user-modified
     */
    void apply_default_fec_preferences(bool enable, uint8_t redundancy_percent) {
        m_use_reed_solomon = enable;
        m_rs_redundancy_percent = redundancy_percent;
        // Don't set m_fec_loaded_from_file - these are just defaults
    }    /**
     * @brief Check if Reed-Solomon encoding is enabled
     * @return true if RS will be used on next save
     */
    bool is_reed_solomon_enabled() const { return m_use_reed_solomon; }

    /**
     * @brief Check if FEC settings were loaded from the opened file
     * @return true if settings came from file, false if from preferences or user-modified
     */
    bool is_fec_from_file() const { return m_fec_loaded_from_file; }

    /**
     * @brief Set RS redundancy percentage for future saves
     * @param percent Redundancy level (5-50%)
     * @return true if valid, false if out of range
     */
    bool set_rs_redundancy_percent(uint8_t percent);

    /**
     * @brief Get current RS redundancy percentage setting
     * @return Redundancy percentage (5-50%)
     */
    uint8_t get_rs_redundancy_percent() const { return m_rs_redundancy_percent; }

    /**
     * @brief Set clipboard timeout for current vault
     * @param timeout_seconds Timeout in seconds (0 = disabled)
     * @note This setting is stored in the vault file
     */
    void set_clipboard_timeout(int timeout_seconds);

    /**
     * @brief Get clipboard timeout for current vault
     * @return Timeout in seconds (0 if not set or vault closed)
     * @note Returns vault-specific setting, not global default
     */
    [[nodiscard]] int get_clipboard_timeout() const;

    /**
     * @brief Set auto-lock enabled for current vault
     * @param enabled true to enable auto-lock, false to disable
     * @note This setting is stored in the vault file (security-critical)
     */
    void set_auto_lock_enabled(bool enabled);

    /**
     * @brief Get auto-lock enabled for current vault
     * @return true if auto-lock is enabled, false otherwise
     * @note Returns vault-specific setting, not global preference
     */
    [[nodiscard]] bool get_auto_lock_enabled() const;

    /**
     * @brief Set auto-lock timeout for current vault
     * @param timeout_seconds Timeout in seconds (0 = disabled)
     * @note This setting is stored in the vault file
     */
    void set_auto_lock_timeout(int timeout_seconds);

    /**
     * @brief Get auto-lock timeout for current vault
     * @return Timeout in seconds (0 if not set or vault closed)
     * @note Returns vault-specific setting, not global default
     */
    [[nodiscard]] int get_auto_lock_timeout() const;

    /**
     * @brief Set undo/redo enabled for current vault
     * @param enabled true to enable undo/redo, false to disable
     * @note This setting is stored in the vault file
     */
    void set_undo_redo_enabled(bool enabled);

    /**
     * @brief Get undo/redo enabled for current vault
     * @return true if enabled (false if not set or vault closed)
     * @note Returns vault-specific setting, not global default
     */
    [[nodiscard]] bool get_undo_redo_enabled() const;

    /**
     * @brief Set undo/redo history limit for current vault
     * @param limit Maximum operations to keep (1-100)
     * @note This setting is stored in the vault file
     */
    void set_undo_history_limit(int limit);

    /**
     * @brief Get undo/redo history limit for current vault
     * @return History limit (0 if not set or vault closed)
     * @note Returns vault-specific setting, not global default
     */
    [[nodiscard]] int get_undo_history_limit() const;

    /**
     * @brief Set account password history enabled for current vault
     * @param enabled true to prevent password reuse, false to allow
     * @note This setting is stored in the vault file
     */
    void set_account_password_history_enabled(bool enabled);

    /**
     * @brief Get account password history enabled for current vault
     * @return true if enabled (false if not set or vault closed)
     * @note Returns vault-specific setting, not global default
     */
    [[nodiscard]] bool get_account_password_history_enabled() const;

    /**
     * @brief Set account password history limit for current vault
     * @param limit Number of previous passwords to check (0-24)
     * @note This setting is stored in the vault file
     */
    void set_account_password_history_limit(int limit);

    /**
     * @brief Get account password history limit for current vault
     * @return History limit (0 if not set or vault closed)
     * @note Returns vault-specific setting, not global default
     */
    [[nodiscard]] int get_account_password_history_limit() const;

    // Backup configuration

    /**
     * @brief Enable or disable automatic timestamped backups
     * @param enable true to enable backups, false to disable
     */
    void set_backup_enabled(bool enable);

    /**
     * @brief Check if automatic backups are enabled
     * @return true if backups are enabled
     */
    bool is_backup_enabled() const { return m_backup_enabled; }

    /**
     * @brief Set maximum number of backups to maintain
     * @param count Maximum backup count (1-50)
     * @return true if valid, false if out of range
     */
    bool set_backup_count(int count);

    /**
     * @brief Get maximum number of backups to maintain
     * @return Maximum backup count
     */
    int get_backup_count() const { return m_backup_count; }

    /**
     * @brief Set custom backup directory path
     * @param path Directory path for backup files (empty=same as vault)
     */
    void set_backup_path(const std::string& path);

    /**
     * @brief Get custom backup directory path
     * @return Backup directory path (empty if same as vault)
     */
    [[nodiscard]] std::string get_backup_path() const { return m_backup_path; }

    /**
     * @brief Restore vault from most recent backup
     * @return Expected void or VaultError
     *
     * Finds the most recent timestamped backup and restores it.
     * Current vault must be closed before calling.
     *
     * @note This replaces the current vault file with the backup
     */
    [[nodiscard]] KeepTower::VaultResult<> restore_from_most_recent_backup(const std::string& vault_path);

#ifdef HAVE_YUBIKEY_SUPPORT
    // YubiKey multi-key management

    /**
     * @brief Get list of configured YubiKeys for current vault
     * @return Vector of YubiKey entries (serial, name, added_at)
     *
     * @note Returns empty vector if vault not open or no YubiKeys configured
     */
    std::vector<keeptower::YubiKeyEntry> get_yubikey_list() const;

    /**
     * @brief Add a backup YubiKey to the vault
     * @param name Friendly name for the key (e.g., "Backup", "Office Key")
     * @return true if added successfully, false on error
     *
     * Adds the currently connected YubiKey as a backup. The key must be
     * programmed with the same HMAC secret as the primary key.
     *
     * @note Requires vault to be open and YubiKey-protected
     * @note Requires a YubiKey to be connected
     * @note Call save_vault() to persist changes
     */
    [[nodiscard]] bool add_backup_yubikey(const std::string& name);

    /**
     * @brief Remove a YubiKey from the vault's authorized list
     * @param serial Serial number of the key to remove
     * @return true if removed successfully, false on error
     *
     * @note Cannot remove the last remaining key
     * @note Requires vault to be open
     * @note Call save_vault() to persist changes
     */
    [[nodiscard]] bool remove_yubikey(const std::string& serial);

    /**
     * @brief Check if a YubiKey serial is authorized for this vault
     * @param serial Serial number to check
     * @return true if authorized, false otherwise
     */
    [[nodiscard]] bool is_yubikey_authorized(const std::string& serial) const;

    /**
     * @brief Check if current vault uses YubiKey authentication
     * @return true if YubiKey is required, false otherwise
     */
    bool is_using_yubikey() const { return m_yubikey_required; }

    /**
     * @brief Check if current user requires YubiKey authentication
     * @return true if current user's key slot requires YubiKey, false otherwise
     * @note For V2 vaults, checks current user's key slot. For V1 vaults, checks m_yubikey_required flag.
     */
    [[nodiscard]] bool current_user_requires_yubikey() const;
#endif

    /**
     * @brief Verify credentials against the current vault
     * @param password Password to verify
     * @param serial YubiKey serial number (if vault uses YubiKey)
     * @return true if credentials are valid, false otherwise
     *
     * @note Requires vault to be open
     */
    [[nodiscard]] bool verify_credentials(const Glib::ustring& password, const std::string& serial = "");

    /**
     * @brief Get current authenticated username (V2 vaults only)
     * @return Username if V2 vault is open and authenticated, empty string otherwise
     */
    [[nodiscard]] std::string get_current_username() const;

private:
    // Helper methods for open_vault() refactoring
#ifdef HAVE_YUBIKEY_SUPPORT
    KeepTower::VaultResult<> authenticate_yubikey(
        const KeepTower::VaultFileMetadata& metadata,
        std::vector<uint8_t>& encryption_key);
#endif
    KeepTower::VaultResult<keeptower::VaultData> decrypt_and_parse_vault(
        const std::vector<uint8_t>& ciphertext,
        const std::vector<uint8_t>& key,
        const std::vector<uint8_t>& iv);

    // Secure memory clearing and locking
    void secure_clear(std::vector<uint8_t>& data);
    void secure_clear(std::string& data);
    bool lock_memory(std::vector<uint8_t>& data);
    bool lock_memory(void* data, size_t size);  // Overload for std::array and raw pointers
    void unlock_memory(std::vector<uint8_t>& data);
    void unlock_memory(void* data, size_t size);  // Overload for std::array and raw pointers

    // Schema migration
    bool migrate_vault_schema();

    // State
    bool m_vault_open;
    bool m_modified;
    std::string m_current_vault_path;
    std::vector<uint8_t> m_encryption_key;
    std::vector<uint8_t> m_salt;

    // V2 Multi-User state
    bool m_is_v2_vault;                         // True if current vault is V2 format
    std::optional<KeepTower::VaultHeaderV2> m_v2_header;   // V2 header with security policy and key slots
    std::optional<KeepTower::UserSession> m_current_session;  // Current authenticated user session
    std::array<uint8_t, 32> m_v2_dek;           // V2 vault Data Encryption Key (wrapped in key slots)

    // Reed-Solomon error correction
    std::unique_ptr<ReedSolomon> m_reed_solomon;
    bool m_use_reed_solomon;
    uint8_t m_rs_redundancy_percent;
    bool m_fec_loaded_from_file;  // Track if FEC settings came from opened file

    // Backup configuration
    bool m_backup_enabled;
    int m_backup_count;
    std::string m_backup_path;  // Custom backup directory (empty=same as vault)

    bool m_memory_locked;  // Track if sensitive memory is locked

    // Thread safety
    mutable std::mutex m_vault_mutex;  // Protects vault data and encryption key

    // YubiKey configuration
    bool m_yubikey_required;           // Whether YubiKey is required for this vault
    std::string m_yubikey_serial;      // YubiKey serial number (for multi-key support)
    std::vector<uint8_t> m_yubikey_challenge;  // 64-byte challenge for this vault

    // In-memory vault data (protobuf)
    keeptower::VaultData m_vault_data;

    // Managers for specific responsibilities
    std::unique_ptr<KeepTower::AccountManager> m_account_manager;
    std::unique_ptr<KeepTower::GroupManager> m_group_manager;

    // Phase 2 Day 5: Service instances for orchestrator (lazy initialization)
    std::shared_ptr<KeepTower::VaultCryptoService> m_crypto_service;
    std::shared_ptr<KeepTower::VaultYubiKeyService> m_yubikey_service;
    std::shared_ptr<KeepTower::VaultFileService> m_file_service;

    /**
     * @brief Create and configure VaultCreationOrchestrator with services
     * @return Configured orchestrator instance
     *
     * Lazy-initializes service instances on first use and injects them
     * into a new orchestrator. Services are shared across orchestrator
     * instances to maintain consistent state.
     */
    std::unique_ptr<KeepTower::VaultCreationOrchestrator> create_orchestrator();

    // Vault file format constants
    static constexpr uint32_t VAULT_MAGIC = 0x4B505457;  // "KPTW" (KeepTower)
    static constexpr uint32_t VAULT_VERSION = 1;

    // Current vault PBKDF2 iterations (configurable per vault)
    int m_pbkdf2_iterations;

    /** @name FIPS-140-3 Global State (Thread-Safe)
     * @brief Process-wide FIPS mode tracking with atomic thread safety
     *
     * These static members track the FIPS provider state across all VaultManager
     * instances. They use std::atomic<bool> for lock-free, thread-safe access.
     *
     * **Thread Safety Guarantees:**
     * - Atomic loads/stores prevent data races
     * - Compare-exchange ensures single initialization
     * - Memory ordering: sequential consistency (default)
     *
     * **Initialization State Machine:**
     * 1. **Uninitialized:** All false, init_fips_mode() not called
     * 2. **Default Provider:** initialized=true, available=false, enabled=false
     * 3. **FIPS Available:** initialized=true, available=true, enabled=false
     * 4. **FIPS Enabled:** initialized=true, available=true, enabled=true
     *
     * @{
     */

    /**
     * @brief Tracks whether FIPS initialization has been performed
     *
     * Set to true after first successful call to init_fips_mode().
     * Used with compare_exchange to ensure single initialization.
     *
     * @invariant Once true, remains true for process lifetime
     */
    static std::atomic<bool> s_fips_mode_initialized;

    /**
     * @brief Tracks whether OpenSSL FIPS provider is available
     *
     * Set to true if FIPS provider loads successfully during init_fips_mode().
     * Remains false if FIPS module not installed or load fails.
     *
     * @invariant Can only be true if s_fips_mode_initialized is true
     */
    static std::atomic<bool> s_fips_mode_available;

    /**
     * @brief Tracks whether FIPS mode is currently active
     *
     * Set to true when FIPS provider is loaded and enabled.
     * Can be toggled via set_fips_mode() if provider is available.
     *
     * @invariant Can only be true if s_fips_mode_available is true
     */
    static std::atomic<bool> s_fips_mode_enabled;

    /** @} */ // end of FIPS-140-3 global state
};

#endif // VAULTMANAGER_H
