// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2024 KeepTower Contributors
// File: src/core/services/VaultFileService.h

#ifndef KEEPTOWER_VAULT_FILE_SERVICE_H
#define KEEPTOWER_VAULT_FILE_SERVICE_H

#include "../VaultError.h"
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace KeepTower {

/**
 * @brief Service for vault file I/O operations
 *
 * VaultFileService encapsulates ALL file system operations related to vault files,
 * following the Single Responsibility Principle. This service is responsible ONLY
 * for reading, writing, backup management, and format detection - no cryptographic
 * operations or business logic.
 *
 * **Design Principles:**
 * - **SRP Compliance**: ONLY file I/O operations, nothing else
 * - **Stateless**: All methods are static or operate on passed parameters
 * - **Thread-Safe**: All operations are thread-safe (no shared mutable state)
 * - **Atomic Operations**: Writes use temporary files + rename for atomicity
 * - **Error Recovery**: FEC-aware reading with recovery support
 *
 * **Responsibilities:**
 * 1. Reading vault files from disk (with FEC recovery)
 * 2. Writing vault files atomically (temp file + rename)
 * 3. Format version detection (V1 vs V2)
 * 4. Backup creation and restoration
 * 5. Backup rotation and cleanup
 * 6. Secure file permissions (0600 on Unix)
 *
 * **NOT Responsible For:**
 * - Encryption/decryption (VaultCryptoService)
 * - YubiKey operations (VaultYubiKeyService)
 * - Vault parsing/serialization (VaultFormat classes)
 * - Business logic (VaultManager)
 *
 * @section file_formats File Formats
 *
 * **V1 Format:**
 * ```
 * [Magic: 4 bytes] [Version: 4 bytes] [PBKDF2 Iterations: 4 bytes]
 * [Encryption Data: variable]
 * ```
 *
 * **V2 Format:**
 * ```
 * [Full V2 Header with FEC: variable] [Encrypted Vault Data: variable]
 * ```
 *
 * @section atomic_writes Atomic Write Operations
 *
 * All writes follow the pattern:
 * 1. Write to temporary file (path + ".tmp")
 * 2. Set secure permissions (0600)
 * 3. Flush and fsync
 * 4. Rename temporary file to target (atomic)
 * 5. Fsync directory (durability guarantee)
 *
 * This ensures vault files are never left in a corrupted state, even during
 * power failures or system crashes.
 *
 * @section backup_management Backup Management
 *
 * Backups are created with ISO 8601 timestamps:
 * - Format: `vault_name.YYYY-MM-DDTHH-MM-SS.backup`
 * - Example: `myvault.vault.2026-01-10T18-30-45.backup`
 * - Automatic cleanup keeps only N most recent backups
 *
 * @section security Security Considerations
 *
 * - Files written with 0600 permissions (owner read/write only)
 * - Atomic rename ensures no partial writes visible
 * - Directory fsync for durability guarantees
 * - No sensitive data cached in memory
 * - All errors properly propagated
 *
 * @section usage Usage Example
 *
 * @code
 * // Read vault file
 * std::vector<uint8_t> data;
 * int iterations;
 * auto read_result = VaultFileService::read_vault_file("/path/vault.vault", data, iterations);
 * if (!read_result) {
 *     // Handle error: read_result.error()
 * }
 *
 * // Detect format version
 * auto version = VaultFileService::detect_vault_version(data);
 * if (version == 2) {
 *     // V2 vault
 * }
 *
 * // Write vault file atomically
 * std::vector<uint8_t> new_data = {...};
 * auto write_result = VaultFileService::write_vault_file("/path/vault.vault", new_data, true);
 * if (!write_result) {
 *     // Handle error
 * }
 *
 * // Create backup
 * auto backup_result = VaultFileService::create_backup("/path/vault.vault");
 * if (backup_result) {
 *     std::cout << "Backup created: " << backup_result.value() << std::endl;
 * }
 *
 * // Cleanup old backups (keep only 5)
 * VaultFileService::cleanup_old_backups("/path/vault.vault", 5);
 * @endcode
 *
 * @section testing Testing Strategy
 *
 * - **Unit Tests**: File operations with temporary test files
 * - **Integration Tests**: End-to-end with actual VaultManager
 * - **Atomic Write Tests**: Verify no partial writes
 * - **Backup Tests**: Creation, restoration, rotation
 * - **Format Detection**: V1 vs V2 vs invalid
 * - **Error Handling**: Permission errors, disk full, etc.
 *
 * @note This service is NOT thread-safe for concurrent writes to the same file
 *       (by design - vault files should only be accessed by one process)
 *
 * @note All paths must be absolute; relative paths may cause undefined behavior
 *
 * @since Version 0.3.2 (Phase 1 Refactoring)
 * @author KeepTower Development Team
 */
class VaultFileService {
public:
    // ========================================================================
    // File Reading Operations
    // ========================================================================

    /**
     * @brief Read vault file from disk
     *
     * Reads a complete vault file into memory. For V1 vaults, extracts the
     * PBKDF2 iteration count from the header. For V2 vaults, reads the entire
     * file including FEC-protected header.
     *
     * @param path Absolute path to vault file
     * @param data Output buffer for file contents
     * @param pbkdf2_iterations Output for V1 PBKDF2 iterations (0 for V2)
     * @return VaultResult<void> Success or VaultError
     *
     * @note For V2 vaults, pbkdf2_iterations is set to 0 (not used)
     * @note data will contain complete file contents (including headers)
     */
    [[nodiscard]] static VaultResult<> read_vault_file(
        const std::string& path,
        std::vector<uint8_t>& data,
        int& pbkdf2_iterations);

    // ========================================================================
    // File Writing Operations
    // ========================================================================

    /**
     * @brief Write vault file atomically to disk
     *
     * Performs an atomic write operation using temporary file + rename.
     * For V1 vaults, prepends file header. For V2 vaults, writes data
     * directly (header already included).
     *
     * @param path Absolute path to target vault file
     * @param data Complete vault data to write
     * @param is_v2_vault true if V2 format, false if V1
     * @param pbkdf2_iterations PBKDF2 iterations for V1 header (ignored for V2)
     * @return VaultResult<void> Success or VaultError
     *
     * @note Automatically sets file permissions to 0600 (owner only)
     * @note Uses fsync for durability guarantees
     * @note Never leaves partial writes visible
     */
    [[nodiscard]] static VaultResult<> write_vault_file(
        const std::string& path,
        const std::vector<uint8_t>& data,
        bool is_v2_vault,
        int pbkdf2_iterations = 0);

    // ========================================================================
    // Format Detection
    // ========================================================================

    /**
     * @brief Detect vault format version from file contents
     *
     * Examines file data to determine if it's a V1 or V2 vault.
     * Uses magic numbers and header structure to identify format.
     *
     * @param data File contents to examine
     * @return std::optional<uint32_t> Version number (1 or 2), nullopt if invalid
     *
     * @note Returns nullopt for corrupted or non-vault files
     * @note Does not validate file integrity, only format identification
     */
    [[nodiscard]] static std::optional<uint32_t> detect_vault_version(
        const std::vector<uint8_t>& data);

    /**
     * @brief Detect vault format version from file path
     *
     * Convenience method that reads file and detects version in one call.
     *
     * @param path Absolute path to vault file
     * @return std::optional<uint32_t> Version number (1 or 2), nullopt if invalid
     *
     * @note Returns nullopt if file cannot be read or format is invalid
     */
    [[nodiscard]] static std::optional<uint32_t> detect_vault_version_from_file(
        const std::string& path);

    // ========================================================================
    // Backup Management
    // ========================================================================

    /**
     * @brief Create timestamped backup of vault file
     *
     * Creates a backup copy of the vault file with ISO 8601 timestamp.
     * Backup is created in the same directory as the vault file unless
     * a different backup directory is specified.
     *
     * @param vault_path Absolute path to vault file to backup
     * @param backup_dir Optional custom backup directory (empty = same as vault)
     * @return VaultResult<std::string> Backup file path on success, error otherwise
     *
     * @note Backup format: `vault_name.YYYY-MM-DDTHH-MM-SS.backup`
     * @note Does not modify original vault file
     * @note Backup includes complete file (headers, data, FEC)
     */
    [[nodiscard]] static VaultResult<std::string> create_backup(
        std::string_view vault_path,
        std::string_view backup_dir = "");

    /**
     * @brief Restore vault from most recent backup
     *
     * Finds the most recent backup file and restores it to the original
     * vault location. Original vault is moved to .old before restoration.
     *
     * @param vault_path Absolute path to vault file to restore
     * @param backup_dir Optional custom backup directory (empty = same as vault)
     * @return VaultResult<> Success or error
     *
     * @note Original vault moved to `vault_path.old` for safety
     * @note Fails if no backups exist
     * @note Automatically cleans up .old file after successful restoration
     */
    [[nodiscard]] static VaultResult<> restore_from_backup(
        std::string_view vault_path,
        std::string_view backup_dir = "");

    /**
     * @brief List all backup files for a vault
     *
     * Returns list of backup file paths sorted by timestamp (newest first).
     *
     * @param vault_path Absolute path to vault file
     * @param backup_dir Optional custom backup directory (empty = same as vault)
     * @return std::vector<std::string> List of backup paths (sorted, newest first)
     *
     * @note Returns empty vector if no backups exist
     * @note Backup files must match pattern: `basename.YYYY-MM-DDTHH-MM-SS.backup`
     */
    [[nodiscard]] static std::vector<std::string> list_backups(
        std::string_view vault_path,
        std::string_view backup_dir = "");

    /**
     * @brief Remove old backups, keeping only N most recent
     *
     * Automatically deletes old backup files, keeping only the specified
     * number of most recent backups. Useful for preventing disk exhaustion.
     *
     * @param vault_path Absolute path to vault file
     * @param max_backups Maximum number of backups to keep (must be > 0)
     * @param backup_dir Optional custom backup directory (empty = same as vault)
     *
     * @note Non-fatal errors (e.g., permission issues) are logged but not thrown
     * @note If max_backups = 5, keeps 5 newest and deletes older ones
     * @note Never deletes the vault file itself, only .backup files
     */
    static void cleanup_old_backups(
        std::string_view vault_path,
        int max_backups,
        std::string_view backup_dir = "");

    // ========================================================================
    // File System Utilities (Private Implementation Details)
    // ========================================================================

    /**
     * @brief Check if file exists and is readable
     *
     * @param path File path to check
     * @return true if file exists and is readable, false otherwise
     */
    [[nodiscard]] static bool file_exists(const std::string& path);

    /**
     * @brief Get file size in bytes
     *
     * @param path File path to query
     * @return File size in bytes, 0 if file doesn't exist or error
     */
    [[nodiscard]] static size_t get_file_size(const std::string& path);
};

} // namespace KeepTower

#endif // KEEPTOWER_VAULT_FILE_SERVICE_H
