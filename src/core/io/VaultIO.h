// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file VaultIO.h
 * @brief Secure file I/O operations for vault persistence
 *
 * This file contains the VaultIO utility class which handles all file system
 * operations for vault storage, including atomic writes, backup management,
 * and secure file permissions.
 */

#ifndef KEEPTOWER_VAULTIO_H
#define KEEPTOWER_VAULTIO_H

#include <string>
#include <string_view>
#include <vector>
#include <cstdint>
#include "VaultError.h"

namespace KeepTower {

/**
 * @brief Utility class for secure vault file I/O operations
 *
 * VaultIO provides static methods for reading, writing, and managing vault files
 * with atomic operations, backup creation/rotation, and secure permissions.
 *
 * @section features Features
 * - Atomic file writes using temporary files and rename
 * - Secure file permissions (0600 on Unix systems)
 * - Timestamped backup creation and management
 * - Directory synchronization for durability
 * - Support for both V1 and V2 vault formats
 *
 * @section security Security Considerations
 * - Files written with owner-only read/write permissions
 * - Atomic rename ensures no partial writes visible
 * - Directory fsync ensures durability on power loss
 * - Backup files automatically rotated to prevent disk exhaustion
 *
 * @section limitations Limitations
 * - No file locking mechanism implemented. Concurrent writes from multiple processes
 *   or threads may result in "Last Writer Wins" race conditions.
 *   TODO: Implement file locking (flock/fcntl) to prevent data loss during concurrent access.
 *
 * @section usage Usage Example
 * @code
 * std::vector<uint8_t> data = {...};
 *
 * // Write vault file atomically
 * if (!VaultIO::write_file("/path/vault.dat", data, false)) {
 *     // Handle error
 * }
 *
 * // Create backup before modifying
 * auto backup_result = VaultIO::create_backup("/path/vault.dat");
 * VaultIO::cleanup_old_backups("/path/vault.dat", 5);
 * @endcode
 *
 * @note This is a utility class with deleted constructors (static methods only)
 */
class VaultIO {
public:
    /**
     * @brief Default PBKDF2 iteration count (OWASP recommended minimum 2023)
     */
    static constexpr int DEFAULT_PBKDF2_ITERATIONS = 600000;

    /**
     * @brief Magic number identifying KeepTower vault files
     */
    static constexpr uint32_t VAULT_MAGIC = 0x4B545654;  // "KTVT" in hex

    /**
     * @brief Current vault format version (V1 legacy format)
     */
    static constexpr uint32_t VAULT_VERSION = 1;

    /**
     * @brief Read a vault file from disk
     *
     * Reads the entire vault file into memory, automatically detecting and parsing
     * the file format version. For V1 vaults, strips the header before returning
     * data. For V2 vaults, returns the complete file including header.
     *
     * @param path Absolute path to the vault file
     * @param data Output buffer for file contents
     * @param is_v2_vault Flag indicating if vault is V2 format (affects header handling)
     * @param pbkdf2_iterations Output parameter for PBKDF2 iterations from header
     * @return true if file read successfully, false on error
     *
     * @throws No exceptions (uses C++ iostream error handling)
     *
     * @note For files without magic header (legacy format), assumes DEFAULT_PBKDF2_ITERATIONS
     * @note Output data vector is resized to fit file contents
     *
     * @par Example:
     * @code
     * std::vector<uint8_t> vault_data;
     * int iterations = 0;
     * if (VaultIO::read_file("/path/vault.dat", vault_data, false, iterations)) {
     *     std::cout << "Vault uses " << iterations << " PBKDF2 iterations\n";
     * }
     * @endcode
     */
    [[nodiscard]] static bool read_file(
        const std::string& path,
        std::vector<uint8_t>& data,
        bool is_v2_vault,
        int& pbkdf2_iterations);

    /**
     * @brief Write a vault file to disk atomically
     *
     * Performs an atomic write operation by first writing to a temporary file,
     * then using rename(2) to atomically replace the target file. This ensures
     * that the vault file is never left in a partially-written state.
     *
     * For V1 vaults, prepends a file header with magic number, version, and
     * PBKDF2 iterations. For V2 vaults, writes data directly (header already
     * included in data buffer).
     *
     * @param path Absolute path to the vault file
     * @param data Complete vault data (with or without header depending on format)
     * @param is_v2_vault true for V2 format (data contains header), false for V1
     * @param pbkdf2_iterations PBKDF2 iteration count (V1 only, written to header)
     * @return true if file written successfully, false on error
     *
     * @throws No exceptions (catches and logs all errors)
     *
     * @post File permissions set to 0600 (owner read/write only)
     * @post Parent directory synchronized to ensure durability
     * @post Temporary file removed on failure
     *
     * @par Security:
     * - Temporary file written with secure permissions
     * - Atomic rename prevents partial writes from being visible
     * - Directory fsync ensures metadata changes are durable
     *
     * @par Example:
     * @code
     * std::vector<uint8_t> encrypted_data = {...};
     * if (!VaultIO::write_file("/path/vault.dat", encrypted_data, false, 600000)) {
     *     std::cerr << "Failed to save vault\n";
     * }
     * @endcode
     */
    [[nodiscard]] static bool write_file(
        const std::string& path,
        const std::vector<uint8_t>& data,
        bool is_v2_vault,
        int pbkdf2_iterations);

    /**
     * @brief Create a timestamped backup of a vault file
     *
     * Creates a backup with format: <path>.backup.<timestamp>
     * Timestamp format: YYYYmmdd_HHMMSS_milliseconds
     *
     * @param path Path to the vault file to backup
     * @param backup_dir Optional custom backup directory (empty=same as vault)
     * @return VaultResult<> indicating success or error
     *
     * @throws No exceptions (catches filesystem errors)
     *
     * @note Non-fatal operation - returns success even if source file doesn't exist
     * @note Overwrites existing backup with same timestamp (unlikely but possible)
     * @note If backup_dir specified, creates directory if it doesn't exist
     *
     * @par Example:
     * @code
     * auto result = VaultIO::create_backup("/path/vault.dat", "/backups");
     * if (!result) {
     *     std::cerr << "Backup failed but continuing...\n";
     * }
     * @endcode
     */
    [[nodiscard]] static VaultResult<> create_backup(std::string_view path, std::string_view backup_dir = "");

    /**
     * @brief Restore vault from most recent backup
     *
     * Finds the most recent timestamped backup file and restores it by copying
     * over the current vault file. Falls back to legacy .backup format if no
     * timestamped backups exist.
     *
     * @param path Path to the vault file to restore
     * @return VaultResult<> indicating success or FileNotFound/FileReadFailed
     *
     * @throws No exceptions (catches filesystem errors)
     *
     * @warning Overwrites the current vault file if backup exists
     *
     * @par Example:
     * @code
     * auto result = VaultIO::restore_from_backup("/path/vault.dat");
     * if (!result) {
     *     std::cerr << "Restore failed: " << static_cast<int>(result.error()) << "\n";
     * }
     * @endcode
     */
    [[nodiscard]] static VaultResult<> restore_from_backup(std::string_view path);

    /**
     * @brief List all backup files for a vault, sorted newest first
     *
     * Searches for files matching the pattern: <path>.backup.*
     * Returns paths sorted by timestamp (newest first).
     *
     * @param path Path to the vault file
     * @param backup_dir Optional custom backup directory (empty=same as vault)
     * @return Vector of absolute backup file paths, sorted newest to oldest
     *
     * @throws No exceptions (catches filesystem errors)
     *
     * @note Returns empty vector if directory doesn't exist or on error
     * @note Ignores non-regular files (directories, symlinks, etc.)
     *
     * @par Example:
     * @code
     * auto backups = VaultIO::list_backups("/path/vault.dat", "/backups");
     * std::cout << "Found " << backups.size() << " backup files\n";
     * if (!backups.empty()) {
     *     std::cout << "Most recent: " << backups[0] << "\n";
     * }
     * @endcode
     */
    [[nodiscard]] static std::vector<std::string> list_backups(std::string_view path, std::string_view backup_dir = "");

    /**
     * @brief Delete old backup files, keeping only the N most recent
     *
     * Deletes backup files older than the specified count, preserving the most
     * recent backups. Uses list_backups() to find and sort backup files.
     *
     * @param path Path to the vault file
     * @param max_backups Maximum number of backup files to keep (must be >= 1)
     * @param backup_dir Optional custom backup directory (empty=same as vault)
     *
     * @throws No exceptions (logs warnings on deletion failure)
     *
     * @note If max_backups < 1, no cleanup is performed
     * @note Deletion failures are logged but don't stop cleanup of remaining files
     *
     * @par Example:
     * @code
     * // Keep only the 5 most recent backups
     * VaultIO::cleanup_old_backups("/path/vault.dat", 5, "/backups");
     * @endcode
     */
    static void cleanup_old_backups(std::string_view path, int max_backups, std::string_view backup_dir = "");

    // Deleted constructors (utility class)
    VaultIO() = delete;
    ~VaultIO() = delete;
    VaultIO(const VaultIO&) = delete;
    VaultIO& operator=(const VaultIO&) = delete;
    VaultIO(VaultIO&&) = delete;
    VaultIO& operator=(VaultIO&&) = delete;
};

}  // namespace KeepTower

#endif  // KEEPTOWER_VAULTIO_H
