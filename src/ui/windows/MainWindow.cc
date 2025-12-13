// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "MainWindow.h"
#include "../dialogs/CreatePasswordDialog.h"
#include "../dialogs/PasswordDialog.h"
#include "../dialogs/PreferencesDialog.h"
#include "../dialogs/YubiKeyPromptDialog.h"
#include "../../core/VaultError.h"
#include "../../utils/SettingsValidator.h"
#include "../../utils/ImportExport.h"
#include "../../utils/Log.h"
#include "config.h"
#include <cstring>  // For memset
#ifdef HAVE_YUBIKEY_SUPPORT
#include "../../core/YubiKeyManager.h"
#include "../dialogs/YubiKeyManagerDialog.h"
#endif
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
    add_action("import-csv", sigc::mem_fun(*this, &MainWindow::on_import_from_csv));
    add_action("export-csv", sigc::mem_fun(*this, &MainWindow::on_export_to_csv));
    add_action("delete-account", sigc::mem_fun(*this, &MainWindow::on_delete_account));
#ifdef HAVE_YUBIKEY_SUPPORT
    add_action("test-yubikey", sigc::mem_fun(*this, &MainWindow::on_test_yubikey));
    add_action("manage-yubikeys", sigc::mem_fun(*this, &MainWindow::on_manage_yubikeys));
#endif

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
    m_primary_menu->append("_Import Accounts...", "win.import-csv");
    m_primary_menu->append("_Export Accounts...", "win.export-csv");
#ifdef HAVE_YUBIKEY_SUPPORT
    m_primary_menu->append("Manage _YubiKeys", "win.manage-yubikeys");
    m_primary_menu->append("Test _YubiKey", "win.test-yubikey");
#endif
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

    // Tags configuration
    m_tags_label.set_xalign(0.0);
    m_tags_entry.set_placeholder_text("Add tag (press Enter)");
    m_tags_entry.set_margin_bottom(6);
    m_tags_scrolled.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::NEVER);
    m_tags_scrolled.set_min_content_height(40);
    m_tags_scrolled.set_max_content_height(120);
    m_tags_scrolled.set_child(m_tags_flowbox);
    m_tags_scrolled.set_margin_bottom(12);
    m_tags_flowbox.set_selection_mode(Gtk::SelectionMode::NONE);
    m_tags_flowbox.set_max_children_per_line(10);
    m_tags_flowbox.set_homogeneous(false);

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
    m_details_box.append(m_tags_label);
    m_details_box.append(m_tags_entry);
    m_details_box.append(m_tags_scrolled);
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

    // Add CSS styling for tag chips
    auto css_provider = Gtk::CssProvider::create();
    css_provider->load_from_data(R"(
        .tag-chip {
            background-color: alpha(@accent_bg_color, 0.2);
            border-radius: 12px;
            padding: 2px 4px;
        }
        .tag-chip:hover {
            background-color: alpha(@accent_bg_color, 0.3);
        }
        .tag-chip label {
            font-size: 0.9em;
        }
        .tag-chip button {
            min-width: 16px;
            min-height: 16px;
            padding: 0;
        }
    )");
    Gtk::StyleContext::add_provider_for_display(
        Gdk::Display::get_default(),
        css_provider,
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );

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
    m_tags_entry.signal_activate().connect(
        sigc::mem_fun(*this, &MainWindow::on_tags_entry_activate)
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
                    bool require_yubikey = pwd_dialog->get_yubikey_enabled();

                    // Load default FEC preferences for new vault
                    auto settings = Gio::Settings::create("com.tjdeveng.keeptower");
                    bool use_rs = settings->get_boolean("use-reed-solomon");
                    int rs_redundancy = settings->get_int("rs-redundancy-percent");
                    m_vault_manager->apply_default_fec_preferences(use_rs, rs_redundancy);

#ifdef HAVE_YUBIKEY_SUPPORT
                    // Show touch prompt if YubiKey is required
                    YubiKeyPromptDialog* touch_dialog = nullptr;
                    if (require_yubikey) {
                        pwd_dialog->hide();
                        touch_dialog = Gtk::make_managed<YubiKeyPromptDialog>(*this,
                            YubiKeyPromptDialog::PromptType::TOUCH);
                        touch_dialog->present();

                        // Force GTK to process events and render the dialog
                        auto context = Glib::MainContext::get_default();
                        while (context->pending()) {
                            context->iteration(false);
                        }
                        g_usleep(150000);  // 150ms
                    }
#endif

                    // Create encrypted vault file with password (and optionally YubiKey)
                    auto result = m_vault_manager->create_vault(vault_path.raw(), password, require_yubikey);

#ifdef HAVE_YUBIKEY_SUPPORT
                    if (touch_dialog) {
                        touch_dialog->hide();
                    }
#endif
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

#ifdef HAVE_YUBIKEY_SUPPORT
            // Check if vault requires YubiKey
            std::string yubikey_serial;
            bool yubikey_required = m_vault_manager->check_vault_requires_yubikey(vault_path.raw(), yubikey_serial);

            if (yubikey_required) {
                // Check if YubiKey is present
                YubiKeyManager yk_manager;
                [[maybe_unused]] bool yk_init = yk_manager.initialize();

                if (!yk_manager.is_yubikey_present()) {
                    // Show "Insert YubiKey" dialog
                    auto yk_dialog = Gtk::make_managed<YubiKeyPromptDialog>(*this,
                        YubiKeyPromptDialog::PromptType::INSERT, yubikey_serial);

                    yk_dialog->signal_response().connect([this, yk_dialog, vault_path](int yk_response) {
                        if (yk_response == Gtk::ResponseType::OK) {
                            // User clicked Retry - try opening again
                            yk_dialog->hide();
                            on_open_vault();  // Restart the process
                            return;
                        }
                        yk_dialog->hide();
                    });

                    yk_dialog->show();
                    dialog->hide();
                    return;
                }
            }
#endif

            // Show password dialog to decrypt vault
            auto pwd_dialog = Gtk::make_managed<PasswordDialog>(*this);
            pwd_dialog->signal_response().connect([this, pwd_dialog, vault_path](int pwd_response) {
                if (pwd_response == Gtk::ResponseType::OK) {
                    Glib::ustring password = pwd_dialog->get_password();

#ifdef HAVE_YUBIKEY_SUPPORT
                    // Check again if YubiKey is required (for touch prompt)
                    std::string yubikey_serial;
                    bool yubikey_required = m_vault_manager->check_vault_requires_yubikey(vault_path.raw(), yubikey_serial);

                    YubiKeyPromptDialog* touch_dialog = nullptr;
                    if (yubikey_required) {
                        // Hide password dialog to show touch prompt
                        pwd_dialog->hide();

                        // Show touch prompt dialog
                        touch_dialog = Gtk::make_managed<YubiKeyPromptDialog>(*this,
                            YubiKeyPromptDialog::PromptType::TOUCH);
                        touch_dialog->present();

                        // Force GTK to process events and render the dialog
                        auto context = Glib::MainContext::get_default();
                        while (context->pending()) {
                            context->iteration(false);
                        }

                        // Additional small delay to ensure dialog is fully rendered
                        g_usleep(150000);  // 150ms
                    }
#endif

                    auto result = m_vault_manager->open_vault(vault_path.raw(), password);

#ifdef HAVE_YUBIKEY_SUPPORT
                    // Hide touch prompt if it was shown
                    if (touch_dialog) {
                        touch_dialog->hide();
                    }
#endif

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

    // Clear tags
    auto child = m_tags_flowbox.get_first_child();
    while (child) {
        auto next = child->get_next_sibling();
        m_tags_flowbox.remove(*child);
        child = next;
    }
    m_tags_entry.set_text("");

    m_account_name_entry.set_sensitive(false);
    m_user_name_entry.set_sensitive(false);
    m_password_entry.set_sensitive(false);
    m_email_entry.set_sensitive(false);
    m_website_entry.set_sensitive(false);
    m_notes_view.set_sensitive(false);
    m_tags_entry.set_sensitive(false);
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

    // Update tags display
    update_tags_display();

    // Enable fields for editing
    m_account_name_entry.set_sensitive(true);
    m_user_name_entry.set_sensitive(true);
    m_password_entry.set_sensitive(true);
    m_email_entry.set_sensitive(true);
    m_website_entry.set_sensitive(true);
    m_notes_view.set_sensitive(true);
    m_tags_entry.set_sensitive(true);
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
    const std::string old_password = account->password();
    const std::string new_password = m_password_entry.get_text().raw();

    // Check password history settings
    auto settings = Gio::Settings::create("com.tjdeveng.keeptower");
    const bool history_enabled = SettingsValidator::is_password_history_enabled(settings);
    const int history_limit = SettingsValidator::get_password_history_limit(settings);

    // Check if password changed and prevent reuse
    if (new_password != old_password && history_enabled) {
        // Check against previous passwords to prevent reuse
        for (int i = 0; i < account->password_history_size(); ++i) {
            if (account->password_history(i) == new_password) {
                show_error_dialog("Password reuse detected!\n\n"
                    "This password was used previously. Please choose a different password.\n\n"
                    "Using unique passwords for each change improves security.");
                return false;
            }
        }

        // Add old password to history if it's not empty
        if (!old_password.empty()) {
            account->add_password_history(old_password);

            // Enforce history limit - remove oldest entries if exceeded
            while (account->password_history_size() > history_limit) {
                // Remove the first (oldest) entry
                account->mutable_password_history()->erase(
                    account->mutable_password_history()->begin()
                );
            }
        }

        // Update password_changed_at timestamp when password changes
        account->set_password_changed_at(std::time(nullptr));
    }

    // Update the account with current field values
    account->set_account_name(m_account_name_entry.get_text().raw());
    account->set_user_name(m_user_name_entry.get_text().raw());
    account->set_password(new_password);
    account->set_email(m_email_entry.get_text().raw());
    account->set_website(m_website_entry.get_text().raw());
    account->set_notes(notes_text.raw());

    // Update tags
    account->clear_tags();
    auto current_tags = get_current_tags();
    for (const auto& tag : current_tags) {
        account->add_tags(tag);
    }

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
    auto* dialog = Gtk::make_managed<Gtk::MessageDialog>(*this, "Validation Error", false, Gtk::MessageType::ERROR, Gtk::ButtonsType::OK, true);
    dialog->set_secondary_text(message);
    dialog->set_modal(true);
    dialog->set_hide_on_close(true);

    dialog->signal_response().connect([dialog](int) {
        dialog->hide();
    });

    dialog->show();
}

void MainWindow::on_tags_entry_activate() {
    Glib::ustring tag_text = m_tags_entry.get_text();

    // Trim whitespace
    size_t start = tag_text.find_first_not_of(" \t\n\r");
    size_t end = tag_text.find_last_not_of(" \t\n\r");
    if (start == Glib::ustring::npos) {
        return;  // Empty or only whitespace
    }
    tag_text = tag_text.substr(start, end - start + 1);

    if (tag_text.empty()) {
        return;
    }

    // Validate tag (no commas, max 50 chars)
    if (tag_text.find(',') != Glib::ustring::npos) {
        show_error_dialog("Tags cannot contain commas.");
        return;
    }

    if (tag_text.length() > 50) {
        show_error_dialog("Tag is too long (maximum 50 characters).");
        return;
    }

    // Check for duplicates
    auto current_tags = get_current_tags();
    std::string tag_str = tag_text.raw();
    if (std::find(current_tags.begin(), current_tags.end(), tag_str) != current_tags.end()) {
        m_tags_entry.set_text("");
        return;  // Tag already exists, silently ignore
    }

    // Add the tag chip
    add_tag_chip(tag_str);

    // Clear the entry
    m_tags_entry.set_text("");

    // Mark vault as modified
    if (m_vault_manager && m_selected_account_index >= 0) {
        save_current_account();
    }
}

void MainWindow::add_tag_chip(const std::string& tag) {
    // Create a box for the tag chip
    auto chip_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 4);
    chip_box->set_margin_start(4);
    chip_box->set_margin_end(4);
    chip_box->set_margin_top(4);
    chip_box->set_margin_bottom(4);
    chip_box->add_css_class("tag-chip");

    // Add tag label
    auto label = Gtk::make_managed<Gtk::Label>(tag);
    label->set_margin_start(8);
    chip_box->append(*label);

    // Add remove button
    auto remove_button = Gtk::make_managed<Gtk::Button>();
    remove_button->set_icon_name("window-close-symbolic");
    remove_button->add_css_class("flat");
    remove_button->add_css_class("circular");
    remove_button->set_margin_end(4);
    remove_button->set_tooltip_text("Remove tag");

    // Connect remove button signal - capture tag by value
    std::string tag_copy = tag;
    remove_button->signal_clicked().connect([this, tag_copy]() {
        remove_tag_chip(tag_copy);
    });

    chip_box->append(*remove_button);

    // Add to flowbox
    m_tags_flowbox.append(*chip_box);
}

void MainWindow::remove_tag_chip(const std::string& tag) {
    // Find and remove the chip with matching tag from flowbox
    auto child = m_tags_flowbox.get_first_child();
    while (child) {
        auto next = child->get_next_sibling();

        // FlowBox wraps our widgets in FlowBoxChild, so we need to get the actual child
        Gtk::Widget* actual_child = nullptr;
        if (auto* flowbox_child = dynamic_cast<Gtk::FlowBoxChild*>(child)) {
            actual_child = flowbox_child->get_child();
        } else {
            actual_child = child;
        }

        // Each child should be a Box containing a Label and Button
        if (actual_child) {
            if (auto* box = dynamic_cast<Gtk::Box*>(actual_child)) {
                // Get the first child which should be the label
                if (auto* label = dynamic_cast<Gtk::Label*>(box->get_first_child())) {
                    if (label->get_text().raw() == tag) {
                        m_tags_flowbox.remove(*child);
                        break;
                    }
                }
            }
        }
        child = next;
    }
    // Save the changes
    if (m_vault_manager && m_selected_account_index >= 0) {
        save_current_account();
    }
}

void MainWindow::update_tags_display() {
    // Clear existing tags
    auto child = m_tags_flowbox.get_first_child();
    while (child) {
        auto next = child->get_next_sibling();
        m_tags_flowbox.remove(*child);
        child = next;
    }

    // Load tags from current account
    if (m_vault_manager && m_selected_account_index >= 0) {
        const auto* account = m_vault_manager->get_account(m_selected_account_index);
        if (account) {
            for (int i = 0; i < account->tags_size(); ++i) {
                add_tag_chip(account->tags(i));
            }
        }
    }
}

std::vector<std::string> MainWindow::get_current_tags() {
    std::vector<std::string> tags;

    // Iterate through flowbox children
    // Note: FlowBox wraps children in FlowBoxChild widgets
    auto child = m_tags_flowbox.get_first_child();
    while (child) {
        // FlowBox wraps our widgets in FlowBoxChild, so we need to get the actual child
        Gtk::Widget* actual_child = nullptr;
        if (auto* flowbox_child = dynamic_cast<Gtk::FlowBoxChild*>(child)) {
            actual_child = flowbox_child->get_child();
        } else {
            actual_child = child;
        }

        // Each child should be a Box containing a Label and Button
        if (actual_child) {
            if (auto* box = dynamic_cast<Gtk::Box*>(actual_child)) {
                // Get the first child which should be the label
                if (auto* label = dynamic_cast<Gtk::Label*>(box->get_first_child())) {
                    tags.push_back(label->get_text().raw());
                }
            }
        }
        child = child->get_next_sibling();
    }

    return tags;
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

void MainWindow::on_import_from_csv() {
    if (!m_vault_open) {
        show_error_dialog("Please open a vault first before importing accounts.");
        return;
    }

    // Create file chooser dialog
    auto dialog = Gtk::make_managed<Gtk::FileChooserDialog>(*this, "Import Accounts", Gtk::FileChooser::Action::OPEN);
    dialog->set_modal(true);
    dialog->set_hide_on_close(true);

    dialog->add_button("_Cancel", Gtk::ResponseType::CANCEL);
    dialog->add_button("_Import", Gtk::ResponseType::OK);

    // Add file filters for supported formats
    auto csv_filter = Gtk::FileFilter::create();
    csv_filter->set_name("CSV files (*.csv)");
    csv_filter->add_pattern("*.csv");
    dialog->add_filter(csv_filter);

    auto keepass_filter = Gtk::FileFilter::create();
    keepass_filter->set_name("KeePass XML (*.xml)");
    keepass_filter->add_pattern("*.xml");
    dialog->add_filter(keepass_filter);

    auto onepassword_filter = Gtk::FileFilter::create();
    onepassword_filter->set_name("1Password 1PIF (*.1pif)");
    onepassword_filter->add_pattern("*.1pif");
    dialog->add_filter(onepassword_filter);

    // Add all files filter
    auto all_filter = Gtk::FileFilter::create();
    all_filter->set_name("All files");
    all_filter->add_pattern("*");
    dialog->add_filter(all_filter);

    dialog->signal_response().connect([this, dialog](int response) {
        if (response == Gtk::ResponseType::OK) {
            auto file = dialog->get_file();
            if (!file) {
                dialog->hide();
                return;
            }

            std::string path = file->get_path();

            // Detect format from file extension and perform the import
            std::expected<std::vector<keeptower::AccountRecord>, ImportExport::ImportError> result;
            std::string format_name;

            if (path.ends_with(".xml")) {
                result = ImportExport::import_from_keepass_xml(path);
                format_name = "KeePass XML";
            } else if (path.ends_with(".1pif")) {
                result = ImportExport::import_from_1password(path);
                format_name = "1Password 1PIF";
            } else {
                // Default to CSV
                result = ImportExport::import_from_csv(path);
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
                update_account_list();
                filter_accounts(m_search_entry.get_text());

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

                auto result_dialog = Gtk::make_managed<Gtk::MessageDialog>(
                    *this,
                    failed_count == 0 ? "Import Successful" : "Import Completed with Issues",
                    false,
                    msg_type,
                    Gtk::ButtonsType::OK,
                    true
                );
                result_dialog->set_modal(true);
                result_dialog->set_hide_on_close(true);
                result_dialog->set_secondary_text(message);
                result_dialog->signal_response().connect([result_dialog](int) {
                    result_dialog->hide();
                });
                result_dialog->show();
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
                show_error_dialog(std::format("Import failed: {}", error_msg));
            }
        }
        dialog->hide();
    });

    dialog->show();
}

void MainWindow::on_export_to_csv() {
    if (!m_vault_open) {
        show_error_dialog("Please open a vault first before exporting accounts.");
        return;
    }

    // Security warning dialog (Step 1)
    auto warning_dialog = Gtk::make_managed<Gtk::MessageDialog>(
        *this,
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

    warning_dialog->signal_response().connect([this, warning_dialog](int response) {
        try {
            warning_dialog->hide();

            if (response != Gtk::ResponseType::OK) {
                return;
            }

            // Schedule password dialog via idle callback (flat chain)
            Glib::signal_idle().connect_once([this]() {
                show_export_password_dialog();
            });
        } catch (const std::exception& e) {
            KeepTower::Log::error("Exception in export warning handler: {}", e.what());
            show_error_dialog(std::format("Export failed: {}", e.what()));
        } catch (...) {
            KeepTower::Log::error("Unknown exception in export warning handler");
            show_error_dialog("Export failed due to unknown error");
        }
    });

    warning_dialog->show();
}

void MainWindow::show_export_password_dialog() {
    try {
        // Step 2: Show password dialog (warning dialog is now fully closed)
        auto* password_dialog = Gtk::make_managed<PasswordDialog>(*this);
        password_dialog->set_title("Authenticate to Export");
        password_dialog->set_modal(true);
        password_dialog->set_hide_on_close(true);

        password_dialog->signal_response().connect([this, password_dialog](int response) {
            if (response != Gtk::ResponseType::OK) {
                password_dialog->hide();
                return;
            }

            try {
                std::string password = password_dialog->get_password();

#ifdef HAVE_YUBIKEY_SUPPORT
                // If vault requires YubiKey, show touch prompt and do authentication synchronously
                YubiKeyPromptDialog* touch_dialog = nullptr;
                if (m_vault_manager && m_vault_manager->is_using_yubikey()) {
                    // Get YubiKey serial
                    YubiKeyManager yk_manager;
                    if (!yk_manager.initialize() || !yk_manager.is_yubikey_present()) {
                        password_dialog->hide();
                        show_error_dialog("YubiKey not detected.");
                        return;
                    }

                    auto device_info = yk_manager.get_device_info();
                    if (!device_info) {
                        password_dialog->hide();
                        show_error_dialog("Failed to get YubiKey information.");
                        return;
                    }

                    std::string serial_number = device_info->serial_number;

                    // Hide password dialog to show touch prompt
                    password_dialog->hide();

                    // Show touch prompt dialog
                    touch_dialog = Gtk::make_managed<YubiKeyPromptDialog>(*this,
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
                        // Securely clear password before returning
                        if (!password.empty()) {
                            volatile char* p = const_cast<char*>(password.data());
                            for (size_t i = 0; i < password.size(); i++) {
                                p[i] = '\0';
                            }
                            password.clear();
                        }
                        show_error_dialog("YubiKey authentication failed. Export cancelled.");
                        return;
                    }
                } else
#endif
                {
                    // No YubiKey - just verify password
                    password_dialog->hide();

                    bool auth_success = m_vault_manager->verify_credentials(password);

                    if (!auth_success) {
                        // Securely clear password before returning
                        if (!password.empty()) {
                            volatile char* p = const_cast<char*>(password.data());
                            for (size_t i = 0; i < password.size(); i++) {
                                p[i] = '\0';
                            }
                            password.clear();
                        }
                        show_error_dialog("Authentication failed. Export cancelled.");
                        return;
                    }
                }

                // Securely clear password after successful authentication
                if (!password.empty()) {
                    volatile char* p = const_cast<char*>(password.data());
                    for (size_t i = 0; i < password.size(); i++) {
                        p[i] = '\0';
                    }
                    password.clear();
                }

                // Authentication successful - show file chooser
                show_export_file_chooser();

            } catch (const std::exception& e) {
                KeepTower::Log::error("Exception in password dialog handler: {}", e.what());
                show_error_dialog(std::format("Authentication failed: {}", e.what()));
                password_dialog->hide();
            } catch (...) {
                KeepTower::Log::error("Unknown exception in password dialog handler");
                show_error_dialog("Authentication failed due to unknown error");
                password_dialog->hide();
            }
        });

        password_dialog->show();
    } catch (const std::exception& e) {
        KeepTower::Log::error("Exception showing password dialog: {}", e.what());
        show_error_dialog(std::format("Failed to show authentication dialog: {}", e.what()));
    }
}

void MainWindow::show_export_file_chooser() {
    try {
        // Validate state before proceeding
        if (!m_vault_open || !m_vault_manager) {
            show_error_dialog("Export cancelled: vault is not open");
            return;
        }

        // Ensure we're in main GTK thread and event loop is ready
        auto context = Glib::MainContext::get_default();
        if (!context) {
            KeepTower::Log::error("No GTK main context available");
            show_error_dialog("Internal error: GTK context unavailable");
            return;
        }

        // Process any pending events before showing new dialog
        while (context->pending()) {
            context->iteration(false);
        }

        // Show file chooser for export location (all previous dialogs are now closed)
        KeepTower::Log::info("Creating file chooser dialog");
        auto dialog = Gtk::make_managed<Gtk::FileChooserDialog>(*this, "Export Accounts", Gtk::FileChooser::Action::SAVE);
        KeepTower::Log::info("File chooser dialog created successfully");
        dialog->set_modal(true);
        dialog->set_hide_on_close(true);

        dialog->add_button("_Cancel", Gtk::ResponseType::CANCEL);
        dialog->add_button("_Export", Gtk::ResponseType::OK);

        // Set default filename
        dialog->set_current_name("passwords_export.csv");

        // Add file filters for different formats
        auto csv_filter = Gtk::FileFilter::create();
        csv_filter->set_name("CSV files (*.csv)");
        csv_filter->add_pattern("*.csv");
        dialog->add_filter(csv_filter);

        auto keepass_filter = Gtk::FileFilter::create();
        keepass_filter->set_name("KeePass XML (*.xml) - Not fully tested");
        keepass_filter->add_pattern("*.xml");
        dialog->add_filter(keepass_filter);

        auto onepassword_filter = Gtk::FileFilter::create();
        onepassword_filter->set_name("1Password 1PIF (*.1pif) - Not fully tested");
        onepassword_filter->add_pattern("*.1pif");
        dialog->add_filter(onepassword_filter);

        auto all_filter = Gtk::FileFilter::create();
        all_filter->set_name("All files");
        all_filter->add_pattern("*");
        dialog->add_filter(all_filter);

        // Update file extension when filter changes
        dialog->property_filter().signal_changed().connect([dialog, csv_filter, keepass_filter, onepassword_filter]() {
            auto current_filter = dialog->get_filter();
            std::string current_name = dialog->get_current_name();

            // Remove existing extension
            size_t dot_pos = current_name.find_last_of('.');
            std::string base_name = (dot_pos != std::string::npos) ? current_name.substr(0, dot_pos) : current_name;

            // Add appropriate extension based on filter
            if (current_filter == csv_filter) {
                dialog->set_current_name(base_name + ".csv");
            } else if (current_filter == keepass_filter) {
                dialog->set_current_name(base_name + ".xml");
            } else if (current_filter == onepassword_filter) {
                dialog->set_current_name(base_name + ".1pif");
            }
        });

        dialog->signal_response().connect([this, dialog](int response) {
            try {
                if (response == Gtk::ResponseType::OK) {
                    auto file = dialog->get_file();
                    if (!file) {
                        dialog->hide();
                        return;
                    }

                    std::string path = file->get_path();

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
                        auto success_dialog = Gtk::make_managed<Gtk::MessageDialog>(
                            *this,
                            "Export Successful",
                            false,
                            Gtk::MessageType::INFO,
                            Gtk::ButtonsType::OK,
                            true
                        );
                        success_dialog->set_modal(true);
                        success_dialog->set_hide_on_close(true);
                        success_dialog->set_secondary_text(
                            std::format("Successfully exported {} account(s) to {} format:\n{}\n\n{}",
                                       accounts.size(), format_name, path, warning_text)
                        );
                        success_dialog->signal_response().connect([success_dialog](int) {
                            success_dialog->hide();
                        });
                        success_dialog->show();
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
                        show_error_dialog(std::format("Export failed: {}", error_msg));
                    }
                }
                dialog->hide();
            } catch (const std::exception& e) {
                KeepTower::Log::error("Exception in file chooser handler: {}", e.what());
                show_error_dialog(std::format("Export failed: {}", e.what()));
                dialog->hide();
            } catch (...) {
                KeepTower::Log::error("Unknown exception in file chooser handler");
                show_error_dialog("Export failed due to unknown error");
                dialog->hide();
            }
        });

        dialog->show();
    } catch (const std::exception& e) {
        KeepTower::Log::error("Exception showing file chooser: {}", e.what());
        show_error_dialog(std::format("Failed to show file chooser: {}", e.what()));
    }
}

void MainWindow::on_generate_password() {
    if (!m_vault_open || m_selected_account_index < 0) {
        return;
    }

    // Create password generator options dialog
    auto* dialog = Gtk::make_managed<Gtk::Dialog>("Generate Password", *this, true);
    dialog->add_button("_Cancel", Gtk::ResponseType::CANCEL);
    dialog->add_button("_Generate", Gtk::ResponseType::OK);
    dialog->set_default_response(Gtk::ResponseType::OK);

    auto* box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 12);
    box->set_margin(24);

    // Password length selector
    auto* length_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 12);
    auto* length_label = Gtk::make_managed<Gtk::Label>("Password Length:");
    length_label->set_xalign(0.0);
    auto* length_spin = Gtk::make_managed<Gtk::SpinButton>();
    length_spin->set_range(8, 64);
    length_spin->set_increments(1, 5);
    length_spin->set_value(20);
    length_spin->set_hexpand(true);
    length_box->append(*length_label);
    length_box->append(*length_spin);

    // Character type options
    auto* uppercase_check = Gtk::make_managed<Gtk::CheckButton>("Include Uppercase (A-Z)");
    uppercase_check->set_active(true);
    auto* lowercase_check = Gtk::make_managed<Gtk::CheckButton>("Include Lowercase (a-z)");
    lowercase_check->set_active(true);
    auto* digits_check = Gtk::make_managed<Gtk::CheckButton>("Include Digits (2-9)");
    digits_check->set_active(true);
    auto* symbols_check = Gtk::make_managed<Gtk::CheckButton>("Include Symbols (!@#$%...)");
    symbols_check->set_active(true);
    auto* ambiguous_check = Gtk::make_managed<Gtk::CheckButton>("Exclude ambiguous (0/O, 1/l/I)");
    ambiguous_check->set_active(true);

    box->append(*length_box);
    box->append(*uppercase_check);
    box->append(*lowercase_check);
    box->append(*digits_check);
    box->append(*symbols_check);
    box->append(*ambiguous_check);

    dialog->get_content_area()->append(*box);

    dialog->signal_response().connect([this, dialog, length_spin, uppercase_check, lowercase_check,
                                       digits_check, symbols_check, ambiguous_check](int response) {
        if (response == Gtk::ResponseType::OK) {
            const int length = static_cast<int>(length_spin->get_value());

            // Build charset based on options
            std::string charset;
            if (lowercase_check->get_active()) {
                charset += ambiguous_check->get_active() ? "abcdefghjkmnpqrstuvwxyz" : "abcdefghijklmnopqrstuvwxyz";
            }
            if (uppercase_check->get_active()) {
                charset += ambiguous_check->get_active() ? "ABCDEFGHJKMNPQRSTUVWXYZ" : "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
            }
            if (digits_check->get_active()) {
                charset += ambiguous_check->get_active() ? "23456789" : "0123456789";
            }
            if (symbols_check->get_active()) {
                charset += "!@#$%^&*()-_=+[]{}|;:,.<>?";
            }

            if (charset.empty()) {
                show_error_dialog("Please select at least one character type.");
                dialog->hide();
                return;
            }

            // Generate password
            std::random_device rd;
            if (rd.entropy() == 0.0) {
                g_warning("std::random_device has zero entropy, password may be less secure");
            }

            std::mt19937 gen(rd());
            std::uniform_int_distribution<std::size_t> dis(0, charset.size() - 1);

            std::string password;
            password.reserve(length);

            for (int i = 0; i < length; ++i) {
                password += charset[dis(gen)];
            }

            m_password_entry.set_text(password);
            m_status_label.set_text(std::format("Generated {}-character password", length));
        }
        dialog->hide();
    });

    dialog->show();
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

#ifdef HAVE_YUBIKEY_SUPPORT
void MainWindow::on_test_yubikey() {
    using namespace KeepTower::Log;

    info("Testing YubiKey detection...");

    YubiKeyManager yk_manager{};

    // Initialize YubiKey subsystem
    if (!yk_manager.initialize()) {
        auto dialog = Gtk::AlertDialog::create("YubiKey Initialization Failed");
        dialog->set_detail("Could not initialize YubiKey subsystem. Make sure the required libraries are installed.");
        dialog->set_buttons({"OK"});
        dialog->choose(*this, {});
        error("YubiKey initialization failed");
        return;
    }

    // Test challenge-response functionality FIRST (without calling get_device_info)
    // This avoids any state issues from yk_get_status()
    std::string challenge_result;
    const unsigned char test_challenge[64] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
        0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
        0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
        0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
        0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40
    };

    auto challenge_resp = yk_manager.challenge_response(
        std::span<const unsigned char>(test_challenge, 64),
        true,  // require_touch
        15000  // 15 second timeout
    );

    if (challenge_resp.success) {
        // Now get device info for display
        auto device_info = yk_manager.get_device_info();

        const std::string message = device_info ?
            std::format(
                "YubiKey Test Results\n\n"
                "Serial Number: {}\n"
                "Firmware Version: {}\n"
                "Slot 2 Configured: Yes\n\n"
                "✓ Challenge-Response Working\n"
                "HMAC-SHA1 response received successfully!",
                device_info->serial_number,
                device_info->version_string()
            ) :
            "✓ Challenge-Response Working\nHMAC-SHA1 response received successfully!";

        auto dialog = Gtk::AlertDialog::create("YubiKey Test Passed");
        dialog->set_detail(message);
        dialog->set_buttons({"OK"});
        dialog->choose(*this, {});
        info("YubiKey test passed");
    } else {
        challenge_result = std::format("✗ Challenge-Response Failed\n{}",
                                      challenge_resp.error_message);

        auto dialog = Gtk::AlertDialog::create("YubiKey Test Failed");
        dialog->set_detail(challenge_result);
        dialog->set_buttons({"OK"});
        dialog->choose(*this, {});
        warning("YubiKey challenge-response failed: {}", challenge_resp.error_message);
    }
}

void MainWindow::on_manage_yubikeys() {
    using namespace KeepTower::Log;

    // Check if vault is open and YubiKey-protected
    if (!m_vault_open) {
        auto dialog = Gtk::AlertDialog::create("No Vault Open");
        dialog->set_detail("Please open a vault first.");
        dialog->set_buttons({"OK"});
        dialog->choose(*this, {});
        return;
    }

    auto keys = m_vault_manager->get_yubikey_list();
    if (keys.empty()) {
        auto dialog = Gtk::AlertDialog::create("Vault Not YubiKey-Protected");
        dialog->set_detail("This vault does not use YubiKey authentication.");
        dialog->set_buttons({"OK"});
        dialog->choose(*this, {});
        return;
    }

    // Show YubiKey manager dialog (heap-allocated so it persists)
    auto* dialog = Gtk::make_managed<YubiKeyManagerDialog>(*this, m_vault_manager.get());
    dialog->show();
}
#endif
