// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "../../core/VaultManager.h"
#include "VaultIOHandler.h"
#include "DialogManager.h"
#include "../../utils/Log.h"
#include "../../utils/ImportExport.h"
#include "../windows/MainWindow.h"
#include "../dialogs/PasswordDialog.h"
#include "../dialogs/VaultMigrationDialog.h"

// Forward declare OPENSSL_cleanse to avoid OpenSSL UI type conflict with our UI namespace
extern "C" {
    void OPENSSL_cleanse(void *ptr, size_t len);
}

#ifdef HAVE_YUBIKEY_SUPPORT
#include "../dialogs/YubiKeyPromptDialog.h"
#include "../../core/managers/YubiKeyManager.h"
#endif

#include <format>

namespace UI {

VaultIOHandler::VaultIOHandler(MainWindow& window,
                               VaultManager* vault_manager,
                               DialogManager* dialog_manager)
    : m_window(window)
    , m_vault_manager(vault_manager)
    , m_dialog_manager(dialog_manager) {
}

void VaultIOHandler::handle_import(const UpdateCallback& on_update) {
    // Create file chooser dialog
    std::vector<std::pair<std::string, std::string>> filters = {
        {"CSV files (*.csv)", "*.csv"},
        {"KeePass XML (*.xml)", "*.xml"},
        {"1Password 1PIF (*.1pif)", "*.1pif"},
        {"All files", "*"}
    };

    m_dialog_manager->show_open_file_dialog(
        "Import Accounts",
        [this, on_update](const std::string& file_path_str) {
            // Detect format from file extension and perform the import
            std::expected<std::vector<keeptower::AccountRecord>, ImportExport::ImportError> result;
            std::string format_name;

            if (file_path_str.ends_with(".xml")) {
                result = ImportExport::import_from_keepass_xml(file_path_str);
                format_name = "KeePass XML";
            } else if (file_path_str.ends_with(".1pif")) {
                result = ImportExport::import_from_1password(file_path_str);
                format_name = "1Password 1PIF";
            } else {
                // Default to CSV
                result = ImportExport::import_from_csv(file_path_str);
                format_name = "CSV";
            }

            if (result.has_value()) {
                auto& accounts = result.value();

                // Add each account to the vault, tracking failures
                int imported_count = 0;
                int failed_count = 0;
                std::vector<std::string> failed_accounts;

                for (const auto& account : accounts) {
                    if (m_vault_manager->add_account(account)) {
                        imported_count++;
                    } else {
                        failed_count++;
                        // Limit failure list to avoid huge dialogs
                        if (failed_accounts.size() < 10) {
                            failed_accounts.push_back(account.account_name());
                        }
                    }
                }

                // Update UI
                if (on_update) {
                    on_update();
                }

                // Show result message (success or partial success)
                std::string message;
                Gtk::MessageType msg_type;

                if (failed_count == 0) {
                    message = std::format("Successfully imported {} account(s) from {} format.", imported_count, format_name);
                    msg_type = Gtk::MessageType::INFO;
                } else if (imported_count > 0) {
                    message = std::format("Imported {} account(s) successfully.\n"
                                         "{} account(s) failed to import.",
                                         imported_count, failed_count);
                    if (!failed_accounts.empty()) {
                        message += "\n\nFailed accounts:\n";
                        for (size_t i = 0; i < failed_accounts.size(); i++) {
                            message += "• " + failed_accounts[i] + "\n";
                        }
                        if (static_cast<size_t>(failed_count) > failed_accounts.size()) {
                            message += std::format("... and {} more", failed_count - static_cast<int>(failed_accounts.size()));
                        }
                    }
                    msg_type = Gtk::MessageType::WARNING;
                } else {
                    message = "Failed to import all accounts.";
                    msg_type = Gtk::MessageType::ERROR;
                }

                // Show result dialog based on import outcome
                if (msg_type == Gtk::MessageType::INFO) {
                    m_dialog_manager->show_info_dialog(message, "Import Successful");
                } else if (msg_type == Gtk::MessageType::WARNING) {
                    m_dialog_manager->show_warning_dialog(message, "Import Completed with Issues");
                } else {
                    m_dialog_manager->show_error_dialog(message, "Import Failed");
                }
            } else {
                // Show error message
                const char* error_msg = nullptr;
                switch (result.error()) {
                    case ImportExport::ImportError::FILE_NOT_FOUND:
                        error_msg = "File not found";
                        break;
                    case ImportExport::ImportError::INVALID_FORMAT:
                        error_msg = "Invalid CSV format";
                        break;
                    case ImportExport::ImportError::PARSE_ERROR:
                        error_msg = "Failed to parse CSV file";
                        break;
                    case ImportExport::ImportError::UNSUPPORTED_VERSION:
                        error_msg = "Unsupported file version";
                        break;
                    case ImportExport::ImportError::EMPTY_FILE:
                        error_msg = "Empty file";
                        break;
                    case ImportExport::ImportError::ENCRYPTION_ERROR:
                        error_msg = "Encryption error";
                        break;
                }
                m_dialog_manager->show_error_dialog(std::format("Import failed: {}", error_msg));
            }
        },
        filters
    );
}

void VaultIOHandler::handle_export(const std::string& current_vault_path, bool vault_open) {
    if (!vault_open) {
        m_dialog_manager->show_error_dialog("Please open a vault first before exporting accounts.");
        return;
    }

    // Security warning dialog (Step 1)
    auto warning_dialog = Gtk::make_managed<Gtk::MessageDialog>(
        m_window,
        "Export Accounts to Plaintext?",
        false,
        Gtk::MessageType::WARNING,
        Gtk::ButtonsType::NONE,
        true
    );
    warning_dialog->set_modal(true);
    warning_dialog->set_hide_on_close(true);
    warning_dialog->set_secondary_text(
        "Warning: ALL export formats save passwords in UNENCRYPTED PLAINTEXT.\n\n"
        "Supported formats: CSV, KeePass XML, 1Password 1PIF\n\n"
        "The exported file will NOT be encrypted. Anyone with access to the file\n"
        "will be able to read all your passwords.\n\n"
        "To proceed, you must re-authenticate with your master password."
    );

    warning_dialog->add_button("_Cancel", Gtk::ResponseType::CANCEL);
    auto export_button = warning_dialog->add_button("_Continue", Gtk::ResponseType::OK);
    export_button->add_css_class("destructive-action");

    warning_dialog->signal_response().connect([this, warning_dialog, current_vault_path](int response) {
        try {
            warning_dialog->hide();

            if (response != Gtk::ResponseType::OK) {
                return;
            }

            // Schedule password dialog via idle callback (flat chain)
            Glib::signal_idle().connect_once([this, current_vault_path]() {
                show_export_password_dialog(current_vault_path);
            });
        } catch (const std::exception& e) {
            KeepTower::Log::error("Exception in export warning handler: {}", e.what());
            m_dialog_manager->show_error_dialog(std::format("Export failed: {}", e.what()));
        } catch (...) {
            KeepTower::Log::error("Unknown exception in export warning handler");
            m_dialog_manager->show_error_dialog("Export failed due to unknown error");
        }
    });

    warning_dialog->show();
}

void VaultIOHandler::show_export_password_dialog(const std::string& current_vault_path) {
    try {
        // Step 2: Show password dialog (warning dialog is now fully closed)
        auto* password_dialog = Gtk::make_managed<PasswordDialog>(m_window);

        // Get current username for V2 vaults and update title
        std::string current_username;
        bool is_v2_vault = false;
        if (m_vault_manager) {
            is_v2_vault = m_vault_manager->is_v2_vault();
            if (is_v2_vault) {
                current_username = m_vault_manager->get_current_username();
                if (!current_username.empty()) {
                    password_dialog->set_title(std::format("Authenticate to Export (User: {})", current_username));
                } else {
                    password_dialog->set_title("Authenticate to Export");
                }
            } else {
                password_dialog->set_title("Authenticate to Export");
            }
        } else {
            password_dialog->set_title("Authenticate to Export");
        }

        password_dialog->set_modal(true);
        password_dialog->set_hide_on_close(true);

        password_dialog->signal_response().connect([this, password_dialog, current_vault_path, current_username, is_v2_vault](int response) {
            if (response != Gtk::ResponseType::OK) {
                password_dialog->hide();
                return;
            }

            try {
                Glib::ustring password = password_dialog->get_password();

#ifdef HAVE_YUBIKEY_SUPPORT
                // Check if YubiKey is required for current user
                bool yubikey_required = false;
                if (m_vault_manager) {
                    yubikey_required = m_vault_manager->current_user_requires_yubikey();
                }

                // If current user requires YubiKey, show touch prompt and do authentication synchronously
                YubiKeyPromptDialog* touch_dialog = nullptr;
                if (yubikey_required) {
                    // Get YubiKey serial
                    YubiKeyManager yk_manager;
                    if (!yk_manager.initialize() || !yk_manager.is_yubikey_present()) {
                        password_dialog->hide();
                        m_dialog_manager->show_error_dialog("YubiKey not detected.");
                        return;
                    }

                    auto device_info = yk_manager.get_device_info();
                    if (!device_info) {
                        password_dialog->hide();
                        m_dialog_manager->show_error_dialog("Failed to get YubiKey information.");
                        return;
                    }

                    std::string serial_number = device_info->serial_number;

                    // Hide password dialog to show touch prompt
                    password_dialog->hide();

                    // Show touch prompt dialog
                    touch_dialog = Gtk::make_managed<YubiKeyPromptDialog>(m_window,
                        YubiKeyPromptDialog::PromptType::TOUCH);
                    touch_dialog->present();

                    // Force GTK to process events and render the dialog
                    auto context = Glib::MainContext::get_default();
                    while (context->pending()) {
                        context->iteration(false);
                    }

                    // Small delay to ensure dialog is fully rendered
                    g_usleep(150000);  // 150ms

                    // Perform authentication with YubiKey (blocking call) - SYNCHRONOUSLY
                    bool auth_success = m_vault_manager->verify_credentials(password, serial_number);

                    // Hide touch prompt
                    if (touch_dialog) {
                        touch_dialog->hide();
                    }

                    if (!auth_success) {
                        // Securely clear password before returning (FIPS-compliant)
                        if (!password.empty()) {
                            OPENSSL_cleanse(const_cast<char*>(password.data()), password.bytes());
                            password.clear();
                        }
                        m_dialog_manager->show_error_dialog("YubiKey authentication failed. Export cancelled.");
                        return;
                    }
                } else
#endif
                {
                    // No YubiKey - just verify password
                    password_dialog->hide();

                    bool auth_success = m_vault_manager->verify_credentials(password);

                    if (!auth_success) {
                        // Securely clear password before returning (FIPS-compliant)
                        if (!password.empty()) {
                            OPENSSL_cleanse(const_cast<char*>(password.data()), password.bytes());
                            password.clear();
                        }
                        m_dialog_manager->show_error_dialog("Authentication failed. Export cancelled.");
                        return;
                    }
                }

                // Securely clear password after successful authentication (FIPS-compliant)
                if (!password.empty()) {
                    OPENSSL_cleanse(const_cast<char*>(password.data()), password.bytes());
                    password.clear();
                }

                // Authentication successful - show file chooser
                show_export_file_dialog(current_vault_path);

            } catch (const std::exception& e) {
                KeepTower::Log::error("Exception in password dialog handler: {}", e.what());
                m_dialog_manager->show_error_dialog(std::format("Authentication failed: {}", e.what()));
                password_dialog->hide();
            } catch (...) {
                KeepTower::Log::error("Unknown exception in password dialog handler");
                m_dialog_manager->show_error_dialog("Authentication failed due to unknown error");
                password_dialog->hide();
            }
        });

        password_dialog->show();
    } catch (const std::exception& e) {
        KeepTower::Log::error("Exception showing password dialog: {}", e.what());
        m_dialog_manager->show_error_dialog(std::format("Failed to show authentication dialog: {}", e.what()));
    }
}

void VaultIOHandler::show_export_file_dialog([[maybe_unused]] const std::string& current_vault_path) {
    try {
        // Validate state before proceeding
        if (!m_vault_manager) {
            m_dialog_manager->show_error_dialog("Export cancelled: vault is not open");
            return;
        }

        // Ensure we're in main GTK thread and event loop is ready
        auto context = Glib::MainContext::get_default();
        if (!context) {
            KeepTower::Log::error("No GTK main context available");
            m_dialog_manager->show_error_dialog("Internal error: GTK context unavailable");
            return;
        }

        // Process any pending events before showing new dialog
        while (context->pending()) {
            context->iteration(false);
        }

        // Show file chooser for export location (all previous dialogs are now closed)
        KeepTower::Log::info("Creating file chooser dialog");
        std::vector<std::pair<std::string, std::string>> filters = {
            {"CSV files (*.csv)", "*.csv"},
            {"KeePass XML (*.xml) - Not fully tested", "*.xml"},
            {"1Password 1PIF (*.1pif) - Not fully tested", "*.1pif"},
            {"All files", "*"}
        };

        m_dialog_manager->show_save_file_dialog(
            "Export Accounts",
            "passwords_export.csv",
            [this](const std::string& file_path_str) {
                // Validate that we received a valid file path
                if (file_path_str.empty()) {
                    KeepTower::Log::error("Export failed: No file path provided");
                    m_dialog_manager->show_error_dialog("Export failed: No file was selected");
                    return;
                }

                KeepTower::Log::info("File chosen for export: {}", file_path_str);
                std::string path = file_path_str;

                // Get all accounts from vault (optimized with reserve)
                std::vector<keeptower::AccountRecord> accounts;
                int account_count = m_vault_manager->get_account_count();
                accounts.reserve(account_count);  // Pre-allocate

                for (int i = 0; i < account_count; i++) {
                    const auto* account = m_vault_manager->get_account(i);
                    if (account) {
                        accounts.emplace_back(*account);  // Use emplace_back
                    }
                }

                // Detect format from file extension
                std::expected<void, ImportExport::ExportError> result;
                std::string format_name;
                std::string warning_text = "Warning: This file contains UNENCRYPTED passwords!";

                if (path.ends_with(".xml")) {
                    result = ImportExport::export_to_keepass_xml(path, accounts);
                    format_name = "KeePass XML";
                    warning_text += "\n\nNOTE: KeePass import compatibility not fully tested.";
                } else if (path.ends_with(".1pif")) {
                    result = ImportExport::export_to_1password_1pif(path, accounts);
                    format_name = "1Password 1PIF";
                    warning_text += "\n\nNOTE: 1Password import compatibility not fully tested.";
                } else {
                    // Default to CSV (or if .csv extension)
                    result = ImportExport::export_to_csv(path, accounts);
                    format_name = "CSV";
                }

                if (result.has_value()) {
                    // Show success message
                    m_dialog_manager->show_info_dialog(
                        std::format("Successfully exported {} account(s) to {} format:\n{}\n\n{}",
                                   accounts.size(), format_name, path, warning_text),
                        "Export Successful"
                    );
                } else {
                    // Show error message
                    const char* error_msg = nullptr;
                    switch (result.error()) {
                        case ImportExport::ExportError::FILE_WRITE_ERROR:
                            error_msg = "Failed to write file";
                            break;
                        case ImportExport::ExportError::PERMISSION_DENIED:
                            error_msg = "Permission denied";
                            break;
                        case ImportExport::ExportError::INVALID_DATA:
                            error_msg = "Invalid data";
                            break;
                    }
                    m_dialog_manager->show_error_dialog(std::format("Export failed: {}", error_msg));
                }
            },
            filters
        );
    } catch (const std::exception& e) {
        KeepTower::Log::error("Exception showing file chooser: {}", e.what());
        m_dialog_manager->show_error_dialog(std::format("Failed to show file chooser: {}", e.what()));
    }
}

void VaultIOHandler::handle_migration(const std::string& current_vault_path,
                                      bool vault_open,
                                      const UpdateCallback& on_success) {
    // Validation: Must have V1 vault open
    if (!vault_open) {
        m_dialog_manager->show_error_dialog("No vault is currently open.\nPlease open a vault first.");
        return;
    }

    // Check if already V2 (V2 vaults have multi-user support)
    auto session = m_vault_manager->get_current_user_session();
    if (session.has_value()) {
        m_dialog_manager->show_error_dialog("This vault is already in V2 multi-user format.\nNo migration needed.");
        return;
    }

    // Show migration dialog
    auto* migration_dialog = Gtk::make_managed<VaultMigrationDialog>(m_window, current_vault_path);

    migration_dialog->signal_response().connect([this, migration_dialog, on_success, current_vault_path](int response) {
        if (response == Gtk::ResponseType::OK) {
            // Get migration parameters
            auto admin_username = migration_dialog->get_admin_username();
            auto admin_password = migration_dialog->get_admin_password();
            auto min_length = migration_dialog->get_min_password_length();
            auto iterations = migration_dialog->get_pbkdf2_iterations();

            // Load vault user password history default setting
            auto settings = Gio::Settings::create("com.tjdeveng.keeptower");
            int vault_password_history_depth = settings->get_int("vault-user-password-history-depth");
            vault_password_history_depth = std::clamp(vault_password_history_depth, 0, 24);

            // Create security policy
            KeepTower::VaultSecurityPolicy policy;
            policy.min_password_length = min_length;
            policy.pbkdf2_iterations = iterations;
            policy.password_history_depth = vault_password_history_depth;
            policy.require_yubikey = false;

            // Perform migration
            auto result = m_vault_manager->convert_v1_to_v2(admin_username, admin_password, policy);

            if (result) {
                // Success - call success callback
                if (on_success) {
                    on_success();
                }

                // Show success dialog
                m_dialog_manager->show_info_dialog(
                    "Your vault has been successfully upgraded to V2 multi-user format.\n\n"
                    "• Administrator account: " + admin_username.raw() + "\n"
                    "• Backup created: " + current_vault_path + ".v1.backup\n"
                    "• You can now add additional users via Tools → Manage Users",
                    "Migration Successful"
                );

            } else {
                // Migration failed - show error
                std::string error_message = "Failed to migrate vault to V2 format.\n\n";

                switch (result.error()) {
                    case KeepTower::VaultError::VaultNotOpen:
                        error_message += "Vault is not open.";
                        break;
                    case KeepTower::VaultError::InvalidUsername:
                        error_message += "Invalid username format.";
                        break;
                    case KeepTower::VaultError::InvalidPassword:
                        error_message += "Invalid password format.";
                        break;
                    case KeepTower::VaultError::WeakPassword:
                        error_message += "Password does not meet minimum length requirement.";
                        break;
                    case KeepTower::VaultError::FileWriteError:
                        error_message += "Failed to create backup file.";
                        break;
                    case KeepTower::VaultError::CryptoError:
                        error_message += "Cryptographic operation failed.";
                        break;
                    default:
                        error_message += "Unknown error occurred.";
                        break;
                }

                m_dialog_manager->show_error_dialog(error_message);
            }
        }

        migration_dialog->hide();
    });

    migration_dialog->show();
}

} // namespace UI
