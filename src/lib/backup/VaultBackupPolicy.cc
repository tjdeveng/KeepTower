// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#include "lib/backup/VaultBackupPolicy.h"

#include "io/VaultIO.h"
#include "record.pb.h"
#include "utils/Log.h"

#include <filesystem>

namespace KeepTower {

VaultBackupPolicy::VaultBackupPolicy(bool enabled, int max_backups, std::string backup_path)
    : m_enabled(enabled),
      m_max_backups(max_backups),
      m_backup_path(std::move(backup_path)) {
    if (m_max_backups < kMinBackups || m_max_backups > kMaxBackups) {
        m_max_backups = 5;
    }
}

void VaultBackupPolicy::set_enabled(bool enabled) {
    m_enabled = enabled;
}

bool VaultBackupPolicy::is_enabled() const {
    return m_enabled;
}

bool VaultBackupPolicy::set_max_backups(int count) {
    if (count < kMinBackups || count > kMaxBackups) {
        return false;
    }
    m_max_backups = count;
    return true;
}

int VaultBackupPolicy::max_backups() const {
    return m_max_backups;
}

void VaultBackupPolicy::set_backup_path(std::string path) {
    m_backup_path = std::move(path);
}

const std::string& VaultBackupPolicy::backup_path() const {
    return m_backup_path;
}

bool VaultBackupPolicy::load_from_vault_data(const keeptower::VaultData& vault_data) {
    if (vault_data.backup_count() <= 0) {
        return false;
    }

    const int backup_count = vault_data.backup_count();
    if (!set_max_backups(backup_count)) {
        Log::warning("VaultBackupPolicy: Ignoring invalid backup_count from vault: {}",
                     backup_count);
        return false;
    }

    set_enabled(vault_data.backup_enabled());
    return true;
}

void VaultBackupPolicy::store_to_vault_data(keeptower::VaultData& vault_data) const {
    vault_data.set_backup_enabled(is_enabled());
    vault_data.set_backup_count(max_backups());
}

void VaultBackupPolicy::maybe_create_backup(std::string_view vault_path, bool explicit_save) const {
    if (!explicit_save || !m_enabled) {
        return;
    }

    auto backup_result = VaultIO::create_backup(vault_path, m_backup_path);
    if (!backup_result) {
        Log::warning("VaultBackupPolicy: Failed to create backup: {}",
                     static_cast<int>(backup_result.error()));
        return;
    }

    VaultIO::cleanup_old_backups(vault_path, m_max_backups, m_backup_path);
}

VaultResult<> VaultBackupPolicy::restore_from_most_recent_backup(std::string_view vault_path) const {
    namespace fs = std::filesystem;

    auto backups = VaultIO::list_backups(vault_path, m_backup_path);
    if (backups.empty()) {
        Log::error("VaultBackupPolicy: No backups found for vault");
        return std::unexpected(VaultError::FileNotFound);
    }

    try {
        const std::string& most_recent_backup = backups[0];
        fs::copy_file(most_recent_backup, std::string(vault_path), fs::copy_options::overwrite_existing);
        Log::info("VaultBackupPolicy: Restored vault from backup: {}", most_recent_backup);
        return {};
    } catch (const fs::filesystem_error& e) {
        Log::error("VaultBackupPolicy: Failed to restore from backup: {}", e.what());
        return std::unexpected(VaultError::FileReadFailed);
    }
}

}  // namespace KeepTower
