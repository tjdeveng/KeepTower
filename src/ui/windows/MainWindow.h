#include <string>

// Forward declarations for widget classes
namespace Gtk { class Box; class HeaderBar; class Button; class MenuButton; class Label; class Entry; class TextView; class ScrolledWindow; class FlowBox; }
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file MainWindow.h
 * @brief Main application window for KeepTower
 *
 * Refactoring Status:
 * - Phase 1: Extracted controllers (AccountViewController, SearchController, etc.)
 * - Phase 2: Integrated Repository Pattern for data access layer
 * - Ongoing: Gradually replacing direct VaultManager data calls with repository calls
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
#include "../../core/managers/YubiKeyManager.h"
#endif

// Project widget headers (must come after GTKmm includes)
#include "../widgets/AccountTreeWidget.h"
#include "../widgets/AccountDetailWidget.h"

// Phase 1: View controllers for improved maintainability
#include "../controllers/AccountViewController.h"
#include "../controllers/SearchController.h"
#include "../controllers/AutoLockManager.h"
#include "../controllers/ClipboardManager.h"

// Phase 2: Repository pattern for data access
#include "../../core/repositories/IAccountRepository.h"
#include "../../core/repositories/IGroupRepository.h"

// Phase 3: Service layer for business logic
#include "../../core/services/IAccountService.h"
#include "../../core/services/IGroupService.h"

// Phase 5: Manager classes for MainWindow reduction
#include "../managers/DialogManager.h"
#include "../managers/MenuManager.h"
#include "../managers/UIStateManager.h"
#include "../managers/V2AuthenticationHandler.h"
#include "../managers/VaultIOHandler.h"
#include "../managers/YubiKeyHandler.h"
#include "../managers/GroupHandler.h"
#include "../managers/AccountEditHandler.h"
#include "../managers/AutoLockHandler.h"
#include "../managers/UserAccountHandler.h"
#include "../managers/VaultOpenHandler.h"

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
 * @section architecture Architecture
 * MainWindow uses a layered architecture:
 * - Controllers: Handle specific UI concerns (AccountViewController, SearchController)
 * - Repositories: Provide data access abstraction (AccountRepository, GroupRepository)
 * - VaultManager: Handles vault-level operations (open, close, save, encryption)
 *
 * @section features Window Features
 * - Split pane with account list and detail view
 * - Search/filter accounts
 * - Password visibility toggle
 * - Copy password to clipboard with auto-clear
 * - Input validation on all fields
 * - Real-time account updates
 * - Undo/redo support for account operations
 *
 * @section security Security Features
 * - Field length validation
 * - Clipboard auto-clear (30 seconds)
 * - Password masking by default
 * - Auto-lock on inactivity
 * - Multi-user support with role-based permissions (V2 vaults)
 *
 * @section refactoring Refactoring Progress
 * - Phase 1 (Complete): Extracted controllers from MainWindow
 * - Phase 2 (Complete): Integrated Repository Pattern
 * - Phase 3 (Pending): Service layer for business logic
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
    void on_import_from_csv();  ///< Import accounts from CSV/KeePass/1Password (Phase 5g)
    void on_export_to_csv();    ///< Export accounts with re-authentication (Phase 5g)
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

    // Security handlers (Phase 5k: delegated to AutoLockHandler)
    void on_user_activity();  ///< Reset inactivity timer (delegates to AutoLockHandler)
    bool on_auto_lock_timeout();  ///< Auto-lock vault after inactivity (delegates to AutoLockHandler)
    void lock_vault();  ///< Lock the vault requiring re-authentication (delegates to AutoLockHandler)

    // Helper methods
    bool save_current_account();  ///< Returns false if validation fails

    /** @brief Refresh account list display (delegates to AccountTreeWidget) */
    void update_account_list();

    /** @brief Apply search filter to accounts (delegates to SearchController)
     *  @param search_text Text to search for in account fields */
    void filter_accounts(const Glib::ustring& search_text);

    /** @brief Clear account details panel */
    void clear_account_details();

    /** @brief Display account details in right panel
     *  @param index Account index in vault */
    void display_account_details(int index);

    /** @brief Show error dialog with message
     *  @param message Error message to display */
    void show_error_dialog(const Glib::ustring& message);

    /** @brief Validate field length against maximum
     *  @param field_name Name of field for error message
     *  @param value Field value to validate
     *  @param max_length Maximum allowed length
     *  @return true if valid, false if too long */
    bool validate_field_length(const Glib::ustring& field_name, const Glib::ustring& value, int max_length);

    /** @brief Validate email address format
     *  @param email Email address to validate
     *  @return true if format is valid */
    bool validate_email_format(const Glib::ustring& email);
    bool prompt_save_if_modified();  ///< Prompt to save if vault modified, return false if user cancels
    void update_undo_redo_sensitivity(bool can_undo, bool can_redo);  ///< Update undo/redo menu item sensitivity
    // [REMOVED] Legacy drag-and-drop setup (handled by AccountTreeWidget)

    // Phase 2: Repository management
    /**
     * @brief Initialize repositories after vault is opened
     *
     * Creates AccountRepository and GroupRepository instances that wrap
     * the VaultManager for data access. Should be called immediately after
     * a vault is successfully opened.
     *
     * @note Logs warning if VaultManager is null
     */
    void initialize_repositories();

    /**
     * @brief Reset repositories when vault is closed
     *
     * Destroys repository instances to free resources and ensure
     * no operations are attempted on a closed vault.
     */
    void reset_repositories();

    // Phase 3: Service management
    /**
     * @brief Initialize services after repositories are created
     *
     * Creates AccountService and GroupService instances that provide
     * business logic validation on top of repository data access.
     * Should be called immediately after initialize_repositories().
     *
     * @note Logs warning if repositories are null
     */
    void initialize_services();

    /**
     * @brief Reset services when vault is closed
     *
     * Destroys service instances to free resources.
     * Should be called before reset_repositories().
     */
    void reset_services();
    // V2 multi-user vault helpers
    std::optional<uint32_t> detect_vault_version(const std::string& vault_path);  ///< Detect if vault is V1 or V2
    void handle_v2_vault_open(const std::string& vault_path);  ///< Handle V2 user authentication (delegates to V2AuthenticationHandler)
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

    /** @brief Find account vault index by unique ID
     *  @param account_id Account unique identifier
     *  @return Account index or -1 if not found */
    int find_account_index_by_id(const std::string& account_id) const;

    /** @brief Filter account list to show only accounts in specific group
     *  @param group_id Group unique identifier */
    void filter_accounts_by_group(const std::string& group_id);

    /** @brief Display account context menu at pointer position
     *  @param account_id Account unique identifier
     *  @param widget Widget to position menu relative to
     *  @param x X coordinate
     *  @param y Y coordinate */
    void show_account_context_menu(const std::string& account_id, Gtk::Widget* widget, double x, double y);

    /** @brief Display group context menu at pointer position
     *  @param group_id Group unique identifier
     *  @param widget Widget to position menu relative to
     *  @param x X coordinate
     *  @param y Y coordinate */
    void show_group_context_menu(const std::string& group_id, Gtk::Widget* widget, double x, double y);

    /** @brief Handle account drag-and-drop reorder
     *  @param account_id Account being moved
     *  @param target_group_id Destination group ID
     *  @param new_index New position index */
    void on_account_reordered(const std::string& account_id, const std::string& target_group_id, int new_index);

    /** @brief Handle group drag-and-drop reorder
     *  @param group_id Group being moved
     *  @param new_index New position index */
    void on_group_reordered(const std::string& group_id, int new_index);

    // Member widgets
    Gtk::Box m_main_box;              ///< Main vertical container
    Gtk::HeaderBar m_header_bar;      ///< Application header bar with title and buttons

    Gtk::Button m_new_button;         ///< "New Vault" toolbar button
    Gtk::Button m_open_button;        ///< "Open Vault" toolbar button
    Gtk::Button m_save_button;        ///< "Save" toolbar button
    Gtk::Button m_close_button;       ///< "Close Vault" toolbar button
    Gtk::Button m_add_account_button; ///< "Add Account" toolbar button
    Gtk::MenuButton m_menu_button;    ///< Primary menu button

    // Primary menu
    Glib::RefPtr<Gio::Menu> m_primary_menu;  ///< Application primary menu model

    // Search panel
    Gtk::Box m_search_box;                             ///< Search controls container
    Gtk::SearchEntry m_search_entry;                   ///< Text search entry widget
    Gtk::DropDown m_field_filter_dropdown;             ///< Field selector dropdown
    Glib::RefPtr<Gtk::StringList> m_field_filter_model; ///< Field filter options model
    Gtk::DropDown m_tag_filter_dropdown;               ///< Tag filter dropdown
    Glib::RefPtr<Gtk::StringList> m_tag_filter_model;  ///< Tag filter options model
    std::string m_selected_tag_filter;                 ///< Current tag filter (empty = all)
    Gtk::Button m_sort_button;                         ///< A-Z / Z-A sort toggle button

    // Split view: account list | details
    Gtk::Paned m_paned;  ///< Horizontal paned splitter

    // New widget-based UI
    std::unique_ptr<AccountTreeWidget> m_account_tree_widget;   ///< Hierarchical account/group tree
    std::unique_ptr<AccountDetailWidget> m_account_detail_widget; ///< Account details panel

    Gtk::Label m_status_label;  ///< Status bar text label

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
    sigc::connection m_row_inserted_conn;     ///< Connection for detecting drag-and-drop reordering
    std::vector<sigc::connection> m_signal_connections;  ///< Persistent widget signal connections
    Glib::RefPtr<Gio::Settings> m_desktop_settings;      ///< GNOME desktop settings for theme monitoring
    sigc::connection m_theme_changed_connection;         ///< Connection for system theme changes

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

    // Phase 2: Repositories for data access (created after vault is opened)
    std::unique_ptr<KeepTower::IAccountRepository> m_account_repo;  ///< Repository for account operations
    std::unique_ptr<KeepTower::IGroupRepository> m_group_repo;      ///< Repository for group operations

    // Phase 3: Services for business logic (created after repositories)
    std::unique_ptr<KeepTower::IAccountService> m_account_service;  ///< Service for account business logic
    std::unique_ptr<KeepTower::IGroupService> m_group_service;      ///< Service for group business logic

    // Phase 1: View controllers (reduce MainWindow complexity)
    std::unique_ptr<AccountViewController> m_account_controller;  ///< Manages account list logic
    std::unique_ptr<SearchController> m_search_controller;        ///< Manages search/filter logic
    std::unique_ptr<KeepTower::AutoLockManager> m_auto_lock_manager;         ///< Manages inactivity timeout
    std::unique_ptr<KeepTower::ClipboardManager> m_clipboard_manager;        ///< Manages clipboard auto-clear

    // Phase 5: Managers for MainWindow reduction
    std::unique_ptr<UI::DialogManager> m_dialog_manager;          ///< Centralized dialog management
    std::unique_ptr<UI::MenuManager> m_menu_manager;              ///< Centralized menu management
    std::unique_ptr<UI::UIStateManager> m_ui_state_manager;       ///< Centralized state management
    std::unique_ptr<UI::V2AuthenticationHandler> m_v2_auth_handler; ///< V2 vault authentication
    std::unique_ptr<UI::VaultIOHandler> m_vault_io_handler;       ///< Import/Export/Migration
    std::unique_ptr<UI::YubiKeyHandler> m_yubikey_handler;        ///< YubiKey operations
    std::unique_ptr<UI::GroupHandler> m_group_handler;            ///< Group management
    std::unique_ptr<UI::AccountEditHandler> m_account_edit_handler; ///< Account editing
    std::unique_ptr<UI::AutoLockHandler> m_auto_lock_handler;     ///< Auto-lock and activity monitoring
    std::unique_ptr<UI::UserAccountHandler> m_user_account_handler; ///< V2 user account operations
    std::unique_ptr<UI::VaultOpenHandler> m_vault_open_handler;   ///< Vault creation and opening
};

#endif // MAINWINDOW_H
