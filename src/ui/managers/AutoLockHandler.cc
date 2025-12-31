// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2024 Travis E. Hansen

#include "AutoLockHandler.h"
#include "DialogManager.h"
#include "UIStateManager.h"
#include "../../core/VaultManager.h"
#include "../controllers/AutoLockManager.h"
#include "../../utils/SettingsValidator.h"
#include "../../utils/Log.h"
#include "../../utils/StringHelpers.h"

#ifdef HAVE_YUBIKEY_SUPPORT
#include "../dialogs/YubiKeyPromptDialog.h"
#endif

namespace UI {

AutoLockHandler::AutoLockHandler(Gtk::Window& window,
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
                                 GetSearchTextCallback get_search_text_callback)
    : m_window(window)
    , m_vault_manager(vault_manager)
    , m_auto_lock_manager(auto_lock_manager)
    , m_dialog_manager(dialog_manager)
    , m_ui_state_manager(ui_state_manager)
    , m_vault_open(vault_open_ref)
    , m_is_locked(is_locked_ref)
    , m_current_vault_path(current_vault_path_ref)
    , m_cached_master_password(cached_master_password_ref)
    , m_save_account_callback(std::move(save_account_callback))
    , m_close_vault_callback(std::move(close_vault_callback))
    , m_update_account_list_callback(std::move(update_account_list_callback))
    , m_filter_accounts_callback(std::move(filter_accounts_callback))
    , m_handle_v2_vault_open_callback(std::move(handle_v2_vault_open_callback))
    , m_is_v2_vault_open_callback(std::move(is_v2_vault_open_callback))
    , m_is_vault_modified_callback(std::move(is_vault_modified_callback))
    , m_get_search_text_callback(std::move(get_search_text_callback))
{
}

void AutoLockHandler::setup_activity_monitoring() {
    // Create event controllers to monitor user activity
    auto key_controller = Gtk::EventControllerKey::create();
    key_controller->signal_key_pressed().connect(
        [this](guint, guint, Gdk::ModifierType) {
            handle_user_activity();
            return false;  // Don't block event
        }, false);
    m_window.add_controller(key_controller);

    auto motion_controller = Gtk::EventControllerMotion::create();
    motion_controller->signal_motion().connect(
        [this](double, double) {
            handle_user_activity();
        });
    m_window.add_controller(motion_controller);

    auto click_controller = Gtk::GestureClick::create();
    click_controller->signal_pressed().connect(
        [this](int, double, double) {
            handle_user_activity();
        });
    m_window.add_controller(click_controller);
}

void AutoLockHandler::handle_user_activity() {
    if (!m_vault_open || m_is_locked || !m_auto_lock_manager) {
        return;
    }

    // Check if auto-lock is enabled
    // CRITICAL: Read from vault if open (security policy), otherwise from GSettings (user preference)
    bool auto_lock_enabled;
    if (m_vault_manager && m_vault_manager->is_vault_open()) {
        auto_lock_enabled = m_vault_manager->get_auto_lock_enabled();
    } else {
        static const auto settings = Gio::Settings::create("com.tjdeveng.keeptower");
        auto_lock_enabled = SettingsValidator::is_auto_lock_enabled(settings);
    }

    // Update AutoLockManager state
    m_auto_lock_manager->set_enabled(auto_lock_enabled);

    if (!auto_lock_enabled) {
        return;
    }

    // Get timeout from vault if open, otherwise from defaults
    int timeout_seconds;
    if (m_vault_manager && m_vault_manager->is_vault_open()) {
        timeout_seconds = m_vault_manager->get_auto_lock_timeout();
        // Clamp to valid range (60-3600 seconds)
        timeout_seconds = std::clamp(timeout_seconds, 60, 3600);
    } else {
        static const auto settings = Gio::Settings::create("com.tjdeveng.keeptower");
        timeout_seconds = SettingsValidator::get_auto_lock_timeout(settings);
    }

    // Phase 1.3: Use AutoLockManager to reset inactivity timer
    m_auto_lock_manager->set_timeout_seconds(timeout_seconds);
    m_auto_lock_manager->reset_timer();
}

bool AutoLockHandler::handle_auto_lock_timeout() {
    if (!m_vault_open || m_is_locked) {
        return false;
    }

    // For V2 vaults, auto-lock means automatic logout (no cached password)
    if (m_is_v2_vault_open_callback()) {
        KeepTower::Log::info("AutoLockHandler: Auto-lock timeout triggered for V2 vault, forcing logout");

        // Auto-save only if vault has been modified (security timeout)
        bool had_unsaved_changes = false;
        if (m_is_vault_modified_callback()) {
            had_unsaved_changes = true;
            m_save_account_callback();
            if (!m_vault_manager->save_vault()) {
                KeepTower::Log::warning("Failed to save vault before auto-lock");
            }
        }

        // Force logout without allowing cancellation (security timeout)
        std::string vault_path{m_current_vault_path};  // Save path before close (convert to std::string)
        m_close_vault_callback();

        // Show notification that auto-lock occurred
        std::string message = had_unsaved_changes
            ? "Your session has been automatically logged out due to inactivity.\nAny unsaved changes have been saved."
            : "Your session has been automatically logged out due to inactivity.";

        m_dialog_manager->show_info_dialog(message, "Session Timeout");

        // Schedule vault reopen after dialog is shown
        if (!vault_path.empty()) {
            Glib::signal_idle().connect_once([this, vault_path]() {
                m_handle_v2_vault_open_callback(vault_path);
            });
        }
    } else {
        // For V1 vaults, use traditional lock/unlock mechanism
        lock_vault();
    }
    return false;  // Don't repeat
}

void AutoLockHandler::lock_vault() {
    if (!m_vault_open || m_is_locked) {
        return;
    }

    // This should only be called for V1 vaults
    if (m_is_v2_vault_open_callback()) {
        KeepTower::Log::warning("AutoLockHandler: lock_vault() called for V2 vault, use logout instead");
        return;
    }

    // Password should already be cached from when vault was opened
    if (m_cached_master_password.empty()) {
        // Can't lock without being able to unlock
        g_warning("Cannot lock vault - master password not cached! This shouldn't happen.");
        return;
    }

    // Save any unsaved changes
    m_save_account_callback();
    if (!m_vault_manager->save_vault()) {
        g_warning("Failed to save vault before locking");
    }

    // Phase 5: Use UIStateManager for state management
    m_ui_state_manager->set_vault_locked(true);
    m_ui_state_manager->set_status("Vault locked due to inactivity");

    // Cache lock state locally to avoid repeated VaultManager queries
    m_is_locked = true;

    // Clear account details (via callback which calls clear_account_details())
    // This will clear fields and set m_selected_account_index = -1
    m_update_account_list_callback();  // Clear the list

    // Create unlock dialog using Gtk::Window for full control
    auto* dialog = Gtk::make_managed<Gtk::Window>();
    dialog->set_transient_for(m_window);
    dialog->set_modal(true);
    dialog->set_title("Vault Locked - Authentication Required");
    dialog->set_default_size(450, 200);
    dialog->set_resizable(false);

    // Create main layout
    auto* main_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 0);

    // Content area
    auto* content_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 12);
    content_box->set_margin_start(24);
    content_box->set_margin_end(24);
    content_box->set_margin_top(24);
    content_box->set_margin_bottom(24);

    auto* message_label = Gtk::make_managed<Gtk::Label>();
    message_label->set_markup("<b>Your vault has been locked due to inactivity.</b>");
    message_label->set_wrap(true);
    message_label->set_xalign(0.0);
    content_box->append(*message_label);

    auto* instruction_label = Gtk::make_managed<Gtk::Label>("Enter your master password to unlock and continue working.");
    instruction_label->set_wrap(true);
    instruction_label->set_xalign(0.0);
    content_box->append(*instruction_label);

    auto* password_entry = Gtk::make_managed<Gtk::Entry>();
    password_entry->set_visibility(false);
    password_entry->set_placeholder_text("Enter master password to unlock");
    content_box->append(*password_entry);

    main_box->append(*content_box);

    // Button area
    auto* button_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    button_box->set_margin_start(24);
    button_box->set_margin_end(24);
    button_box->set_margin_bottom(24);
    button_box->set_halign(Gtk::Align::END);

    auto* cancel_button = Gtk::make_managed<Gtk::Button>("_Cancel");
    cancel_button->set_use_underline(true);
    button_box->append(*cancel_button);

    auto* ok_button = Gtk::make_managed<Gtk::Button>("_OK");
    ok_button->set_use_underline(true);
    ok_button->add_css_class("suggested-action");
    button_box->append(*ok_button);

    main_box->append(*button_box);
    dialog->set_child(*main_box);

    // Handle OK button
    ok_button->signal_clicked().connect([this, dialog, password_entry]() {
        const std::string entered_password{KeepTower::safe_ustring_to_string(password_entry->get_text(), "unlock_password")};

#ifdef HAVE_YUBIKEY_SUPPORT
        // Check if YubiKey is required for this vault
        std::string yubikey_serial;
        bool yubikey_required = m_vault_manager->check_vault_requires_yubikey(std::string{m_current_vault_path}, yubikey_serial);

        YubiKeyPromptDialog* touch_dialog = nullptr;
        if (yubikey_required) {
            // Show touch prompt dialog
            touch_dialog = Gtk::make_managed<YubiKeyPromptDialog>(*dialog,
                YubiKeyPromptDialog::PromptType::TOUCH);
            touch_dialog->present();

            // Force GTK to process events and render the dialog
            auto context = Glib::MainContext::get_default();
            while (context->pending()) {
                context->iteration(false);
            }
            g_usleep(150000);  // 150ms delay for rendering
        }
#endif

        // Verify password by attempting to open vault
        const auto temp_vault = std::make_unique<VaultManager>();
        const bool success = temp_vault->open_vault(std::string{m_current_vault_path}, entered_password);

#ifdef HAVE_YUBIKEY_SUPPORT
        // Hide touch prompt if it was shown
        if (touch_dialog) {
            touch_dialog->hide();
        }
#endif

        if (success && entered_password == m_cached_master_password) {
            // Phase 5: Use UIStateManager for state management
            m_ui_state_manager->set_vault_locked(false);
            m_ui_state_manager->set_status("Vault unlocked");

            // Cache lock state locally to avoid repeated VaultManager queries
            m_is_locked = false;

            // Restore account list and selection
            m_update_account_list_callback();
            m_filter_accounts_callback(m_get_search_text_callback());

            // Reset activity monitoring
            handle_user_activity();

            delete dialog;
        } else {
            // Unlock failed - could be wrong password or missing YubiKey
            password_entry->set_text("");
            password_entry->grab_focus();

#ifdef HAVE_YUBIKEY_SUPPORT
            // Provide more specific error message if YubiKey is required
            const char* error_message = "Unlock Failed";
            const char* error_detail;
            if (yubikey_required) {
                error_detail = "Unable to unlock vault. This could be due to:\n"
                              "• Incorrect password\n"
                              "• YubiKey not inserted\n"
                              "• YubiKey not touched in time\n"
                              "• Wrong YubiKey inserted\n\n"
                              "Please verify your password and ensure the correct YubiKey is connected.";
            } else {
                error_detail = "The password you entered is incorrect. Please try again.";
            }
#else
            const char* error_message = "Incorrect Password";
            const char* error_detail = "The password you entered is incorrect. Please try again.";
#endif

            auto* error_dialog = Gtk::make_managed<Gtk::MessageDialog>(
                *dialog,
                error_message,
                false,
                Gtk::MessageType::ERROR,
                Gtk::ButtonsType::OK,
                true
            );
            error_dialog->set_secondary_text(error_detail);
            error_dialog->signal_response().connect([error_dialog, password_entry](int) {
                error_dialog->hide();
                password_entry->grab_focus();
            });
            error_dialog->show();
        }
    });

    // Handle Cancel button
    cancel_button->signal_clicked().connect([this, dialog]() {
        // Save and close application
        if (m_vault_open) {
            m_save_account_callback();
            if (!m_vault_manager->save_vault()) {
                g_warning("Failed to save vault before closing locked application");
            }
        }

        delete dialog;
        m_window.close();
    });

    // Handle Enter key in password entry
    password_entry->signal_activate().connect([ok_button]() {
        ok_button->activate();
    });

    // Show the unlock dialog and set focus
    dialog->present();
    password_entry->grab_focus();
}

std::string AutoLockHandler::get_master_password_for_lock() {
    // We need to get the master password to cache it for unlock
    // This is called when locking, so we prompt the user
    auto* dialog = Gtk::make_managed<Gtk::MessageDialog>(
        m_window,
        "Verify Password for Auto-Lock",
        false,
        Gtk::MessageType::QUESTION,
        Gtk::ButtonsType::OK_CANCEL
    );
    dialog->set_secondary_text("Enter your master password to verify your identity.\nThis allows the vault to auto-lock after inactivity and be unlocked with the same password.");
    dialog->set_modal(true);
    dialog->set_hide_on_close(true);

    auto* content = dialog->get_message_area();
    auto* password_entry = Gtk::make_managed<Gtk::Entry>();
    password_entry->set_visibility(false);
    password_entry->set_placeholder_text("Enter master password");
    password_entry->set_margin_start(12);
    password_entry->set_margin_end(12);
    password_entry->set_margin_top(12);
    password_entry->set_activates_default(true);
    content->append(*password_entry);

    dialog->set_default_response(Gtk::ResponseType::OK);

    std::string result;
    dialog->signal_response().connect([&result, password_entry](const int response) {
        if (response == Gtk::ResponseType::OK) {
            result = std::string{password_entry->get_text()};
        }
    });

    dialog->set_hide_on_close(true);
    dialog->show();

    // Wait for dialog to close (use modern GTK4 pattern)
    while (dialog->get_visible()) {
        g_main_context_iteration(nullptr, true);
    }

    return result;
}

}  // namespace UI
