// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2024 KeepTower Contributors
// File: src/core/services/VaultFileService.h

#ifndef KEEPTOWER_VAULT_FILE_SERVICE_H
#define KEEPTOWER_VAULT_FILE_SERVICE_H

#include "../VaultError.h"
#include "../MultiUserTypes.h"
#include <cstdint>
#include <optional>
#include <array>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace KeepTower {

/**
 * @brief Manager-facing storage facade for V2 vault workflows
 *
 * VaultFileService is the high-level storage facade used by VaultManager and
 * VaultCreationOrchestrator. It defines the storage contract expected by the
 * core workflow layer while delegating lower-level storage primitives to the
 * keeptower-storage library where appropriate.
 *
 * **Design Principles:**
 * - **Facade Role**: Presents manager-friendly V2 storage operations
 * - **Stateless**: All methods are static or operate on passed parameters
 * - **Atomic Operations**: Writes preserve the validated V2 service semantics
 * - **Delegation Friendly**: Backup helpers reuse lower-level storage utilities
 * - **Compatibility Aware**: Preserves existing caller-visible behavior
 *
 * **Responsibilities:**
 * 1. Reading V2 vault files from disk for manager/orchestrator flows
 * 2. Building and writing V2 vault files atomically with the existing service contract
 * 3. Parsing manager-facing V2 header metadata for workflow code
 * 4. Format version detection for workflow code
 * 5. Backup creation, restoration, listing, and cleanup via the storage layer
 * 6. Preserving service-specific error mapping and behavior guarantees
 *
 * **NOT Responsible For:**
 * - Encryption/decryption (VaultCryptoService)
 * - YubiKey operations (VaultYubiKeyService)
 * - Raw compatibility-oriented storage primitives (see VaultIO)
 * - Protobuf vault-data serialization or schema migration (VaultDataService)
 * - Business logic (VaultManager)
 *
 * @section file_formats File Formats
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
 * Backup lifecycle operations are exposed through this facade, but the lower-level
 * naming and enumeration logic is implemented in VaultIO. The facade preserves the
 * manager-facing contract while the storage layer handles compatibility details.
 *
 * @section vault_file_service_security Security Considerations
 *
 * - Files written with 0600 permissions (owner read/write only)
 * - Atomic rename ensures no partial writes visible
 * - Directory fsync for durability guarantees
 * - No sensitive data cached in memory
 * - All errors properly propagated
 *
 * @section vault_file_service_usage Usage Example
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
 * - **Format Detection**: Valid V2 vs invalid/unsupported inputs
 * - **Error Handling**: Permission errors, disk full, etc.
 *
 * @note This facade is intentionally V2-oriented. Compatibility-heavy raw file
 *       behavior remains in VaultIO.

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
    /**
     * @brief Manager-facing V2 header metadata extracted from file bytes.
     *
     * This is the reduced view of the on-disk V2 header exposed to workflow
     * code. It intentionally hides the lower-level VaultFormatV2::V2FileHeader
     * type while preserving the fields needed for authentication and payload
     * decryption.
     */
    struct V2VaultMetadata {
        uint32_t pbkdf2_iterations = 0;
        uint8_t fec_redundancy_percent = 0;
        VaultHeaderV2 vault_header;
        std::array<uint8_t, 32> data_salt{};
        std::array<uint8_t, 12> data_iv{};
        size_t data_offset = 0;
    };

    // ========================================================================
    // File Reading Operations
    // ========================================================================

    /**
     * @brief Read vault file from disk
     *
        * Reads a complete vault file into memory (V2 only).
     *
     * @param path Absolute path to vault file
     * @param data Output buffer for file contents
        * @param pbkdf2_iterations Reserved (always set to 0 for V2)
     * @return VaultResult<void> Success or VaultError
     *
        * @note V1 vault files are no longer supported and will return UnsupportedVersion.
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
        * Writes a V2 vault file atomically. V1 vault files are not supported.
     *
     * @param path Absolute path to target vault file
     * @param data Complete vault data to write
        * @param is_v2_vault Must be true (V2). If false, returns UnsupportedVersion.
        * @param pbkdf2_iterations Reserved (ignored)
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

    /**
     * @brief Build and write a V2 vault file from encrypted components
     *
     * Assembles the V2 on-disk header, clears plaintext usernames before
     * serialization, appends encrypted payload data, and persists the complete
     * file atomically using the standard vault write semantics.
     *
     * @param path Absolute path to target vault file
     * @param vault_header Vault header to serialize
     * @param pbkdf2_iterations PBKDF2 iteration count to store in the file header
     * @param data_salt Stored data salt bytes (copied up to 32 bytes)
     * @param data_iv Stored data IV bytes (must be exactly 12 bytes)
     * @param ciphertext Encrypted vault payload
     * @param enable_header_fec Whether to enable header FEC encoding
     * @param data_fec_redundancy User-selected data FEC redundancy percentage
     * @return VaultResult<void> Success or VaultError
     */
    [[nodiscard]] static VaultResult<> write_v2_vault(
        const std::string& path,
        const VaultHeaderV2& vault_header,
        uint32_t pbkdf2_iterations,
        std::span<const uint8_t> data_salt,
        std::span<const uint8_t> data_iv,
        const std::vector<uint8_t>& ciphertext,
        bool enable_header_fec = true,
        uint8_t data_fec_redundancy = 0);

    /**
     * @brief Parse V2 vault metadata from already-loaded file bytes
     *
     * Extracts the manager-facing header information required for authentication
     * and decrypting the payload while hiding the lower-level format type.
     *
     * @param file_data Complete V2 vault file bytes
     * @return V2VaultMetadata on success, VaultError on parse failure
     */
    [[nodiscard]] static VaultResult<V2VaultMetadata> read_v2_metadata(
        const std::vector<uint8_t>& file_data);

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

    /**
     * @brief Check whether a vault requires YubiKey authentication.
     *
     * Reads and parses the V2 vault header to determine whether YubiKey is
     * required by policy or by any active enrolled user slot.
     *
     * @param path Absolute path to vault file
     * @param serial Output parameter for an enrolled YubiKey serial, if present
     * @return true if YubiKey is required, false otherwise or on error
     */
    [[nodiscard]] static bool check_vault_requires_yubikey(
        const std::string& path,
        std::string& serial);

    // ========================================================================
    // Backup Management
    // ========================================================================

    /**
    * @brief Create backup of vault file using the storage-layer backup policy
     *
    * Creates a backup copy of the vault file and returns the created backup path.
    * The exact on-disk naming scheme is delegated to the lower-level storage layer.
     *
     * @param vault_path Absolute path to vault file to backup
     * @param backup_dir Optional custom backup directory (empty = same as vault)
     * @return VaultResult<std::string> Backup file path on success, error otherwise
     *
    * @note Does not modify original vault file
    * @note Backup includes complete file contents (headers, data, FEC)
     */
    [[nodiscard]] static VaultResult<std::string> create_backup(
        std::string_view vault_path,
        std::string_view backup_dir = "");

    /**
     * @brief Restore vault from most recent backup
     *
        * Finds the most recent compatible backup file and restores it to the original
        * vault location using the storage-layer implementation.
     *
     * @param vault_path Absolute path to vault file to restore
     * @param backup_dir Optional custom backup directory (empty = same as vault)
     * @return VaultResult<> Success or error
     *
        * @note Fails if no backups exist
     */
    [[nodiscard]] static VaultResult<> restore_from_backup(
        std::string_view vault_path,
        std::string_view backup_dir = "");

    /**
     * @brief List all backup files for a vault
     *
        * Returns backup file paths sorted newest first according to the storage layer.
     *
     * @param vault_path Absolute path to vault file
     * @param backup_dir Optional custom backup directory (empty = same as vault)
     * @return std::vector<std::string> List of backup paths (sorted, newest first)
     *
     * @note Returns empty vector if no backups exist
        * @note Both legacy and newer compatible backup naming patterns may be recognized
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
