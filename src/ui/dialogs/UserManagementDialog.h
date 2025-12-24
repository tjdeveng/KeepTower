// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file UserManagementDialog.h
 * @brief Admin-only dialog for managing vault users
 *
 * Provides administrators with tools to:
 * - View all users and their roles
 * - Add new users with temporary passwords
 * - Remove users (with safety checks)
 * - Reset user passwords
 *
 * @section security Security Considerations
 * - Only accessible by ADMINISTRATOR role users
 * - Prevents removal of last administrator
 * - Prevents self-removal of administrators
 * - Temporary passwords are securely cleared after display
 * - All password operations use RAII and secure memory clearing
 */

#ifndef USER_MANAGEMENT_DIALOG_H
#define USER_MANAGEMENT_DIALOG_H

#include <gtkmm/dialog.h>
#include <gtkmm/box.h>
#include <gtkmm/listbox.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/label.h>
#include <gtkmm/button.h>
#include <gtkmm/entry.h>
#include <gtkmm/dropdown.h>
#include <gtkmm/stringlist.h>
#include <string>
#include <string_view>
#include <vector>
#include "../../core/VaultManager.h"
#include "../../core/MultiUserTypes.h"

/**
 * @brief User management dialog for administrators
 *
 * This dialog provides a complete user management interface for vault
 * administrators. It displays all users, their roles, and provides
 * operations to add, remove, and manage user accounts.
 *
 * @section operations Supported Operations
 * - **Add User**: Create new user with temporary password
 * - **Remove User**: Delete user (with safety checks)
 * - **Reset Password**: Generate new temporary password
 * - **View Users**: List all users with roles and status
 *
 * @section safety Safety Mechanisms
 * - Cannot remove last administrator
 * - Cannot remove self (administrators)
 * - Temporary passwords shown once, then securely cleared
 * - Confirmation dialogs for destructive operations
 */
class UserManagementDialog : public Gtk::Dialog {
public:
    /**
     * @brief Construct user management dialog
     * @param parent Parent window for modal behavior
     * @param vault_manager Vault manager for user operations
     * @param current_username Username of current logged-in user
     */
    explicit UserManagementDialog(
        Gtk::Window& parent,
        VaultManager& vault_manager,
        std::string_view current_username
    );

    virtual ~UserManagementDialog() = default;

    // Prevent copying and moving (contains parent reference)
    UserManagementDialog(const UserManagementDialog&) = delete;
    UserManagementDialog& operator=(const UserManagementDialog&) = delete;
    UserManagementDialog(UserManagementDialog&&) = delete;
    UserManagementDialog& operator=(UserManagementDialog&&) = delete;

    // Signals
    sigc::signal<void(const std::string&)> m_signal_request_relogin;  ///< Emitted when admin wants to switch to new user

private:
    /**
     * @brief Refresh the user list display
     *
     * Queries VaultManager for all users and updates the ListBox.
     * Called after any user management operation.
     */
    void refresh_user_list();

    /**
     * @brief Handle "Add User" button click
     *
     * Shows input dialog for username and role, generates temporary
     * password, and creates new user in vault.
     */
    void on_add_user();

    /**
     * @brief Handle "Remove User" button click
     * @param username Username to remove
     *
     * Confirms deletion, performs safety checks (not self, not last admin),
     * and removes user from vault.
     */
    void on_remove_user(std::string_view username);

    /**
     * @brief Handle "Reset Password" button click
     * @param username Username whose password to reset
     *
     * Generates new temporary password and displays it to admin.
     * User will be required to change password on next login.
     * Uses admin_reset_user_password (no old password required).
     */
    void on_reset_password(std::string_view username);

    /**
     * @brief Create user list row widget
     * @param user User metadata to display
     * @return Widget containing user info and action buttons
     */
    [[nodiscard]] Gtk::Widget* create_user_row(const KeepTower::KeySlot& user);

    /**
     * @brief Show temporary password to admin
     * @param username Username for context
     * @param temp_password Generated temporary password
     * @param on_closed Optional callback invoked when dialog closes
     *
     * Displays password in a dialog with copy button.
     * Password is securely cleared after dialog closes.
     * If on_closed callback is provided, it will be called after dialog dismissal.
     */
    void show_temporary_password(std::string_view username, const Glib::ustring& temp_password, std::function<void()> on_closed = nullptr);

    /**
     * @brief Generate random temporary password
     * @return Random password meeting vault security policy
     *
     * Generates password with:
     * - Minimum length from vault policy
     * - Mix of uppercase, lowercase, digits, symbols
     * - Cryptographically random (OpenSSL RAND_bytes)
     */
    [[nodiscard]] Glib::ustring generate_temporary_password();

    /**
     * @brief Get role display name
     * @param role User role enum
     * @return Human-readable role name
     */
    [[nodiscard]] static std::string get_role_display_name(KeepTower::UserRole role) noexcept;

    /**
     * @brief Check if user can be removed safely
     * @param username Username to check
     * @param user_role Role of user to remove
     * @return True if removal is allowed
     *
     * Prevents:
     * - Removing self
     * - Removing last administrator
     */
    [[nodiscard]] bool can_remove_user(std::string_view username, KeepTower::UserRole user_role) const noexcept;

    // Member widgets
    Gtk::Box m_content_box;                        ///< Main content container
    Gtk::ScrolledWindow m_scrolled_window;         ///< Scrollable user list container
    Gtk::ListBox m_user_list;                      ///< List of users
    Gtk::Box m_button_box;                         ///< Bottom button bar
    Gtk::Button m_add_user_button;                 ///< "Add User" button
    Gtk::Button m_close_button;                    ///< "Close" button

    // State
    VaultManager& m_vault_manager;                 ///< Reference to vault manager
    std::string m_current_username;                ///< Current logged-in user
};

#endif // USER_MANAGEMENT_DIALOG_H
