// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "MainWindow.h"
#include "../dialogs/CreatePasswordDialog.h"
#include "../dialogs/PasswordDialog.h"
#include "../dialogs/PreferencesDialog.h"
#include "../../core/VaultError.h"
#include "config.h"
#include "record.pb.h"
#include <regex>
#include <algorithm>
#include <ctime>
#include <format>

MainWindow::MainWindow()
    : m_main_box(Gtk::Orientation::VERTICAL, 0),
      m_search_box(Gtk::Orientation::HORIZONTAL, 12),
      m_paned(Gtk::Orientation::HORIZONTAL),
      m_details_box(Gtk::Orientation::VERTICAL, 12),
      m_account_name_label("Account Name:"),
      m_user_name_label("User Name:"),
      m_password_label("Password:"),
      m_show_password_button(""),
      m_copy_password_button("Copy"),
      m_email_label("Email:"),
      m_website_label("Website:"),
      m_notes_label("Notes:"),
      m_status_label("No vault open"),
      m_vault_open(false),
      m_updating_selection(false),
      m_selected_account_index(-1),
      m_vault_manager(std::make_unique<VaultManager>()) {

    // Set window properties
    set_title(PROJECT_NAME);
    set_default_size(1000, 700);

    // Load settings from GSettings
    auto settings = Gio::Settings::create("com.tjdeveng.keeptower");

    // Apply color scheme
    Glib::ustring color_scheme = settings->get_string("color-scheme");
    auto gtk_settings = Gtk::Settings::get_default();
    if (gtk_settings) {
        if (color_scheme == "light") {
            gtk_settings->property_gtk_application_prefer_dark_theme() = false;
        } else if (color_scheme == "dark") {
            gtk_settings->property_gtk_application_prefer_dark_theme() = true;
        } else {
            // Default: follow system preference
            gtk_settings->reset_property("gtk-application-prefer-dark-theme");
        }
    }

    // Load Reed-Solomon settings and apply to VaultManager
    bool use_rs = settings->get_boolean("use-reed-solomon");
    int rs_redundancy = settings->get_int("rs-redundancy-percent");
    m_vault_manager->set_reed_solomon_enabled(use_rs);
    m_vault_manager->set_rs_redundancy_percent(rs_redundancy);

    // Load backup settings and apply to VaultManager
    bool backup_enabled = settings->get_boolean("backup-enabled");
    int backup_count = settings->get_int("backup-count");
    m_vault_manager->set_backup_enabled(backup_enabled);
    m_vault_manager->set_backup_count(backup_count);

    // Setup HeaderBar (modern GNOME design)
    set_titlebar(m_header_bar);
    m_header_bar.set_show_title_buttons(true);

    // Left side of HeaderBar - Vault operations
    m_new_button.set_icon_name("document-new-symbolic");
    m_new_button.set_tooltip_text("Create New Vault");
    m_header_bar.pack_start(m_new_button);

    m_open_button.set_icon_name("document-open-symbolic");
    m_open_button.set_tooltip_text("Open Vault");
    m_header_bar.pack_start(m_open_button);

    m_close_button.set_icon_name("window-close-symbolic");
    m_close_button.set_tooltip_text("Close Vault");
    m_close_button.add_css_class("destructive-action");
    m_header_bar.pack_start(m_close_button);

    m_save_button.set_icon_name("document-save-symbolic");
    m_save_button.set_tooltip_text("Save Vault");
    m_save_button.add_css_class("suggested-action");
    m_header_bar.pack_start(m_save_button);

    // Right side of HeaderBar - Record operations and menu
    m_add_account_button.set_icon_name("list-add-symbolic");
    m_add_account_button.set_tooltip_text("Add Account");
    m_header_bar.pack_end(m_add_account_button);

    // Primary menu (hamburger menu)
    m_primary_menu = Gio::Menu::create();
    m_primary_menu->append("_Preferences", "app.preferences");
    m_primary_menu->append("_Keyboard Shortcuts", "win.show-help-overlay");
    m_primary_menu->append("_About KeepTower", "app.about");
    m_menu_button.set_icon_name("open-menu-symbolic");
    m_menu_button.set_menu_model(m_primary_menu);
    m_menu_button.set_tooltip_text("Main Menu");
    m_header_bar.pack_end(m_menu_button);

    // Setup the main container
    set_child(m_main_box);

    // Setup search box (modern GNOME search bar style)
    m_search_box.set_margin_start(12);
    m_search_box.set_margin_end(12);
    m_search_box.set_margin_top(12);
    m_search_box.set_margin_bottom(6);
    m_search_entry.set_hexpand(true);
    m_search_entry.set_placeholder_text("Search accounts…");
    m_search_entry.add_css_class("search");
    m_search_box.append(m_search_entry);
    m_main_box.append(m_search_box);

    // Setup split pane
    m_paned.set_vexpand(true);
    m_paned.set_position(UI::ACCOUNT_LIST_WIDTH);  // Left panel width

    // Setup account list (left side)
    m_account_list_store = Gtk::ListStore::create(m_columns);
    m_account_tree_view.set_model(m_account_list_store);

    m_account_tree_view.append_column("Account", m_columns.m_col_account_name);
    m_account_tree_view.append_column("Username", m_columns.m_col_user_name);

    m_list_scrolled.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    m_list_scrolled.set_child(m_account_tree_view);
    m_paned.set_start_child(m_list_scrolled);

    // Setup account details (right side)
    m_details_box.set_margin_start(18);
    m_details_box.set_margin_end(18);
    m_details_box.set_margin_top(18);
    m_details_box.set_margin_bottom(18);

    m_account_name_label.set_xalign(0.0);
    m_account_name_entry.set_margin_bottom(12);

    m_user_name_label.set_xalign(0.0);
    m_user_name_entry.set_margin_bottom(12);

    m_password_label.set_xalign(0.0);
    Gtk::Box* password_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    m_password_entry.set_hexpand(true);
    m_password_entry.set_visibility(false);
    password_box->append(m_password_entry);
    password_box->append(m_show_password_button);
    password_box->append(m_copy_password_button);

    m_email_label.set_xalign(0.0);
    m_email_entry.set_margin_bottom(12);

    m_website_label.set_xalign(0.0);
    m_website_entry.set_margin_bottom(12);

    m_notes_label.set_xalign(0.0);
    m_notes_scrolled.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    m_notes_scrolled.set_min_content_height(150);
    m_notes_scrolled.set_child(m_notes_view);

    m_details_box.append(m_account_name_label);
    m_details_box.append(m_account_name_entry);
    m_details_box.append(m_user_name_label);
    m_details_box.append(m_user_name_entry);
    m_details_box.append(m_password_label);
    m_details_box.append(*password_box);
    m_details_box.append(m_email_label);
    m_details_box.append(m_email_entry);
    m_details_box.append(m_website_label);
    m_details_box.append(m_website_entry);
    m_details_box.append(m_notes_label);
    m_details_box.append(m_notes_scrolled);

    m_details_scrolled.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    m_details_scrolled.set_child(m_details_box);
    m_paned.set_end_child(m_details_scrolled);

    m_main_box.append(m_paned);

    // Setup status bar
    m_status_label.set_margin_start(12);
    m_status_label.set_margin_end(12);
    m_status_label.set_margin_top(6);
    m_status_label.set_margin_bottom(6);
    m_status_label.set_xalign(0.0);
    m_status_label.add_css_class("dim-label");
    m_main_box.append(m_status_label);

    // Configure buttons
    m_save_button.set_sensitive(false);
    m_close_button.set_sensitive(false);
    m_add_account_button.set_sensitive(false);

    // Set remaining button icons
    m_show_password_button.set_icon_name("view-reveal-symbolic");
    m_copy_password_button.set_icon_name("edit-copy-symbolic");

    // Connect signals
    m_new_button.signal_clicked().connect(
        sigc::mem_fun(*this, &MainWindow::on_new_vault)
    );
    m_open_button.signal_clicked().connect(
        sigc::mem_fun(*this, &MainWindow::on_open_vault)
    );
    m_save_button.signal_clicked().connect(
        sigc::mem_fun(*this, &MainWindow::on_save_vault)
    );
    m_close_button.signal_clicked().connect(
        sigc::mem_fun(*this, &MainWindow::on_close_vault)
    );
    m_add_account_button.signal_clicked().connect(
        sigc::mem_fun(*this, &MainWindow::on_add_account)
    );
    m_show_password_button.signal_clicked().connect(
        sigc::mem_fun(*this, &MainWindow::on_toggle_password_visibility)
    );
    m_copy_password_button.signal_clicked().connect(
        sigc::mem_fun(*this, &MainWindow::on_copy_password)
    );
    m_search_entry.signal_changed().connect(
        sigc::mem_fun(*this, &MainWindow::on_search_changed)
    );

    // Connect to selection changed signal to handle account switching
    m_account_tree_view.get_selection()->signal_changed().connect(
        sigc::mem_fun(*this, &MainWindow::on_selection_changed)
    );

    // Initially disable search and details
    m_search_entry.set_sensitive(false);
    clear_account_details();
}

MainWindow::~MainWindow() {
    // Clear clipboard if password was copied
    if (m_clipboard_timeout.connected()) {
        m_clipboard_timeout.disconnect();
        get_clipboard()->set_text("");
    }
}

void MainWindow::on_new_vault() {
    // Show file save dialog to choose location for new vault
    auto dialog = Gtk::make_managed<Gtk::FileChooserDialog>(*this, "Create New Vault",
                                         Gtk::FileChooser::Action::SAVE);
    dialog->set_modal(true);
    dialog->add_button("_Cancel", Gtk::ResponseType::CANCEL);
    dialog->add_button("_Create", Gtk::ResponseType::OK);

    // Add file filter
    auto filter = Gtk::FileFilter::create();
    filter->set_name("Vault files");
    filter->add_pattern("*.vault");
    dialog->add_filter(filter);

    auto filter_all = Gtk::FileFilter::create();
    filter_all->set_name("All files");
    filter_all->add_pattern("*");
    dialog->add_filter(filter_all);

    // Set vault filter as default
    dialog->set_filter(filter);

    dialog->set_current_name("Untitled.vault");

    dialog->signal_response().connect([this, dialog](int response) {
        if (response == Gtk::ResponseType::OK) {
            // Show password creation dialog
            auto pwd_dialog = Gtk::make_managed<CreatePasswordDialog>(*this);
            Glib::ustring vault_path = dialog->get_file()->get_path();

            pwd_dialog->signal_response().connect([this, pwd_dialog, vault_path](int pwd_response) {
                if (pwd_response == Gtk::ResponseType::OK) {
                    Glib::ustring password = pwd_dialog->get_password();

                    // Create encrypted vault file with password
                    auto result = m_vault_manager->create_vault(vault_path.raw(), password);
                    if (result) {
                        m_current_vault_path = vault_path;
                        m_vault_open = true;
                        m_save_button.set_sensitive(true);
                        m_close_button.set_sensitive(true);
                        m_add_account_button.set_sensitive(true);
                        m_search_entry.set_sensitive(true);

                        update_account_list();
                        clear_account_details();
                    } else {
                        auto error_msg = std::string("Failed to create vault");
                        auto error_dialog = Gtk::make_managed<Gtk::MessageDialog>(*this, error_msg,
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
        }
        dialog->hide();
    });

    dialog->show();
}

void MainWindow::on_open_vault() {
    auto dialog = Gtk::make_managed<Gtk::FileChooserDialog>(*this, "Open Vault",
                                         Gtk::FileChooser::Action::OPEN);
    dialog->set_modal(true);
    dialog->add_button("_Cancel", Gtk::ResponseType::CANCEL);
    dialog->add_button("_Open", Gtk::ResponseType::OK);

    // Add file filter
    auto filter = Gtk::FileFilter::create();
    filter->set_name("Vault files");
    filter->add_pattern("*.vault");
    dialog->add_filter(filter);

    auto filter_all = Gtk::FileFilter::create();
    filter_all->set_name("All files");
    filter_all->add_pattern("*");
    dialog->add_filter(filter_all);

    // Set vault filter as default
    dialog->set_filter(filter);

    dialog->signal_response().connect([this, dialog](int response) {
        if (response == Gtk::ResponseType::OK) {
            Glib::ustring vault_path = dialog->get_file()->get_path();

            // Show password dialog to decrypt vault
            auto pwd_dialog = Gtk::make_managed<PasswordDialog>(*this);
            pwd_dialog->signal_response().connect([this, pwd_dialog, vault_path](int pwd_response) {
                if (pwd_response == Gtk::ResponseType::OK) {
                    Glib::ustring password = pwd_dialog->get_password();

                    auto result = m_vault_manager->open_vault(vault_path.raw(), password);
                    if (result) {
                        m_current_vault_path = vault_path;
                        m_vault_open = true;
                        m_save_button.set_sensitive(true);
                        m_close_button.set_sensitive(true);
                        m_add_account_button.set_sensitive(true);
                        m_search_entry.set_sensitive(true);

                        update_account_list();
                    } else {
                        auto error_msg = std::string("Failed to open vault");
                        auto error_dialog = Gtk::make_managed<Gtk::MessageDialog>(*this, error_msg,
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
        }
        dialog->hide();
    });

    dialog->show();
}

void MainWindow::on_save_vault() {
    if (!m_vault_open) {
        return;
    }

    // Save current account details before saving vault
    save_current_account();

    auto result = m_vault_manager->save_vault();
    if (result) {
        m_status_label.set_text("Vault saved: " + m_current_vault_path);
    } else {
        auto error_msg = std::string("Failed to save vault");
        m_status_label.set_text(error_msg);
    }
}

void MainWindow::on_close_vault() {
    if (!m_vault_open) {
        return;
    }

    // Clear clipboard if password was copied
    if (m_clipboard_timeout.connected()) {
        m_clipboard_timeout.disconnect();
        get_clipboard()->set_text("");
    }

    // Save the current account before closing
    save_current_account();

    // Prompt to save if modified
    if (!prompt_save_if_modified()) {
        return;  // User cancelled
    }

    auto result = m_vault_manager->close_vault();
    if (!result) {
        auto error_msg = std::string("Error closing vault");
        m_status_label.set_text(error_msg);
        return;
    }

    m_vault_open = false;
    m_current_vault_path.clear();
    m_save_button.set_sensitive(false);
    m_close_button.set_sensitive(false);
    m_add_account_button.set_sensitive(false);
    m_search_entry.set_sensitive(false);
    m_search_entry.set_text("");
    m_account_list_store->clear();
    clear_account_details();
    m_status_label.set_text("No vault open");
}

void MainWindow::on_add_account() {
    if (!m_vault_open) {
        m_status_label.set_text("Please open or create a vault first");
        return;
    }

    // Save the current account before creating a new one
    save_current_account();

    // Create new account record with current timestamp
    keeptower::AccountRecord new_account;
    new_account.set_id(std::to_string(std::time(nullptr)));  // Use timestamp as unique ID string
    new_account.set_created_at(std::time(nullptr));
    new_account.set_modified_at(std::time(nullptr));
    new_account.set_account_name("New Account");
    new_account.set_user_name("");
    new_account.set_password("");
    new_account.set_email("");
    new_account.set_website("");
    new_account.set_notes("");

    // Add to vault manager
    auto result = m_vault_manager->add_account(new_account);
    if (!result) {
        auto error_msg = std::string("Failed to add account");
        m_status_label.set_text(error_msg);
        return;
    }

    // Update the display
    update_account_list();

    // Select the newly added account (it will be at the end)
    auto accounts = m_vault_manager->get_all_accounts();
    int new_index = accounts.size() - 1;

    // Find the row in the tree view and select it
    auto children = m_account_list_store->children();
    for (auto iter = children.begin(); iter != children.end(); ++iter) {
        int stored_index = (*iter)[m_columns.m_col_index];
        if (stored_index == new_index) {
            // Select the row - this will trigger on_account_selected which will call display_account_details
            m_account_tree_view.get_selection()->select(iter);

            // Give the selection event time to process, then focus the name field
            Glib::signal_idle().connect_once([this]() {
                m_account_name_entry.grab_focus();
                m_account_name_entry.select_region(0, -1);
            });

            m_status_label.set_text("New account added - please enter details");
            break;
        }
    }
}void MainWindow::on_copy_password() {
    Glib::ustring password = m_password_entry.get_text();

    if (password.empty()) {
        m_status_label.set_text("No password to copy");
        return;
    }

    // Get the clipboard and set text
    auto clipboard = get_clipboard();
    clipboard->set_text(password);

    m_status_label.set_text("Password copied to clipboard (will clear in 30s)");

    // Cancel previous timeout if exists
    if (m_clipboard_timeout.connected()) {
        m_clipboard_timeout.disconnect();
    }

    // Schedule clipboard clear after 30 seconds
    m_clipboard_timeout = Glib::signal_timeout().connect(
        [clipboard, this]() {
            clipboard->set_text("");
            m_status_label.set_text("Clipboard cleared for security");
            return false;  // Don't repeat
        },
        UI::CLIPBOARD_CLEAR_TIMEOUT_MS
    );
}

void MainWindow::on_toggle_password_visibility() {
    bool current_visibility = m_password_entry.get_visibility();
    m_password_entry.set_visibility(!current_visibility);

    // Update icon based on visibility state
    if (current_visibility) {
        // Now hidden, show "reveal" icon
        m_show_password_button.set_icon_name("view-reveal-symbolic");
    } else {
        // Now visible, show "conceal" icon
        m_show_password_button.set_icon_name("view-conceal-symbolic");
    }
}

void MainWindow::on_search_changed() {
    Glib::ustring search_text = m_search_entry.get_text();
    filter_accounts(search_text);
}

void MainWindow::on_selection_changed() {
    // Prevent recursive calls when we're programmatically updating selection
    if (m_updating_selection) {
        return;
    }

    auto selection = m_account_tree_view.get_selection();
    if (!selection) {
        return;
    }

    auto iter = selection->get_selected();
    if (iter) {
        int new_index = (*iter)[m_columns.m_col_index];

        // Only save if we're switching to a different account
        if (new_index != m_selected_account_index) {
            if (!save_current_account()) {
                // Validation failed, revert to previous selection without triggering display update
                m_updating_selection = true;
                auto prev_iter = m_account_list_store->children().begin();
                while (prev_iter != m_account_list_store->children().end()) {
                    if ((*prev_iter)[m_columns.m_col_index] == m_selected_account_index) {
                        selection->select(prev_iter);
                        m_updating_selection = false;
                        return;
                    }
                    ++prev_iter;
                }
                m_updating_selection = false;
                return;
            }
        }

        display_account_details(new_index);
    }
}

void MainWindow::on_account_selected(const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn* /* column */) {
    auto iter = m_account_list_store->get_iter(path);
    if (iter) {
        int new_index = (*iter)[m_columns.m_col_index];

        // Only save if we're switching to a different account
        if (new_index != m_selected_account_index) {
            if (!save_current_account()) {
                // Validation failed, stay on current account
                return;
            }
        }

        display_account_details(new_index);
    }
}

void MainWindow::update_account_list() {
    m_account_list_store->clear();
    m_filtered_indices.clear();

    auto accounts = m_vault_manager->get_all_accounts();

    for (size_t i = 0; i < accounts.size(); i++) {
        auto row = *(m_account_list_store->append());
        row[m_columns.m_col_account_name] = accounts[i].account_name();
        row[m_columns.m_col_user_name] = accounts[i].user_name();
        row[m_columns.m_col_index] = i;
    }

    m_status_label.set_text("Vault opened: " + m_current_vault_path +
                           " (" + std::to_string(accounts.size()) + " accounts)");
}

void MainWindow::filter_accounts(const Glib::ustring& search_text) {
    m_account_list_store->clear();
    m_filtered_indices.clear();

    if (search_text.empty()) {
        // Show all accounts
        update_account_list();
        return;
    }

    // Create case-insensitive regex pattern for filtering
    try {
        std::string pattern = ".*" + search_text.lowercase().raw() + ".*";
        std::regex search_regex(pattern, std::regex::icase);

        auto accounts = m_vault_manager->get_all_accounts();

        for (size_t i = 0; i < accounts.size(); i++) {
            const auto& account = accounts[i];

            if (std::regex_search(account.account_name(), search_regex) ||
                std::regex_search(account.user_name(), search_regex) ||
                std::regex_search(account.email(), search_regex) ||
                std::regex_search(account.website(), search_regex)) {

                auto row = *(m_account_list_store->append());
                row[m_columns.m_col_account_name] = account.account_name();
                row[m_columns.m_col_user_name] = account.user_name();
                row[m_columns.m_col_index] = i;
                m_filtered_indices.push_back(i);
            }
        }

    } catch (const std::regex_error& e) {
        g_print("Regex error: %s\n", e.what());
    }
}

void MainWindow::clear_account_details() {
    m_account_name_entry.set_text("");
    m_user_name_entry.set_text("");
    m_password_entry.set_text("");
    m_email_entry.set_text("");
    m_website_entry.set_text("");
    m_notes_view.get_buffer()->set_text("");

    m_account_name_entry.set_sensitive(false);
    m_user_name_entry.set_sensitive(false);
    m_password_entry.set_sensitive(false);
    m_email_entry.set_sensitive(false);
    m_website_entry.set_sensitive(false);
    m_notes_view.set_sensitive(false);
    m_show_password_button.set_sensitive(false);
    m_copy_password_button.set_sensitive(false);

    m_selected_account_index = -1;
}

void MainWindow::display_account_details(int index) {
    m_selected_account_index = index;

    // Load account from VaultManager
    auto* account = m_vault_manager->get_account(index);
    if (!account) {
        return;
    }

    m_account_name_entry.set_text(account->account_name());
    m_user_name_entry.set_text(account->user_name());
    m_password_entry.set_text(account->password());
    m_email_entry.set_text(account->email());
    m_website_entry.set_text(account->website());
    m_notes_view.get_buffer()->set_text(account->notes());

    // Enable fields for editing
    m_account_name_entry.set_sensitive(true);
    m_user_name_entry.set_sensitive(true);
    m_password_entry.set_sensitive(true);
    m_email_entry.set_sensitive(true);
    m_website_entry.set_sensitive(true);
    m_notes_view.set_sensitive(true);
    m_show_password_button.set_sensitive(true);
    m_copy_password_button.set_sensitive(true);
}

bool MainWindow::save_current_account() {
    // Only save if we have a valid account selected
    if (m_selected_account_index < 0 || !m_vault_open) {
        return true;  // Nothing to save, allow continue
    }

    // Validate the index is within bounds
    auto accounts = m_vault_manager->get_all_accounts();
    if (m_selected_account_index >= static_cast<int>(accounts.size())) {
        g_warning("Invalid account index %d (total accounts: %zu)",
                  m_selected_account_index, accounts.size());
        return true;  // Invalid state, but don't block navigation
    }

    // Validate field lengths before saving
    if (!validate_field_length("Account Name", m_account_name_entry.get_text(), UI::MAX_ACCOUNT_NAME_LENGTH)) {
        return false;
    }
    if (!validate_field_length("Username", m_user_name_entry.get_text(), UI::MAX_USERNAME_LENGTH)) {
        return false;
    }
    if (!validate_field_length("Password", m_password_entry.get_text(), UI::MAX_PASSWORD_LENGTH)) {
        return false;
    }

    // Validate email format if not empty
    auto email_text = m_email_entry.get_text();
    if (!email_text.empty() && !validate_email_format(email_text)) {
        return false;
    }

    if (!validate_field_length("Email", email_text, UI::MAX_EMAIL_LENGTH)) {
        return false;
    }
    if (!validate_field_length("Website", m_website_entry.get_text(), UI::MAX_WEBSITE_LENGTH)) {
        return false;
    }

    // Validate notes length
    auto buffer = m_notes_view.get_buffer();
    auto notes_text = buffer->get_text();
    if (!validate_field_length("Notes", notes_text, UI::MAX_NOTES_LENGTH)) {
        return false;
    }

    // Get the current account from VaultManager
    auto* account = m_vault_manager->get_account_mutable(m_selected_account_index);
    if (!account) {
        g_warning("Failed to get account at index %d", m_selected_account_index);
        return true;  // Allow navigation even if account not found
    }

    // Store the old account name to detect if it changed
    std::string old_name = account->account_name();

    // Update the account with current field values
    account->set_account_name(m_account_name_entry.get_text().raw());
    account->set_user_name(m_user_name_entry.get_text().raw());
    account->set_password(m_password_entry.get_text().raw());
    account->set_email(m_email_entry.get_text().raw());
    account->set_website(m_website_entry.get_text().raw());
    account->set_notes(notes_text.raw());

    // Update modification timestamp
    account->set_modified_at(std::time(nullptr));

    // Only refresh the list if the account name changed
    if (old_name != account->account_name()) {
        // Find and update just this account's row in the list
        auto iter = m_account_list_store->children().begin();
        while (iter != m_account_list_store->children().end()) {
            if ((*iter)[m_columns.m_col_index] == m_selected_account_index) {
                (*iter)[m_columns.m_col_account_name] = account->account_name();
                (*iter)[m_columns.m_col_user_name] = account->user_name();
                break;
            }
            ++iter;
        }
    }

    return true;  // Save successful
}

bool MainWindow::validate_field_length(const Glib::ustring& field_name, const Glib::ustring& value, int max_length) {
    int current_length = value.length();

    if (current_length > max_length) {
        Glib::ustring message = Glib::ustring::sprintf(
            "%s exceeds maximum length.\n\nCurrent: %d characters\nMaximum: %d characters\n\nPlease shorten the field before saving.",
            field_name,
            current_length,
            max_length
        );
        show_error_dialog(message);
        return false;
    }

    return true;
}

bool MainWindow::validate_email_format(const Glib::ustring& email) {
    // Strict email validation pattern
    // Requires: localpart@domain.tld
    // - Local part: alphanumeric, dots, hyphens, underscores, plus signs
    // - Domain: must have at least one dot
    // - TLD: at least 2 characters
    static const std::regex email_pattern(
        R"(^[a-zA-Z0-9._+-]+@[a-zA-Z0-9-]+\.[a-zA-Z]{2,}(?:\.[a-zA-Z]{2,})*$)",
        std::regex::optimize
    );

    try {
        if (!std::regex_match(email.raw(), email_pattern)) {
            show_error_dialog(
                "Invalid email format.\n\n"
                "Email must be in the format: user@domain.ext\n\n"
                "Examples:\n"
                "  • john@example.com\n"
                "  • jane.doe@company.co.uk\n"
                "  • user+tag@mail.example.org"
            );
            return false;
        }
    } catch (const std::regex_error& e) {
        g_warning("Email validation regex error: %s", e.what());
        return false;
    }

    return true;
}

void MainWindow::show_error_dialog(const Glib::ustring& message) {
    auto* dialog = new Gtk::MessageDialog(*this, "Validation Error", false, Gtk::MessageType::ERROR, Gtk::ButtonsType::OK, true);
    dialog->set_secondary_text(message);
    dialog->set_modal(true);
    dialog->set_hide_on_close(true);

    // Delete dialog when closed
    dialog->signal_response().connect([dialog](int) {
        dialog->hide();
    });

    dialog->signal_hide().connect([dialog]() {
        delete dialog;
    });

    dialog->show();
}

bool MainWindow::prompt_save_if_modified() {
    // Check if vault has unsaved changes
    if (!m_vault_manager->is_modified()) {
        return true;  // No changes, proceed
    }

    // Create a custom dialog
    auto dialog = Gtk::make_managed<Gtk::Dialog>();
    dialog->set_transient_for(*this);
    dialog->set_modal(true);
    dialog->set_title("Save Changes?");

    // Add buttons
    dialog->add_button("Cancel", Gtk::ResponseType::CANCEL);
    dialog->add_button("Don't Save", Gtk::ResponseType::NO);
    dialog->add_button("Save", Gtk::ResponseType::YES);
    dialog->set_default_response(Gtk::ResponseType::YES);

    // Add content
    auto content_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 12);
    content_box->set_margin(20);

    auto primary_label = Gtk::make_managed<Gtk::Label>();
    primary_label->set_markup("<b>Save changes to vault?</b>");
    primary_label->set_xalign(0.0);

    auto secondary_label = Gtk::make_managed<Gtk::Label>(
        "Your vault has unsaved changes. Do you want to save them before closing?"
    );
    secondary_label->set_xalign(0.0);
    secondary_label->set_wrap(true);

    content_box->append(*primary_label);
    content_box->append(*secondary_label);

    dialog->get_content_area()->append(*content_box);

    // Use a flag to track the response
    int response = Gtk::ResponseType::CANCEL;
    bool dialog_done = false;

    dialog->signal_response().connect([&](int response_id) {
        response = response_id;
        dialog_done = true;
        dialog->hide();
    });

    dialog->show();

    // Process events until dialog is closed
    while (!dialog_done) {
        g_main_context_iteration(nullptr, TRUE);
    }

    if (response == Gtk::ResponseType::YES) {
        // User chose to save
        on_save_vault();
        return true;
    } else if (response == Gtk::ResponseType::NO) {
        // User chose not to save
        return true;
    } else {
        // User cancelled
        return false;
    }
}

void MainWindow::on_preferences() {
    // Use unique_ptr for automatic cleanup
    auto dialog = std::make_unique<PreferencesDialog>(*this);
    dialog->set_hide_on_close(true);

    // Transfer ownership to lambda for proper RAII cleanup
    auto* dialog_ptr = dialog.release();
    dialog_ptr->signal_hide().connect([dialog_ptr]() noexcept {
        delete dialog_ptr;
    });

    dialog_ptr->show();
}
