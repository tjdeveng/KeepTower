// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2024 Travis E. Hansen

#include "VaultOpenHandler.h"
#include "DialogManager.h"
#include "UIStateManager.h"
#include "../../core/VaultManager.h"
#include "../../utils/SettingsValidator.h"
#include "../../utils/StringHelpers.h"
#include "../../utils/Log.h"
#include "../dialogs/CreatePasswordDialog.h"
#include "../dialogs/PasswordDialog.h"

#ifdef HAVE_YUBIKEY_SUPPORT
#include "../dialogs/YubiKeyPromptDialog.h"
#include "../../core/managers/YubiKeyManager.h"
#endif

#include <thread>
#include <atomic>

namespace UI {

VaultOpenHandler::VaultOpenHandler(Gtk::Window& window,
                                  VaultManager* vault_manager,
                                  DialogManager* dialog_manager,
                                  UIStateManager* ui_state_manager,
                                  bool& vault_open_ref,
                                  bool& is_locked_ref,
                                  Glib::ustring& current_vault_path_ref,
                                  std::string& cached_master_password_ref,
                                  ErrorDialogCallback error_dialog_callback,
                                  InfoDialogCallback info_dialog_callback,
                                  DetectVaultVersionCallback detect_vault_version_callback,
                                  HandleV2VaultOpenCallback handle_v2_vault_open_callback,
                                  InitializeRepositoriesCallback initialize_repositories_callback,
                                  UpdateAccountListCallback update_account_list_callback,
                                  UpdateTagFilterCallback update_tag_filter_callback,
                                  ClearAccountDetailsCallback clear_account_details_callback,
                                  UpdateUndoRedoSensitivityCallback update_undo_redo_sensitivity_callback,
                                  UpdateMenuForRoleCallback update_menu_for_role_callback,
                                  UpdateSessionDisplayCallback update_session_display_callback,
                                  OnUserActivityCallback on_user_activity_callback)
    : m_window(window)
    , m_vault_manager(vault_manager)
    , m_dialog_manager(dialog_manager)
    , m_ui_state_manager(ui_state_manager)
    , m_vault_open(vault_open_ref)
    , m_is_locked(is_locked_ref)
    , m_current_vault_path(current_vault_path_ref)
    , m_cached_master_password(cached_master_password_ref)
    , m_error_dialog_callback(std::move(error_dialog_callback))
    , m_info_dialog_callback(std::move(info_dialog_callback))
    , m_detect_vault_version_callback(std::move(detect_vault_version_callback))
    , m_handle_v2_vault_open_callback(std::move(handle_v2_vault_open_callback))
    , m_initialize_repositories_callback(std::move(initialize_repositories_callback))
    , m_update_account_list_callback(std::move(update_account_list_callback))
    , m_update_tag_filter_callback(std::move(update_tag_filter_callback))
    , m_clear_account_details_callback(std::move(clear_account_details_callback))
    , m_update_undo_redo_sensitivity_callback(std::move(update_undo_redo_sensitivity_callback))
    , m_update_menu_for_role_callback(std::move(update_menu_for_role_callback))
    , m_update_session_display_callback(std::move(update_session_display_callback))
    , m_on_user_activity_callback(std::move(on_user_activity_callback))
{
}

void VaultOpenHandler::handle_new_vault() {
    // Show file save dialog to choose location for new vault
    std::vector<std::pair<std::string, std::string>> filters = {
        {"Vault files", "*.vault"},
        {"All files", "*"}
    };

    m_dialog_manager->show_save_file_dialog(
        "Create New Vault",
        "Untitled.vault",
        [this](const std::string& vault_path) {
            // User cancelled file dialog - abort vault creation
            if (vault_path.empty()) {
                return;
            }

            // Show combined username + password creation dialog
            auto pwd_dialog = Gtk::make_managed<CreatePasswordDialog>(m_window);
            Glib::ustring vault_path_glib = Glib::ustring(vault_path);

            pwd_dialog->signal_response().connect([this, pwd_dialog, vault_path](int pwd_response) {
                if (pwd_response == Gtk::ResponseType::OK) {
                    Glib::ustring admin_username = pwd_dialog->get_username();
                    Glib::ustring password = pwd_dialog->get_password();
                    bool require_yubikey = pwd_dialog->get_yubikey_enabled();

                    // Load default FEC preferences for new vault
                    auto settings = Gio::Settings::create("com.tjdeveng.keeptower");
                    bool use_rs = settings->get_boolean("use-reed-solomon");
                    int rs_redundancy = settings->get_int("rs-redundancy-percent");
                    m_vault_manager->apply_default_fec_preferences(use_rs, rs_redundancy);

                    // Load vault user password history default setting
                    int vault_password_history_depth = settings->get_int("vault-user-password-history-depth");
                    vault_password_history_depth = std::clamp(vault_password_history_depth, 0, 24);

                    // Load username hashing algorithm from settings with FIPS enforcement
                    // If FIPS mode is enabled, SettingsValidator will block non-FIPS algorithms
                    auto username_hash_algorithm_enum = SettingsValidator::get_username_hash_algorithm(settings);

                    // Convert enum to uint8_t for VaultSecurityPolicy
                    uint8_t username_hash_algorithm = static_cast<uint8_t>(username_hash_algorithm_enum);

                    KeepTower::VaultSecurityPolicy policy;
                    policy.min_password_length = 8;  // NIST minimum
                    policy.pbkdf2_iterations = 100000;  // Default iterations
                    policy.password_history_depth = vault_password_history_depth;
                    policy.require_yubikey = require_yubikey;
                    policy.username_hash_algorithm = username_hash_algorithm;

                    // Create V2 vault with admin account
                    std::optional<std::string> yubikey_pin;
                    if (require_yubikey) {
                        yubikey_pin = pwd_dialog->get_yubikey_pin();
                    }

                    // Define result handler lambda (used for both sync and async paths)
                    auto handle_result = [this, vault_path, admin_username, pwd_dialog](auto result) {
                        if (result) {
                            // Apply default preferences from GSettings to new vault
                            auto settings = Gio::Settings::create("com.tjdeveng.keeptower");

                            // Apply auto-lock settings
                            bool auto_lock_enabled = SettingsValidator::is_auto_lock_enabled(settings);
                            int auto_lock_timeout = SettingsValidator::get_auto_lock_timeout(settings);
                            m_vault_manager->set_auto_lock_enabled(auto_lock_enabled);
                            m_vault_manager->set_auto_lock_timeout(auto_lock_timeout);

                            // Apply clipboard timeout
                            int clipboard_timeout = SettingsValidator::get_clipboard_timeout(settings);
                            m_vault_manager->set_clipboard_timeout(clipboard_timeout);

                            // Apply undo/redo settings
                            bool undo_redo_enabled = settings->get_boolean("undo-redo-enabled");
                            int undo_history_limit = settings->get_int("undo-history-limit");
                            m_vault_manager->set_undo_redo_enabled(undo_redo_enabled);
                            m_vault_manager->set_undo_history_limit(undo_history_limit);

                            // Apply account password history settings
                            bool account_pwd_history_enabled = settings->get_boolean("password-history-enabled");
                            int account_pwd_history_limit = settings->get_int("password-history-limit");
                            m_vault_manager->set_account_password_history_enabled(account_pwd_history_enabled);
                            m_vault_manager->set_account_password_history_limit(account_pwd_history_limit);

                            // Apply FEC (Reed-Solomon) settings to vault metadata
                            bool fec_enabled = settings->get_boolean("use-reed-solomon");
                            int fec_redundancy = settings->get_int("rs-redundancy-percent");
                            m_vault_manager->set_reed_solomon_enabled(fec_enabled);
                            m_vault_manager->set_rs_redundancy_percent(fec_redundancy);

                            // Apply backup settings to vault manager
                            bool backup_enabled = settings->get_boolean("backup-enabled");
                            int backup_count = settings->get_int("backup-count");
                            m_vault_manager->set_backup_enabled(backup_enabled);
                            m_vault_manager->set_backup_count(backup_count);

                            // Save vault to persist all default preferences
                            if (!m_vault_manager->save_vault()) {
                                KeepTower::Log::error("Failed to save vault with default preferences");
                            }

                            // Phase 5: Use UIStateManager for vault opened state
                            m_ui_state_manager->set_vault_opened(vault_path, admin_username);

                            // Update MainWindow state references to mark vault as open
                            m_vault_open = true;
                            m_is_locked = false;
                            m_current_vault_path = vault_path;

                            // Phase 2: Initialize repositories for data access
                            m_initialize_repositories_callback();

                            m_update_account_list_callback();
                            m_update_tag_filter_callback();
                            m_clear_account_details_callback();

                            // Initialize undo/redo state
                            m_update_undo_redo_sensitivity_callback(false, false);

                            // Update menu for V2 vault (enable user management, etc.)
                            m_update_menu_for_role_callback();
                            m_update_session_display_callback();

                            // Start activity monitoring for auto-lock
                            m_on_user_activity_callback();

                            // Show success dialog with username reminder
                            m_info_dialog_callback(
                                "Your vault has been created successfully.\n\n"
                                "Username: " + std::string{admin_username} + "\n\n"
                                "Remember this username - you will need it to reopen the vault. "
                                "You can add additional users through the User Management dialog (Tools → Manage Users).",
                                "Vault Created Successfully"
                            );
                        } else {
                            m_error_dialog_callback("Failed to create vault");
                        }
                        pwd_dialog->hide();
                    };

#ifdef HAVE_YUBIKEY_SUPPORT
                    // Show touch prompt if YubiKey is required
                    if (require_yubikey) {
                        pwd_dialog->hide();
                        auto touch_dialog = Gtk::make_managed<YubiKeyPromptDialog>(m_window,
                            YubiKeyPromptDialog::PromptType::TOUCH,
                            "",  // No serial number
                            "<big><b>Creating Vault with YubiKey</b></big>\n\n"
                            "Please touch the button on your YubiKey when prompted.\n\n"
                            "<i>Note: Two touches will be required.</i>"
                        );

                        // Define progress callback to update touch dialog message
                        auto progress_callback = [touch_dialog](int step, int total, const std::string& desc) {
                            // Update dialog message for YubiKey touch operations
                            if (desc.find("Touch") != std::string::npos) {
                                touch_dialog->update_message(desc);
                            }
                        };

                        // Define completion callback
                        auto completion_callback = [touch_dialog, handle_result](KeepTower::VaultResult<> result) {
                            touch_dialog->hide();
                            handle_result(result);
                        };

                        // Wait for dialog to be fully mapped (shown on screen) before starting work
                        touch_dialog->signal_map().connect([this, touch_dialog, vault_path, admin_username,
                                                           password, policy, yubikey_pin, progress_callback,
                                                           completion_callback]() {
                            KeepTower::Log::info("VaultOpenHandler: Dialog mapped, starting async vault creation");

                            // Use async method with progress callbacks
                            m_vault_manager->create_vault_v2_async(
                                KeepTower::safe_ustring_to_string(Glib::ustring(vault_path), "vault_path"),
                                admin_username,
                                password,
                                policy,
                                progress_callback,
                                completion_callback,
                                yubikey_pin
                            );
                        });

                        touch_dialog->present();
                        return;  // Exit early, result handled in callback
                    }
#endif

                    // Non-YubiKey path: create synchronously
                    auto result = m_vault_manager->create_vault_v2(
                        KeepTower::safe_ustring_to_string(Glib::ustring(vault_path), "vault_path"),
                        admin_username,
                        password,
                        policy,
                        yubikey_pin
                    );

                    handle_result(result);
                }
                pwd_dialog->hide();
            });
            pwd_dialog->show();
        },
        filters
    );
}

void VaultOpenHandler::handle_open_vault() {
    std::vector<std::pair<std::string, std::string>> filters = {
        {"Vault files", "*.vault"},
        {"All files", "*"}
    };

    m_dialog_manager->show_open_file_dialog(
        "Open Vault",
        [this](const std::string& vault_path) {
            std::string vault_path_str = KeepTower::safe_ustring_to_string(Glib::ustring(vault_path), "vault_path");

            // STEP 1: Detect vault version
            auto version_opt = m_detect_vault_version_callback(vault_path_str);
            if (!version_opt) {
                m_error_dialog_callback("Unable to read vault file or invalid format");
                return;
            }

            uint32_t version = *version_opt;

            // STEP 2: Route to appropriate authentication method
            if (version == 2) {
                // V2 multi-user vault - use new authentication flow
                m_handle_v2_vault_open_callback(vault_path_str);
                return;
            }

            // STEP 3: V1 vault - use legacy password dialog authentication
            // (Original code path below)

#ifdef HAVE_YUBIKEY_SUPPORT
            // Check if vault requires YubiKey
            std::string yubikey_serial;
            bool yubikey_required = m_vault_manager->check_vault_requires_yubikey(KeepTower::safe_ustring_to_string(Glib::ustring(vault_path), "vault_path"), yubikey_serial);

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
                            // User clicked Retry - try opening again
                            yk_dialog->hide();
                            handle_open_vault();  // Restart the process
                            return;
                        }
                        yk_dialog->hide();
                    });

                    yk_dialog->show();
                    return;
                }
            }
#endif

            // Show password dialog to decrypt vault
            auto pwd_dialog = Gtk::make_managed<PasswordDialog>(m_window);
            pwd_dialog->signal_response().connect([this, pwd_dialog, vault_path](int pwd_response) {
                if (pwd_response == Gtk::ResponseType::OK) {
                    Glib::ustring password = pwd_dialog->get_password();

#ifdef HAVE_YUBIKEY_SUPPORT
                    // Check again if YubiKey is required (for touch prompt)
                    std::string yubikey_serial;
                    bool yubikey_required = m_vault_manager->check_vault_requires_yubikey(KeepTower::safe_ustring_to_string(Glib::ustring(vault_path), "vault_path"), yubikey_serial);

                    YubiKeyPromptDialog* touch_dialog = nullptr;
                    if (yubikey_required) {
                        // Hide password dialog to show touch prompt
                        pwd_dialog->hide();

                        // Show touch prompt dialog
                        touch_dialog = Gtk::make_managed<YubiKeyPromptDialog>(m_window,
                            YubiKeyPromptDialog::PromptType::TOUCH);
                        touch_dialog->present();

                        // Force GTK to process events and render the dialog
                        auto context = Glib::MainContext::get_default();
                        while (context->pending()) {
                            context->iteration(false);
                        }

                        // Additional small delay to ensure dialog is fully rendered
                        g_usleep(150000);  // 150ms
                    }
#endif

                    auto result = m_vault_manager->open_vault(KeepTower::safe_ustring_to_string(Glib::ustring(vault_path), "vault_path"), password);

#ifdef HAVE_YUBIKEY_SUPPORT
                    // Hide touch prompt if it was shown
                    if (touch_dialog) {
                        touch_dialog->hide();
                    }
#endif

                    if (result) {
                        // Phase 5: Use UIStateManager for vault opened state
                        m_ui_state_manager->set_vault_opened(vault_path);

                    // Maintain local state cache for quick access without VaultManager queries
                        // Phase 2: Initialize repositories for data access
                        m_initialize_repositories_callback();

                        // Cache password for auto-lock/unlock
                        m_cached_master_password = KeepTower::safe_ustring_to_string(password, "master_password");

                        m_update_account_list_callback();
                        m_update_tag_filter_callback();

                        // Initialize undo/redo state
                        m_update_undo_redo_sensitivity_callback(false, false);

                        // Update menu for V2 vault if applicable
                        m_update_menu_for_role_callback();
                        m_update_session_display_callback();

                        // Start activity monitoring for auto-lock
                        m_on_user_activity_callback();
                    } else {
                        constexpr std::string_view error_msg{"Failed to open vault"};
                        auto error_dialog = Gtk::make_managed<Gtk::MessageDialog>(m_window, std::string{error_msg},
                            false, Gtk::MessageType::ERROR, Gtk::ButtonsType::OK, true);
                        error_dialog->signal_response().connect([=](int) {
                            error_dialog->hide();
                        });
                        error_dialog->show();
                    }
                }
                pwd_dialog->hide();
            });
            pwd_dialog->show();
        },
        filters
    );
}

}  // namespace UI
