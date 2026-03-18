// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2024 Travis E. Hansen

#ifndef KEEPTOWER_VAULT_OPEN_HANDLER_H
#define KEEPTOWER_VAULT_OPEN_HANDLER_H

#include <gtkmm.h>
#include <functional>
#include <string>
#include <optional>

// Forward declarations
class VaultManager;

namespace UI {

// Forward declarations
class DialogManager;

/**
 * @brief Handles vault creation and opening operations
 *
 * Phase 5l: Extracted from MainWindow to centralize vault creation/opening
 * logic including file dialogs, password input, YubiKey prompts, and vault initialization.
 */
class VaultOpenHandler {
public:
    // Callback types
    /** @brief Callback to display error message dialog */
    using ErrorDialogCallback = std::function<void(const std::string&)>;

    /** @brief Callback to display informational dialog (title, message) */
    using InfoDialogCallback = std::function<void(const std::string&, const std::string&)>;

    /** @brief Callback to detect vault version from file path
     *  @return Vault version (1 or 2) or std::nullopt if invalid */
    using DetectVaultVersionCallback = std::function<std::optional<uint32_t>(const std::string&)>;

    /** @brief Callback to handle V2 vault opening (multi-user authentication) */
    using HandleV2VaultOpenCallback = std::function<void(const std::string&)>;

    /** @brief Callback to initialize account/group repositories after opening */
    using InitializeRepositoriesCallback = std::function<void()>;

    /** @brief Callback to refresh account list display */
    using UpdateAccountListCallback = std::function<void()>;

    /** @brief Callback to update tag filter dropdown with available tags */
    using UpdateTagFilterCallback = std::function<void()>;

    /** @brief Callback to clear account detail widget */
    using ClearAccountDetailsCallback = std::function<void()>;

    /** @brief Callback to update undo/redo button sensitivity
     *  @param can_undo Whether undo is available
     *  @param can_redo Whether redo is available */
    using UpdateUndoRedoSensitivityCallback = std::function<void(bool, bool)>;

    /** @brief Callback to update menu for V2 user role (admin/regular) */
    using UpdateMenuForRoleCallback = std::function<void()>;

    /** @brief Callback to update session info display (V2 username/role) */
    using UpdateSessionDisplayCallback = std::function<void()>;

    /** @brief Callback to notify user activity (for auto-lock timer reset) */
    using OnUserActivityCallback = std::function<void()>;

    /** @brief Callback invoked when a vault has been opened/created successfully
     *  @param vault_path Opened vault path
     *  @param username Username (V2) or empty for V1 */
    using OnVaultOpenedCallback = std::function<void(const std::string& vault_path, const std::string& username)>;

    /** @brief Construct VaultOpenHandler with dependencies
     *  @param window Parent window for dialogs
     *  @param vault_manager VaultManager instance for vault operations
     *  @param dialog_manager DialogManager for file/password dialogs
     *  @param vault_open_ref Reference to MainWindow vault_open flag
     *  @param is_locked_ref Reference to MainWindow is_locked flag
     *  @param current_vault_path_ref Reference to MainWindow current vault path
     *  @param cached_master_password_ref Reference to cached master password (V1)
     *  @param error_dialog_callback Callback to show error dialogs
     *  @param info_dialog_callback Callback to show info dialogs
     *  @param detect_vault_version_callback Callback to detect vault version
     *  @param handle_v2_vault_open_callback Callback to handle V2 authentication
        *  @param on_vault_opened_callback Callback invoked after a vault is opened/created */
    VaultOpenHandler(Gtk::Window& window,
                    VaultManager* vault_manager,
                    DialogManager* dialog_manager,
                    bool& vault_open_ref,
                    bool& is_locked_ref,
                    Glib::ustring& current_vault_path_ref,
                    std::string& cached_master_password_ref,
                    ErrorDialogCallback error_dialog_callback,
                    InfoDialogCallback info_dialog_callback,
                    DetectVaultVersionCallback detect_vault_version_callback,
                    HandleV2VaultOpenCallback handle_v2_vault_open_callback,
                    OnVaultOpenedCallback on_vault_opened_callback);

    /**
     * @brief Handle new vault creation
     */
    void handle_new_vault();

    /**
     * @brief Handle opening existing vault
     */
    void handle_open_vault();

private:
    Gtk::Window& m_window;
    VaultManager* m_vault_manager;
    DialogManager* m_dialog_manager;

    // References to MainWindow state
    bool& m_vault_open;
    bool& m_is_locked;
    Glib::ustring& m_current_vault_path;
    std::string& m_cached_master_password;

    // Callbacks for MainWindow operations
    ErrorDialogCallback m_error_dialog_callback;
    InfoDialogCallback m_info_dialog_callback;
    DetectVaultVersionCallback m_detect_vault_version_callback;
    HandleV2VaultOpenCallback m_handle_v2_vault_open_callback;
    OnVaultOpenedCallback m_on_vault_opened_callback;
};

}  // namespace UI

#endif  // KEEPTOWER_VAULT_OPEN_HANDLER_H
