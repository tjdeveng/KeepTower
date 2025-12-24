#include <string>

// Forward declarations for widget classes
namespace Gtk { class Box; class HeaderBar; class Button; class MenuButton; class Label; class Entry; class TextView; class ScrolledWindow; class FlowBox; }
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file MainWindow.h
 * @brief Main application window for KeepTower
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

// GTKmm includes
#include <gtkmm/window.h>
#include <gtkmm/box.h>
#include <gtkmm/headerbar.h>
#include <gtkmm/button.h>
#include <gtkmm/menubutton.h>
#include <gtkmm/label.h>
#include <gtkmm/entry.h>
#include <gtkmm/textview.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/flowbox.h>
#include <vector>
#include <memory>
#include "../../core/VaultManager.h"
#include "../../core/commands/UndoManager.h"
#ifdef HAVE_YUBIKEY_SUPPORT
#include "../../core/YubiKeyManager.h"
#endif

// Project widget headers (must come after GTKmm includes)
#include "../widgets/AccountTreeWidget.h"
#include "../widgets/AccountDetailWidget.h"

/**
 * @namespace UI
 * @brief User interface constants and configuration
 */
namespace UI {
    // Window dimensions
    inline constexpr int DEFAULT_WIDTH = 800;  ///< Default window width in pixels
    inline constexpr int DEFAULT_HEIGHT = 600; ///< Default window height in pixels
    inline constexpr int ACCOUNT_LIST_WIDTH = 220; ///< Account list pane width

    // Dialog dimensions
    inline constexpr int PASSWORD_DIALOG_WIDTH = 500;  ///< Password dialog width
    inline constexpr int PASSWORD_DIALOG_HEIGHT = 400; ///< Password dialog height

    // Timing (milliseconds)
    inline constexpr int CLIPBOARD_CLEAR_TIMEOUT_MS = 30000;  ///< Auto-clear clipboard after 30 seconds

    // Field length limits (characters)
    inline constexpr int MAX_NOTES_LENGTH = 1000;         ///< Maximum notes field length
    inline constexpr int MAX_ACCOUNT_NAME_LENGTH = 256;   ///< Maximum account name length
    inline constexpr int MAX_USERNAME_LENGTH = 256;       ///< Maximum username length
    inline constexpr int MAX_PASSWORD_LENGTH = 512;       ///< Maximum password length
    inline constexpr int MAX_EMAIL_LENGTH = 256;          ///< Maximum email length
    inline constexpr int MAX_WEBSITE_LENGTH = 512;        ///< Maximum website URL length
}

/**
 * @brief Main application window for password management
 *
 * Provides the primary user interface for managing encrypted password vaults.
 * Supports creating, opening, and managing multiple account records within vaults.
 *
 * @section features Window Features
 * - Split pane with account list and detail view
 * - Search/filter accounts
 * - Password visibility toggle
 * - Copy password to clipboard with auto-clear
 * - Input validation on all fields
 * - Real-time account updates
 *
 * @section security Security Features
 * - Field length validation
 * - Clipboard auto-clear (30 seconds)
 * - Password masking by default
 */
class MainWindow : public Gtk::ApplicationWindow {
public:
    MainWindow();
    virtual ~MainWindow();

protected:
    // Signal handlers
    void on_new_vault();      ///< Create new vault
    void on_open_vault();     ///< Open existing vault
    void on_save_vault();     ///< Save current vault
    void on_close_vault();    ///< Close current vault
    void on_migrate_v1_to_v2(); ///< Migrate V1 vault to V2 multi-user format (Phase 8)
    void on_add_account();    ///< Add new account
    void on_delete_account(); ///< Delete selected account
    void on_preferences();    ///< Show preferences dialog
    void on_import_from_csv();  ///< Import accounts from CSV
    void on_export_to_csv();    ///< Export accounts to CSV (with re-authentication)
    void show_export_password_dialog();  ///< Show password dialog and perform export auth
    void show_export_file_chooser();  ///< Show file chooser and perform export
    void on_test_yubikey();   ///< Test YubiKey detection
    void on_manage_yubikeys();  ///< Manage YubiKey backup keys
    void on_copy_password();  ///< Copy password to clipboard
    void on_generate_password();  ///< Generate random password
    void on_toggle_password_visibility();  ///< Show/hide password
    void on_undo();  ///< Undo last operation
    void on_redo();  ///< Redo last undone operation
    void on_star_column_clicked(const Gtk::TreeModel::Path& path);  ///< Toggle favorite by clicking star column (legacy)
    void on_favorite_toggled(int account_index);  ///< Handle favorite toggle from AccountRowWidget
    [[nodiscard]] bool is_undo_redo_enabled() const;  ///< Check if undo/redo is enabled in preferences
    void on_tags_entry_activate();  ///< Add tag when Enter is pressed
    void add_tag_chip(const std::string& tag);  ///< Add a tag chip to the flowbox
    void remove_tag_chip(const std::string& tag);  ///< Remove a tag chip
    void update_tags_display();  ///< Refresh tags display from current account
    std::vector<std::string> get_current_tags();  ///< Get tags from current account
    void update_tag_filter_dropdown();  ///< Update tag filter dropdown with all unique tags
    void on_tag_filter_changed();  ///< Handle tag filter selection change
    void on_field_filter_changed();  ///< Handle search field filter selection change
    void on_sort_button_clicked();  ///< Toggle sort direction between A-Z and Z-A

    // Account list handlers
    void on_search_changed(); ///< Filter accounts by search
    void on_selection_changed();  ///< Handle account selection
    // [REMOVED] Legacy TreeView/TreeStore handlers (migrated to AccountTreeWidget)

    // Group management handlers
    void on_create_group();  ///< Show dialog to create new group
    void on_rename_group(const std::string& group_id, const Glib::ustring& current_name);  ///< Rename a group
    void on_delete_group(const std::string& group_id);  ///< Delete a group
    void on_add_account_to_group(int account_index, const std::string& group_id);  ///< Add account to group
    void on_remove_account_from_group(int account_index, const std::string& group_id);  ///< Remove account from group

    // Security handlers
    void on_user_activity();  ///< Reset inactivity timer
    bool on_auto_lock_timeout();  ///< Auto-lock vault after inactivity
    void lock_vault();  ///< Lock the vault requiring re-authentication

    // Helper methods
    bool save_current_account();  ///< Returns false if validation fails
    void update_account_list(); // Will be stubbed, legacy logic removed
    void filter_accounts(const Glib::ustring& search_text); // Will be stubbed, legacy logic removed
    void clear_account_details();
    void display_account_details(int index);
    void show_error_dialog(const Glib::ustring& message);
    bool validate_field_length(const Glib::ustring& field_name, const Glib::ustring& value, int max_length);
    bool validate_email_format(const Glib::ustring& email);
    bool prompt_save_if_modified();  ///< Prompt to save if vault modified, return false if user cancels
    void setup_activity_monitoring();  ///< Setup event monitors for user activity
    std::string get_master_password_for_lock();  ///< Get master password to re-open after lock
    void update_undo_redo_sensitivity(bool can_undo, bool can_redo);  ///< Update undo/redo menu item sensitivity
    // [REMOVED] Legacy drag-and-drop setup (handled by AccountTreeWidget)

    // V2 multi-user vault helpers
    std::optional<uint32_t> detect_vault_version(const std::string& vault_path);  ///< Detect if vault is V1 or V2
    void handle_v2_vault_open(const std::string& vault_path);  ///< Handle V2 user authentication
    void handle_password_change_required(const std::string& username);  ///< Force password change for first login
    void handle_yubikey_enrollment_required(const std::string& username);  ///< Force YubiKey enrollment if policy requires
    void complete_vault_opening(const std::string& vault_path, const std::string& username);  ///< Complete vault opening after authentication
    void update_session_display();  ///< Update header bar with session info (username, role)

    // Phase 4: Permissions & role-based UI
    void on_change_my_password();  ///< Allow user to change their own password
    void on_logout();  ///< Logout from V2 vault (return to login screen)
    void on_manage_users();  ///< Admin: Show user management dialog
    void update_menu_for_role();  ///< Update menu item visibility based on user role
    [[nodiscard]] bool is_v2_vault_open() const noexcept;  ///< Check if V2 vault is currently open
    [[nodiscard]] bool is_current_user_admin() const noexcept;  ///< Check if current user is administrator

    // Helper methods for widget-based UI
    int find_account_index_by_id(const std::string& account_id) const;
    void filter_accounts_by_group(const std::string& group_id);
    void show_account_context_menu(const std::string& account_id, Gtk::Widget* widget, double x, double y);
    void show_group_context_menu(const std::string& group_id, Gtk::Widget* widget, double x, double y);
    void on_account_reordered(const std::string& account_id, const std::string& target_group_id, int new_index);
    void on_group_reordered(const std::string& group_id, int new_index);

    // Member widgets
    Gtk::Box m_main_box;
    Gtk::HeaderBar m_header_bar;

    Gtk::Button m_new_button;
    Gtk::Button m_open_button;
    Gtk::Button m_save_button;
    Gtk::Button m_close_button;
    Gtk::Button m_add_account_button;
    Gtk::MenuButton m_menu_button;

    // Primary menu
    Glib::RefPtr<Gio::Menu> m_primary_menu;

    // Search panel
    Gtk::Box m_search_box;
    Gtk::SearchEntry m_search_entry;
    Gtk::DropDown m_field_filter_dropdown;
    Glib::RefPtr<Gtk::StringList> m_field_filter_model;
    Gtk::DropDown m_tag_filter_dropdown;
    Glib::RefPtr<Gtk::StringList> m_tag_filter_model;
    std::string m_selected_tag_filter;  // Empty for "All", otherwise the tag to filter by
    Gtk::Button m_sort_button;  // Toggle A-Z / Z-A sorting

    // Split view: account list | details
    Gtk::Paned m_paned;       // Splits accounts from details

    // New widget-based UI
    std::unique_ptr<AccountTreeWidget> m_account_tree_widget;
    std::unique_ptr<AccountDetailWidget> m_account_detail_widget;

    Gtk::Label m_status_label;

    // Tree model columns for accounts and groups
    // [REMOVED] Legacy TreeModel columns (migrated to AccountTreeWidget)

    // State
    bool m_vault_open;                        ///< True if a vault is currently open
    bool m_updating_selection;                ///< Prevent recursive selection change handling
    bool m_is_locked;                         ///< Vault is locked, requires re-authentication
    Glib::ustring m_current_vault_path;       ///< Path to currently open vault file
    std::string m_cached_master_password;     ///< Cached for re-opening after lock (cleared on close)
    int m_selected_account_index;             ///< Currently selected account index (-1 if none)
    std::vector<int> m_filtered_indices;      ///< Indices matching current search filter
    sigc::connection m_clipboard_timeout;     ///< Connection for clipboard auto-clear timer
    sigc::connection m_auto_lock_timeout;     ///< Connection for auto-lock inactivity timer
    sigc::connection m_row_inserted_conn;     ///< Connection for detecting drag-and-drop reordering
    std::vector<sigc::connection> m_signal_connections;  ///< Persistent widget signal connections

    // Context menu state
    std::string m_context_menu_account_id;  ///< Account ID for current context menu
    std::string m_context_menu_group_id;     ///< Group ID for current context menu

    // V2 multi-user session state
    Gtk::Label m_session_label;              ///< Display current user and role in header

    // Phase 4: Role-based menu actions (for enabling/disabling)
    Glib::RefPtr<Gio::SimpleAction> m_change_password_action;  ///< Change current user's password
    Glib::RefPtr<Gio::SimpleAction> m_logout_action;           ///< Logout from V2 vault
    Glib::RefPtr<Gio::SimpleAction> m_manage_users_action;     ///< Admin-only user management
    Glib::RefPtr<Gio::SimpleAction> m_export_action;           ///< Admin-only export (V2 vaults)

    // Vault manager
    std::unique_ptr<VaultManager> m_vault_manager;  ///< Manages vault encryption/decryption
    UndoManager m_undo_manager;  ///< Manages undo/redo history for vault operations
};

#endif // MAINWINDOW_H
