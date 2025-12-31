// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2024 Travis E. Hansen

#ifndef KEEPTOWER_USER_ACCOUNT_HANDLER_H
#define KEEPTOWER_USER_ACCOUNT_HANDLER_H

#include <gtkmm.h>
#include <functional>
#include <string>

// Forward declarations
class VaultManager;

namespace UI {

// Forward declarations
class DialogManager;

/**
 * @brief Handles V2 vault user account operations
 *
 * Phase 5l: Extracted from MainWindow to centralize user account management
 * for V2 multi-user vaults (password changes, logout, user management).
 */
class UserAccountHandler {
public:
    // Callback types
    /** @brief Callback to update status bar message */
    using StatusCallback = std::function<void(const std::string&)>;

    /** @brief Callback to display error message dialog */
    using ErrorDialogCallback = std::function<void(const std::string&)>;

    /** @brief Callback to close vault */
    using CloseVaultCallback = std::function<void()>;

    /** @brief Callback to handle V2 vault re-authentication */
    using HandleV2VaultOpenCallback = std::function<void(const std::string&)>;

    /** @brief Callback to check if V2 vault is open
     *  @return true if V2 vault is currently open */
    using IsV2VaultOpenCallback = std::function<bool()>;

    /** @brief Callback to check if current user is admin
     *  @return true if current user has admin role */
    using IsCurrentUserAdminCallback = std::function<bool()>;

    /** @brief Callback to prompt save if vault has unsaved changes
     *  @return true if operation should continue, false if canceled */
    using PromptSaveIfModifiedCallback = std::function<bool()>;

    /** @brief Construct UserAccountHandler with dependencies
     *  @param window Parent window for dialogs
     *  @param vault_manager VaultManager instance
     *  @param dialog_manager DialogManager for password/user dialogs
     *  @param current_vault_path_ref Reference to MainWindow current vault path
     *  @param status_callback Callback to update status bar
     *  @param error_dialog_callback Callback to show error dialogs
     *  @param close_vault_callback Callback to close vault
     *  @param handle_v2_vault_open_callback Callback to handle V2 re-authentication
     *  @param is_v2_vault_open_callback Callback to check if V2 vault is open
     *  @param is_current_user_admin_callback Callback to check if current user is admin
     *  @param prompt_save_if_modified_callback Callback to prompt save if modified */
    UserAccountHandler(Gtk::Window& window,
                      VaultManager* vault_manager,
                      DialogManager* dialog_manager,
                      Glib::ustring& current_vault_path_ref,
                      StatusCallback status_callback,
                      ErrorDialogCallback error_dialog_callback,
                      CloseVaultCallback close_vault_callback,
                      HandleV2VaultOpenCallback handle_v2_vault_open_callback,
                      IsV2VaultOpenCallback is_v2_vault_open_callback,
                      IsCurrentUserAdminCallback is_current_user_admin_callback,
                      PromptSaveIfModifiedCallback prompt_save_if_modified_callback);

    /**
     * @brief Handle password change for current user
     */
    void handle_change_password();

    /**
     * @brief Handle user logout (close and reopen vault)
     */
    void handle_logout();

    /**
     * @brief Handle user management dialog (admin only)
     */
    void handle_manage_users();

private:
    Gtk::Window& m_window;
    VaultManager* m_vault_manager;
    DialogManager* m_dialog_manager;

    // Reference to MainWindow state
    Glib::ustring& m_current_vault_path;

    // Callbacks for MainWindow operations
    StatusCallback m_status_callback;
    ErrorDialogCallback m_error_dialog_callback;
    CloseVaultCallback m_close_vault_callback;
    HandleV2VaultOpenCallback m_handle_v2_vault_open_callback;
    IsV2VaultOpenCallback m_is_v2_vault_open_callback;
    IsCurrentUserAdminCallback m_is_current_user_admin_callback;
    PromptSaveIfModifiedCallback m_prompt_save_if_modified_callback;
};

}  // namespace UI

#endif  // KEEPTOWER_USER_ACCOUNT_HANDLER_H
