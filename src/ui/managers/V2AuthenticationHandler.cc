// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "V2AuthenticationHandler.h"
#include "DialogManager.h"
#include "../../core/VaultManager.h"
#include "../../utils/Log.h"
#include "../windows/MainWindow.h"
#include "../dialogs/V2UserLoginDialog.h"
#include "../dialogs/ChangePasswordDialog.h"

#ifdef HAVE_YUBIKEY_SUPPORT
#include "../dialogs/YubiKeyPromptDialog.h"
#include "../../core/managers/YubiKeyManager.h"
#endif

// Forward declare OPENSSL_cleanse to avoid including openssl headers that conflict with UI namespace
extern "C" {
    void OPENSSL_cleanse(void *ptr, size_t len);
}

namespace UI {

V2AuthenticationHandler::V2AuthenticationHandler(MainWindow& window,
                                                 VaultManager* vault_manager,
                                                 DialogManager* dialog_manager)
    : m_window(window)
    , m_vault_manager(vault_manager)
    , m_dialog_manager(dialog_manager) {
}

void V2AuthenticationHandler::handle_vault_open(const std::string& vault_path,
                                                 AuthSuccessCallback on_success) {
    m_current_vault_path = vault_path;
    m_success_callback = std::move(on_success);

    // Check if YubiKey is required
    std::string yubikey_serial;
    bool yubikey_required = m_vault_manager->check_vault_requires_yubikey(vault_path, yubikey_serial);

#ifdef HAVE_YUBIKEY_SUPPORT
    if (yubikey_required) {
        // Check if YubiKey is present
        YubiKeyManager yk_manager;
        [[maybe_unused]] bool yk_init = yk_manager.initialize();

        if (!yk_manager.is_yubikey_present()) {
            // Show "Insert YubiKey" dialog
            auto yk_dialog = Gtk::make_managed<YubiKeyPromptDialog>(m_window,
                YubiKeyPromptDialog::PromptType::INSERT, yubikey_serial);

            yk_dialog->signal_response().connect([this, yk_dialog](int yk_response) {
                if (yk_response == Gtk::ResponseType::OK) {
                    // User clicked Retry
                    yk_dialog->hide();
                    handle_vault_open(m_current_vault_path, m_success_callback);  // Retry
                    return;
                }
                yk_dialog->hide();
            });

            yk_dialog->show();
            return;
        }
    }
#endif

    // Show V2 user login dialog
    auto login_dialog = Gtk::make_managed<V2UserLoginDialog>(m_window, yubikey_required);

    login_dialog->signal_response().connect([this, login_dialog, yubikey_required](int response) {
        if (response != Gtk::ResponseType::OK) {
            login_dialog->hide();
            return;
        }

        // Get credentials from dialog
        auto creds = login_dialog->get_credentials();
        login_dialog->hide();

#ifdef HAVE_YUBIKEY_SUPPORT
        // Show YubiKey touch prompt if required
        YubiKeyPromptDialog* touch_dialog = nullptr;
        if (yubikey_required) {
            touch_dialog = Gtk::make_managed<YubiKeyPromptDialog>(m_window,
                YubiKeyPromptDialog::PromptType::TOUCH);
            touch_dialog->present();

            // Force GTK to process events
            auto context = Glib::MainContext::get_default();
            while (context->pending()) {
                context->iteration(false);
            }
            g_usleep(150000);  // 150ms delay
        }
#endif

        // Attempt V2 vault authentication
        auto result = m_vault_manager->open_vault_v2(m_current_vault_path, creds.username, creds.password);

        // Clear credentials immediately
        creds.clear();

#ifdef HAVE_YUBIKEY_SUPPORT
        if (touch_dialog) {
            touch_dialog->hide();
        }
#endif

        if (!result) {
            // Authentication failed
            std::string error_message = "Authentication failed";
            if (result.error() == KeepTower::VaultError::AuthenticationFailed) {
                error_message = "Invalid username or password";
            } else if (result.error() == KeepTower::VaultError::UserNotFound) {
                error_message = "User not found";
            }

            m_dialog_manager->show_error_dialog(error_message);
            return;
        }

        // Successfully authenticated - check for password change requirement
        KeepTower::Log::info("V2AuthenticationHandler: Authentication succeeded");
        auto session_opt = m_vault_manager->get_current_user_session();
        if (!session_opt) {
            KeepTower::Log::error("V2AuthenticationHandler: No session after successful authentication!");
            m_dialog_manager->show_error_dialog("Internal error: No session after successful authentication");
            return;
        }

        const auto& session = *session_opt;
        KeepTower::Log::info("V2AuthenticationHandler: Session obtained - username='{}', password_change_required={}",
            session.username, session.password_change_required);

        // Check if password change is required
        if (session.password_change_required) {
            KeepTower::Log::info("V2AuthenticationHandler: Password change required");
            handle_password_change_required(session.username);
            return;
        }

        // Complete authentication - call success callback
        KeepTower::Log::info("V2AuthenticationHandler: Authentication complete");
        if (m_success_callback) {
            m_success_callback(m_current_vault_path, session.username);
        }
    });

    login_dialog->show();
}

void V2AuthenticationHandler::handle_password_change_required(const std::string& username) {
    // Get vault security policy for min password length
    auto policy_opt = m_vault_manager->get_vault_security_policy();
    const uint32_t min_length = policy_opt ? policy_opt->min_password_length : 12;

    // Show password change dialog in forced mode
    auto change_dialog = Gtk::make_managed<ChangePasswordDialog>(m_window, min_length, true);

    change_dialog->signal_response().connect([this, change_dialog, username, min_length](int response) {
        if (response != Gtk::ResponseType::OK) {
            // User cancelled - cannot proceed
            change_dialog->hide();
            m_dialog_manager->show_error_dialog(
                "Password change is required to access this vault.\nVault has been closed.");
            return;
        }

        // Get new password
        auto req = change_dialog->get_request();
        change_dialog->hide();

        // Validate password BEFORE showing YubiKey prompt
        auto validation = m_vault_manager->validate_new_password(username, req.new_password);
        if (!validation) {
            // Validation failed
            std::string error_msg = "Failed to validate password";
            if (validation.error() == KeepTower::VaultError::WeakPassword) {
                error_msg = "New password must be at least " + std::to_string(min_length) + " characters";
            } else if (validation.error() == KeepTower::VaultError::PasswordReused) {
                error_msg = "This password was used previously. Please choose a different password.";
            }

            auto error_dialog = Gtk::make_managed<Gtk::MessageDialog>(
                m_window, error_msg, false, Gtk::MessageType::ERROR, Gtk::ButtonsType::OK, true);
            error_dialog->signal_response().connect([this, username](int) {
                // Retry after error
                handle_password_change_required(username);
            });
            error_dialog->present();

            req.clear();
            return;
        }

#ifdef HAVE_YUBIKEY_SUPPORT
        // Validation passed - show YubiKey prompt if enrolled
        YubiKeyPromptDialog* touch_dialog = nullptr;
        auto users = m_vault_manager->list_users();
        for (const auto& user : users) {
            if (user.username == username && user.yubikey_enrolled) {
                touch_dialog = Gtk::make_managed<YubiKeyPromptDialog>(m_window,
                    YubiKeyPromptDialog::PromptType::TOUCH);
                touch_dialog->present();

                // Force GTK to process events
                auto context = Glib::MainContext::get_default();
                while (context->pending()) {
                    context->iteration(false);
                }
                g_usleep(150000);
                break;
            }
        }
#endif

        // Attempt password change
        auto result = m_vault_manager->change_user_password(username, req.current_password, req.new_password);

#ifdef HAVE_YUBIKEY_SUPPORT
        if (touch_dialog) {
            touch_dialog->hide();
        }
#endif

        if (!result) {
            // Password change failed
            std::string error_msg = "Failed to change password";
            if (result.error() == KeepTower::VaultError::WeakPassword) {
                error_msg = "New password must be at least " + std::to_string(min_length) + " characters";
            } else if (result.error() == KeepTower::VaultError::PasswordReused) {
                error_msg = "This password was used previously. Please choose a different password.";
            }

            auto error_dialog = Gtk::make_managed<Gtk::MessageDialog>(
                m_window, error_msg, false, Gtk::MessageType::ERROR, Gtk::ButtonsType::OK, true);
            error_dialog->set_title("Password Change Failed");
            error_dialog->signal_response().connect([this, error_dialog, username](int) {
                error_dialog->hide();
                // Retry password change
                handle_password_change_required(username);
            });
            error_dialog->show();

            // Clear passwords immediately
            req.clear();
            return;
        }

        // Save vault after password change to persist new wrapped_dek
        if (!m_vault_manager->save_vault()) {
            auto error_dialog = Gtk::make_managed<Gtk::MessageDialog>(
                m_window,
                "Password changed successfully but failed to save vault.\nPlease save manually.",
                false,
                Gtk::MessageType::WARNING,
                Gtk::ButtonsType::OK,
                true);
            error_dialog->set_title("Save Failed");
            error_dialog->signal_response().connect([error_dialog](int) {
                error_dialog->hide();
            });
            error_dialog->show();
        }

        // Password changed successfully - check for YubiKey enrollment requirement
        auto session_opt = m_vault_manager->get_current_user_session();
        if (session_opt && session_opt->requires_yubikey_enrollment) {
            // Save new password temporarily for enrollment (will be cleared after enrollment)
            Glib::ustring new_password = req.new_password;
            req.clear();  // Clear the request but keep new_password for enrollment

            handle_yubikey_enrollment_required(username, new_password);
            return;
        }

        // Clear passwords immediately
        req.clear();

        // Complete authentication
        if (m_success_callback) {
            m_success_callback(m_current_vault_path, username);
        }
    });

    change_dialog->show();
}

void V2AuthenticationHandler::handle_yubikey_enrollment_required(const std::string& username,
                                                                  const Glib::ustring& password) {
#ifdef HAVE_YUBIKEY_SUPPORT
    // Show message dialog explaining requirement
    auto info_dialog = Gtk::make_managed<Gtk::MessageDialog>(
        m_window,
        "YubiKey enrollment is required by vault policy.\n\n"
        "You must enroll your YubiKey to access this vault.\n\n"
        "Please ensure your YubiKey is connected, then click OK to continue.",
        false,
        Gtk::MessageType::INFO,
        Gtk::ButtonsType::OK_CANCEL,
        true);
    info_dialog->set_title("YubiKey Enrollment Required");

    info_dialog->signal_response().connect([this, info_dialog, username, password](int response) {
        info_dialog->hide();

        if (response != Gtk::ResponseType::OK) {
            // User cancelled
            m_dialog_manager->show_error_dialog("YubiKey enrollment is required.\nVault has been closed.");
            return;
        }

        // If password was provided (from password change), only ask for PIN
        // Otherwise, ask for both password and PIN
        if (!password.empty()) {
            // Password already known - just ask for PIN
            auto pin_dialog = Gtk::make_managed<Gtk::MessageDialog>(
                m_window,
                "Set a YubiKey PIN for enrollment:",
                false,
                Gtk::MessageType::QUESTION,
                Gtk::ButtonsType::OK_CANCEL,
                true);
            pin_dialog->set_title("YubiKey PIN");

            auto* content_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
            content_box->set_spacing(12);

            // PIN entry
            auto* pin_label = Gtk::make_managed<Gtk::Label>("YubiKey PIN (4-63 characters):");
            pin_label->set_halign(Gtk::Align::START);
            content_box->append(*pin_label);

            auto* pin_entry = Gtk::make_managed<Gtk::Entry>();
            pin_entry->set_visibility(false);
            pin_entry->set_activates_default(true);
            pin_entry->set_max_length(63);
            pin_entry->set_placeholder_text("Set a PIN for your YubiKey (4-63 chars)");
            content_box->append(*pin_entry);

            // Info label
            auto* info_label = Gtk::make_managed<Gtk::Label>(
                "This PIN will be stored securely and required for all future logins with this YubiKey.");
            info_label->set_wrap(true);
            info_label->set_halign(Gtk::Align::START);
            info_label->set_margin_top(8);
            info_label->add_css_class("dim-label");
            content_box->append(*info_label);

            pin_dialog->get_content_area()->append(*content_box);
            pin_dialog->set_default_response(Gtk::ResponseType::OK);

            pin_dialog->signal_response().connect([this, pin_dialog, pin_entry, username, password](int pin_response) {
                if (pin_response != Gtk::ResponseType::OK) {
                    pin_dialog->hide();
                    m_dialog_manager->show_error_dialog("YubiKey enrollment cancelled.\nVault has been closed.");
                    return;
                }

                auto pin = pin_entry->get_text().raw();
                pin_dialog->hide();

                // Validate PIN length
                if (pin.empty() || pin.length() < 4 || pin.length() > 63) {
                    auto error_dialog = Gtk::make_managed<Gtk::MessageDialog>(
                        m_window,
                        "YubiKey PIN must be between 4 and 63 characters.",
                        false,
                        Gtk::MessageType::ERROR,
                        Gtk::ButtonsType::OK,
                        true);
                    error_dialog->set_title("Invalid PIN");
                    error_dialog->signal_response().connect([this, error_dialog, username, password](int) {
                        error_dialog->hide();
                        handle_yubikey_enrollment_required(username, password);  // Retry
                    });
                    error_dialog->show();

                    // Clear sensitive data
                    pin_entry->set_text("");
                    if (!pin.empty()) {
                        OPENSSL_cleanse(const_cast<char*>(pin.data()), pin.size());
                    }
                    pin.clear();
                    return;
                }

                // Show YubiKey touch prompt
                auto touch_dialog = Gtk::make_managed<YubiKeyPromptDialog>(m_window,
                    YubiKeyPromptDialog::PromptType::TOUCH);
                touch_dialog->present();

                // Force GTK to process events
                auto context = Glib::MainContext::get_default();
                while (context->pending()) {
                    context->iteration(false);
                }
                g_usleep(150000);

                // Attempt YubiKey enrollment with PIN
                auto result = m_vault_manager->enroll_yubikey_for_user(username, password, pin);

                // Clear PIN immediately
                pin_entry->set_text("");
                if (!pin.empty()) {
                    OPENSSL_cleanse(const_cast<char*>(pin.data()), pin.size());
                }
                pin.clear();

                touch_dialog->hide();

                if (!result) {
                    std::string error_msg = "Failed to enroll YubiKey";
                    if (result.error() == KeepTower::VaultError::YubiKeyNotPresent) {
                        error_msg = "YubiKey not detected. Please connect your YubiKey and try again.";
                    } else if (result.error() == KeepTower::VaultError::AuthenticationFailed) {
                        error_msg = "Authentication failed. The password or PIN may be incorrect.";
                    } else if (result.error() == KeepTower::VaultError::CryptoError) {
                        error_msg = "Cryptographic operation failed during enrollment.";
                    } else if (result.error() == KeepTower::VaultError::YubiKeyError) {
                        error_msg = "YubiKey error occurred. Please check your YubiKey.";
                    } else {
                        error_msg = "Failed to enroll YubiKey (error code: " +
                                   std::to_string(static_cast<int>(result.error())) + ")";
                    }

                    KeepTower::Log::error("V2AuthenticationHandler: YubiKey enrollment failed - {}", error_msg);

                    // Show error and retry
                    auto error_dialog = Gtk::make_managed<Gtk::MessageDialog>(
                        m_window, error_msg, false, Gtk::MessageType::ERROR, Gtk::ButtonsType::OK, true);
                    error_dialog->set_title("Enrollment Failed");
                    error_dialog->signal_response().connect([this, error_dialog, username, password](int) {
                        error_dialog->hide();
                        handle_yubikey_enrollment_required(username, password);  // Retry with same password
                    });
                    error_dialog->show();
                    return;
                }

                // YubiKey enrolled successfully - save vault
                if (!m_vault_manager->save_vault()) {
                    m_dialog_manager->show_error_dialog("Failed to save vault after YubiKey enrollment.");
                    return;
                }

                // Complete authentication
                if (m_success_callback) {
                    m_success_callback(m_current_vault_path, username);
                }

                // Show success message
                auto success_dialog = Gtk::make_managed<Gtk::MessageDialog>(
                    m_window,
                    "YubiKey enrolled successfully!\n\nYour YubiKey will be required for all future logins.",
                    false,
                    Gtk::MessageType::INFO,
                    Gtk::ButtonsType::OK,
                    true);
                success_dialog->set_title("Enrollment Complete");
                success_dialog->signal_response().connect([success_dialog](int) {
                    success_dialog->hide();
                });
                success_dialog->show();
            });

            pin_dialog->show();
            return;
        }

        // Password not provided - ask for both password and PIN
        auto pwd_dialog = Gtk::make_managed<Gtk::MessageDialog>(
            m_window,
            "Enter your password and set a YubiKey PIN for enrollment:",
            false,
            Gtk::MessageType::QUESTION,
            Gtk::ButtonsType::OK_CANCEL,
            true);
        pwd_dialog->set_title("YubiKey Enrollment");

        auto* content_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
        content_box->set_spacing(12);

        // Password entry
        auto* pwd_label = Gtk::make_managed<Gtk::Label>("Password:");
        pwd_label->set_halign(Gtk::Align::START);
        content_box->append(*pwd_label);

        auto* password_entry = Gtk::make_managed<Gtk::Entry>();
        password_entry->set_visibility(false);
        password_entry->set_activates_default(false);
        password_entry->set_placeholder_text("Enter your password");
        content_box->append(*password_entry);

        // PIN entry
        auto* pin_label = Gtk::make_managed<Gtk::Label>("YubiKey PIN (4-63 characters):");
        pin_label->set_halign(Gtk::Align::START);
        pin_label->set_margin_top(8);
        content_box->append(*pin_label);

        auto* pin_entry = Gtk::make_managed<Gtk::Entry>();
        pin_entry->set_visibility(false);
        pin_entry->set_activates_default(true);
        pin_entry->set_max_length(63);
        pin_entry->set_placeholder_text("Set a PIN for your YubiKey (4-63 chars)");
        content_box->append(*pin_entry);

        // Info label
        auto* info_label = Gtk::make_managed<Gtk::Label>(
            "This PIN will be stored securely and required for all future logins with this YubiKey.");
        info_label->set_wrap(true);
        info_label->set_halign(Gtk::Align::START);
        info_label->set_margin_top(8);
        info_label->add_css_class("dim-label");
        content_box->append(*info_label);

        pwd_dialog->get_content_area()->append(*content_box);
        pwd_dialog->set_default_response(Gtk::ResponseType::OK);

        pwd_dialog->signal_response().connect([this, pwd_dialog, password_entry, pin_entry, username](int pwd_response) {
            if (pwd_response != Gtk::ResponseType::OK) {
                pwd_dialog->hide();
                m_dialog_manager->show_error_dialog("YubiKey enrollment cancelled.\nVault has been closed.");
                return;
            }

            auto password = password_entry->get_text();
            auto pin = pin_entry->get_text().raw();
            pwd_dialog->hide();

            // Validate PIN length
            if (pin.empty() || pin.length() < 4 || pin.length() > 63) {
                auto error_dialog = Gtk::make_managed<Gtk::MessageDialog>(
                    m_window,
                    "YubiKey PIN must be between 4 and 63 characters.",
                    false,
                    Gtk::MessageType::ERROR,
                    Gtk::ButtonsType::OK,
                    true);
                error_dialog->set_title("Invalid PIN");
                error_dialog->signal_response().connect([this, error_dialog, username](int) {
                    error_dialog->hide();
                    handle_yubikey_enrollment_required(username);  // Retry
                });
                error_dialog->show();

                // Clear sensitive data
                password_entry->set_text("");
                pin_entry->set_text("");
                password = "";
                if (!pin.empty()) {
                    OPENSSL_cleanse(const_cast<char*>(pin.data()), pin.size());
                }
                pin.clear();
                return;
            }

            // Show YubiKey touch prompt
            auto touch_dialog = Gtk::make_managed<YubiKeyPromptDialog>(m_window,
                YubiKeyPromptDialog::PromptType::TOUCH);
            touch_dialog->present();

            // Force GTK to process events
            auto context = Glib::MainContext::get_default();
            while (context->pending()) {
                context->iteration(false);
            }
            g_usleep(150000);

            // Attempt YubiKey enrollment with PIN
            auto result = m_vault_manager->enroll_yubikey_for_user(username, password, pin);

            // Clear password and PIN immediately
            password_entry->set_text("");
            pin_entry->set_text("");
            password = "";
            if (!pin.empty()) {
                OPENSSL_cleanse(const_cast<char*>(pin.data()), pin.size());
            }
            pin.clear();

            touch_dialog->hide();

            if (!result) {
                std::string error_msg = "Failed to enroll YubiKey";
                if (result.error() == KeepTower::VaultError::YubiKeyNotPresent) {
                    error_msg = "YubiKey not detected. Please connect your YubiKey and try again.";
                } else if (result.error() == KeepTower::VaultError::AuthenticationFailed) {
                    error_msg = "Incorrect password. Please enter your current password.";
                } else if (result.error() == KeepTower::VaultError::CryptoError) {
                    error_msg = "Cryptographic operation failed during enrollment.";
                } else if (result.error() == KeepTower::VaultError::YubiKeyError) {
                    error_msg = "YubiKey error occurred. Please check your YubiKey.";
                } else {
                    error_msg = "Failed to enroll YubiKey (error code: " +
                               std::to_string(static_cast<int>(result.error())) + ")";
                }

                KeepTower::Log::error("V2AuthenticationHandler: YubiKey enrollment failed - {}", error_msg);

                // Show error and retry
                auto error_dialog = Gtk::make_managed<Gtk::MessageDialog>(
                    m_window, error_msg, false, Gtk::MessageType::ERROR, Gtk::ButtonsType::OK, true);
                error_dialog->set_title("Enrollment Failed");
                error_dialog->signal_response().connect([this, error_dialog, username](int) {
                    error_dialog->hide();
                    handle_yubikey_enrollment_required(username);  // Retry
                });
                error_dialog->show();
                return;
            }

            // YubiKey enrolled successfully - save vault
            if (!m_vault_manager->save_vault()) {
                m_dialog_manager->show_error_dialog("Failed to save vault after YubiKey enrollment.");
                return;
            }

            // Complete authentication
            if (m_success_callback) {
                m_success_callback(m_current_vault_path, username);
            }

            // Show success message
            auto success_dialog = Gtk::make_managed<Gtk::MessageDialog>(
                m_window,
                "YubiKey enrolled successfully!\n\nYour YubiKey will be required for all future logins.",
                false,
                Gtk::MessageType::INFO,
                Gtk::ButtonsType::OK,
                true);
            success_dialog->set_title("Enrollment Complete");
            success_dialog->signal_response().connect([success_dialog](int) {
                success_dialog->hide();
            });
            success_dialog->show();
        });

        pwd_dialog->show();
    });

    info_dialog->show();
#else
    // YubiKey support not compiled
    m_dialog_manager->show_error_dialog("YubiKey enrollment required but YubiKey support is not available.");
#endif
}

} // namespace UI
