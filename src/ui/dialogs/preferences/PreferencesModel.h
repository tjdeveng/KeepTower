// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#ifndef KEEPTOWER_UI_DIALOGS_PREFERENCES_PREFERENCESMODEL_H
#define KEEPTOWER_UI_DIALOGS_PREFERENCES_PREFERENCESMODEL_H

#include <cstdint>
#include <string>

namespace KeepTower::Ui {

/**
 * @brief In-memory representation of preferences edited in the Preferences dialog.
 *
 * This model is used to transfer state between the presenter and the extracted
 * preference page widgets. Some fields represent vault-scoped state when a vault
 * is open; otherwise they represent defaults persisted in application settings.
 */
struct PreferencesModel {
    // Meta / context
    bool vault_open = false;  ///< True when a vault is currently open
    bool vault_admin = false;  ///< True when the current user is an administrator
    bool fips_available = false;  ///< True when FIPS functionality is available

    // Appearance
    std::string color_scheme;  ///< "default" | "light" | "dark"

    // Storage
    bool apply_to_current_vault_fec = false;  ///< When vault open, apply FEC to current vault instead of defaults
    bool rs_enabled = false;  ///< Enable Reed-Solomon error correction
    int rs_redundancy_percent = 10;  ///< Reed-Solomon redundancy percentage

    bool backup_enabled = false;  ///< Enable automatic backups (vault-scoped when vault_open)
    int backup_count = 5;  ///< Number of backups to keep (vault-scoped when vault_open)
    std::string backup_path;  ///< Backup directory path (empty = vault directory). Applied to open vault when available.

    // Account security
    // When a vault is open these are typically vault-scoped; implementations may
    // fall back to GSettings defaults if the vault does not store the value.
    int clipboard_timeout_seconds = 30;  ///< Clipboard clear timeout in seconds

    bool account_password_history_enabled = false;  ///< Prevent account password reuse
    int account_password_history_limit = 5;  ///< Number of previous passwords to remember

    bool undo_redo_enabled = true;  ///< Enable undo/redo functionality
    int undo_history_limit = 50;  ///< Maximum number of undo operations

    // Vault security
    bool auto_lock_enabled = true;  ///< Enable auto-lock after inactivity
    int auto_lock_timeout_seconds = 300;  ///< Auto-lock timeout in seconds

    // Default for new vaults only
    int vault_user_password_history_depth_default = 5;  ///< Default vault user password history depth

    // FIPS preference (persistent; used at next app startup)
    bool fips_mode_enabled = false;  ///< Enable FIPS mode preference

    // Username hashing algorithm for new vaults
    std::string username_hash_algorithm;  ///< e.g. "sha3-256", "pbkdf2-sha256", "argon2id", "plaintext"
    std::uint32_t username_pbkdf2_iterations = 100000;  ///< PBKDF2 iterations for username/password derivation
    std::uint32_t username_argon2_memory_mb = 256;  ///< Argon2 memory in MB
    std::uint32_t username_argon2_iterations = 4;  ///< Argon2 iterations/time cost
};

}  // namespace KeepTower::Ui

#endif
