// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

/**
 * @file VaultUiCoordinator.h
 * @brief Coordinates vault lifecycle UI flows (open completion/save/close)
 *
 * Issue #5 refactor: extract vault lifecycle orchestration from MainWindow.
 */

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include <gtkmm/window.h>
#include <glibmm/ustring.h>

// Forward declarations
class VaultManager;

namespace UI {
class DialogManager;
class MenuManager;
class VaultUiStateApplier;
class V2AuthenticationHandler;
class VaultOpenHandler;
}

/**
 * @brief Orchestrates vault lifecycle actions from the UI layer.
 *
 * VaultUiCoordinator wires UI actions (new/open/save/close) to VaultManager
 * operations and applies resulting state changes to menus and widgets via
 * injected helpers.
 */
class VaultUiCoordinator {
public:
    /** @brief Callback to detect vault version from a file path. */
    using DetectVaultVersionFn = std::function<std::optional<uint32_t>(const std::string&)>;

    /** @brief Callback to persist any in-progress account edits before closing/switching. */
    using SaveCurrentAccountFn = std::function<bool()>;

    /** @brief Callback to prompt the user to save changes if modified. */
    using PromptSaveIfModifiedFn = std::function<bool()>;

    /** @brief Callback to construct repositories after a vault is opened. */
    using InitializeRepositoriesFn = std::function<void()>;

    /** @brief Callback to reset repositories when a vault is closed. */
    using ResetRepositoriesFn = std::function<void()>;

    /** @brief Callback to construct services after a vault is opened. */
    using InitializeServicesFn = std::function<void()>;

    /** @brief Callback to reset services when a vault is closed. */
    using ResetServicesFn = std::function<void()>;

    /** @brief Callback to refresh the account list view. */
    using UpdateAccountListFn = std::function<void()>;

    /** @brief Callback to refresh the tag filter UI. */
    using UpdateTagFilterFn = std::function<void()>;

    /** @brief Callback to clear account detail widgets. */
    using ClearAccountDetailsFn = std::function<void()>;

    /** @brief Callback to clear the account tree widget. */
    using ClearAccountTreeFn = std::function<void()>;

    /** @brief Callback to update undo/redo sensitivity based on app state. */
    using UpdateUndoRedoSensitivityFn = std::function<void(bool, bool)>;

    /** @brief Callback to clear undo history when switching vaults. */
    using ClearUndoHistoryFn = std::function<void()>;

    /** @brief Callback invoked to record user activity (for auto-lock timers). */
    using OnUserActivityFn = std::function<void()>;

    /** @brief Callback to set the main status text. */
    using SetStatusTextFn = std::function<void(const std::string&)>;

    /** @brief Callback invoked before closing a vault to perform UI cleanup. */
    using PreCloseCleanupFn = std::function<void()>;

    /**
     * @brief Minimal UI state tracked by the coordinator.
     */
    struct VaultState {
        bool vault_open = false;                 ///< True when a vault is open
        bool is_locked = false;                  ///< True when the open vault is locked
        Glib::ustring current_vault_path;        ///< Current vault path (empty if none)
        std::string cached_master_password;      ///< Cached password for session operations (UI-layer only)
    };

    /**
     * @brief Construct a coordinator with injected UI dependencies and callbacks.
     *
     * @param parent_window Parent GTK window used for dialogs
     * @param vault_manager Vault manager instance to operate on
     * @param dialog_manager Dialog manager used for user prompts
     * @param menu_manager Menu manager used for role-based menu updates
     * @param ui_state_applier Applies high-level state to widgets
     * @param v2_auth_handler Handler for V2 authentication flows
     * @param detect_vault_version Callback to detect vault version
     * @param save_current_account Callback to persist in-progress edits
     * @param prompt_save_if_modified Callback to prompt to save if modified
     * @param initialize_repositories Callback to initialize repositories
     * @param reset_repositories Callback to reset repositories
     * @param initialize_services Callback to initialize services
     * @param reset_services Callback to reset services
     * @param update_account_list Callback to refresh account list
     * @param update_tag_filter Callback to refresh tag filter
     * @param clear_account_details Callback to clear account details view
     * @param clear_account_tree Callback to clear account tree
     * @param update_undo_redo_sensitivity Callback to update undo/redo enabled state
     * @param clear_undo_history Callback to clear undo history
     * @param on_user_activity Callback to record user activity
     * @param set_status_text Callback to set status text
     * @param pre_close_cleanup Callback invoked before closing the vault
     */
    VaultUiCoordinator(
        Gtk::Window& parent_window,
        VaultManager* vault_manager,
        UI::DialogManager* dialog_manager,
        UI::MenuManager* menu_manager,
        UI::VaultUiStateApplier* ui_state_applier,
        UI::V2AuthenticationHandler* v2_auth_handler,
        DetectVaultVersionFn detect_vault_version,
        SaveCurrentAccountFn save_current_account,
        PromptSaveIfModifiedFn prompt_save_if_modified,
        InitializeRepositoriesFn initialize_repositories,
        ResetRepositoriesFn reset_repositories,
        InitializeServicesFn initialize_services,
        ResetServicesFn reset_services,
        UpdateAccountListFn update_account_list,
        UpdateTagFilterFn update_tag_filter,
        ClearAccountDetailsFn clear_account_details,
        ClearAccountTreeFn clear_account_tree,
        UpdateUndoRedoSensitivityFn update_undo_redo_sensitivity,
        ClearUndoHistoryFn clear_undo_history,
        OnUserActivityFn on_user_activity,
        SetStatusTextFn set_status_text,
        PreCloseCleanupFn pre_close_cleanup
    );

    /** @brief Destructor. */
    ~VaultUiCoordinator();

    VaultUiCoordinator(const VaultUiCoordinator&) = delete;
    VaultUiCoordinator& operator=(const VaultUiCoordinator&) = delete;
    VaultUiCoordinator(VaultUiCoordinator&&) = delete;
    VaultUiCoordinator& operator=(VaultUiCoordinator&&) = delete;

    /** @brief Handle "New vault" UI action. */
    void on_new_vault();

    /** @brief Handle "Open vault" UI action. */
    void on_open_vault();

    /** @brief Handle "Save vault" UI action. */
    void on_save_vault();

    /** @brief Handle "Close vault" UI action. */
    void on_close_vault();

    /** @brief Continue V2 open flow after selecting a vault path. */
    void handle_v2_vault_open(const std::string& vault_path);

    /** @brief Update session label/status based on current state. */
    void update_session_display();

    /** @brief Update menus based on the current user's role. */
    void update_menu_for_role();

    /** @brief Apply lock/unlock UI state and status message. */
    void apply_lock_state_ui(bool locked, const std::string& status);

    /** @brief Get read-only coordinator state. */
    [[nodiscard]] const VaultState& state() const noexcept { return m_state; }

    /** @brief True when a vault is currently open. */
    [[nodiscard]] bool vault_open() const noexcept { return m_state.vault_open; }

    /** @brief True when the open vault is locked. */
    [[nodiscard]] bool is_locked() const noexcept { return m_state.is_locked; }

    /** @brief Get the current vault path. */
    [[nodiscard]] const Glib::ustring& current_vault_path() const noexcept { return m_state.current_vault_path; }

    /** @brief Mutable reference to vault_open flag (internal wiring). */
    [[nodiscard]] bool& vault_open_ref() noexcept { return m_state.vault_open; }

    /** @brief Mutable reference to is_locked flag (internal wiring). */
    [[nodiscard]] bool& is_locked_ref() noexcept { return m_state.is_locked; }

    /** @brief Mutable reference to current vault path (internal wiring). */
    [[nodiscard]] Glib::ustring& current_vault_path_ref() noexcept { return m_state.current_vault_path; }

    /** @brief Mutable reference to cached master password (internal wiring). */
    [[nodiscard]] std::string& cached_master_password_ref() noexcept { return m_state.cached_master_password; }

    /** @brief True if an open vault is a V2 (multi-user) vault. */
    [[nodiscard]] bool is_v2_vault_open() const noexcept;

    /** @brief True if the current authenticated user is an administrator. */
    [[nodiscard]] bool is_current_user_admin() const noexcept;

    /** @brief Finalize UI state after a vault is opened and authenticated. */
    void complete_vault_opening(const std::string& vault_path, const std::string& username);

private:
    Gtk::Window& m_parent_window;
    VaultManager* m_vault_manager;

    UI::DialogManager* m_dialog_manager;
    UI::MenuManager* m_menu_manager;
    UI::VaultUiStateApplier* m_vault_ui_state_applier;
    UI::V2AuthenticationHandler* m_v2_auth_handler;

    std::unique_ptr<UI::VaultOpenHandler> m_vault_open_handler;

    VaultState m_state;

    DetectVaultVersionFn m_detect_vault_version;

    SaveCurrentAccountFn m_save_current_account;
    PromptSaveIfModifiedFn m_prompt_save_if_modified;

    InitializeRepositoriesFn m_initialize_repositories;
    ResetRepositoriesFn m_reset_repositories;

    InitializeServicesFn m_initialize_services;
    ResetServicesFn m_reset_services;

    UpdateAccountListFn m_update_account_list;
    UpdateTagFilterFn m_update_tag_filter;
    ClearAccountDetailsFn m_clear_account_details;
    ClearAccountTreeFn m_clear_account_tree;

    UpdateUndoRedoSensitivityFn m_update_undo_redo_sensitivity;
    ClearUndoHistoryFn m_clear_undo_history;
    OnUserActivityFn m_on_user_activity;

    SetStatusTextFn m_set_status_text;
    PreCloseCleanupFn m_pre_close_cleanup;
};
