// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2024 Travis E. Hansen

#ifndef KEEPTOWER_AUTO_LOCK_HANDLER_H
#define KEEPTOWER_AUTO_LOCK_HANDLER_H

#include <gtkmm.h>
#include <functional>
#include <string>

// Forward declarations
class VaultManager;

namespace KeepTower {
class AutoLockManager;
}

namespace UI {

// Forward declarations
class DialogManager;
class UIStateManager;

/**
 * @brief Handles auto-lock and activity monitoring functionality
 *
 * Phase 5k: Extracted from MainWindow to centralize auto-lock behavior,
 * activity monitoring, and vault locking/unlocking logic.
 */
class AutoLockHandler {
public:
    // Callback types
    /** @brief Callback to save current account before locking */
    using SaveAccountCallback = std::function<void()>;

    /** @brief Callback to close vault */
    using CloseVaultCallback = std::function<void()>;

    /** @brief Callback to refresh account list display */
    using UpdateAccountListCallback = std::function<void()>;

    /** @brief Callback to filter accounts by search text */
    using FilterAccountsCallback = std::function<void(const Glib::ustring&)>;

    /** @brief Callback to handle V2 vault re-authentication after unlock */
    using HandleV2VaultOpenCallback = std::function<void(const std::string&)>;

    /** @brief Callback to check if V2 vault is open
     *  @return true if V2 vault is currently open */
    using IsV2VaultOpenCallback = std::function<bool()>;

    /** @brief Callback to check if vault has unsaved changes
     *  @return true if vault has modifications */
    using IsVaultModifiedCallback = std::function<bool()>;

    /** @brief Callback to get current search text
     *  @return Current search text from search entry */
    using GetSearchTextCallback = std::function<Glib::ustring()>;

    /** @brief Construct AutoLockHandler with dependencies
     *  @param window Parent window for event monitoring
     *  @param vault_manager VaultManager instance
     *  @param auto_lock_manager AutoLockManager instance for timer management
     *  @param dialog_manager DialogManager for password dialogs
     *  @param ui_state_manager UIStateManager for UI state updates
     *  @param vault_open_ref Reference to MainWindow vault_open flag
     *  @param is_locked_ref Reference to MainWindow is_locked flag
     *  @param current_vault_path_ref Reference to MainWindow current vault path
     *  @param cached_master_password_ref Reference to cached master password
     *  @param save_account_callback Callback to save current account
     *  @param close_vault_callback Callback to close vault
     *  @param update_account_list_callback Callback to refresh account list
     *  @param filter_accounts_callback Callback to filter accounts
     *  @param handle_v2_vault_open_callback Callback to handle V2 re-authentication
     *  @param is_v2_vault_open_callback Callback to check if V2 vault is open
     *  @param is_vault_modified_callback Callback to check if vault is modified
     *  @param get_search_text_callback Callback to get current search text */
    AutoLockHandler(Gtk::Window& window,
                   VaultManager* vault_manager,
                   KeepTower::AutoLockManager* auto_lock_manager,
                   DialogManager* dialog_manager,
                   UIStateManager* ui_state_manager,
                   bool& vault_open_ref,
                   bool& is_locked_ref,
                   Glib::ustring& current_vault_path_ref,
                   std::string& cached_master_password_ref,
                   SaveAccountCallback save_account_callback,
                   CloseVaultCallback close_vault_callback,
                   UpdateAccountListCallback update_account_list_callback,
                   FilterAccountsCallback filter_accounts_callback,
                   HandleV2VaultOpenCallback handle_v2_vault_open_callback,
                   IsV2VaultOpenCallback is_v2_vault_open_callback,
                   IsVaultModifiedCallback is_vault_modified_callback,
                   GetSearchTextCallback get_search_text_callback);

    /**
     * @brief Set up event controllers to monitor user activity
     */
    void setup_activity_monitoring();

    /**
     * @brief Handle user activity (reset auto-lock timer)
     */
    void handle_user_activity();

    /**
     * @brief Handle auto-lock timeout
     * @return false (don't repeat timer)
     */
    bool handle_auto_lock_timeout();

    /**
     * @brief Lock the vault (V1 vaults only)
     */
    void lock_vault();

private:
    /**
     * @brief Get master password for locking (verification dialog)
     * @return Master password string (empty if cancelled)
     */
    std::string get_master_password_for_lock();

    Gtk::Window& m_window;
    VaultManager* m_vault_manager;
    KeepTower::AutoLockManager* m_auto_lock_manager;
    DialogManager* m_dialog_manager;
    UIStateManager* m_ui_state_manager;

    // References to MainWindow state (to be eliminated in Phase 6)
    bool& m_vault_open;
    bool& m_is_locked;
    Glib::ustring& m_current_vault_path;
    std::string& m_cached_master_password;

    // Callbacks for MainWindow operations
    SaveAccountCallback m_save_account_callback;
    CloseVaultCallback m_close_vault_callback;
    UpdateAccountListCallback m_update_account_list_callback;
    FilterAccountsCallback m_filter_accounts_callback;
    HandleV2VaultOpenCallback m_handle_v2_vault_open_callback;
    IsV2VaultOpenCallback m_is_v2_vault_open_callback;
    IsVaultModifiedCallback m_is_vault_modified_callback;
    GetSearchTextCallback m_get_search_text_callback;
};

}  // namespace UI

#endif  // KEEPTOWER_AUTO_LOCK_HANDLER_H
