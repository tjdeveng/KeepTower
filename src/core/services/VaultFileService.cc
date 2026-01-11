// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2024 KeepTower Contributors
// File: src/core/services/VaultFileService.cc

#include "VaultFileService.h"
#include "../format/VaultFormat.h"
#include "../VaultFormatV2.h"
#include "../../utils/Log.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;
namespace Log = KeepTower::Log;

namespace KeepTower {

// ============================================================================
// File Reading Operations
// ============================================================================

VaultResult<> VaultFileService::read_vault_file(
    const std::string& path,
    std::vector<uint8_t>& data,
    int& pbkdf2_iterations) {

    try {
        // Check file exists and is readable
        if (!fs::exists(path)) {
            Log::error("VaultFileService: File does not exist: {}", path);
            return std::unexpected(VaultError::FileNotFound);
        }

        if (!fs::is_regular_file(path)) {
            Log::error("VaultFileService: Not a regular file: {}", path);
            return std::unexpected(VaultError::FileReadError);
        }

        // Open file for binary reading
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) {
            Log::error("VaultFileService: Failed to open file: {}", path);
            return std::unexpected(VaultError::FileReadError);
        }

        // Get file size
        const std::streamsize file_size = file.tellg();
        if (file_size <= 0) {
            Log::error("VaultFileService: Empty or invalid file: {}", path);
            return std::unexpected(VaultError::InvalidData);
        }

        // Read entire file into buffer
        file.seekg(0, std::ios::beg);
        data.resize(static_cast<size_t>(file_size));

        if (!file.read(reinterpret_cast<char*>(data.data()), file_size)) {
            Log::error("VaultFileService: Failed to read file contents: {}", path);
            return std::unexpected(VaultError::FileReadError);
        }

        // Detect format and extract PBKDF2 iterations for V1
        auto version = detect_vault_version(data);
        if (!version) {
            Log::error("VaultFileService: Invalid vault format: {}", path);
            return std::unexpected(VaultError::InvalidData);
        }

        if (*version == 1) {
            // V1 format: Extract PBKDF2 iterations from header
            // Header: [Magic: 4] [Version: 4] [Iterations: 4]
            if (data.size() < 12) {
                Log::error("VaultFileService: V1 header too short");
                return std::unexpected(VaultError::InvalidData);
            }

            // Read iterations (bytes 8-11, little-endian)
            pbkdf2_iterations = static_cast<int>(
                static_cast<uint32_t>(data[8]) |
                (static_cast<uint32_t>(data[9]) << 8) |
                (static_cast<uint32_t>(data[10]) << 16) |
                (static_cast<uint32_t>(data[11]) << 24)
            );

            Log::debug("VaultFileService: Read V1 vault (PBKDF2: {})", pbkdf2_iterations);
        } else {
            // V2 format: No PBKDF2 iterations in file header (per-user in key slots)
            pbkdf2_iterations = 0;
            Log::debug("VaultFileService: Read V2 vault ({} bytes)", data.size());
        }

        return {};

    } catch (const std::exception& e) {
        Log::error("VaultFileService: Exception reading file: {}", e.what());
        return std::unexpected(VaultError::FileReadError);
    }
}

// ============================================================================
// File Writing Operations
// ============================================================================

VaultResult<> VaultFileService::write_vault_file(
    const std::string& path,
    const std::vector<uint8_t>& data,
    bool is_v2_vault,
    int pbkdf2_iterations) {

    const std::string temp_path = path + ".tmp";

    try {
        // Ensure parent directory exists
        const fs::path file_path(path);
        const fs::path parent_dir = file_path.parent_path();

        if (!parent_dir.empty() && !fs::exists(parent_dir)) {
            fs::create_directories(parent_dir);
        }

        // Write to temporary file
        {
            std::ofstream file(temp_path, std::ios::binary | std::ios::trunc);
            if (!file) {
                Log::error("VaultFileService: Failed to create temporary file: {}", temp_path);
                return std::unexpected(VaultError::FileWriteError);
            }

            if (is_v2_vault) {
                // V2: Data already contains full header, write directly
                file.write(reinterpret_cast<const char*>(data.data()),
                          static_cast<std::streamsize>(data.size()));
            } else {
                // V1: Prepend file header
                // Header: [Magic: "KPT"] [Padding: 0x00] [Version: 1] [Iterations: uint32_t]
                const uint8_t header[] = {
                    'K', 'P', 'T', 0x00,                                    // Magic
                    0x01, 0x00, 0x00, 0x00,                                 // Version 1
                    static_cast<uint8_t>(pbkdf2_iterations & 0xFF),         // Iterations (LE)
                    static_cast<uint8_t>((pbkdf2_iterations >> 8) & 0xFF),
                    static_cast<uint8_t>((pbkdf2_iterations >> 16) & 0xFF),
                    static_cast<uint8_t>((pbkdf2_iterations >> 24) & 0xFF)
                };

                file.write(reinterpret_cast<const char*>(header), sizeof(header));
                file.write(reinterpret_cast<const char*>(data.data()),
                          static_cast<std::streamsize>(data.size()));
            }

            // Flush and sync to disk
            file.flush();
            if (!file.good()) {
                Log::error("VaultFileService: Failed to write data to temporary file");
                return std::unexpected(VaultError::FileWriteError);
            }
        }

        // Set secure permissions (owner read/write only)
#ifndef _WIN32
        fs::permissions(temp_path,
                       fs::perms::owner_read | fs::perms::owner_write,
                       fs::perm_options::replace);
#endif

        // Atomic rename (overwrites target if exists)
        fs::rename(temp_path, path);

        // Sync parent directory for durability
#ifndef _WIN32
        try {
            const int dir_fd = open(parent_dir.c_str(), O_RDONLY | O_DIRECTORY);
            if (dir_fd >= 0) {
                fsync(dir_fd);
                close(dir_fd);
            }
        } catch (...) {
            // Non-fatal: directory sync is optimization
        }
#endif

        Log::debug("VaultFileService: Wrote {} vault ({} bytes)",
                  is_v2_vault ? "V2" : "V1", data.size());

        return {};

    } catch (const std::exception& e) {
        Log::error("VaultFileService: Exception writing file: {}", e.what());

        // Cleanup temporary file if it exists
        try {
            if (fs::exists(temp_path)) {
                fs::remove(temp_path);
            }
        } catch (...) {
            // Ignore cleanup errors
        }

        return std::unexpected(VaultError::FileWriteError);
    }
}

// ============================================================================
// Format Detection
// ============================================================================

std::optional<uint32_t> VaultFileService::detect_vault_version(
    const std::vector<uint8_t>& data) {

    // Minimum size check (at least magic + version)
    if (data.size() < 8) {
        return std::nullopt;
    }

    // Check V1 magic: "KPT\0"
    if (data[0] == 'K' && data[1] == 'P' && data[2] == 'T' && data[3] == 0x00) {
        // V1 format detected
        // Read version (bytes 4-7, little-endian)
        const uint32_t version =
            static_cast<uint32_t>(data[4]) |
            (static_cast<uint32_t>(data[5]) << 8) |
            (static_cast<uint32_t>(data[6]) << 16) |
            (static_cast<uint32_t>(data[7]) << 24);

        if (version == 1) {
            return 1;
        }
    }

    // Try V2 format detection (delegate to VaultFormatV2)
    auto v2_result = VaultFormatV2::detect_version(data);
    if (v2_result && v2_result.value() == 2) {
        return 2;
    }

    // Unknown format
    return std::nullopt;
}

std::optional<uint32_t> VaultFileService::detect_vault_version_from_file(
    const std::string& path) {

    std::vector<uint8_t> data;
    int iterations;

    auto result = read_vault_file(path, data, iterations);
    if (!result) {
        return std::nullopt;
    }

    return detect_vault_version(data);
}

// ============================================================================
// Backup Management
// ============================================================================

VaultResult<std::string> VaultFileService::create_backup(
    std::string_view vault_path,
    std::string_view backup_dir) {

    try {
        const fs::path vault(vault_path);

        // Check source file exists
        if (!fs::exists(vault)) {
            Log::error("VaultFileService: Source vault does not exist: {}", vault_path);
            return std::unexpected(VaultError::FileNotFound);
        }

        // Determine backup directory
        const fs::path backup_path = backup_dir.empty()
            ? vault.parent_path()
            : fs::path(backup_dir);

        // Create backup directory if needed
        if (!backup_path.empty() && !fs::exists(backup_path)) {
            fs::create_directories(backup_path);
        }

        // Generate timestamp (ISO 8601 format, safe for filenames)
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::tm local_time{};

#ifdef _WIN32
        localtime_s(&local_time, &time_t_now);
#else
        localtime_r(&time_t_now, &local_time);
#endif

        std::ostringstream timestamp;
        timestamp << std::put_time(&local_time, "%Y-%m-%dT%H-%M-%S");

        // Build backup filename: basename.timestamp.backup
        const std::string backup_filename =
            vault.filename().string() + "." + timestamp.str() + ".backup";

        const fs::path backup_file = backup_path / backup_filename;

        // Copy file
        fs::copy_file(vault, backup_file, fs::copy_options::overwrite_existing);

        Log::info("VaultFileService: Created backup: {}", backup_file.string());

        return backup_file.string();

    } catch (const std::exception& e) {
        Log::error("VaultFileService: Exception creating backup: {}", e.what());
        return std::unexpected(VaultError::FileWriteFailed);
    }
}

VaultResult<> VaultFileService::restore_from_backup(
    std::string_view vault_path,
    std::string_view backup_dir) {

    try {
        // Find most recent backup
        auto backups = list_backups(vault_path, backup_dir);
        if (backups.empty()) {
            Log::error("VaultFileService: No backups found for: {}", vault_path);
            return std::unexpected(VaultError::FileNotFound);
        }

        const fs::path vault(vault_path);
        const fs::path backup(backups[0]);  // Most recent (list is sorted)
        const fs::path old_vault = fs::path(vault_path).string() + ".old";

        // Move current vault to .old
        if (fs::exists(vault)) {
            fs::rename(vault, old_vault);
        }

        // Copy backup to vault location
        try {
            fs::copy_file(backup, vault, fs::copy_options::overwrite_existing);

            // Success - remove .old file
            if (fs::exists(old_vault)) {
                fs::remove(old_vault);
            }

            Log::info("VaultFileService: Restored from backup: {}", backup.string());
            return {};

        } catch (const std::exception& e) {
            // Restore failed - restore .old file
            if (fs::exists(old_vault)) {
                fs::rename(old_vault, vault);
            }

            Log::error("VaultFileService: Failed to restore backup: {}", e.what());
            return std::unexpected(VaultError::FileWriteError);
        }

    } catch (const std::exception& e) {
        Log::error("VaultFileService: Exception restoring backup: {}", e.what());
        return std::unexpected(VaultError::FileWriteFailed);
    }
}

std::vector<std::string> VaultFileService::list_backups(
    std::string_view vault_path,
    std::string_view backup_dir) {

    std::vector<std::string> backups;

    try {
        const fs::path vault(vault_path);
        const std::string vault_filename = vault.filename().string();

        // Determine backup directory
        const fs::path search_dir = backup_dir.empty()
            ? vault.parent_path()
            : fs::path(backup_dir);

        if (!fs::exists(search_dir)) {
            return backups;
        }

        // Scan directory for matching backups
        for (const auto& entry : fs::directory_iterator(search_dir)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            const std::string filename = entry.path().filename().string();

            // Check if filename matches pattern: vault_name.*.backup
            const std::string prefix = vault_filename + ".";
            const std::string suffix = ".backup";

            if (filename.size() > prefix.size() + suffix.size() &&
                filename.substr(0, prefix.size()) == prefix &&
                filename.substr(filename.size() - suffix.size()) == suffix) {

                backups.push_back(entry.path().string());
            }
        }

        // Sort by filename (timestamp is in filename, so lexicographic sort works)
        std::sort(backups.begin(), backups.end(), std::greater<>());  // Newest first

    } catch (const std::exception& e) {
        Log::warning("VaultFileService: Exception listing backups: {}", e.what());
    }

    return backups;
}

void VaultFileService::cleanup_old_backups(
    std::string_view vault_path,
    int max_backups,
    std::string_view backup_dir) {

    if (max_backups <= 0) {
        Log::warning("VaultFileService: Invalid max_backups: {}", max_backups);
        return;
    }

    try {
        auto backups = list_backups(vault_path, backup_dir);

        // Delete backups beyond max_backups limit
        if (backups.size() > static_cast<size_t>(max_backups)) {
            for (size_t i = static_cast<size_t>(max_backups); i < backups.size(); ++i) {
                try {
                    fs::remove(backups[i]);
                    Log::debug("VaultFileService: Deleted old backup: {}", backups[i]);
                } catch (const std::exception& e) {
                    Log::warning("VaultFileService: Failed to delete backup {}: {}",
                               backups[i], e.what());
                }
            }
        }

    } catch (const std::exception& e) {
        Log::warning("VaultFileService: Exception during backup cleanup: {}", e.what());
    }
}

// ============================================================================
// File System Utilities
// ============================================================================

bool VaultFileService::file_exists(const std::string& path) {
    try {
        return fs::exists(path) && fs::is_regular_file(path);
    } catch (...) {
        return false;
    }
}

size_t VaultFileService::get_file_size(const std::string& path) {
    try {
        if (!fs::exists(path)) {
            return 0;
        }
        return static_cast<size_t>(fs::file_size(path));
    } catch (...) {
        return 0;
    }
}

} // namespace KeepTower
