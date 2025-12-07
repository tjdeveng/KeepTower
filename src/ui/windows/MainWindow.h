// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file MainWindow.h
 * @brief Main application window for KeepTower
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <gtkmm.h>
#include <vector>
#include <memory>
#include "../../core/VaultManager.h"

/**
 * @namespace UI
 * @brief User interface constants and configuration
 */
namespace UI {
    // Window dimensions
    inline constexpr int DEFAULT_WIDTH = 800;  ///< Default window width in pixels
    inline constexpr int DEFAULT_HEIGHT = 600; ///< Default window height in pixels
    inline constexpr int ACCOUNT_LIST_WIDTH = 300; ///< Account list pane width

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
    void on_add_account();    ///< Add new account
    void on_copy_password();  ///< Copy password to clipboard
    void on_toggle_password_visibility();  ///< Show/hide password
    void on_search_changed(); ///< Filter accounts by search
    void on_selection_changed();  ///< Handle account selection
    void on_account_selected(const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn* column);  ///< Double-click handler

    // Helper methods
    void save_current_account();
    void update_account_list();
    void filter_accounts(const Glib::ustring& search_text);
    void clear_account_details();
    void display_account_details(int index);
    void show_error_dialog(const Glib::ustring& message);
    bool validate_field_length(const Glib::ustring& field_name, const Glib::ustring& value, int max_length);

    // Member widgets
    Gtk::Box m_main_box;
    Gtk::Box m_toolbar_box;

    Gtk::Button m_new_button;
    Gtk::Button m_open_button;
    Gtk::Button m_save_button;
    Gtk::Button m_close_button;
    Gtk::Button m_add_account_button;

    Gtk::Separator m_separator;

    // Search panel
    Gtk::Box m_search_box;
    Gtk::Label m_search_label;
    Gtk::SearchEntry m_search_entry;

    // Split view: list on left, details on right
    Gtk::Paned m_paned;

    // Account list (left side)
    Gtk::ScrolledWindow m_list_scrolled;
    Gtk::TreeView m_account_tree_view;
    Glib::RefPtr<Gtk::ListStore> m_account_list_store;

    // Account details (right side)
    Gtk::ScrolledWindow m_details_scrolled;
    Gtk::Box m_details_box;

    Gtk::Label m_account_name_label;
    Gtk::Entry m_account_name_entry;

    Gtk::Label m_user_name_label;
    Gtk::Entry m_user_name_entry;

    Gtk::Label m_password_label;
    Gtk::Entry m_password_entry;
    Gtk::Button m_show_password_button;
    Gtk::Button m_copy_password_button;

    Gtk::Label m_email_label;
    Gtk::Entry m_email_entry;

    Gtk::Label m_website_label;
    Gtk::Entry m_website_entry;

    Gtk::Label m_notes_label;
    Gtk::TextView m_notes_view;
    Gtk::ScrolledWindow m_notes_scrolled;

    Gtk::Label m_status_label;

    // Tree model columns
    class ModelColumns : public Gtk::TreeModel::ColumnRecord {
    public:
        ModelColumns() {
            add(m_col_account_name);
            add(m_col_user_name);
            add(m_col_index);
        }

        Gtk::TreeModelColumn<Glib::ustring> m_col_account_name;
        Gtk::TreeModelColumn<Glib::ustring> m_col_user_name;
        Gtk::TreeModelColumn<int> m_col_index;  // Index in vault data
    };

    ModelColumns m_columns;

    // State
    bool m_vault_open;
    Glib::ustring m_current_vault_path;
    int m_selected_account_index;
    std::vector<int> m_filtered_indices;  // Indices matching current search
    sigc::connection m_clipboard_timeout;

    // Vault manager
    std::unique_ptr<VaultManager> m_vault_manager;
};

#endif // MAINWINDOW_H
