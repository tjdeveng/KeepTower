// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#include "VaultUiCoordinator.h"

#include "../../utils/Log.h"

#include <optional>

#include "../managers/DialogManager.h"
#include "../managers/MenuManager.h"
#include "../managers/VaultUiStateApplier.h"
#include "../managers/V2AuthenticationHandler.h"
#include "../managers/VaultOpenHandler.h"

#include "../../core/VaultManager.h"

VaultUiCoordinator::VaultUiCoordinator(
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
    PreCloseCleanupFn pre_close_cleanup)
    : m_parent_window(parent_window),
      m_vault_manager(vault_manager),
      m_dialog_manager(dialog_manager),
      m_menu_manager(menu_manager),
            m_vault_ui_state_applier(ui_state_applier),
      m_v2_auth_handler(v2_auth_handler),
            m_detect_vault_version(std::move(detect_vault_version)),
      m_save_current_account(std::move(save_current_account)),
      m_prompt_save_if_modified(std::move(prompt_save_if_modified)),
      m_initialize_repositories(std::move(initialize_repositories)),
      m_reset_repositories(std::move(reset_repositories)),
      m_initialize_services(std::move(initialize_services)),
      m_reset_services(std::move(reset_services)),
      m_update_account_list(std::move(update_account_list)),
      m_update_tag_filter(std::move(update_tag_filter)),
      m_clear_account_details(std::move(clear_account_details)),
      m_clear_account_tree(std::move(clear_account_tree)),
      m_update_undo_redo_sensitivity(std::move(update_undo_redo_sensitivity)),
      m_clear_undo_history(std::move(clear_undo_history)),
            m_on_user_activity(std::move(on_user_activity)),
            m_set_status_text(std::move(set_status_text)),
            m_pre_close_cleanup(std::move(pre_close_cleanup)) {

    if (!m_dialog_manager || !m_vault_ui_state_applier || !m_vault_manager) {
        KeepTower::Log::debug("VaultUiCoordinator: Missing required dependencies for VaultOpenHandler");
        return;
    }

    if (!m_detect_vault_version) {
        KeepTower::Log::debug("VaultUiCoordinator: Missing callbacks for vault open flow");
        return;
    }

    m_vault_open_handler = std::make_unique<UI::VaultOpenHandler>(
        m_parent_window,
        m_vault_manager,
        m_dialog_manager,
        m_state.vault_open,
        m_state.is_locked,
        m_state.current_vault_path,
        m_state.cached_master_password,
        [this](const std::string& message) {
            if (m_dialog_manager) {
                m_dialog_manager->show_error_dialog(message);
            }
        },
        [this](const std::string& message, const std::string& title) {
            if (m_dialog_manager) {
                m_dialog_manager->show_info_dialog(message, title);
            }
        },
        [this](const std::string& path) {
            return m_detect_vault_version(path);
        },
        [this](const std::string& path) {
            handle_v2_vault_open(path);
        },
        [this](const std::string& vault_path, const std::string& username) {
            complete_vault_opening(vault_path, username);
        }
    );
}

VaultUiCoordinator::~VaultUiCoordinator() {
    if (!m_state.cached_master_password.empty()) {
        std::fill(m_state.cached_master_password.begin(), m_state.cached_master_password.end(), '\0');
        m_state.cached_master_password.clear();
    }
}

void VaultUiCoordinator::on_new_vault() {
    if (!m_vault_open_handler) {
        KeepTower::Log::debug("VaultUiCoordinator: VaultOpenHandler missing");
        return;
    }
    m_vault_open_handler->handle_new_vault();
}

void VaultUiCoordinator::on_open_vault() {
    if (!m_vault_open_handler) {
        KeepTower::Log::debug("VaultUiCoordinator: VaultOpenHandler missing");
        return;
    }
    m_vault_open_handler->handle_open_vault();
}

void VaultUiCoordinator::on_save_vault() {
    if (!m_state.vault_open) {
        return;
    }

    if (m_save_current_account) {
        (void)m_save_current_account();
    }

    if (!m_vault_manager) {
        if (m_set_status_text) {
            m_set_status_text("Failed to save vault");
        }
        return;
    }

    const bool result = m_vault_manager->save_vault();
    if (m_set_status_text) {
        if (result) {
            m_set_status_text("Vault saved: " + m_state.current_vault_path);
        } else {
            m_set_status_text("Failed to save vault");
        }
    }
}

void VaultUiCoordinator::on_close_vault() {
    KeepTower::Log::debug("VaultUiCoordinator: on_close_vault() called - m_vault_open={}", m_state.vault_open);
    if (!m_state.vault_open) {
        KeepTower::Log::debug("VaultUiCoordinator: Vault not open, returning early");
        return;
    }

    KeepTower::Log::debug("VaultUiCoordinator: Proceeding with vault close");

    if (m_pre_close_cleanup) {
        m_pre_close_cleanup();
    }

    // Clear cached password
    if (!m_state.cached_master_password.empty()) {
        std::fill(m_state.cached_master_password.begin(), m_state.cached_master_password.end(), '\0');
        m_state.cached_master_password.clear();
    }

    // Save the current account before closing
    if (m_save_current_account) {
        (void)m_save_current_account();
    }

    // Prompt to save if modified
    if (m_prompt_save_if_modified) {
        if (!m_prompt_save_if_modified()) {
            return; // user cancelled
        }
    }

    if (!m_vault_manager) {
        if (m_set_status_text) {
            m_set_status_text("Error closing vault");
        }
        return;
    }

    const bool result = m_vault_manager->close_vault();
    if (!result) {
        if (m_set_status_text) {
            m_set_status_text("Error closing vault");
        }
        return;
    }

    if (m_clear_undo_history) {
        m_clear_undo_history();
    }

    if (m_reset_repositories) {
        m_reset_repositories();
    }

    if (m_vault_ui_state_applier) {
        m_vault_ui_state_applier->set_vault_closed();
    }

    // Reset local state cache to maintain consistency
    m_state.vault_open = false;
    m_state.is_locked = false;
    m_state.current_vault_path.clear();

    update_menu_for_role();

    if (m_clear_account_tree) {
        m_clear_account_tree();
    }
    if (m_clear_account_details) {
        m_clear_account_details();
    }
}

void VaultUiCoordinator::handle_v2_vault_open(const std::string& vault_path) {
    if (!m_v2_auth_handler) {
        KeepTower::Log::debug("VaultUiCoordinator: V2AuthenticationHandler missing");
        if (m_dialog_manager) {
            m_dialog_manager->show_error_dialog("V2 authentication handler not available");
        }
        return;
    }

    m_v2_auth_handler->handle_vault_open(
        vault_path,
        [this](const std::string& path, const std::string& username) {
            // Save vault after successful authentication (for password changes, etc.)
            on_save_vault();
            complete_vault_opening(path, username);
        }
    );
}

void VaultUiCoordinator::complete_vault_opening(const std::string& vault_path, const std::string& username) {
    KeepTower::Log::debug(
        "VaultUiCoordinator: complete_vault_opening() called - vault_path='{}', username='{}'",
        vault_path,
        username
    );

    if (m_vault_ui_state_applier) {
        KeepTower::Log::debug("VaultUiCoordinator: Setting vault opened state");
        m_vault_ui_state_applier->set_vault_opened(vault_path, username);
    }

    // Maintain local state cache for quick access without manager queries
    m_state.current_vault_path = vault_path;
    m_state.vault_open = true;
    m_state.is_locked = false;

    if (m_initialize_repositories) {
        KeepTower::Log::debug("VaultUiCoordinator: Initializing repositories");
        m_initialize_repositories();
    }

    KeepTower::Log::debug("VaultUiCoordinator: About to call update_session_display()");
    update_session_display();
    KeepTower::Log::debug("VaultUiCoordinator: Returned from update_session_display()");

    if (m_update_account_list) {
        KeepTower::Log::debug("VaultUiCoordinator: About to call update_account_list()");
        m_update_account_list();
    }
    if (m_update_tag_filter) {
        KeepTower::Log::debug("VaultUiCoordinator: About to call update_tag_filter_dropdown()");
        m_update_tag_filter();
    }

    if (m_update_undo_redo_sensitivity) {
        KeepTower::Log::debug("VaultUiCoordinator: Setting undo/redo sensitivity");
        m_update_undo_redo_sensitivity(false, false);
    }

    if (m_on_user_activity) {
        KeepTower::Log::debug("VaultUiCoordinator: Starting activity monitoring");
        m_on_user_activity();
    }

    if (m_vault_ui_state_applier) {
        KeepTower::Log::debug("VaultUiCoordinator: Setting status label");
        if (!username.empty()) {
            m_vault_ui_state_applier->set_status("Vault opened: " + vault_path + " (User: " + username + ")");
        } else {
            m_vault_ui_state_applier->set_status("Vault opened: " + vault_path);
        }
    }

    KeepTower::Log::debug("VaultUiCoordinator: complete_vault_opening() completed successfully");
}

void VaultUiCoordinator::update_session_display() {
    KeepTower::Log::debug("VaultUiCoordinator: update_session_display() called");

    if (!m_vault_ui_state_applier) {
        return;
    }

    std::optional<std::string> session_text;
    if (m_vault_manager) {
        const auto session_opt = m_vault_manager->get_current_user_session();
        if (session_opt.has_value()) {
            const auto& session = *session_opt;
            session_text = "User: " + session.username;

            if (session.is_admin()) {
                *session_text += " (Admin)";
            } else {
                *session_text += " (Standard)";
            }

            if (session.password_change_required) {
                *session_text += " [Password Change Required]";
            }
        }
    }

    m_vault_ui_state_applier->set_session_text(session_text);

    KeepTower::Log::debug("VaultUiCoordinator: Calling update_menu_for_role() after session update");
    update_menu_for_role();

    KeepTower::Log::debug("VaultUiCoordinator: update_session_display() completed");
}

void VaultUiCoordinator::update_menu_for_role() {
    KeepTower::Log::debug("VaultUiCoordinator: update_menu_for_role() called");

    if (!m_menu_manager) {
        return;
    }

    const bool is_v2 = is_v2_vault_open();
    const bool is_admin = is_v2 && is_current_user_admin();
    m_menu_manager->update_menu_for_role(is_v2, is_admin, m_state.vault_open);

    KeepTower::Log::debug("VaultUiCoordinator: update_menu_for_role() completed (V2={}, Admin={})", is_v2, is_admin);
}

void VaultUiCoordinator::apply_lock_state_ui(bool locked, const std::string& status) {
    if (!m_vault_ui_state_applier) {
        return;
    }

    m_vault_ui_state_applier->set_vault_locked(locked, m_state.vault_open);
    if (!status.empty()) {
        m_vault_ui_state_applier->set_status(status);
    }
}

bool VaultUiCoordinator::is_v2_vault_open() const noexcept {
    const bool has_manager = (m_vault_manager != nullptr);
    const bool vault_open_flag = m_state.vault_open;
    const bool is_v2 = has_manager && m_vault_manager->is_v2_vault();

    KeepTower::Log::debug(
        "VaultUiCoordinator: is_v2_vault_open() check - manager={}, m_vault_open={}, is_v2_vault()={}",
        has_manager,
        vault_open_flag,
        is_v2
    );

    if (!m_vault_manager || !m_state.vault_open) {
        return false;
    }

    return m_vault_manager->is_v2_vault();
}

bool VaultUiCoordinator::is_current_user_admin() const noexcept {
    if (!m_vault_manager) {
        return false;
    }

    auto session_opt = m_vault_manager->get_current_user_session();
    if (!session_opt) {
        return false;
    }

    return session_opt->is_admin();
}
