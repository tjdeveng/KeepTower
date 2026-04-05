// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#pragma once

#include <string>
#include <string_view>

#include "core/VaultError.h"

namespace keeptower {
class VaultData;
}

namespace KeepTower {

/**
 * @brief Encapsulates backup configuration and backup/restore policy actions.
 *
 * Keeps backup-related state and policy decisions out of VaultManager while
 * delegating low-level filesystem work to the storage layer. This preserves the
 * existing backup behavior and retention limits without exposing storage details
 * to manager-facing code.
 */
class VaultBackupPolicy {
public:
    /**
     * @brief Construct backup policy with defaults matching existing behavior.
        * @param enabled True to enable automatic backups.
        * @param max_backups Maximum number of retained backups.
        * @param backup_path Optional backup directory override.
     */
    explicit VaultBackupPolicy(bool enabled = true,
                               int max_backups = 5,
                               std::string backup_path = "");

    /** @brief Enable or disable automatic backups.
     *  @param enabled True to enable automatic backups. */
    void set_enabled(bool enabled);

    /** @brief Check whether automatic backups are enabled.
     *  @return True when automatic backups are enabled. */
    [[nodiscard]] bool is_enabled() const;

    /**
     * @brief Set maximum number of retained backups.
        * @param count Requested retention count.
     * @return false if out of supported range [1, 50].
     */
    [[nodiscard]] bool set_max_backups(int count);

        /** @brief Get maximum number of retained backups.
        *  @return Configured retention count. */
    [[nodiscard]] int max_backups() const;

        /** @brief Set custom backup directory path (empty means same directory as vault).
        *  @param path Backup directory override. */
    void set_backup_path(std::string path);

        /** @brief Get configured backup directory path.
        *  @return Configured backup directory override. */
    [[nodiscard]] const std::string& backup_path() const;

    /**
     * @brief Load persisted backup settings from vault data if present and valid.
     *
     * Only enabled/count are vault-persisted. Backup path remains a local runtime setting.
        * @param vault_data Vault data containing persisted backup settings.
        * @return True when valid settings were loaded.
     */
    [[nodiscard]] bool load_from_vault_data(const keeptower::VaultData& vault_data);

    /**
     * @brief Persist current backup settings back into vault data.
     *
     * Only enabled/count are serialized into vault data. Backup path is intentionally excluded.
        * @param vault_data Vault data object to update.
     */
    void store_to_vault_data(keeptower::VaultData& vault_data) const;

    /**
    * @brief Create and rotate backups if policy allows for this save.
     *
    * Non-fatal by design: failures are logged but do not abort save flow.
    * The policy determines when backup work should run; the storage layer
    * performs the underlying file and backup operations.
    * @param vault_path Vault file path being saved.
    * @param explicit_save True when the save was explicitly triggered by the user.
     */
    void maybe_create_backup(std::string_view vault_path, bool explicit_save) const;

    /**
    * @brief Restore vault from most recent backup according to configured path policy.
    *
    * Uses the storage layer's compatibility-aware backup lookup so both legacy
    * and newer backup naming conventions can be restored transparently.
    * @param vault_path Vault path whose latest backup should be restored.
    * @return Success or an error from the restore operation.
     */
    [[nodiscard]] VaultResult<> restore_from_most_recent_backup(std::string_view vault_path) const;

private:
    static constexpr int kMinBackups = 1;
    static constexpr int kMaxBackups = 50;

    bool m_enabled;
    int m_max_backups;
    std::string m_backup_path;
};

}  // namespace KeepTower
