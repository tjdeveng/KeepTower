// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#ifndef KEEPTOWER_CORE_VAULT_RUNTIME_PREFERENCES_H
#define KEEPTOWER_CORE_VAULT_RUNTIME_PREFERENCES_H

/**
 * @file VaultRuntimePreferences.h
 * @brief Runtime preferences for open vaults
 *
 * Encapsulates vault-scoped runtime preferences that are stored in the vault file:
 * - Clipboard auto-clear timeout
 * - Auto-lock settings (enabled, timeout)
 * - Undo/redo settings (enabled, history limit)
 * - Account password history (enabled, limit)
 *
 * These preferences are not stored in GSettings (application preferences).
 * They are vault-specific and loaded/saved with the vault file.
 *
 * Default values match GSettings schema defaults to ensure consistency
 * when no vault is open.
 */

namespace KeepTower {

/**
 * @brief Encapsulates vault-scoped runtime preferences
 *
 * Provides accessor methods for vault-specific preference settings.
 * When a vault is open, these reflect the vault's stored values.
 * When no vault is open, they return schema defaults.
 *
 * @note This is an internal utility class; use VaultManager::preferences()
 *       to access these values from application code.
 */
class VaultRuntimePreferences {
public:
    /**
     * @brief Set clipboard timeout for current vault
     * @param timeout_seconds Timeout in seconds (0 = disabled)
     *
     * @note This setting is stored in the vault file
     */
    void set_clipboard_timeout(int timeout_seconds) noexcept {
        m_clipboard_timeout_seconds = timeout_seconds;
    }

    /**
     * @brief Get clipboard timeout for current vault
     * @return Timeout in seconds (0 if not set or vault closed)
     *
     * @note Default: 30 seconds (matches GSettings schema)
     */
    [[nodiscard]] int get_clipboard_timeout() const noexcept {
        return m_clipboard_timeout_seconds;
    }

    /**
     * @brief Set auto-lock enabled for current vault
     * @param enabled true to enable auto-lock, false to disable
     *
     * @note This setting is stored in the vault file
     */
    void set_auto_lock_enabled(bool enabled) noexcept {
        m_auto_lock_enabled = enabled;
    }

    /**
     * @brief Get auto-lock enabled for current vault
     * @return true if auto-lock is enabled, false otherwise
     *
     * @note Default: true (matches GSettings schema)
     */
    [[nodiscard]] bool get_auto_lock_enabled() const noexcept {
        return m_auto_lock_enabled;
    }

    /**
     * @brief Set auto-lock timeout for current vault
     * @param timeout_seconds Timeout in seconds (0 = disabled)
     *
     * @note This setting is stored in the vault file
     */
    void set_auto_lock_timeout(int timeout_seconds) noexcept {
        m_auto_lock_timeout_seconds = timeout_seconds;
    }

    /**
     * @brief Get auto-lock timeout for current vault
     * @return Timeout in seconds (0 if not set or vault closed)
     *
     * @note Default: 300 seconds (5 minutes, matches GSettings schema)
     */
    [[nodiscard]] int get_auto_lock_timeout() const noexcept {
        return m_auto_lock_timeout_seconds;
    }

    /**
     * @brief Set undo/redo enabled for current vault
     * @param enabled true to enable undo/redo, false to disable
     *
     * @note This setting is stored in the vault file
     */
    void set_undo_redo_enabled(bool enabled) noexcept {
        m_undo_redo_enabled = enabled;
    }

    /**
     * @brief Get undo/redo enabled for current vault
     * @return true if enabled (false if not set or vault closed)
     *
     * @note Default: true (matches GSettings schema)
     */
    [[nodiscard]] bool get_undo_redo_enabled() const noexcept {
        return m_undo_redo_enabled;
    }

    /**
     * @brief Set undo/redo history limit for current vault
     * @param limit Maximum operations to keep (1-100)
     *
     * @note This setting is stored in the vault file
     */
    void set_undo_history_limit(int limit) noexcept {
        m_undo_history_limit = limit;
    }

    /**
     * @brief Get undo/redo history limit for current vault
     * @return History limit (0 if not set or vault closed)
     *
     * @note Default: 50 (matches GSettings schema)
     */
    [[nodiscard]] int get_undo_history_limit() const noexcept {
        return m_undo_history_limit;
    }

    /**
     * @brief Set account password history enabled for current vault
     * @param enabled true to prevent password reuse, false to allow
     *
     * @note This setting is stored in the vault file
     */
    void set_account_password_history_enabled(bool enabled) noexcept {
        m_account_password_history_enabled = enabled;
    }

    /**
     * @brief Get account password history enabled for current vault
     * @return true if enabled (false if not set or vault closed)
     *
     * @note Default: false (matches GSettings schema)
     */
    [[nodiscard]] bool get_account_password_history_enabled() const noexcept {
        return m_account_password_history_enabled;
    }

    /**
     * @brief Set account password history limit for current vault
     * @param limit Number of previous passwords to check (0-24)
     *
     * @note This setting is stored in the vault file
     */
    void set_account_password_history_limit(int limit) noexcept {
        m_account_password_history_limit = limit;
    }

    /**
     * @brief Get account password history limit for current vault
     * @return History limit (0 if not set or vault closed)
     *
     * @note Default: 5 (matches GSettings schema)
     */
    [[nodiscard]] int get_account_password_history_limit() const noexcept {
        return m_account_password_history_limit;
    }

    /**
     * @brief Reset all preferences to schema defaults
     *
     * Used when vault is closed to ensure consistent defaults.
     */
    /**
     * @brief Sync all preferences from vault metadata values
     *
     * Loads preference values from vault metadata into the cache.
     * Called by VaultManager after loading a vault file.
     */
    void sync_from_vault_metadata(
        int clipboard_timeout,
        bool auto_lock_enabled,
        int auto_lock_timeout,
        bool undo_redo_enabled,
        int undo_history_limit,
        bool password_history_enabled,
        int password_history_limit
    ) noexcept {
        m_clipboard_timeout_seconds = clipboard_timeout;
        m_auto_lock_enabled = auto_lock_enabled;
        m_auto_lock_timeout_seconds = auto_lock_timeout;
        m_undo_redo_enabled = undo_redo_enabled;
        m_undo_history_limit = undo_history_limit;
        m_account_password_history_enabled = password_history_enabled;
        m_account_password_history_limit = password_history_limit;
    }

    void reset_to_defaults() noexcept {
        m_clipboard_timeout_seconds = 30;
        m_auto_lock_enabled = true;
        m_auto_lock_timeout_seconds = 300;
        m_undo_redo_enabled = true;
        m_undo_history_limit = 50;
        m_account_password_history_enabled = false;
        m_account_password_history_limit = 5;
    }

private:
    // All values match GSettings schema defaults when vault is closed
    int m_clipboard_timeout_seconds = 30;          ///< Default: 30 seconds
    bool m_auto_lock_enabled = true;               ///< Default: enabled
    int m_auto_lock_timeout_seconds = 300;         ///< Default: 300 seconds (5 min)
    bool m_undo_redo_enabled = true;               ///< Default: enabled
    int m_undo_history_limit = 50;                 ///< Default: 50 operations
    bool m_account_password_history_enabled = false;  ///< Default: disabled
    int m_account_password_history_limit = 5;      ///< Default: 5 previous passwords
};

}  // namespace KeepTower

#endif
