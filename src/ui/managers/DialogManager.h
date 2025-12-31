// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file DialogManager.h
 * @brief Centralized dialog management for MainWindow
 *
 * Part of Phase 5 refactoring: MainWindow size reduction
 * Extracts dialog creation and management logic from MainWindow
 */

#pragma once

#include <gtkmm.h>
#include <glibmm/ustring.h>
#include <string>
#include <functional>
#include <memory>

// Forward declarations
class VaultManager;
class PreferencesDialog;
class CreatePasswordDialog;
class PasswordDialog;
class YubiKeyPromptDialog;
class VaultMigrationDialog;

namespace UI {

/**
 * @brief Manages dialog creation and presentation for MainWindow
 *
 * Centralizes all dialog-related logic to reduce MainWindow complexity.
 * Provides consistent dialog patterns and error handling.
 *
 * Design Goals:
 * - Reduce MainWindow size by ~300-400 lines
 * - Centralize dialog patterns
 * - Improve testability of dialog logic
 * - Maintain consistent UX
 *
 * @note This class does not own the parent window, just references it
 */
class DialogManager {
public:
    /**
     * @brief File filter for file chooser dialogs
     */
    struct FileFilter {
        std::string name;        ///< Display name of the filter
        std::vector<std::string> patterns; ///< File patterns (e.g., "*.vault")
    };

    /**
     * @brief Construct dialog manager for a parent window
     * @param parent Reference to parent Gtk::Window (usually MainWindow)
     * @param vault_manager Pointer to VaultManager for vault operations
     */
    explicit DialogManager(Gtk::Window& parent, VaultManager* vault_manager);

    ~DialogManager() = default;

    // Delete copy and move to ensure parent reference validity
    DialogManager(const DialogManager&) = delete;
    DialogManager& operator=(const DialogManager&) = delete;
    DialogManager(DialogManager&&) = delete;
    DialogManager& operator=(DialogManager&&) = delete;

    /**
     * @brief Show error message dialog
     * @param message Error message to display
     * @param title Optional dialog title (default: "Error")
     */
    void show_error_dialog(const std::string& message,
                          const std::string& title = "Error");

    /**
     * @brief Show info message dialog
     * @param message Info message to display
     * @param title Optional dialog title (default: "Information")
     */
    void show_info_dialog(const std::string& message,
                         const std::string& title = "Information");

    /**
     * @brief Show warning message dialog
     * @param message Warning message to display
     * @param title Optional dialog title (default: "Warning")
     */
    void show_warning_dialog(const std::string& message,
                            const std::string& title = "Warning");

    /**
     * @brief Show confirmation dialog with Yes/No buttons
     * @param message Question to ask user
     * @param title Optional dialog title (default: "Confirm")
     * @param callback Function to call with result (true = yes, false = no)
     */
    void show_confirmation_dialog(const std::string& message,
                                 const std::string& title,
                                 const std::function<void(bool)>& callback);

    /**
     * @brief Show file chooser dialog for opening
     * @param title Dialog title
     * @param callback Function to call with selected file path (empty if cancelled)
     * @param filters Optional file filters
     */
    void show_open_file_dialog(const std::string& title,
                              const std::function<void(std::string)>& callback,
                              const std::vector<std::pair<std::string, std::string>>& filters = {});

    /**
     * @brief Show file chooser dialog for saving
     * @param title Dialog title
     * @param suggested_name Optional suggested filename
     * @param callback Function to call with selected file path (empty if cancelled)
     * @param filters Optional file filters
     */
    void show_save_file_dialog(const std::string& title,
                              const std::string& suggested_name,
                              const std::function<void(std::string)>& callback,
                              const std::vector<std::pair<std::string, std::string>>& filters = {});

    /**
     * @brief Show password creation dialog for new vaults
     * @param callback Function to call with password (empty if cancelled)
     */
    void show_create_password_dialog(const std::function<void(std::string)>& callback);

    /**
     * @brief Show password entry dialog for opening vaults
     * @param callback Function to call with password (empty if cancelled)
     */
    void show_password_dialog(const std::function<void(std::string)>& callback);

    /**
     * @brief Show YubiKey touch prompt dialog
     * @param message Prompt message
     * @param callback Function to call when done (bool success)
     */
    void show_yubikey_prompt_dialog(const std::string& message,
                                   const std::function<void(bool)>& callback);

    /**
     * @brief Show preferences dialog
     */
    void show_preferences_dialog();

    /**
     * @brief Show vault migration dialog for V1→V2 upgrade
     * @param vault_path Path to vault being migrated
     * @param callback Function to call with result (true = migrated)
     */
    void show_vault_migration_dialog(const std::string& vault_path,
                                    std::function<void(bool)> callback);

    /**
     * @brief Show validation error dialog with details
     * @param field_name Name of field that failed validation
     * @param error_details Detailed error message
     */
    void show_validation_error(const std::string& field_name,
                              const std::string& error_details);

private:
    Gtk::Window& m_parent;           ///< Parent window for modal dialogs
    VaultManager* m_vault_manager;   ///< Vault manager for vault operations

    /**
     * @brief Apply common dialog settings (modal, transient, etc.)
     * @param dialog Dialog to configure
     */
    void configure_dialog(Gtk::Window& dialog);

    /**
     * @brief Add file filters to file chooser
     * @param chooser File chooser dialog
     * @param filters List of (name, pattern) pairs
     */
    void add_file_filters(Gtk::FileChooser& chooser,
                         const std::vector<std::pair<std::string, std::string>>& filters);
};

} // namespace UI
