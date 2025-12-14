// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "MainWindow.h"
#include "../dialogs/CreatePasswordDialog.h"
#include "../dialogs/PasswordDialog.h"
#include "../dialogs/PreferencesDialog.h"
#include "../dialogs/YubiKeyPromptDialog.h"
#include "../../core/VaultError.h"
#include "../../core/commands/AccountCommands.h"
#include "../../utils/SettingsValidator.h"
#include "../../utils/ImportExport.h"
#include "../../utils/helpers/FuzzyMatch.h"
#include "../../utils/Log.h"
#include "config.h"
#include <cstring>  // For memset
#ifdef HAVE_YUBIKEY_SUPPORT
#include "../../core/YubiKeyManager.h"
#include "../dialogs/YubiKeyManagerDialog.h"
#endif
#include "record.pb.h"
#include <regex>
#include <set>
#include <algorithm>
#include <ctime>
#include <format>
#include <random>

MainWindow::MainWindow()
    : m_main_box(Gtk::Orientation::VERTICAL, 0),
      m_search_box(Gtk::Orientation::HORIZONTAL, 12),
      m_paned(Gtk::Orientation::HORIZONTAL),
      m_details_box(Gtk::Orientation::VERTICAL, 12),
      m_details_paned(Gtk::Orientation::HORIZONTAL),
      m_details_fields_box(Gtk::Orientation::VERTICAL, 12),
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
    add_action("undo", sigc::mem_fun(*this, &MainWindow::on_undo));
    add_action("redo", sigc::mem_fun(*this, &MainWindow::on_redo));
#ifdef HAVE_YUBIKEY_SUPPORT
    add_action("test-yubikey", sigc::mem_fun(*this, &MainWindow::on_test_yubikey));
    add_action("manage-yubikeys", sigc::mem_fun(*this, &MainWindow::on_manage_yubikeys));
#endif

    // Set keyboard shortcuts
    auto app = get_application();
    if (app) {
        app->set_accel_for_action("win.preferences", "<Ctrl>comma");
        app->set_accel_for_action("win.undo", "<Ctrl>z");
        app->set_accel_for_action("win.redo", "<Ctrl><Shift>z");
    }

    // Setup undo/redo state change callback
    m_undo_manager.set_state_changed_callback([this](bool can_undo, bool can_redo) {
        update_undo_redo_sensitivity(can_undo, can_redo);
    });

    // Load undo/redo settings and apply to UndoManager
    bool undo_redo_enabled = settings->get_boolean("undo-redo-enabled");
    int undo_history_limit = settings->get_int("undo-history-limit");
    undo_history_limit = std::clamp(undo_history_limit, 1, 100);
    m_undo_manager.set_max_history(undo_history_limit);

    // Update action sensitivity based on preference
    if (!undo_redo_enabled) {
        m_undo_manager.clear();
        update_undo_redo_sensitivity(false, false);
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

    // Edit section
    auto edit_section = Gio::Menu::create();
    edit_section->append("_Undo", "win.undo");
    edit_section->append("_Redo", "win.redo");
    m_primary_menu->append_section(edit_section);

    // Actions section
    auto actions_section = Gio::Menu::create();
    actions_section->append("_Preferences", "win.preferences");
    actions_section->append("_Import Accounts...", "win.import-csv");
    actions_section->append("_Export Accounts...", "win.export-csv");
#ifdef HAVE_YUBIKEY_SUPPORT
    actions_section->append("Manage _YubiKeys", "win.manage-yubikeys");
    actions_section->append("Test _YubiKey", "win.test-yubikey");
#endif
    m_primary_menu->append_section(actions_section);

    // Help section
    auto help_section = Gio::Menu::create();
    help_section->append("_Keyboard Shortcuts", "win.show-help-overlay");
    help_section->append("_About KeepTower", "app.about");
    m_primary_menu->append_section(help_section);

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

    // Setup field filter dropdown
    m_field_filter_model = Gtk::StringList::create({"All Fields", "Account Name", "Username", "Email", "Website", "Notes", "Tags"});
    m_field_filter_dropdown.set_model(m_field_filter_model);
    m_field_filter_dropdown.set_selected(0);  // Default to "All Fields"
    m_field_filter_dropdown.set_tooltip_text("Search in specific field");
    m_field_filter_dropdown.set_margin_start(6);
    m_search_box.append(m_field_filter_dropdown);

    // Setup tag filter dropdown
    m_tag_filter_model = Gtk::StringList::create({"All tags"});
    m_tag_filter_dropdown.set_model(m_tag_filter_model);
    m_tag_filter_dropdown.set_selected(0);
    m_tag_filter_dropdown.set_tooltip_text("Filter by tag");
    m_tag_filter_dropdown.set_margin_start(6);
    m_search_box.append(m_tag_filter_dropdown);

    m_main_box.append(m_search_box);

    // Setup split pane with resizable sections
    m_paned.set_vexpand(true);
    m_paned.set_wide_handle(true);  // Make drag handle more visible
    constexpr int account_list_width = UI::ACCOUNT_LIST_WIDTH;
    m_paned.set_position(account_list_width);  // Initial left panel width
    m_paned.set_resize_start_child(false);  // Left side (list) doesn't resize with window
    m_paned.set_resize_end_child(true);  // Right side (details) resizes with window
    m_paned.set_shrink_start_child(false);  // Don't allow left side to shrink below min
    m_paned.set_shrink_end_child(false);  // Don't allow right side to shrink below min

    // Setup account list (left side)
    m_account_list_store = Gtk::ListStore::create(m_columns);
    m_account_tree_view.set_model(m_account_list_store);

    // Add star column for favorites (clickable) - using GTK symbolic icons
    auto* star_renderer = Gtk::make_managed<Gtk::CellRendererPixbuf>();
    int star_col_num = m_account_tree_view.append_column("", *star_renderer);
    if (auto* column = m_account_tree_view.get_column(star_col_num - 1)) {
        column->set_cell_data_func(*star_renderer, [](Gtk::CellRenderer* cell, const Gtk::TreeModel::const_iterator& iter) {
            if (auto* pixbuf_cell = dynamic_cast<Gtk::CellRendererPixbuf*>(cell)) {
                bool is_favorite = false;
                iter->get_value(0, is_favorite);
                pixbuf_cell->property_icon_name() = is_favorite ? "starred-symbolic" : "non-starred-symbolic";
            }
        });
        column->set_fixed_width(32);
        column->set_sizing(Gtk::TreeViewColumn::Sizing::FIXED);
    }

    m_account_tree_view.append_column("Account", m_columns.m_col_account_name);
    m_account_tree_view.append_column("Username", m_columns.m_col_user_name);

    // Setup drag-and-drop for account reordering
    setup_drag_and_drop();

    m_list_scrolled.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    m_list_scrolled.set_child(m_account_tree_view);
    m_paned.set_start_child(m_list_scrolled);

    // Setup account details (right side)
    m_details_box.set_margin_start(18);
    m_details_box.set_margin_end(18);
    m_details_box.set_margin_top(18);
    m_details_box.set_margin_bottom(18);

    // Left side: Input fields
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

    // Build left side fields box
    m_details_fields_box.append(m_account_name_label);
    m_details_fields_box.append(m_account_name_entry);
    m_details_fields_box.append(m_user_name_label);
    m_details_fields_box.append(m_user_name_entry);
    m_details_fields_box.append(m_password_label);
    m_details_fields_box.append(*password_box);
    m_details_fields_box.append(m_email_label);
    m_details_fields_box.append(m_email_entry);
    m_details_fields_box.append(m_website_label);
    m_details_fields_box.append(m_website_entry);
    m_details_fields_box.append(m_tags_label);
    m_details_fields_box.append(m_tags_entry);
    m_details_fields_box.append(m_tags_scrolled);

    // Right side: Notes (with label above)
    auto* notes_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 0);
    m_notes_label.set_xalign(0.0);
    m_notes_label.set_margin_bottom(6);
    m_notes_scrolled.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    m_notes_scrolled.set_vexpand(true);
    m_notes_scrolled.set_hexpand(true);
    m_notes_scrolled.set_child(m_notes_view);
    notes_box->append(m_notes_label);
    notes_box->append(m_notes_scrolled);

    // Configure horizontal resizable split: fields on left, notes on right
    m_details_paned.set_wide_handle(true);  // Make drag handle more visible
    m_details_paned.set_position(400);  // Initial split position (adjust as needed)
    m_details_paned.set_resize_start_child(false);  // Fields don't resize with window
    m_details_paned.set_resize_end_child(true);  // Notes resize with window
    m_details_paned.set_shrink_start_child(false);  // Don't shrink fields below min
    m_details_paned.set_shrink_end_child(false);  // Don't shrink notes below min
    m_details_paned.set_start_child(m_details_fields_box);
    m_details_paned.set_end_child(*notes_box);

    // Main details box: resizable split + delete button at bottom
    m_details_box.append(m_details_paned);

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
    m_field_filter_dropdown.property_selected().signal_changed().connect(
        sigc::mem_fun(*this, &MainWindow::on_field_filter_changed)
    );
    m_tags_entry.signal_activate().connect(
        sigc::mem_fun(*this, &MainWindow::on_tags_entry_activate)
    );
    m_tag_filter_dropdown.property_selected().signal_changed().connect(
        sigc::mem_fun(*this, &MainWindow::on_tag_filter_changed)
    );

    // Prevent Enter key from propagating to parent and selecting next account
    auto key_controller = Gtk::EventControllerKey::create();
    key_controller->signal_key_pressed().connect([this](guint keyval, guint, Gdk::ModifierType) {
        if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
            return true;  // Stop propagation
        }
        return false;
    }, false);
    m_tags_entry.add_controller(key_controller);

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

    // Setup click handler for star column (favorite toggle)
    auto star_gesture = Gtk::GestureClick::create();
    star_gesture->set_button(GDK_BUTTON_PRIMARY);  // Left-click
    star_gesture->signal_released().connect([this](int /* n_press */, double x, double y) {
        int bin_x, bin_y;
        m_account_tree_view.convert_widget_to_bin_window_coords(
            static_cast<int>(x), static_cast<int>(y), bin_x, bin_y);

        Gtk::TreeModel::Path path;
        Gtk::TreeViewColumn* column = nullptr;
        int cell_x, cell_y;

        if (m_account_tree_view.get_path_at_pos(bin_x, bin_y, path, column, cell_x, cell_y)) {
            // Check if click was in the first column (star column)
            if (column == m_account_tree_view.get_column(0)) {
                on_star_column_clicked(path);
            }
        }
    });
    m_account_tree_view.add_controller(star_gesture);

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
                        update_tag_filter_dropdown();
                        clear_account_details();

                        // Initialize undo/redo state
                        update_undo_redo_sensitivity(false, false);

                        // Initialize undo/redo state
                        update_undo_redo_sensitivity(false, false);

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

                        // Cache password for auto-lock/unlock
                        m_cached_master_password = password.raw();

                        update_account_list();
                        update_tag_filter_dropdown();

                        // Initialize undo/redo state
                        update_undo_redo_sensitivity(false, false);

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

    // Disconnect drag-and-drop signal handlers for memory safety
    if (m_row_inserted_conn.connected()) {
        m_row_inserted_conn.disconnect();
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

    // Clear undo/redo history
    m_undo_manager.clear();

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

    // Create command with UI callback
    auto ui_callback = [this]() {
        // Clear search filter so new account is visible
        m_search_entry.set_text("");

        // Clear selection before updating list to avoid stale index issues
        clear_account_details();

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
                // Select the row
                m_account_tree_view.get_selection()->select(iter);

                // Explicitly display the account details
                display_account_details(new_index);

                // Focus the name field and select all text for easy editing
                Glib::signal_idle().connect_once([this]() {
                    m_account_name_entry.grab_focus();
                    m_account_name_entry.select_region(0, -1);
                });

                m_status_label.set_text("New account added - please enter details");
                break;
            }
        }
    };

    auto command = std::make_unique<AddAccountCommand>(
        m_vault_manager.get(),
        std::move(new_account),
        ui_callback
    );

    // Check if undo/redo is enabled
    if (is_undo_redo_enabled()) {
        if (!m_undo_manager.execute_command(std::move(command))) {
            constexpr std::string_view error_msg{"Failed to add account"};
            m_status_label.set_text(std::string{error_msg});
        }
    } else {
        // Execute directly without undo history
        if (!command->execute()) {
            constexpr std::string_view error_msg{"Failed to add account"};
            m_status_label.set_text(std::string{error_msg});
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

void MainWindow::on_star_column_clicked(const Gtk::TreeModel::Path& path) {
    if (!m_vault_open) {
        return;
    }

    auto iter = m_account_list_store->get_iter(path);
    if (!iter) {
        return;
    }

    int account_index = (*iter)[m_columns.m_col_index];

    // Create command with UI callback
    auto ui_callback = [this, account_index]() {
        // Refresh list with current search/filter (preserves search state)
        int current_selection = m_selected_account_index;
        filter_accounts(m_search_entry.get_text());

        // Re-select the current account if one was selected
        if (current_selection >= 0) {
            m_updating_selection = true;
            auto list_iter = m_account_list_store->children().begin();
            while (list_iter != m_account_list_store->children().end()) {
                if ((*list_iter)[m_columns.m_col_index] == current_selection) {
                    m_account_tree_view.set_cursor(m_account_list_store->get_path(list_iter));
                    break;
                }
                ++list_iter;
            }
            m_updating_selection = false;
        }
    };

    auto command = std::make_unique<ToggleFavoriteCommand>(
        m_vault_manager.get(),
        account_index,
        ui_callback
    );

    // Check if undo/redo is enabled
    if (is_undo_redo_enabled()) {
        (void)m_undo_manager.execute_command(std::move(command));
    } else {
        // Execute directly without undo history
        (void)command->execute();
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

    // Create sorted index vector
    std::vector<std::pair<size_t, bool>> sorted_indices;
    sorted_indices.reserve(accounts.size());

    for (size_t i = 0; i < accounts.size(); i++) {
        sorted_indices.push_back({i, accounts[i].is_favorite()});
    }

    // Check if custom ordering is enabled
    bool has_custom_order = m_vault_manager->has_custom_global_ordering();

    if (has_custom_order) {
        // Sort by global_display_order (custom drag-and-drop order)
        std::sort(sorted_indices.begin(), sorted_indices.end(),
            [&accounts](const auto& a, const auto& b) {
                int32_t order_a = accounts[a.first].global_display_order();
                int32_t order_b = accounts[b.first].global_display_order();

                // If global_display_order is same or invalid, fall back to name
                if (order_a == order_b || order_a < 0 || order_b < 0) {
                    return accounts[a.first].account_name() < accounts[b.first].account_name();
                }
                return order_a < order_b;
            });
    } else {
        // Use automatic sorting: favorites first, then alphabetically
        std::sort(sorted_indices.begin(), sorted_indices.end(),
            [&accounts](const auto& a, const auto& b) {
                if (a.second != b.second) {
                    return a.second > b.second;  // Favorites first
                }
                return accounts[a.first].account_name() < accounts[b.first].account_name();
            });
    }

    // Add sorted accounts to list
    for (const auto& [index, is_favorite] : sorted_indices) {
        auto row = *(m_account_list_store->append());
        row[m_columns.m_col_is_favorite] = is_favorite;
        row[m_columns.m_col_account_name] = accounts[index].account_name();
        row[m_columns.m_col_user_name] = accounts[index].user_name();
        row[m_columns.m_col_index] = index;
    }

    m_status_label.set_text("Vault opened: " + m_current_vault_path +
                           " (" + std::to_string(accounts.size()) + " accounts)");
}

void MainWindow::filter_accounts(const Glib::ustring& search_text) {
    m_account_list_store->clear();
    m_filtered_indices.clear();

    const auto accounts = m_vault_manager->get_all_accounts();

    // If both search and tag filter are empty, show all accounts
    if (search_text.empty() && m_selected_tag_filter.empty()) {
        update_account_list();
        return;
    }

    const bool has_text_filter = !search_text.empty();
    const std::string query = search_text.lowercase().raw();

    // Get selected field filter
    const guint field_filter = m_field_filter_dropdown.get_selected();
    // 0=All, 1=Account Name, 2=Username, 3=Email, 4=Website, 5=Notes, 6=Tags

    // Try regex first (for advanced users), fall back to fuzzy if it fails
    std::regex search_regex;
    bool use_regex = false;
    if (has_text_filter) {
        try {
            std::string pattern = ".*" + query + ".*";
            search_regex = std::regex(pattern, std::regex::icase);
            use_regex = true;
        } catch (const std::regex_error&) {
            // Fall back to fuzzy matching
            use_regex = false;
        }
    }

    // Structure to hold index and match score
    struct FilterResult {
        size_t index;
        int score;
    };
    std::vector<FilterResult> filtered_results;

    // Filter accounts with scoring
    for (size_t i = 0; i < accounts.size(); ++i) {
        const auto& account = accounts[i];

        // Check tag filter first (no scoring needed)
        bool tag_match = m_selected_tag_filter.empty();
        if (!m_selected_tag_filter.empty()) {
            for (int j = 0; j < account.tags_size(); ++j) {
                if (account.tags(j) == m_selected_tag_filter) {
                    tag_match = true;
                    break;
                }
            }
        }

        if (!tag_match) continue;

        // If no text filter, include all tag-matched accounts
        if (!has_text_filter) {
            filtered_results.push_back({i, 100});  // Perfect score when no search
            continue;
        }

        // Calculate match score based on field filter
        int best_score = 0;

        auto check_field = [&](std::string_view field_value, int field_boost = 0) {
            if (use_regex) {
                if (std::regex_search(std::string(field_value), search_regex)) {
                    return 100 + field_boost;  // Regex match gets perfect score
                }
                return 0;
            } else {
                int score = KeepTower::FuzzyMatch::fuzzy_score(query, field_value);
                return score > 0 ? score + field_boost : 0;
            }
        };

        // Check selected field(s) with boost for specific field matches
        if (field_filter == 0 || field_filter == 1) {
            best_score = std::max(best_score, check_field(account.account_name(), 10));
        }
        if (field_filter == 0 || field_filter == 2) {
            best_score = std::max(best_score, check_field(account.user_name(), 5));
        }
        if (field_filter == 0 || field_filter == 3) {
            best_score = std::max(best_score, check_field(account.email(), 5));
        }
        if (field_filter == 0 || field_filter == 4) {
            best_score = std::max(best_score, check_field(account.website(), 5));
        }
        if (field_filter == 0 || field_filter == 5) {
            best_score = std::max(best_score, check_field(account.notes(), 0));
        }
        if (field_filter == 0 || field_filter == 6) {
            for (int j = 0; j < account.tags_size(); ++j) {
                best_score = std::max(best_score, check_field(account.tags(j), 8));
            }
        }

        // Include account if score meets threshold (30)
        if (best_score >= 30) {
            filtered_results.push_back({i, best_score});
        }
    }

    // Sort by: 1) favorites first, 2) score (descending), 3) alphabetically
    std::sort(filtered_results.begin(), filtered_results.end(),
        [&accounts](const FilterResult& a, const FilterResult& b) {
            const bool a_fav = accounts[a.index].is_favorite();
            const bool b_fav = accounts[b.index].is_favorite();

            if (a_fav != b_fav) {
                return a_fav > b_fav;  // Favorites first
            }
            if (a.score != b.score) {
                return a.score > b.score;  // Higher score first
            }
            return accounts[a.index].account_name() < accounts[b.index].account_name();
        });

    // Populate list store and filtered indices
    for (const auto& result : filtered_results) {
        m_filtered_indices.push_back(result.index);
        const auto& account = accounts[result.index];
        auto row = *(m_account_list_store->append());
        row[m_columns.m_col_is_favorite] = account.is_favorite();
        row[m_columns.m_col_account_name] = account.account_name();
        row[m_columns.m_col_user_name] = account.user_name();
        row[m_columns.m_col_index] = result.index;
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

    // Debug: Log what we're saving
    g_debug("Saved account '%s' (index %d): notes length = %zu",
            account->account_name().c_str(), m_selected_account_index, notes_text.length());

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

void MainWindow::update_tag_filter_dropdown() {
    // Get all unique tags from all accounts
    std::set<std::string> all_tags;

    if (m_vault_manager && m_vault_manager->is_vault_open()) {
        const auto accounts = m_vault_manager->get_all_accounts();
        for (const auto& account : accounts) {
            for (int i = 0; i < account.tags_size(); ++i) {
                all_tags.insert(account.tags(i));
            }
        }
    }

    // Rebuild the dropdown model
    m_tag_filter_model = Gtk::StringList::create({});
    m_tag_filter_model->append("All tags");

    for (const auto& tag : all_tags) {
        m_tag_filter_model->append(tag);
    }

    m_tag_filter_dropdown.set_model(m_tag_filter_model);
    m_tag_filter_dropdown.set_selected(0);  // Reset to "All tags"
    m_selected_tag_filter.clear();
}

void MainWindow::on_tag_filter_changed() {
    guint selected = m_tag_filter_dropdown.get_selected();

    if (selected == 0) {
        // "All tags" selected
        m_selected_tag_filter.clear();
    } else {
        // Specific tag selected (index - 1 because "All tags" is at index 0)
        auto item = m_tag_filter_model->get_string(selected);
        m_selected_tag_filter = item.raw();
    }

    // Re-apply current search with tag filter
    filter_accounts(m_search_entry.get_text());
}

void MainWindow::on_field_filter_changed() {
    // Re-apply current search with new field filter
    filter_accounts(m_search_entry.get_text());
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
        update_tag_filter_dropdown();
    }

    // Keep focus on tags entry to prevent selection jump
    m_tags_entry.grab_focus();
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
        update_tag_filter_dropdown();
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
    dialog_ptr->signal_hide().connect([this, dialog_ptr]() noexcept {
        // Reload undo/redo settings when preferences dialog closes
        auto settings = Gio::Settings::create("com.tjdeveng.keeptower");
        bool undo_redo_enabled = settings->get_boolean("undo-redo-enabled");
        int undo_history_limit = settings->get_int("undo-history-limit");
        undo_history_limit = std::clamp(undo_history_limit, 1, 100);

        m_undo_manager.set_max_history(undo_history_limit);

        // Update undo/redo state based on new settings
        if (!undo_redo_enabled) {
            // If disabled, clear history and disable buttons
            m_undo_manager.clear();
            update_undo_redo_sensitivity(false, false);
        } else {
            // If enabled, update buttons based on current state
            update_undo_redo_sensitivity(m_undo_manager.can_undo(), m_undo_manager.can_redo());
        }

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

    // Adjust message based on whether undo/redo is enabled
    Glib::ustring message = "Are you sure you want to delete '" + account_name + "'?";
    if (!is_undo_redo_enabled()) {
        message += "\nThis action cannot be undone.";
    }

    dialog->set_secondary_text(message);

    dialog->add_button("Cancel", Gtk::ResponseType::CANCEL);
    auto delete_button = dialog->add_button("Delete", Gtk::ResponseType::OK);
    delete_button->add_css_class("destructive-action");

    dialog->signal_response().connect([this, dialog](int response) {
        if (response == Gtk::ResponseType::OK) {
            // Create command with UI callback
            auto ui_callback = [this]() {
                clear_account_details();
                update_account_list();
                filter_accounts(m_search_entry.get_text());
            };

            auto command = std::make_unique<DeleteAccountCommand>(
                m_vault_manager.get(),
                m_selected_account_index,
                ui_callback
            );

            // Check if undo/redo is enabled
            if (is_undo_redo_enabled()) {
                if (!m_undo_manager.execute_command(std::move(command))) {
                    show_error_dialog("Failed to delete account");
                }
            } else {
                // Execute directly without undo history
                if (!command->execute()) {
                    show_error_dialog("Failed to delete account");
                }
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

    // Password should already be cached from when vault was opened
    if (m_cached_master_password.empty()) {
        // Can't lock without being able to unlock
        g_warning("Cannot lock vault - master password not cached! This shouldn't happen.");
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
    using namespace std::string_view_literals;
    m_status_label.set_text(std::string{"Vault locked due to inactivity"sv});

    // Hide account details for security (clears fields and sets m_selected_account_index = -1)
    clear_account_details();

    // Clear tree view selection to prevent any stale selection
    m_account_tree_view.get_selection()->unselect_all();

    // Clear and hide account list for security
    m_account_list_store->clear();
    m_filtered_indices.clear();
    m_list_scrolled.set_visible(false);

    // Create unlock dialog using Gtk::Window for full control
    auto* dialog = Gtk::make_managed<Gtk::Window>();
    dialog->set_transient_for(*this);
    dialog->set_modal(true);
    dialog->set_title("Vault Locked - Authentication Required");
    dialog->set_default_size(450, 200);
    dialog->set_resizable(false);

    // Create main layout
    auto* main_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 0);

    // Content area
    auto* content_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 12);
    content_box->set_margin_start(24);
    content_box->set_margin_end(24);
    content_box->set_margin_top(24);
    content_box->set_margin_bottom(24);

    auto* message_label = Gtk::make_managed<Gtk::Label>();
    message_label->set_markup("<b>Your vault has been locked due to inactivity.</b>");
    message_label->set_wrap(true);
    message_label->set_xalign(0.0);
    content_box->append(*message_label);

    auto* instruction_label = Gtk::make_managed<Gtk::Label>("Enter your master password to unlock and continue working.");
    instruction_label->set_wrap(true);
    instruction_label->set_xalign(0.0);
    content_box->append(*instruction_label);

    auto* password_entry = Gtk::make_managed<Gtk::Entry>();
    password_entry->set_visibility(false);
    password_entry->set_placeholder_text("Enter master password to unlock");
    content_box->append(*password_entry);

    main_box->append(*content_box);

    // Button area
    auto* button_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    button_box->set_margin_start(24);
    button_box->set_margin_end(24);
    button_box->set_margin_bottom(24);
    button_box->set_halign(Gtk::Align::END);

    auto* cancel_button = Gtk::make_managed<Gtk::Button>("_Cancel");
    cancel_button->set_use_underline(true);
    button_box->append(*cancel_button);

    auto* ok_button = Gtk::make_managed<Gtk::Button>("_OK");
    ok_button->set_use_underline(true);
    ok_button->add_css_class("suggested-action");
    button_box->append(*ok_button);

    main_box->append(*button_box);
    dialog->set_child(*main_box);

    // Handle OK button
    ok_button->signal_clicked().connect([this, dialog, password_entry]() {
        const std::string entered_password{password_entry->get_text().raw()};

#ifdef HAVE_YUBIKEY_SUPPORT
        // Check if YubiKey is required for this vault
        std::string yubikey_serial;
        bool yubikey_required = m_vault_manager->check_vault_requires_yubikey(m_current_vault_path, yubikey_serial);

        YubiKeyPromptDialog* touch_dialog = nullptr;
        if (yubikey_required) {
            // Show touch prompt dialog
            touch_dialog = Gtk::make_managed<YubiKeyPromptDialog>(*dialog,
                YubiKeyPromptDialog::PromptType::TOUCH);
            touch_dialog->present();

            // Force GTK to process events and render the dialog
            auto context = Glib::MainContext::get_default();
            while (context->pending()) {
                context->iteration(false);
            }
            g_usleep(150000);  // 150ms delay for rendering
        }
#endif

        // Verify password by attempting to open vault
        const auto temp_vault = std::make_unique<VaultManager>();
        const bool success = temp_vault->open_vault(std::string{m_current_vault_path}, entered_password);

#ifdef HAVE_YUBIKEY_SUPPORT
        // Hide touch prompt if it was shown
        if (touch_dialog) {
            touch_dialog->hide();
        }
#endif

        if (success && entered_password == m_cached_master_password) {
            // Unlock successful
            m_is_locked = false;

            // Re-enable UI
            m_add_account_button.set_sensitive(true);
            m_save_button.set_sensitive(true);
            m_search_entry.set_sensitive(true);

            // Show account list again
            m_list_scrolled.set_visible(true);

            // Restore account list and selection
            update_account_list();
            filter_accounts(m_search_entry.get_text());

            // Reset activity monitoring
            on_user_activity();

            using namespace std::string_view_literals;
            m_status_label.set_text(std::string{"Vault unlocked"sv});

            delete dialog;
        } else {
            // Unlock failed - could be wrong password or missing YubiKey
            password_entry->set_text("");
            password_entry->grab_focus();

#ifdef HAVE_YUBIKEY_SUPPORT
            // Provide more specific error message if YubiKey is required
            const char* error_message = "Unlock Failed";
            const char* error_detail;
            if (yubikey_required) {
                error_detail = "Unable to unlock vault. This could be due to:\n"
                              "• Incorrect password\n"
                              "• YubiKey not inserted\n"
                              "• YubiKey not touched in time\n"
                              "• Wrong YubiKey inserted\n\n"
                              "Please verify your password and ensure the correct YubiKey is connected.";
            } else {
                error_detail = "The password you entered is incorrect. Please try again.";
            }
#else
            const char* error_message = "Incorrect Password";
            const char* error_detail = "The password you entered is incorrect. Please try again.";
#endif

            auto* error_dialog = Gtk::make_managed<Gtk::MessageDialog>(
                *dialog,
                error_message,
                false,
                Gtk::MessageType::ERROR,
                Gtk::ButtonsType::OK,
                true
            );
            error_dialog->set_secondary_text(error_detail);
            error_dialog->signal_response().connect([error_dialog, password_entry](int) {
                error_dialog->hide();
                password_entry->grab_focus();
            });
            error_dialog->show();
        }
    });

    // Handle Cancel button
    cancel_button->signal_clicked().connect([this, dialog]() {
        // Save and close application
        if (m_vault_open) {
            save_current_account();
            if (!m_vault_manager->save_vault()) {
                g_warning("Failed to save vault before closing locked application");
            }
        }

        delete dialog;
        close();
    });

    // Handle Enter key in password entry
    password_entry->signal_activate().connect([ok_button]() {
        ok_button->activate();
    });

    // Show the unlock dialog and set focus
    dialog->present();
    password_entry->grab_focus();
}

std::string MainWindow::get_master_password_for_lock() {
    // We need to get the master password to cache it for unlock
    // This is called when locking, so we prompt the user
    auto* dialog = Gtk::make_managed<Gtk::MessageDialog>(
        *this,
        "Verify Password for Auto-Lock",
        false,
        Gtk::MessageType::QUESTION,
        Gtk::ButtonsType::OK_CANCEL
    );
    dialog->set_secondary_text("Enter your master password to verify your identity.\nThis allows the vault to auto-lock after inactivity and be unlocked with the same password.");
    dialog->set_modal(true);
    dialog->set_hide_on_close(true);

    auto* content = dialog->get_message_area();
    auto* password_entry = Gtk::make_managed<Gtk::Entry>();
    password_entry->set_visibility(false);
    password_entry->set_placeholder_text("Enter master password");
    password_entry->set_margin_start(12);
    password_entry->set_margin_end(12);
    password_entry->set_margin_top(12);
    password_entry->set_activates_default(true);
    content->append(*password_entry);

    dialog->set_default_response(Gtk::ResponseType::OK);

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

void MainWindow::on_undo() {
    if (!m_vault_open) {
        return;
    }

    if (m_undo_manager.undo()) {
        const std::string msg = "Undid: " + m_undo_manager.get_redo_description();
        m_status_label.set_text(msg);
    } else {
        m_status_label.set_text("Nothing to undo");
    }
}

void MainWindow::on_redo() {
    if (!m_vault_open) {
        return;
    }

    if (m_undo_manager.redo()) {
        const std::string msg = "Redid: " + m_undo_manager.get_undo_description();
        m_status_label.set_text(msg);
    } else {
        m_status_label.set_text("Nothing to redo");
    }
}

void MainWindow::update_undo_redo_sensitivity(bool can_undo, bool can_redo) {
    // Update action sensitivity
    auto undo_action = std::dynamic_pointer_cast<Gio::SimpleAction>(lookup_action("undo"));
    auto redo_action = std::dynamic_pointer_cast<Gio::SimpleAction>(lookup_action("redo"));

    // Check if undo/redo is enabled in preferences
    bool undo_redo_enabled = is_undo_redo_enabled();

    if (undo_action) {
        bool should_enable = can_undo && m_vault_open && undo_redo_enabled;
        undo_action->set_enabled(should_enable);
    }

    if (redo_action) {
        bool should_enable = can_redo && m_vault_open && undo_redo_enabled;
        redo_action->set_enabled(should_enable);
    }
}

bool MainWindow::is_undo_redo_enabled() const {
    auto settings = Gio::Settings::create("com.tjdeveng.keeptower");
    return settings->get_boolean("undo-redo-enabled");
}

/**
 * @brief Setup drag-and-drop support for account reordering
 *
 * Enables TreeView's built-in reorderable mode for drag-and-drop account
 * reordering. When users drag accounts to new positions, the changes are
 * automatically persisted to the vault via VaultManager::reorder_account().
 *
 * Security considerations:
 * - Reordering is disabled during search/filter to prevent confusion
 * - Only works when vault is open and decrypted
 * - Changes are immediately saved to vault file
 *
 * @note Uses deprecated set_reorderable() API. Future versions should migrate
 *       to ListView/ColumnView as recommended by GTK4.10+
 *
 * @note This method should be called after the TreeView model is set up
 */
void MainWindow::setup_drag_and_drop() {
    // Security: Disable reordering during search/filter to prevent confusion
    // between visual order and actual vault order
    if (!m_search_entry.get_text().empty() || !m_selected_tag_filter.empty()) {
        m_account_tree_view.set_reorderable(false);
        return;
    }

    // Disconnect any existing signal connection to prevent duplicates
    if (m_row_inserted_conn.connected()) {
        m_row_inserted_conn.disconnect();
    }

    // Enable built-in drag-and-drop reordering
    // Note: set_reorderable() is deprecated in GTK4.10+ but remains the most
    // stable approach for TreeView. Future migration to ListView recommended.
    m_account_tree_view.set_reorderable(true);

    // Connect to row-inserted signal to detect when drag-and-drop reordering occurs
    // The TreeModel emits row-inserted after a row is moved via drag-and-drop
    m_row_inserted_conn = m_account_list_store->signal_row_inserted().connect(
        [this](const Gtk::TreeModel::Path& path, const Gtk::TreeModel::iterator& iter) {
            // Ignore insertions during initial list population
            if (!m_vault_open) {
                return;
            }

            // Ignore insertions during search/filter (shouldn't happen, but safety check)
            if (!m_search_entry.get_text().empty() || !m_selected_tag_filter.empty()) {
                return;
            }

            // Get the account index from the iterator
            const int account_index = (*iter)[m_columns.m_col_index];
            const int new_position = path[0];  // Path[0] is the row number

            // Validate indices for security
            const auto account_count = m_vault_manager->get_account_count();
            if (account_index < 0 || static_cast<size_t>(account_index) >= account_count) {
                return;
            }
            if (new_position < 0 || static_cast<size_t>(new_position) >= account_count) {
                return;
            }

            // Persist the reorder to vault
            // Note: This will trigger save_vault() internally
            if (m_vault_manager->reorder_account(
                    static_cast<size_t>(account_index),
                    static_cast<size_t>(new_position))) {
                m_status_label.set_text("Account reordered");

                // TODO: Create ReorderAccountCommand for undo/redo support
                // For now, reordering works but isn't undoable
            }
        }
    );
}
