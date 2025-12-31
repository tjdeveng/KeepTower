// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2024 Travis E. Hansen

#include "UserAccountHandler.h"
#include "DialogManager.h"
#include "../../core/VaultManager.h"
#include "../dialogs/ChangePasswordDialog.h"
#include "../dialogs/UserManagementDialog.h"

#ifdef HAVE_YUBIKEY_SUPPORT
#include "../dialogs/YubiKeyPromptDialog.h"
#endif

namespace UI {

UserAccountHandler::UserAccountHandler(Gtk::Window& window,
                                      VaultManager* vault_manager,
                                      DialogManager* dialog_manager,
                                      Glib::ustring& current_vault_path_ref,
                                      StatusCallback status_callback,
                                      ErrorDialogCallback error_dialog_callback,
                                      CloseVaultCallback close_vault_callback,
                                      HandleV2VaultOpenCallback handle_v2_vault_open_callback,
                                      IsV2VaultOpenCallback is_v2_vault_open_callback,
                                      IsCurrentUserAdminCallback is_current_user_admin_callback,
                                      PromptSaveIfModifiedCallback prompt_save_if_modified_callback)
    : m_window(window)
    , m_vault_manager(vault_manager)
    , m_dialog_manager(dialog_manager)
    , m_current_vault_path(current_vault_path_ref)
    , m_status_callback(std::move(status_callback))
    , m_error_dialog_callback(std::move(error_dialog_callback))
    , m_close_vault_callback(std::move(close_vault_callback))
    , m_handle_v2_vault_open_callback(std::move(handle_v2_vault_open_callback))
    , m_is_v2_vault_open_callback(std::move(is_v2_vault_open_callback))
    , m_is_current_user_admin_callback(std::move(is_current_user_admin_callback))
    , m_prompt_save_if_modified_callback(std::move(prompt_save_if_modified_callback))
{
}

void UserAccountHandler::handle_change_password() {
    if (!m_vault_manager || !m_is_v2_vault_open_callback()) {
        m_error_dialog_callback("No V2 vault is open");
        return;
    }

    auto session_opt = m_vault_manager->get_current_user_session();
    if (!session_opt) {
        m_error_dialog_callback("No active user session");
        return;
    }

    const auto& session = *session_opt;

    // Get vault security policy for password requirements
    auto policy_opt = m_vault_manager->get_vault_security_policy();
    const uint32_t min_length = policy_opt ? policy_opt->min_password_length : 12;

    // Show password change dialog (voluntary mode)
    auto* change_dialog = new ChangePasswordDialog(m_window, min_length, false);  // false = voluntary

    change_dialog->signal_response().connect([this, change_dialog, username = session.username, min_length](int response) {
        if (response != Gtk::ResponseType::OK) {
            change_dialog->hide();
            delete change_dialog;
            return;
        }

        // Get new password
        auto req = change_dialog->get_request();
        change_dialog->hide();
        delete change_dialog;

        // Validate password BEFORE showing YubiKey prompt
        // This allows fail-fast for invalid passwords without YubiKey interaction
        auto validation = m_vault_manager->validate_new_password(username, req.new_password);
        if (!validation) {
            // Validation failed - show error
            std::string error_msg = "Failed to validate password";
            if (validation.error() == KeepTower::VaultError::WeakPassword) {
                error_msg = "New password must be at least " + std::to_string(min_length) + " characters";
            } else if (validation.error() == KeepTower::VaultError::PasswordReused) {
                error_msg = "This password was used previously. Please choose a different password.";
            }

            m_error_dialog_callback(error_msg);
            req.clear();
            return;
        }

#ifdef HAVE_YUBIKEY_SUPPORT
        // Validation passed - now show YubiKey prompt if enrolled
        YubiKeyPromptDialog* touch_dialog = nullptr;
        auto users = m_vault_manager->list_users();
        for (const auto& user : users) {
            if (user.username == username && user.yubikey_enrolled) {
                touch_dialog = Gtk::make_managed<YubiKeyPromptDialog>(m_window,
                    YubiKeyPromptDialog::PromptType::TOUCH);
                touch_dialog->present();

                // Force GTK to process events and render the dialog
                auto context = Glib::MainContext::get_default();
                while (context->pending()) {
                    context->iteration(false);
                }
                g_usleep(150000);  // 150ms to ensure dialog is visible
                break;
            }
        }
#endif

        // Attempt password change (password already validated, just needs YubiKey operations)
        auto result = m_vault_manager->change_user_password(username, req.current_password, req.new_password);

#ifdef HAVE_YUBIKEY_SUPPORT
        if (touch_dialog) {
            touch_dialog->hide();
        }
#endif

        // Clear passwords immediately
        req.clear();

        if (!result) {
            // Password change failed
            std::string error_msg = "Failed to change password";
            if (result.error() == KeepTower::VaultError::AuthenticationFailed) {
                error_msg = "Current password is incorrect";
            } else if (result.error() == KeepTower::VaultError::WeakPassword) {
                error_msg = "New password must be at least " + std::to_string(min_length) + " characters";
            } else if (result.error() == KeepTower::VaultError::PasswordReused) {
                error_msg = "This password was used previously. Please choose a different password.";
            }

            m_error_dialog_callback(error_msg);
            return;
        }

        // Password changed successfully
        m_status_callback("Password changed successfully");

        auto* success_dlg = new Gtk::MessageDialog(
            m_window,
            "Password changed successfully",
            false,
            Gtk::MessageType::INFO
        );
        success_dlg->set_modal(true);
        success_dlg->signal_response().connect([success_dlg](int) {
            success_dlg->hide();
            delete success_dlg;
        });
        success_dlg->show();
    });

    change_dialog->show();
}

void UserAccountHandler::handle_logout() {
    if (!m_vault_manager || !m_is_v2_vault_open_callback()) {
        return;
    }

    // Prompt to save if modified
    if (!m_prompt_save_if_modified_callback()) {
        return;  // User cancelled
    }

    // Save vault before logout (prompt_save_if_modified already handles saving)

    // Close vault (this logs out the user)
    std::string vault_path{m_current_vault_path};  // Save path before close (convert to std::string)
    m_close_vault_callback();

    // Reopen the same vault file (will show login dialog)
    if (!vault_path.empty()) {
        m_handle_v2_vault_open_callback(vault_path);
    }
}

void UserAccountHandler::handle_manage_users() {
    if (!m_vault_manager || !m_is_v2_vault_open_callback()) {
        m_error_dialog_callback("No V2 vault is open");
        return;
    }

    // Check if current user is administrator
    if (!m_is_current_user_admin_callback()) {
        m_error_dialog_callback("Only administrators can manage users");
        return;
    }

    auto session_opt = m_vault_manager->get_current_user_session();
    if (!session_opt) {
        m_error_dialog_callback("No active user session");
        return;
    }

    // Show user management dialog
    auto* dialog = new UserManagementDialog(m_window, *m_vault_manager, session_opt->username);

    // Handle relogin request
    dialog->m_signal_request_relogin.connect([this]([[maybe_unused]] const std::string& new_username) {
        // Store vault path before closing
        std::string vault_path{m_current_vault_path};

        // Close current vault (logout)
        m_close_vault_callback();

        // Reopen vault with login dialog (it will default to the last username)
        m_handle_v2_vault_open_callback(vault_path);
    });

    dialog->signal_response().connect([dialog](int) {
        dialog->hide();
        delete dialog;
    });

    dialog->show();
}

}  // namespace UI
