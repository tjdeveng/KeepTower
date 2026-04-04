// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2024 KeepTower Contributors
// File: src/core/services/VaultFileService.cc

#include "VaultFileService.h"
#include "lib/vaultformat/VaultFormatV2.h"
#include "lib/storage/VaultIO.h"
#include "../../utils/Log.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <limits>

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

        // Prevent excessive allocations on corrupted/hostile inputs.
        constexpr std::streamsize MAX_VAULT_FILE_SIZE = 1024LL * 1024 * 1024;
        if (file_size > MAX_VAULT_FILE_SIZE) {
            Log::error("VaultFileService: Vault file too large: {} ({} bytes)", path, file_size);
            return std::unexpected(VaultError::InvalidData);
        }

        if (static_cast<std::uintmax_t>(file_size) > std::numeric_limits<size_t>::max()) {
            Log::error("VaultFileService: Vault file size overflows addressable memory: {}", path);
            return std::unexpected(VaultError::InvalidData);
        }

        // Read entire file into buffer
        file.seekg(0, std::ios::beg);
        data.resize(static_cast<size_t>(file_size));

        if (!file.read(reinterpret_cast<char*>(data.data()), file_size)) {
            Log::error("VaultFileService: Failed to read file contents: {}", path);
            return std::unexpected(VaultError::FileReadError);
        }

        // Detect format (V2 only)
        auto version = detect_vault_version(data);
        if (!version) {
            Log::error("VaultFileService: Invalid vault format: {}", path);
            return std::unexpected(VaultError::InvalidData);
        }
        if (*version != 2) {
            Log::error("VaultFileService: Unsupported vault version {}: {}", *version, path);
            return std::unexpected(VaultError::UnsupportedVersion);
        }

        // V2 format: PBKDF2 iterations are stored in the V2 header and handled by VaultFormatV2.
        pbkdf2_iterations = 0;

        Log::debug("VaultFileService: Read V2 vault ({} bytes)", data.size());

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

            (void)pbkdf2_iterations;

            if (!is_v2_vault) {
                Log::error("VaultFileService: V1 vault format is no longer supported");
                return std::unexpected(VaultError::UnsupportedVersion);
            }

            // V2: Data already contains full header, write directly
            file.write(reinterpret_cast<const char*>(data.data()),
                      static_cast<std::streamsize>(data.size()));

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

        Log::debug("VaultFileService: Wrote V2 vault ({} bytes)", data.size());

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

    // V2 format detection (delegate to VaultFormatV2)
    auto v2_result = VaultFormatV2::detect_version(data);
    if (v2_result && v2_result.value() == 2) {
        return 2;
    }

    // Unknown format
    return std::nullopt;
}

std::optional<uint32_t> VaultFileService::detect_vault_version_from_file(
    const std::string& path) {

    try {
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            return std::nullopt;
        }

        std::vector<uint8_t> header_data(1024);
        file.read(reinterpret_cast<char*>(header_data.data()),
                  static_cast<std::streamsize>(header_data.size()));

        if (!file) {
            const std::streamsize bytes_read = file.gcount();
            if (bytes_read <= 0) {
                return std::nullopt;
            }
            header_data.resize(static_cast<size_t>(bytes_read));
        }

        return detect_vault_version(header_data);
    } catch (const std::exception& e) {
        Log::error("VaultFileService: Exception detecting vault version from file: {}", e.what());
        return std::nullopt;
    }
}

// ============================================================================
// Backup Management
// ============================================================================

VaultResult<std::string> VaultFileService::create_backup(
    std::string_view vault_path,
    std::string_view backup_dir) {
    auto backup_result = VaultIO::create_backup(vault_path, backup_dir);
    if (!backup_result) {
        Log::error("VaultFileService: Failed to create backup for: {}", vault_path);
        return std::unexpected(backup_result.error());
    }

    return backup_result;
}

VaultResult<> VaultFileService::restore_from_backup(
    std::string_view vault_path,
    std::string_view backup_dir) {
    return VaultIO::restore_from_backup(vault_path, backup_dir);
}

std::vector<std::string> VaultFileService::list_backups(
    std::string_view vault_path,
    std::string_view backup_dir) {
    return VaultIO::list_backups(vault_path, backup_dir);
}

void VaultFileService::cleanup_old_backups(
    std::string_view vault_path,
    int max_backups,
    std::string_view backup_dir) {
    if (max_backups <= 0) {
        Log::warning("VaultFileService: Invalid max_backups: {}", max_backups);
        return;
    }

    VaultIO::cleanup_old_backups(vault_path, max_backups, backup_dir);
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
