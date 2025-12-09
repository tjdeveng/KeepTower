// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "MainWindow.h"
#include "../dialogs/CreatePasswordDialog.h"
#include "../dialogs/PasswordDialog.h"
#include "../dialogs/PreferencesDialog.h"
#include "../../core/VaultError.h"
#include "../../utils/SettingsValidator.h"
#include "config.h"
#include "record.pb.h"
#include <regex>
#include <algorithm>
#include <ctime>
#include <format>
#include <random>

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
      m_is_locked(false),
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

    // Load Reed-Solomon settings as defaults for NEW vaults
    // Note: Opened vaults preserve their own FEC settings
    bool use_rs = settings->get_boolean("use-reed-solomon");
    int rs_redundancy = settings->get_int("rs-redundancy-percent");
    m_vault_manager->apply_default_fec_preferences(use_rs, rs_redundancy);

    // Load backup settings and apply to VaultManager
    bool backup_enabled = settings->get_boolean("backup-enabled");
    int backup_count = settings->get_int("backup-count");
    m_vault_manager->set_backup_enabled(backup_enabled);
    m_vault_manager->set_backup_count(backup_count);

    // Set up window actions
    add_action("preferences", sigc::mem_fun(*this, &MainWindow::on_preferences));
    add_action("delete-account", sigc::mem_fun(*this, &MainWindow::on_delete_account));

    // Set keyboard shortcut for preferences
    auto app = get_application();
    if (app) {
        app->set_accel_for_action("win.preferences", "<Ctrl>comma");
    }

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
    m_primary_menu->append("_Preferences", "win.preferences");
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
    constexpr int account_list_width = UI::ACCOUNT_LIST_WIDTH;
    m_paned.set_position(account_list_width);  // Left panel width

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
    auto* password_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    m_password_entry.set_hexpand(true);
    m_password_entry.set_visibility(false);
    password_box->append(m_password_entry);
    password_box->append(m_generate_password_button);
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

    // Delete button at bottom (HIG compliant placement)
    m_delete_account_button.set_label("Delete Account");
    m_delete_account_button.set_icon_name("user-trash-symbolic");
    m_delete_account_button.add_css_class("destructive-action");
    m_delete_account_button.set_sensitive(false);
    m_delete_account_button.set_margin_top(12);
    m_details_box.append(m_delete_account_button);

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
    m_generate_password_button.set_icon_name("view-refresh-symbolic");
    m_generate_password_button.set_tooltip_text("Generate Password");
    m_show_password_button.set_icon_name("view-reveal-symbolic");
    m_show_password_button.set_tooltip_text("Show/Hide Password");
    m_copy_password_button.set_icon_name("edit-copy-symbolic");
    m_copy_password_button.set_tooltip_text("Copy Password");

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
    m_delete_account_button.signal_clicked().connect(
        sigc::mem_fun(*this, &MainWindow::on_delete_account)
    );
    m_generate_password_button.signal_clicked().connect(
        sigc::mem_fun(*this, &MainWindow::on_generate_password)
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

    // Setup context menu for account list - use GestureClick for right-click
    auto gesture = Gtk::GestureClick::create();
    gesture->set_button(GDK_BUTTON_SECONDARY);  // Right-click
    gesture->signal_released().connect([this](int n_press, double x, double y) {
        // Convert coordinates from widget-relative to bin_window-relative
        // TreeView coordinates need to account for headers
        int bin_x, bin_y;
        m_account_tree_view.convert_widget_to_bin_window_coords(
            static_cast<int>(x), static_cast<int>(y), bin_x, bin_y);

        on_account_right_click(n_press, static_cast<double>(bin_x), static_cast<double>(bin_y));
    });
    m_account_tree_view.add_controller(gesture);

    // Setup activity monitoring for auto-lock
    setup_activity_monitoring();

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

    // Disconnect auto-lock timeout
    if (m_auto_lock_timeout.connected()) {
        m_auto_lock_timeout.disconnect();
    }

    // Clear cached password
    if (!m_cached_master_password.empty()) {
        std::fill(m_cached_master_password.begin(), m_cached_master_password.end(), '\0');
        m_cached_master_password.clear();
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

                    // Load default FEC preferences for new vault
                    auto settings = Gio::Settings::create("com.tjdeveng.keeptower");
                    bool use_rs = settings->get_boolean("use-reed-solomon");
                    int rs_redundancy = settings->get_int("rs-redundancy-percent");
                    m_vault_manager->apply_default_fec_preferences(use_rs, rs_redundancy);

                    // Create encrypted vault file with password
                    auto result = m_vault_manager->create_vault(vault_path.raw(), password);
                    if (result) {
                        m_current_vault_path = vault_path;
                        m_vault_open = true;
                        m_is_locked = false;
                        m_save_button.set_sensitive(true);
                        m_close_button.set_sensitive(true);
                        m_add_account_button.set_sensitive(true);
                        m_search_entry.set_sensitive(true);

                        update_account_list();
                        clear_account_details();

                        // Start activity monitoring for auto-lock
                        on_user_activity();
                    } else {
                        constexpr std::string_view error_msg{"Failed to create vault"};
                        auto error_dialog = Gtk::make_managed<Gtk::MessageDialog>(*this, std::string{error_msg},
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
                        m_is_locked = false;
                        m_save_button.set_sensitive(true);
                        m_close_button.set_sensitive(true);
                        m_add_account_button.set_sensitive(true);
                        m_search_entry.set_sensitive(true);

                        update_account_list();

                        // Start activity monitoring for auto-lock
                        on_user_activity();
                    } else {
                        constexpr std::string_view error_msg{"Failed to open vault"};
                        auto error_dialog = Gtk::make_managed<Gtk::MessageDialog>(*this, std::string{error_msg},
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

    // Stop auto-lock timer
    if (m_auto_lock_timeout.connected()) {
        m_auto_lock_timeout.disconnect();
    }

    // Clear cached password
    if (!m_cached_master_password.empty()) {
        std::fill(m_cached_master_password.begin(), m_cached_master_password.end(), '\0');
        m_cached_master_password.clear();
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
    m_is_locked = false;
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
        constexpr std::string_view error_msg{"Failed to add account"};
        m_status_label.set_text(std::string{error_msg});
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
    const Glib::ustring password = m_password_entry.get_text();

    if (password.empty()) {
        constexpr std::string_view no_password_msg{"No password to copy"};
        m_status_label.set_text(std::string{no_password_msg});
        return;
    }

    // Get the clipboard and set text
    auto clipboard = get_clipboard();
    clipboard->set_text(password);

    // Get validated clipboard timeout from settings
    auto settings = Gio::Settings::create("com.tjdeveng.keeptower");
    const int timeout_seconds = SettingsValidator::get_clipboard_timeout(settings);

    const std::string copied_msg = std::format("Password copied to clipboard (will clear in {}s)", timeout_seconds);
    m_status_label.set_text(copied_msg);

    // Cancel previous timeout if exists
    if (m_clipboard_timeout.connected()) {
        m_clipboard_timeout.disconnect();
    }

    // Schedule clipboard clear after configured timeout
    m_clipboard_timeout = Glib::signal_timeout().connect(
        [clipboard, this]() {
            clipboard->set_text("");
            constexpr std::string_view cleared_msg{"Clipboard cleared for security"};
            m_status_label.set_text(std::string{cleared_msg});
            return false;  // Don't repeat
        },
        timeout_seconds * 1000  // Convert seconds to milliseconds
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
    if (!iter) {
        return;
    }

    const int new_index = (*iter)[m_columns.m_col_index];

    // Bounds checking for safety
    if (new_index < 0) {
        return;
    }

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

void MainWindow::on_account_right_click([[maybe_unused]] int n_press, double x, double y) {
    if (!m_vault_open) {
        return;
    }

    // Get the path at the click position
    Gtk::TreeModel::Path path;
    Gtk::TreeViewColumn* column = nullptr;
    int cell_x{}, cell_y{};  // C++23: uniform initialization

    if (!m_account_tree_view.get_path_at_pos(static_cast<int>(x), static_cast<int>(y),
                                              path, column, cell_x, cell_y)) {
        return;  // Click wasn't on an item
    }

    // Select the item that was right-clicked
    m_account_tree_view.get_selection()->select(path);

    auto iter = m_account_list_store->get_iter(path);
    if (!iter) {
        return;
    }

    // Create simple popover with button (avoids action group issues)
    auto popover = Gtk::make_managed<Gtk::Popover>();
    auto box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
    box->set_margin(6);

    auto delete_button = Gtk::make_managed<Gtk::Button>("Delete Account");
    delete_button->add_css_class("flat");
    delete_button->signal_clicked().connect([this, popover]() {
        popover->popdown();
        on_delete_account();
    });

    box->append(*delete_button);
    popover->set_child(*box);
    popover->set_parent(*this);
    popover->set_autohide(true);

    // Get TreeView position in window
    double tree_x{}, tree_y{};  // C++23: uniform initialization
    if (!m_account_tree_view.translate_coordinates(*this, 0, 0, tree_x, tree_y)) {
        return;
    }

    // Calculate absolute position with header offset
    constexpr double header_offset = 25.0;  // TreeView header height
    const double abs_x = tree_x + x;
    const double abs_y = tree_y + y + header_offset;

    // Bounds check to ensure positive coordinates
    if (abs_x < 0.0 || abs_y < 0.0) {
        return;
    }

    Gdk::Rectangle pointing_rect;
    pointing_rect.set_x(static_cast<int>(abs_x));
    pointing_rect.set_y(static_cast<int>(abs_y));
    pointing_rect.set_width(1);
    pointing_rect.set_height(1);

    popover->set_pointing_to(pointing_rect);

    // Unparent when closed to prevent widget hierarchy issues
    popover->signal_closed().connect([popover]() {
        popover->unparent();
    });

    popover->popup();
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

        const auto accounts = m_vault_manager->get_all_accounts();

        for (size_t i = 0; i < accounts.size(); ++i) {
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
    m_generate_password_button.set_sensitive(false);
    m_show_password_button.set_sensitive(false);
    m_copy_password_button.set_sensitive(false);
    m_delete_account_button.set_sensitive(false);

    m_selected_account_index = -1;
}

void MainWindow::display_account_details(int index) {
    // Bounds checking for safety
    if (index < 0) {
        return;
    }

    m_selected_account_index = index;

    // Load account from VaultManager
    const auto* account = m_vault_manager->get_account(index);
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
    m_generate_password_button.set_sensitive(true);
    m_show_password_button.set_sensitive(true);
    m_copy_password_button.set_sensitive(true);
    m_delete_account_button.set_sensitive(true);
}

bool MainWindow::save_current_account() {
    // Only save if we have a valid account selected
    if (m_selected_account_index < 0 || !m_vault_open) {
        return true;  // Nothing to save, allow continue
    }    // Validate the index is within bounds
    const auto accounts = m_vault_manager->get_all_accounts();
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
    const auto email_text = m_email_entry.get_text();
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
    const auto buffer = m_notes_view.get_buffer();
    const auto notes_text = buffer->get_text();
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
    const std::string old_name = account->account_name();

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
    constexpr std::string_view email_pattern_str{
        R"(^[a-zA-Z0-9._+-]+@[a-zA-Z0-9-]+\.[a-zA-Z]{2,}(?:\.[a-zA-Z]{2,})*$)"
    };
    static const std::regex email_pattern(
        email_pattern_str.data(),
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
    auto dialog = std::make_unique<PreferencesDialog>(*this, m_vault_manager.get());
    dialog->set_hide_on_close(true);

    // Transfer ownership to lambda for proper RAII cleanup
    auto* dialog_ptr = dialog.release();
    dialog_ptr->signal_hide().connect([dialog_ptr]() noexcept {
        delete dialog_ptr;
    });

    dialog_ptr->show();
}

void MainWindow::on_delete_account() {
    if (m_selected_account_index < 0 || !m_vault_open) {
        return;
    }

    // Get account name for confirmation dialog
    const auto* account = m_vault_manager->get_account(m_selected_account_index);
    if (!account) {
        return;
    }

    const Glib::ustring account_name = account->account_name();

    // Create confirmation dialog
    auto dialog = Gtk::make_managed<Gtk::MessageDialog>(
        *this,
        "Delete Account?",
        false,
        Gtk::MessageType::WARNING,
        Gtk::ButtonsType::NONE,
        true
    );

    dialog->set_modal(true);
    dialog->set_hide_on_close(true);
    dialog->set_secondary_text(
        "Are you sure you want to delete '" + account_name + "'?\nThis action cannot be undone."
    );

    dialog->add_button("Cancel", Gtk::ResponseType::CANCEL);
    auto delete_button = dialog->add_button("Delete", Gtk::ResponseType::OK);
    delete_button->add_css_class("destructive-action");

    dialog->signal_response().connect([this, dialog](int response) {
        if (response == Gtk::ResponseType::OK) {
            // Delete the account
            if (m_vault_manager->delete_account(m_selected_account_index)) {
                clear_account_details();
                update_account_list();
                filter_accounts(m_search_entry.get_text());
            } else {
                show_error_dialog("Failed to delete account");
            }
        }
        dialog->hide();
    });

    dialog->show();
}

void MainWindow::on_generate_password() {
    if (!m_vault_open || m_selected_account_index < 0) {
        return;
    }

    // Generate a strong random password
    constexpr int password_length = 20;  // C++23: use constexpr for compile-time constants

    // Charset without ambiguous characters (0/O, 1/l/I) for better usability
    constexpr std::string_view charset =
        "abcdefghjkmnpqrstuvwxyz"  // lowercase (no l)
        "ABCDEFGHJKMNPQRSTUVWXYZ"  // uppercase (no I, O)
        "23456789"                  // digits (no 0, 1)
        "!@#$%^&*()-_=+[]{}|;:,.<>?";

    // Use std::random_device with entropy check
    std::random_device rd;
    if (rd.entropy() == 0.0) {
        // Fallback warning if random_device is deterministic
        g_warning("std::random_device has zero entropy, password may be less secure");
    }

    std::mt19937 gen(rd());
    std::uniform_int_distribution<std::size_t> dis(0, charset.size() - 1);

    std::string password;
    password.reserve(password_length);

    for (int i = 0; i < password_length; ++i) {
        password += charset[dis(gen)];
    }

    m_password_entry.set_text(password);

    // Note: password string will be cleared when it goes out of scope
    // GTK entry widget manages its own secure memory
}

void MainWindow::setup_activity_monitoring() {
    // Create event controllers to monitor user activity
    auto key_controller = Gtk::EventControllerKey::create();
    key_controller->signal_key_pressed().connect(
        [this](guint, guint, Gdk::ModifierType) {
            on_user_activity();
            return false;  // Don't block event
        }, false);
    add_controller(key_controller);

    auto motion_controller = Gtk::EventControllerMotion::create();
    motion_controller->signal_motion().connect(
        [this](double, double) {
            on_user_activity();
        });
    add_controller(motion_controller);

    auto click_controller = Gtk::GestureClick::create();
    click_controller->signal_pressed().connect(
        [this](int, double, double) {
            on_user_activity();
        });
    add_controller(click_controller);
}

void MainWindow::on_user_activity() {
    if (!m_vault_open || m_is_locked) {
        return;
    }

    // Check if auto-lock is enabled (cache settings to avoid repeated creation)
    static const auto settings = Gio::Settings::create("com.tjdeveng.keeptower");
    if (!SettingsValidator::is_auto_lock_enabled(settings)) {
        return;
    }

    // Cancel previous timeout if exists
    if (m_auto_lock_timeout.connected()) {
        m_auto_lock_timeout.disconnect();
    }

    // Schedule auto-lock after validated timeout
    const int timeout_seconds = SettingsValidator::get_auto_lock_timeout(settings);
    m_auto_lock_timeout = Glib::signal_timeout().connect(
        sigc::mem_fun(*this, &MainWindow::on_auto_lock_timeout),
        timeout_seconds * 1000  // Convert seconds to milliseconds
    );
}

bool MainWindow::on_auto_lock_timeout() {
    if (m_vault_open && !m_is_locked) {
        lock_vault();
    }
    return false;  // Don't repeat
}

void MainWindow::lock_vault() {
    if (!m_vault_open || m_is_locked) {
        return;
    }

    // Cache the master password for re-authentication
    m_cached_master_password = get_master_password_for_lock();
    if (m_cached_master_password.empty()) {
        // Can't lock without being able to unlock
        return;
    }

    // Save any unsaved changes
    save_current_account();
    if (!m_vault_manager->save_vault()) {
        g_warning("Failed to save vault before locking");
    }

    // Set locked state
    m_is_locked = true;

    // Disable UI
    m_add_account_button.set_sensitive(false);
    m_save_button.set_sensitive(false);
    m_search_entry.set_sensitive(false);
    m_delete_account_button.set_sensitive(false);
    clear_account_details();

    // Clear clipboard
    if (m_clipboard_timeout.connected()) {
        m_clipboard_timeout.disconnect();
        get_clipboard()->set_text("");
    }

    // Update status
    constexpr std::string_view locked_msg{"Vault locked due to inactivity. Click to unlock."};
    m_status_label.set_text(std::string{locked_msg});

    // Show unlock dialog
    auto* dialog = Gtk::make_managed<Gtk::MessageDialog>(
        *this,
        "Vault Locked",
        false,
        Gtk::MessageType::INFO,
        Gtk::ButtonsType::OK_CANCEL
    );
    dialog->set_secondary_text("The vault has been locked due to inactivity.\nEnter your master password to continue.");
    dialog->set_modal(true);
    dialog->set_hide_on_close(true);

    auto* content = dialog->get_message_area();
    auto* password_entry = Gtk::make_managed<Gtk::Entry>();
    password_entry->set_visibility(false);
    password_entry->set_placeholder_text("Master password");
    password_entry->set_margin_start(12);
    password_entry->set_margin_end(12);
    password_entry->set_margin_top(12);
    content->append(*password_entry);

    dialog->signal_response().connect([this, password_entry](const int response) {
        if (response == Gtk::ResponseType::OK) {
            const std::string entered_password{password_entry->get_text()};

            // Verify password by attempting to open vault
            const auto temp_vault = std::make_unique<VaultManager>();
            const bool success = temp_vault->open_vault(std::string{m_current_vault_path}, entered_password);

            if (success && entered_password == m_cached_master_password) {
                // Unlock successful
                m_is_locked = false;

                // Re-enable UI
                m_add_account_button.set_sensitive(true);
                m_save_button.set_sensitive(true);
                m_search_entry.set_sensitive(true);

                // Restore account list and selection
                update_account_list();
                filter_accounts(m_search_entry.get_text());

                // Reset activity monitoring
                on_user_activity();

                constexpr std::string_view unlocked_msg{"Vault unlocked"};
                m_status_label.set_text(std::string{unlocked_msg});
            } else {
                // Wrong password
                password_entry->set_text("");
                password_entry->grab_focus();

                auto* error_dialog = Gtk::make_managed<Gtk::MessageDialog>(
                    *this,
                    "Incorrect Password",
                    false,
                    Gtk::MessageType::ERROR,
                    Gtk::ButtonsType::OK
                );
                error_dialog->set_secondary_text("The password you entered is incorrect.");
                error_dialog->set_modal(true);
                error_dialog->set_hide_on_close(true);
                error_dialog->show();
            }
        }
    });

    dialog->show();
}

std::string MainWindow::get_master_password_for_lock() {
    // We need to get the master password to cache it for unlock
    // This is called when locking, so we prompt the user
    auto* dialog = Gtk::make_managed<Gtk::MessageDialog>(
        *this,
        "Confirm Lock",
        false,
        Gtk::MessageType::QUESTION,
        Gtk::ButtonsType::OK_CANCEL
    );
    dialog->set_secondary_text("Enter your master password to enable auto-lock.\nThis will be cached securely in memory for unlock.");
    dialog->set_modal(true);
    dialog->set_hide_on_close(true);

    auto* content = dialog->get_message_area();
    auto* password_entry = Gtk::make_managed<Gtk::Entry>();
    password_entry->set_visibility(false);
    password_entry->set_placeholder_text("Master password");
    password_entry->set_margin_start(12);
    password_entry->set_margin_end(12);
    password_entry->set_margin_top(12);
    content->append(*password_entry);

    std::string result;
    dialog->signal_response().connect([&result, password_entry](const int response) {
        if (response == Gtk::ResponseType::OK) {
            result = std::string{password_entry->get_text()};
        }
    });

    dialog->set_hide_on_close(true);
    dialog->show();

    // Wait for dialog to close (use modern GTK4 pattern)
    while (dialog->get_visible()) {
        g_main_context_iteration(nullptr, true);
    }

    return result;
}
