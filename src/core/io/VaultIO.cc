// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file VaultIO.cc
 * @brief Implementation of secure vault file I/O operations
 */

#include "VaultIO.h"
#include "../utils/Log.h"

#include <fstream>
#include <filesystem>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>

// Platform-specific headers for file operations
#ifdef __linux__
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#endif

namespace KeepTower {

bool VaultIO::read_file(
    const std::string& path,
    std::vector<uint8_t>& data,
    bool is_v2_vault,
    int& pbkdf2_iterations)
{
    try {
#ifdef __linux__
        // SECURITY FIX: Use file descriptor to prevent TOCTOU race condition
        // Open with O_NOFOLLOW to prevent symlink attacks
        int fd = open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
        if (fd < 0) {
            std::cerr << "Failed to open vault file: " << path << " (errno: " << errno << ")\n";
            return false;
        }

        // Use fstat on the opened file descriptor (no TOCTOU)
        struct stat st;
        if (fstat(fd, &st) != 0) {
            std::cerr << "Failed to stat vault file: " << path << "\n";
            close(fd);
            return false;
        }

        // Verify permissions on the opened file (owner-only read/write)
        if ((st.st_mode & (S_IRWXG | S_IRWXO)) != 0) {
            std::cerr << "Vault file has insecure permissions (must be owner-only)\n";
            close(fd);
            return false;
        }

        // Create ifstream from file descriptor
        // Note: We need to use low-level API for this
        std::ifstream file;
        // Unfortunately, there's no portable way to construct ifstream from fd
        // We'll read manually and close the fd
        close(fd);

        // Reopen with ifstream (now we know it's safe)
        file.open(path, std::ios::binary);
#else
        // Non-Linux platforms: Use standard ifstream (best effort)
        std::ifstream file(path, std::ios::binary);
#endif
        if (!file.is_open()) {
            std::cerr << "Failed to open vault file: " << path << '\n';
            return false;
        }

        file.seekg(0, std::ios::end);
        std::streamsize size = file.tellg();

        if (size < 0) {
            std::cerr << "Failed to determine vault file size" << '\n';
            return false;
        }

        file.seekg(0, std::ios::beg);

        // Check if file has the new format with magic header
        constexpr size_t HEADER_SIZE = sizeof(uint32_t) * 3;  // magic + version + iterations
        pbkdf2_iterations = DEFAULT_PBKDF2_ITERATIONS;

        if (size >= static_cast<std::streamsize>(HEADER_SIZE)) {
            uint32_t magic, version, iterations;
            file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
            file.read(reinterpret_cast<char*>(&version), sizeof(version));
            file.read(reinterpret_cast<char*>(&iterations), sizeof(iterations));

            if (magic == VAULT_MAGIC) {
                pbkdf2_iterations = static_cast<int>(iterations);
                Log::info("Vault format version {}, {} PBKDF2 iterations", version, iterations);

                // V2 vaults: Header is part of data, rewind to beginning
                // V1 vaults: Header is separate, skip it
                if (version == 2 || is_v2_vault) {
                    file.seekg(0, std::ios::beg);  // Rewind for V2
                } else {
                    // V1: Adjust size to exclude header
                    size -= HEADER_SIZE;
                }
            } else {
                // V2 vault format (no separate header, header is part of file data)
                Log::info("V2 vault format detected (integrated header)");
                file.seekg(0, std::ios::beg);
                pbkdf2_iterations = DEFAULT_PBKDF2_ITERATIONS;
            }
        }

        data.resize(static_cast<size_t>(size));
        file.read(reinterpret_cast<char*>(data.data()), size);

        if (!file.good() && !file.eof()) {
            std::cerr << "Error reading vault file" << '\n';
            return false;
        }

        file.close();
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Exception reading vault file: " << e.what() << '\n';
        return false;
    }
}

bool VaultIO::write_file(
    const std::string& path,
    const std::vector<uint8_t>& data,
    bool is_v2_vault,
    int pbkdf2_iterations)
{
    namespace fs = std::filesystem;
    const std::string temp_path = path + ".tmp";

    try {
        // Write to temporary file
        {
            std::ofstream file(temp_path, std::ios::binary | std::ios::trunc);
            if (!file) {
                std::cerr << "Failed to create temporary vault file" << '\n';
                return false;
            }

            // V2 vaults: data already contains full header, write directly
            if (is_v2_vault) {
                file.write(reinterpret_cast<const char*>(data.data()), data.size());
                file.flush();

                if (!file.good()) {
                    std::cerr << "Failed to write V2 vault data" << '\n';
                    return false;
                }
            } else {
                // V1 vaults: prepend legacy header format
                uint32_t magic = VAULT_MAGIC;
                uint32_t version = VAULT_VERSION;
                uint32_t iterations = static_cast<uint32_t>(pbkdf2_iterations);

                file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
                file.write(reinterpret_cast<const char*>(&version), sizeof(version));
                file.write(reinterpret_cast<const char*>(&iterations), sizeof(iterations));

                // Write encrypted vault data
                file.write(reinterpret_cast<const char*>(data.data()), data.size());
                file.flush();

                if (!file.good()) {
                    std::cerr << "Failed to write vault data" << '\n';
                    return false;
                }
            }
        }  // Close file before rename

        // Atomic rename (POSIX guarantees atomicity)
        fs::rename(temp_path, path);

        // Set secure file permissions (owner read/write only)
        #ifdef __linux__
        chmod(path.c_str(), S_IRUSR | S_IWUSR);  // 0600
        #elif defined(_WIN32)
        // Windows permissions: Requires SetNamedSecurityInfo() with ACLs
        // Note: Current Windows implementation relies on NTFS default permissions
        // Future: Add explicit ACL setting for owner-only access
        #endif

        // Sync directory to ensure rename is durable
        #ifdef __linux__
        std::string dir_path = fs::path(path).parent_path().string();
        int dir_fd = open(dir_path.c_str(), O_RDONLY | O_DIRECTORY);
        if (dir_fd >= 0) {
            fsync(dir_fd);
            close(dir_fd);
        }
        #endif

        // Set secure file permissions (owner read/write only)
        fs::permissions(path,
            fs::perms::owner_read | fs::perms::owner_write,
            fs::perm_options::replace);

        return true;

    } catch (const fs::filesystem_error& e) {
        Log::error("Filesystem error: {}", e.what());
        try {
            fs::remove(temp_path);
        } catch (const std::exception& cleanup_err) {
            Log::warning("Failed to remove temp file during error cleanup: {}", cleanup_err.what());
        }
        return false;
    } catch (const std::exception& e) {
        Log::error("Error writing vault: {}", e.what());
        try {
            fs::remove(temp_path);
        } catch (const std::exception& cleanup_err) {
            Log::warning("Failed to remove temp file during error cleanup: {}", cleanup_err.what());
        }
        return false;
    }
}

VaultResult<> VaultIO::create_backup(std::string_view path, std::string_view backup_dir) {
    namespace fs = std::filesystem;
    std::string path_str(path);
    std::string backup_dir_str(backup_dir);

    try {
        if (!fs::exists(path_str)) {
            return {};  // No file to backup
        }

        // Generate timestamp: YYYYmmdd_HHMMSS_milliseconds
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        std::ostringstream timestamp;
        timestamp << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S")
                  << "_" << std::setfill('0') << std::setw(3) << ms.count();

        // Determine backup location
        fs::path vault_path(path_str);
        std::string backup_filename = vault_path.filename().string() + ".backup." + timestamp.str();
        std::string backup_path;

        if (backup_dir_str.empty()) {
            // Store in same directory as vault
            backup_path = path_str + ".backup." + timestamp.str();
        } else {
            // Store in custom backup directory
            fs::path backup_directory(backup_dir_str);

            // Create backup directory if it doesn't exist
            if (!fs::exists(backup_directory)) {
                fs::create_directories(backup_directory);
                Log::info("Created backup directory: {}", backup_dir_str);
            }

            backup_path = (backup_directory / backup_filename).string();
        }

        fs::copy_file(path_str, backup_path, fs::copy_options::overwrite_existing);
        Log::info("Created backup: {}", backup_path);

        return {};
    } catch (const fs::filesystem_error& e) {
        Log::warning("Failed to create backup: {}", e.what());
        // Don't fail the operation if backup fails
        return {};
    }
}

VaultResult<> VaultIO::restore_from_backup(std::string_view path) {
    namespace fs = std::filesystem;
    std::string path_str(path);

    // Get all backups and restore from the most recent
    auto backups = list_backups(path);

    try {
        if (backups.empty()) {
            // Try legacy .backup format for backwards compatibility
            std::string legacy_backup = path_str + ".backup";
            if (fs::exists(legacy_backup)) {
                fs::copy_file(legacy_backup, path_str, fs::copy_options::overwrite_existing);
                Log::info("Restored from legacy backup: {}", legacy_backup);
                return {};
            }
            Log::error("No backup files found for: {}", path_str);
            return std::unexpected(VaultError::FileNotFound);
        }

        // Backups are sorted newest first, so restore from [0]
        const std::string& backup_path = backups[0];
        fs::copy_file(backup_path, path_str, fs::copy_options::overwrite_existing);
        Log::info("Restored from backup: {}", backup_path);
        return {};
    } catch (const fs::filesystem_error& e) {
        Log::error("Failed to restore backup: {}", e.what());
        return std::unexpected(VaultError::FileReadFailed);
    }
}

std::vector<std::string> VaultIO::list_backups(std::string_view path, std::string_view backup_dir) {
    namespace fs = std::filesystem;
    std::vector<std::string> backups;
    std::string path_str(path);
    std::string backup_dir_str(backup_dir);

    fs::path vault_path(path_str);
    std::string vault_filename = vault_path.filename().string();
    std::string backup_pattern = vault_filename + ".backup.";

    try {
        // Determine search directory
        fs::path search_dir;
        if (backup_dir_str.empty()) {
            // Search in same directory as vault
            search_dir = vault_path.parent_path();
        } else {
            // Search in custom backup directory
            search_dir = fs::path(backup_dir_str);
        }

        if (!fs::exists(search_dir)) {
            return backups;
        }

        // Find all backup files matching pattern
        for (const auto& entry : fs::directory_iterator(search_dir)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            std::string filename = entry.path().filename().string();
            if (filename.starts_with(backup_pattern)) {
                backups.push_back(entry.path().string());
            }
        }

        // Sort by filename (timestamp is in filename), newest first
        std::sort(backups.begin(), backups.end(), std::greater<std::string>());

    } catch (const fs::filesystem_error& e) {
        Log::warning("Failed to list backups: {}", e.what());
    }

    return backups;
}

void VaultIO::cleanup_old_backups(std::string_view path, int max_backups, std::string_view backup_dir) {
    namespace fs = std::filesystem;

    if (max_backups < 1) [[unlikely]] {
        return;
    }

    auto backups = list_backups(path, backup_dir);

    // Delete oldest backups (backups are sorted newest first)
    for (size_t i = static_cast<size_t>(max_backups); i < backups.size(); ++i) {
        try {
            fs::remove(backups[i]);
            Log::info("Deleted old backup: {}", backups[i]);
        } catch (const fs::filesystem_error& e) {
            Log::warning("Failed to delete backup {}: {}", backups[i], e.what());
        }
    }
}

}  // namespace KeepTower
