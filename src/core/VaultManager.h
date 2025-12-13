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
#include <glibmm.h>
#include "record.pb.h"
#include "VaultError.h"
#include "ReedSolomon.h"

// Forward declare for conditional compilation
#if __has_include("config.h")
#include "config.h"
#endif

#ifdef HAVE_YUBIKEY_SUPPORT
#include "YubiKeyManager.h"
#endif

// Forward declarations for OpenSSL types
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
    // Public constants for vault format and testing
    static constexpr uint8_t FLAG_RS_ENABLED = 0x01;      // Reed-Solomon error correction enabled
    static constexpr uint8_t FLAG_YUBIKEY_REQUIRED = 0x02; // YubiKey required for vault access
    static constexpr size_t SALT_LENGTH = 32;
    static constexpr size_t KEY_LENGTH = 32;  // 256 bits
    static constexpr size_t IV_LENGTH = 12;   // GCM recommended
    static constexpr int DEFAULT_PBKDF2_ITERATIONS = 100000;  // NIST recommendation
    static constexpr size_t YUBIKEY_CHALLENGE_SIZE = 64;  // YubiKey challenge size
    static constexpr size_t YUBIKEY_RESPONSE_SIZE = 20;   // HMAC-SHA1 response size
    static constexpr int YUBIKEY_TIMEOUT_MS = 15000;     // YubiKey operation timeout (15 seconds)
    static constexpr int DEFAULT_BACKUP_COUNT = 5;        // Number of backup files to maintain
    static constexpr int DEFAULT_RS_REDUNDANCY = 10;      // Default Reed-Solomon redundancy percentage
    static constexpr size_t VAULT_HEADER_SIZE = 6;        // flags(1) + redundancy(1) + original_size(4)
    static constexpr uint8_t MIN_RS_REDUNDANCY = 5;       // Minimum Reed-Solomon redundancy percentage
    static constexpr uint8_t MAX_RS_REDUNDANCY = 50;      // Maximum Reed-Solomon redundancy percentage
    static constexpr size_t MAX_VAULT_SIZE = 100 * 1024 * 1024;  // 100MB maximum vault size
    static constexpr size_t BIGENDIAN_SHIFT_24 = 24;      // Bit shift for big-endian byte 0
    static constexpr size_t BIGENDIAN_SHIFT_16 = 16;      // Bit shift for big-endian byte 1
    static constexpr size_t BIGENDIAN_SHIFT_8 = 8;        // Bit shift for big-endian byte 2

    VaultManager();
    ~VaultManager();

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
     * @return true if saved successfully, false on error
     *
     * Performs atomic save operation:
     * 1. Creates backup of existing vault (.backup)
     * 2. Writes to temporary file
     * 3. Renames temporary to actual vault
     *
     * @note Requires vault to be open
     */
    [[nodiscard]] bool save_vault();

    /**
     * @brief Close vault and clear sensitive data
     * @return true if closed successfully, false on error
     *
     * Securely erases encryption keys and other sensitive data from memory.
     */
    [[nodiscard]] bool close_vault();

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

    // State queries

    /**
     * @brief Check if vault is currently open
     * @return true if vault is open, false otherwise
     */
    bool is_vault_open() const { return m_vault_open; }
    std::string get_current_vault_path() const { return m_current_vault_path; }
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

    // Backup configuration

    /**
     * @brief Enable or disable automatic timestamped backups
     * @param enable true to enable backups, false to disable
     */
    void set_backup_enabled(bool enable) { m_backup_enabled = enable; }

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
     * @brief Verify credentials against the current vault
     * @param password Password to verify
     * @param serial YubiKey serial number (if vault uses YubiKey)
     * @return true if credentials are valid, false otherwise
     *
     * @note Requires vault to be open
     */
    [[nodiscard]] bool verify_credentials(const Glib::ustring& password, const std::string& serial = "");
#endif

private:
    // Helper structures for vault parsing
    struct VaultFileMetadata {
        std::vector<uint8_t> salt;
        std::vector<uint8_t> iv;
        bool has_fec = false;
        uint8_t fec_redundancy = 0;
        bool requires_yubikey = false;
        std::string yubikey_serial;
        std::vector<uint8_t> yubikey_challenge;
    };

    struct ParsedVaultData {
        VaultFileMetadata metadata;
        std::vector<uint8_t> ciphertext;
    };

    // Helper methods for open_vault() refactoring
    KeepTower::VaultResult<ParsedVaultData> parse_vault_format(const std::vector<uint8_t>& file_data);
    KeepTower::VaultResult<std::vector<uint8_t>> decode_with_reed_solomon(
        const std::vector<uint8_t>& encoded_data,
        uint32_t original_size,
        uint8_t redundancy);
#ifdef HAVE_YUBIKEY_SUPPORT
    KeepTower::VaultResult<> authenticate_yubikey(
        const VaultFileMetadata& metadata,
        std::vector<uint8_t>& encryption_key);
#endif
    KeepTower::VaultResult<keeptower::VaultData> decrypt_and_parse_vault(
        const std::vector<uint8_t>& ciphertext,
        const std::vector<uint8_t>& key,
        const std::vector<uint8_t>& iv);

    // Cryptographic operations
    bool derive_key(const Glib::ustring& password,
                    std::span<const uint8_t> salt,
                    std::vector<uint8_t>& key);

    bool encrypt_data(std::span<const uint8_t> plaintext,
                      std::span<const uint8_t> key,
                      std::vector<uint8_t>& ciphertext,
                      std::vector<uint8_t>& iv);

    bool decrypt_data(std::span<const uint8_t> ciphertext,
                      std::span<const uint8_t> key,
                      std::span<const uint8_t> iv,
                      std::vector<uint8_t>& plaintext);

    // File I/O
    bool read_vault_file(const std::string& path, std::vector<uint8_t>& data);
    bool write_vault_file(const std::string& path, const std::vector<uint8_t>& data);

    // Generate random bytes for salt and IV
    std::vector<uint8_t> generate_random_bytes(size_t length);

    // Secure memory clearing and locking
    void secure_clear(std::vector<uint8_t>& data);
    void secure_clear(std::string& data);
    bool lock_memory(std::vector<uint8_t>& data);
    void unlock_memory(std::vector<uint8_t>& data);

    // Backup management
    KeepTower::VaultResult<> create_backup(std::string_view path);
    KeepTower::VaultResult<> restore_from_backup(std::string_view path);

    // Schema migration
    bool migrate_vault_schema();
    void cleanup_old_backups(std::string_view path, int max_backups);
    std::vector<std::string> list_backups(std::string_view path);

    // State
    bool m_vault_open;
    bool m_modified;
    std::string m_current_vault_path;
    std::vector<uint8_t> m_encryption_key;
    std::vector<uint8_t> m_salt;

    // Reed-Solomon error correction
    std::unique_ptr<ReedSolomon> m_reed_solomon;
    bool m_use_reed_solomon;
    uint8_t m_rs_redundancy_percent;
    bool m_fec_loaded_from_file;  // Track if FEC settings came from opened file

    // Backup configuration
    bool m_backup_enabled;
    int m_backup_count;

    bool m_memory_locked;  // Track if sensitive memory is locked

    // Thread safety
    mutable std::mutex m_vault_mutex;  // Protects vault data and encryption key

    // YubiKey configuration
    bool m_yubikey_required;           // Whether YubiKey is required for this vault
    std::string m_yubikey_serial;      // YubiKey serial number (for multi-key support)
    std::vector<uint8_t> m_yubikey_challenge;  // 64-byte challenge for this vault

    // In-memory vault data (protobuf)
    keeptower::VaultData m_vault_data;

    // Vault file format constants
    static constexpr uint32_t VAULT_MAGIC = 0x4B505457;  // "KPTW" (KeepTower)
    static constexpr uint32_t VAULT_VERSION = 1;

    // Current vault PBKDF2 iterations (configurable per vault)
    int m_pbkdf2_iterations;
};

#endif // VAULTMANAGER_H
