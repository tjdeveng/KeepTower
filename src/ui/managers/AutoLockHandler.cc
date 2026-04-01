// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2024 Travis E. Hansen

#include "AutoLockHandler.h"
#include "DialogManager.h"
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
                                 bool& vault_open_ref,
                                 bool& is_locked_ref,
                                 Glib::ustring& current_vault_path_ref,
                                 std::string& cached_master_password_ref,
                                 ApplyLockUiCallback apply_lock_ui_callback,
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
    , m_vault_open(vault_open_ref)
    , m_is_locked(is_locked_ref)
    , m_current_vault_path(current_vault_path_ref)
    , m_cached_master_password(cached_master_password_ref)
    , m_apply_lock_ui_callback(std::move(apply_lock_ui_callback))
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

    // V2-only behavior: lock == force logout and require re-authentication.
    KeepTower::Log::info("AutoLockHandler: lock_vault() triggered, forcing logout");

    bool had_unsaved_changes = false;
    if (m_is_vault_modified_callback()) {
        had_unsaved_changes = true;
        m_save_account_callback();
        if (!m_vault_manager->save_vault()) {
            KeepTower::Log::warning("Failed to save vault before lock/logout");
        }
    }

    std::string vault_path{m_current_vault_path};
    m_close_vault_callback();

    const std::string message = had_unsaved_changes
        ? "Your session has been locked. Any unsaved changes have been saved.\nPlease sign in again to continue."
        : "Your session has been locked. Please sign in again to continue.";
    m_dialog_manager->show_info_dialog(message, "Vault Locked");

    if (!vault_path.empty()) {
        Glib::signal_idle().connect_once([this, vault_path]() {
            m_handle_v2_vault_open_callback(vault_path);
        });
    }
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
