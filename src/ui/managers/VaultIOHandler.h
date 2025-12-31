// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file VaultIOHandler.h
 * @brief Handles vault import/export/migration operations
 *
 * Part of Phase 5 refactoring: MainWindow size reduction
 * Extracts vault I/O operations from MainWindow
 */

#pragma once

#include <gtkmm.h>
#include <string>
#include <functional>

// Forward declarations
class VaultManager;
class MainWindow;

namespace UI {

class DialogManager;

/**
 * @brief Manages vault I/O operations
 *
 * Handles import from CSV/KeePass/1Password, export to CSV/KeePass/1Password,
 * and V1→V2 migration workflows.
 *
 * Design Goals:
 * - Reduce MainWindow size by ~300-350 lines
 * - Centralize import/export logic
 * - Handle complex authentication flows for export
 * - Simplify migration workflow
 */
class VaultIOHandler {
public:
    /**
     * @brief Callbacks for UI updates after operations
     */
    using UpdateCallback = std::function<void()>;

    /** @brief Callback to save vault after import */
    using SaveCallback = std::function<void()>;

    /**
     * @brief Construct vault I/O handler
     * @param window Reference to MainWindow for dialog parenting
     * @param vault_manager Pointer to VaultManager for operations
     * @param dialog_manager Pointer to DialogManager for dialogs
     */
    VaultIOHandler(MainWindow& window,
                   VaultManager* vault_manager,
                   DialogManager* dialog_manager);

    ~VaultIOHandler() = default;

    // Delete copy and move
    VaultIOHandler(const VaultIOHandler&) = delete;
    VaultIOHandler& operator=(const VaultIOHandler&) = delete;
    VaultIOHandler(VaultIOHandler&&) = delete;
    VaultIOHandler& operator=(VaultIOHandler&&) = delete;

    /**
     * @brief Import accounts from CSV/KeePass/1Password
     * @param on_update Callback to refresh UI after import
     */
    void handle_import(const UpdateCallback& on_update);

    /**
     * @brief Export accounts to CSV/KeePass/1Password
     * @param current_vault_path Path to current vault
     * @param vault_open Whether vault is currently open
     */
    void handle_export(const std::string& current_vault_path, bool vault_open);

    /**
     * @brief Migrate V1 vault to V2 format
     * @param current_vault_path Path to current vault
     * @param vault_open Whether vault is currently open
     * @param on_success Callback when migration succeeds
     */
    void handle_migration(const std::string& current_vault_path,
                         bool vault_open,
                         const UpdateCallback& on_success);

private:
    MainWindow& m_window;
    VaultManager* m_vault_manager;
    DialogManager* m_dialog_manager;

    /**
     * @brief Show password dialog for export authentication
     * @param current_vault_path Path to current vault
     */
    void show_export_password_dialog(const std::string& current_vault_path);

    /**
     * @brief Show file save dialog for export
     * @param current_vault_path Path to current vault
     */
    void show_export_file_dialog(const std::string& current_vault_path);
};

} // namespace UI
