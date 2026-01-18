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
                                      KeepTower::ClipboardManager* clipboard_manager,
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
    , m_clipboard_manager(clipboard_manager)
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

    // Check if user has YubiKey enrolled
    bool yubikey_enrolled = false;
#ifdef HAVE_YUBIKEY_SUPPORT
    auto users = m_vault_manager->list_users();
    for (const auto& user : users) {
        if (user.username == session.username && user.yubikey_enrolled) {
            yubikey_enrolled = true;
            break;
        }
    }
#endif

    // Show password change dialog (voluntary mode)
    auto* change_dialog = new ChangePasswordDialog(m_window, min_length, false);  // false = voluntary

#ifdef HAVE_YUBIKEY_SUPPORT
    // Show PIN field if YubiKey is enrolled
    if (yubikey_enrolled) {
        change_dialog->set_yubikey_required(true);
    }
#endif

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
        // Check if user has YubiKey enrolled (for progress callback)
        YubiKeyPromptDialog* touch_dialog = nullptr;
        bool yubikey_enrolled_for_user = false;
        auto users = m_vault_manager->list_users();
        for (const auto& user : users) {
            if (user.username == username && user.yubikey_enrolled) {
                yubikey_enrolled_for_user = true;
                // Create dialog (will be shown in progress callback)
                touch_dialog = Gtk::make_managed<YubiKeyPromptDialog>(m_window,
                    YubiKeyPromptDialog::PromptType::TOUCH);
                break;
            }
        }
#endif

        // Capture passwords before clearing req
        Glib::ustring current_password_copy = req.current_password;
        Glib::ustring new_password_copy = req.new_password;
        std::optional<std::string> yubikey_pin_opt;
        if (!req.yubikey_pin.empty()) {
            yubikey_pin_opt = req.yubikey_pin;
        }

        // Clear original request
        req.clear();

#ifdef HAVE_YUBIKEY_SUPPORT
        // Use shared_ptr to safely share dialog between callbacks
        auto touch_dialog_ptr = std::make_shared<YubiKeyPromptDialog*>(touch_dialog);

        // Progress callback: update YubiKey touch dialog with specific message for each touch
        auto progress_callback = [this, touch_dialog_ptr, yubikey_enrolled_for_user]
            (int step, int total, const std::string& message) {
            if (yubikey_enrolled_for_user && *touch_dialog_ptr) {
                // Update dialog message with specific touch prompt
                std::string formatted_message = "<big><b>Changing Password with YubiKey</b></big>\n\n" + message;
                (*touch_dialog_ptr)->update_message(formatted_message);

                // Show dialog on first progress update
                if (!(*touch_dialog_ptr)->get_visible()) {
                    (*touch_dialog_ptr)->present();
                }
            }
        };
#else
        auto progress_callback = [](int, int, const std::string&) {};
#endif

        // Completion callback: handle result and clean up
        auto completion_callback = [this, username, min_length,
                                     current_password_copy, new_password_copy
#ifdef HAVE_YUBIKEY_SUPPORT
            , touch_dialog_ptr
#endif
        ](KeepTower::VaultResult<> result) mutable {

#ifdef HAVE_YUBIKEY_SUPPORT
            // Hide touch dialog if shown
            if (*touch_dialog_ptr) {
                (*touch_dialog_ptr)->hide();
                *touch_dialog_ptr = nullptr;
            }
#endif

            // Clear password copies
            const_cast<Glib::ustring&>(current_password_copy).clear();
            const_cast<Glib::ustring&>(new_password_copy).clear();

            if (!result) {
                // Password change failed
                std::string error_msg = "Failed to change password";
                if (result.error() == KeepTower::VaultError::AuthenticationFailed) {
                    error_msg = "Current password is incorrect";
                } else if (result.error() == KeepTower::VaultError::WeakPassword) {
                    error_msg = "New password must be at least " + std::to_string(min_length) + " characters";
                } else if (result.error() == KeepTower::VaultError::PasswordReused) {
                    error_msg = "This password was used previously. Please choose a different password.";
                } else if (result.error() == KeepTower::VaultError::YubiKeyNotPresent) {
                    error_msg = "YubiKey is required but not detected";
                } else if (result.error() == KeepTower::VaultError::YubiKeyError) {
                    error_msg = "YubiKey operation failed. Please try again.";
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
        };

        // Execute async password change (non-blocking)
        m_vault_manager->change_user_password_async(
            username,
            current_password_copy,
            new_password_copy,
            progress_callback,
            completion_callback,
            yubikey_pin_opt
        );
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
    auto* dialog = new UserManagementDialog(m_window, *m_vault_manager, session_opt->username, m_clipboard_manager);

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
