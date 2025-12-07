// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <gtkmm.h>
#include <vector>
#include <memory>
#include "../../core/VaultManager.h"

namespace UI {
    // Window dimensions
    inline constexpr int DEFAULT_WIDTH = 800;
    inline constexpr int DEFAULT_HEIGHT = 600;
    inline constexpr int ACCOUNT_LIST_WIDTH = 300;

    // Dialog dimensions
    inline constexpr int PASSWORD_DIALOG_WIDTH = 500;
    inline constexpr int PASSWORD_DIALOG_HEIGHT = 400;

    // Timing (milliseconds)
    inline constexpr int CLIPBOARD_CLEAR_TIMEOUT_MS = 30000;  // 30 seconds

    // Field length limits (characters)
    inline constexpr int MAX_NOTES_LENGTH = 1000;
    inline constexpr int MAX_ACCOUNT_NAME_LENGTH = 256;
    inline constexpr int MAX_USERNAME_LENGTH = 256;
    inline constexpr int MAX_PASSWORD_LENGTH = 512;
    inline constexpr int MAX_EMAIL_LENGTH = 256;
    inline constexpr int MAX_WEBSITE_LENGTH = 512;
}

class MainWindow : public Gtk::ApplicationWindow {
public:
    MainWindow();
    virtual ~MainWindow();

protected:
    // Signal handlers
    void on_new_vault();
    void on_open_vault();
    void on_save_vault();
    void on_close_vault();
    void on_add_account();
    void on_copy_password();
    void on_toggle_password_visibility();
    void on_search_changed();
    void on_selection_changed();
    void on_account_selected(const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn* column);

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
