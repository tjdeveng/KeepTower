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

class VaultUiCoordinator {
public:
        using DetectVaultVersionFn = std::function<std::optional<uint32_t>(const std::string&)>;

    using SaveCurrentAccountFn = std::function<bool()>;
    using PromptSaveIfModifiedFn = std::function<bool()>;

    using InitializeRepositoriesFn = std::function<void()>;
    using ResetRepositoriesFn = std::function<void()>;

    using InitializeServicesFn = std::function<void()>;
    using ResetServicesFn = std::function<void()>;

    using UpdateAccountListFn = std::function<void()>;
    using UpdateTagFilterFn = std::function<void()>;
    using ClearAccountDetailsFn = std::function<void()>;
    using ClearAccountTreeFn = std::function<void()>;

    using UpdateUndoRedoSensitivityFn = std::function<void(bool, bool)>;
    using ClearUndoHistoryFn = std::function<void()>;

    using OnUserActivityFn = std::function<void()>;

    using SetStatusTextFn = std::function<void(const std::string&)>;
    using PreCloseCleanupFn = std::function<void()>;

    struct VaultState {
        bool vault_open = false;
        bool is_locked = false;
        Glib::ustring current_vault_path;
        std::string cached_master_password;
    };

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

    ~VaultUiCoordinator();

    VaultUiCoordinator(const VaultUiCoordinator&) = delete;
    VaultUiCoordinator& operator=(const VaultUiCoordinator&) = delete;
    VaultUiCoordinator(VaultUiCoordinator&&) = delete;
    VaultUiCoordinator& operator=(VaultUiCoordinator&&) = delete;

    void on_new_vault();
    void on_open_vault();
    void on_save_vault();
    void on_close_vault();

    void handle_v2_vault_open(const std::string& vault_path);

    void update_session_display();
    void update_menu_for_role();

    void apply_lock_state_ui(bool locked, const std::string& status);

    [[nodiscard]] const VaultState& state() const noexcept { return m_state; }
    [[nodiscard]] bool vault_open() const noexcept { return m_state.vault_open; }
    [[nodiscard]] bool is_locked() const noexcept { return m_state.is_locked; }
    [[nodiscard]] const Glib::ustring& current_vault_path() const noexcept { return m_state.current_vault_path; }

    [[nodiscard]] bool& vault_open_ref() noexcept { return m_state.vault_open; }
    [[nodiscard]] bool& is_locked_ref() noexcept { return m_state.is_locked; }
    [[nodiscard]] Glib::ustring& current_vault_path_ref() noexcept { return m_state.current_vault_path; }
    [[nodiscard]] std::string& cached_master_password_ref() noexcept { return m_state.cached_master_password; }

    [[nodiscard]] bool is_v2_vault_open() const noexcept;
    [[nodiscard]] bool is_current_user_admin() const noexcept;

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
