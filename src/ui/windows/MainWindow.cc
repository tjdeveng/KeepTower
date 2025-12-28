#include <sigc++/signal.h>
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "MainWindow.h"
#include "../widgets/AccountTreeWidget.h"
#include "../widgets/AccountDetailWidget.h"
#include "../dialogs/CreatePasswordDialog.h"
#include "../dialogs/PasswordDialog.h"
#include "../dialogs/V2UserLoginDialog.h"
#include "../dialogs/ChangePasswordDialog.h"
#include "../dialogs/UserManagementDialog.h"
#include "../dialogs/VaultMigrationDialog.h"
#include "../dialogs/PreferencesDialog.h"
#include "../dialogs/GroupCreateDialog.h"
#include "../dialogs/GroupRenameDialog.h"
#include "../dialogs/YubiKeyPromptDialog.h"
#include "../../core/VaultError.h"
#include "../../core/VaultFormatV2.h"
#include "../../core/commands/AccountCommands.h"
#include "../../utils/SettingsValidator.h"
#include "../../utils/ImportExport.h"
#include "../../utils/helpers/FuzzyMatch.h"
#include "../../utils/StringHelpers.h"
#include "../../utils/Log.h"
#include "config.h"
#include <cstring>  // For memset
#include <fstream>   // For reading vault files

using KeepTower::safe_ustring_to_string;
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
            // Try to read system color scheme from GNOME desktop settings
            bool applied = false;
            try {
                m_desktop_settings = Gio::Settings::create("org.gnome.desktop.interface");
                auto system_color_scheme = m_desktop_settings->get_string("color-scheme");
                // color-scheme can be: "default", "prefer-dark", "prefer-light"
                gtk_settings->property_gtk_application_prefer_dark_theme() = (system_color_scheme == "prefer-dark");
                applied = true;

                // Monitor system theme changes
                m_theme_changed_connection = m_desktop_settings->signal_changed("color-scheme").connect(
                    [this, gtk_settings](const Glib::ustring& key) {
                        auto system_color_scheme = m_desktop_settings->get_string("color-scheme");
                        gtk_settings->property_gtk_application_prefer_dark_theme() = (system_color_scheme == "prefer-dark");
                    }
                );
            } catch (...) {
                // Schema not available or error reading it
            }

            if (!applied) {
                // Fallback: Check GTK_THEME environment variable
                const char* gtk_theme = std::getenv("GTK_THEME");
                if (gtk_theme && std::string(gtk_theme).find("dark") != std::string::npos) {
                    gtk_settings->property_gtk_application_prefer_dark_theme() = true;
                } else {
                    // Last resort: assume light theme
                    gtk_settings->property_gtk_application_prefer_dark_theme() = false;
                }
            }
        }
    }

    // Monitor app's color-scheme setting changes (when user changes it in preferences)
    settings->signal_changed("color-scheme").connect([this](const Glib::ustring& key) {
        auto settings = Gio::Settings::create("com.tjdeveng.keeptower");
        Glib::ustring color_scheme = settings->get_string("color-scheme");
        auto gtk_settings = Gtk::Settings::get_default();
        if (!gtk_settings) return;

        // Disconnect any existing system theme monitoring
        if (m_theme_changed_connection) {
            m_theme_changed_connection.disconnect();
        }

        if (color_scheme == "light") {
            gtk_settings->property_gtk_application_prefer_dark_theme() = false;
        } else if (color_scheme == "dark") {
            gtk_settings->property_gtk_application_prefer_dark_theme() = true;
        } else {
            // Default: follow system preference and monitor changes
            try {
                if (!m_desktop_settings) {
                    m_desktop_settings = Gio::Settings::create("org.gnome.desktop.interface");
                }
                auto system_color_scheme = m_desktop_settings->get_string("color-scheme");
                gtk_settings->property_gtk_application_prefer_dark_theme() = (system_color_scheme == "prefer-dark");

                // Re-establish system theme monitoring
                m_theme_changed_connection = m_desktop_settings->signal_changed("color-scheme").connect(
                    [gtk_settings, this](const Glib::ustring&) {
                        auto system_color_scheme = m_desktop_settings->get_string("color-scheme");
                        gtk_settings->property_gtk_application_prefer_dark_theme() = (system_color_scheme == "prefer-dark");
                    }
                );
            } catch (...) {
                // Fallback
                const char* gtk_theme = std::getenv("GTK_THEME");
                if (gtk_theme && std::string(gtk_theme).find("dark") != std::string::npos) {
                    gtk_settings->property_gtk_application_prefer_dark_theme() = true;
                } else {
                    gtk_settings->property_gtk_application_prefer_dark_theme() = false;
                }
            }
        }
    });

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
    m_export_action = add_action("export-csv", sigc::mem_fun(*this, &MainWindow::on_export_to_csv));
    add_action("migrate-v1-to-v2", sigc::mem_fun(*this, &MainWindow::on_migrate_v1_to_v2));
    add_action("delete-account", sigc::mem_fun(*this, &MainWindow::on_delete_account));
    add_action("create-group", sigc::mem_fun(*this, &MainWindow::on_create_group));
    add_action("rename-group", [this]() {
        if (!m_context_menu_group_id.empty() && m_vault_manager) {
            // Find the group name
            auto groups = m_vault_manager->get_all_groups();
            for (const auto& group : groups) {
                if (group.group_id() == m_context_menu_group_id) {
                    on_rename_group(m_context_menu_group_id, group.group_name());
                    break;
                }
            }
        }
    });
    add_action("delete-group", [this]() {
        if (!m_context_menu_group_id.empty()) {
            on_delete_group(m_context_menu_group_id);
        }
    });
    add_action("undo", sigc::mem_fun(*this, &MainWindow::on_undo));
    add_action("redo", sigc::mem_fun(*this, &MainWindow::on_redo));
#ifdef HAVE_YUBIKEY_SUPPORT
    // Use lambdas for all YubiKey actions - better lifetime management with GTK4
    add_action("test-yubikey", [this]() {
        on_test_yubikey();
    });
    add_action("manage-yubikeys", [this]() {
        on_manage_yubikeys();
    });
#endif

    // Phase 4: V2 vault user management actions
    m_change_password_action = add_action("change-password", sigc::mem_fun(*this, &MainWindow::on_change_my_password));
    m_logout_action = add_action("logout", sigc::mem_fun(*this, &MainWindow::on_logout));
    m_manage_users_action = add_action("manage-users", sigc::mem_fun(*this, &MainWindow::on_manage_users));

    // Initially disable V2-only actions (enabled when V2 vault opens)
    m_change_password_action->set_enabled(false);
    m_logout_action->set_enabled(false);
    m_manage_users_action->set_enabled(false);

    // Set keyboard shortcuts
    auto app = get_application();
    if (app) {
        app->set_accel_for_action("win.preferences", "<Ctrl>comma");
        app->set_accel_for_action("win.undo", "<Ctrl>Z");
        app->set_accel_for_action("win.redo", "<Ctrl><Shift>Z");
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

    // Center - Session info label (for V2 multi-user vaults)
    m_session_label.set_visible(false);  // Hidden by default (V1 vaults)
    m_session_label.add_css_class("caption");
    m_header_bar.set_title_widget(m_session_label);

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

    // Phase 4: V2 vault user section (initially hidden, enabled when V2 vault opens)
    auto user_section = Gio::Menu::create();
    user_section->append("_Change My Password", "win.change-password");
    user_section->append("Manage _Users", "win.manage-users");  // Admin-only, controlled by enable/disable
    user_section->append("_Logout", "win.logout");
    m_primary_menu->append_section(user_section);

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

    // Setup sort button (A-Z / Z-A toggle)
    m_sort_button.set_icon_name("view-sort-ascending-symbolic");
    m_sort_button.set_tooltip_text("Sort accounts A-Z");
    m_sort_button.set_margin_start(6);
    m_search_box.append(m_sort_button);

    m_main_box.append(m_search_box);

    // Setup split pane for accounts and details using new widgets
    m_paned.set_vexpand(true);
    m_paned.set_wide_handle(true);
    constexpr int account_list_width = UI::ACCOUNT_LIST_WIDTH;
    m_paned.set_position(account_list_width);
    m_paned.set_resize_start_child(false);
    m_paned.set_resize_end_child(true);
    m_paned.set_shrink_start_child(false);
    m_paned.set_shrink_end_child(false);

    // Instantiate new widgets
    m_account_tree_widget = std::make_unique<AccountTreeWidget>();
    m_account_detail_widget = std::make_unique<AccountDetailWidget>();

    // Load and apply sort direction from settings
    {
        auto settings = Gio::Settings::create("com.tjdeveng.keeptower");
        Glib::ustring sort_dir = settings->get_string("sort-direction");
        SortDirection direction = (sort_dir == "descending")
            ? SortDirection::DESCENDING : SortDirection::ASCENDING;
        m_account_tree_widget->set_sort_direction(direction);

        // Update button to match loaded direction
        if (direction == SortDirection::ASCENDING) {
            m_sort_button.set_icon_name("view-sort-ascending-symbolic");
            m_sort_button.set_tooltip_text("Sort accounts A-Z");
        } else {
            m_sort_button.set_icon_name("view-sort-descending-symbolic");
            m_sort_button.set_tooltip_text("Sort accounts Z-A");
        }
    }

    // Connect AccountDetailWidget signals
    if (m_account_detail_widget) {
        // Note: We no longer save on every keystroke (signal_modified)
        // Instead, we save when switching accounts or closing the vault
        // This prevents password validation from running on every keystroke

        m_signal_connections.push_back(
            m_account_detail_widget->signal_delete_requested().connect([this]() {
                on_delete_account();
            })
        );

        m_signal_connections.push_back(
            m_account_detail_widget->signal_generate_password().connect([this]() {
                on_generate_password();
            })
        );

        m_signal_connections.push_back(
            m_account_detail_widget->signal_copy_password().connect([this]() {
                on_copy_password();
            })
        );
    }

    // Add new widgets to the paned split
    m_paned.set_start_child(*m_account_tree_widget);
    m_paned.set_end_child(*m_account_detail_widget);

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

    // Connect signals
    m_signal_connections.push_back(
        m_new_button.signal_clicked().connect(
            sigc::mem_fun(*this, &MainWindow::on_new_vault)
        )
    );
    m_signal_connections.push_back(
        m_open_button.signal_clicked().connect(
            sigc::mem_fun(*this, &MainWindow::on_open_vault)
        )
    );
    m_signal_connections.push_back(
        m_save_button.signal_clicked().connect(
            sigc::mem_fun(*this, &MainWindow::on_save_vault)
        )
    );
    m_signal_connections.push_back(
        m_close_button.signal_clicked().connect(
            sigc::mem_fun(*this, &MainWindow::on_close_vault)
        )
    );
    m_signal_connections.push_back(
        m_add_account_button.signal_clicked().connect(
            sigc::mem_fun(*this, &MainWindow::on_add_account)
        )
    );
    m_signal_connections.push_back(
        m_search_entry.signal_changed().connect(
            sigc::mem_fun(*this, &MainWindow::on_search_changed)
        )
    );
    m_signal_connections.push_back(
        m_field_filter_dropdown.property_selected().signal_changed().connect(
            sigc::mem_fun(*this, &MainWindow::on_field_filter_changed)
        )
    );
    m_signal_connections.push_back(
        m_tag_filter_dropdown.property_selected().signal_changed().connect(
            sigc::mem_fun(*this, &MainWindow::on_tag_filter_changed)
        )
    );
    m_signal_connections.push_back(
        m_sort_button.signal_clicked().connect(
            sigc::mem_fun(*this, &MainWindow::on_sort_button_clicked)
        )
    );

    // Connect AccountTreeWidget signals to MainWindow handlers
    if (m_account_tree_widget) {
        m_signal_connections.push_back(
            m_account_tree_widget->signal_account_selected().connect(
                [this](const std::string& account_id) {
                    // Save current account BEFORE switching to new one
                    if (m_selected_account_index >= 0 && m_vault_open) {
                        save_current_account();
                    }

                    int idx = find_account_index_by_id(account_id);
                    if (idx >= 0) {
                        display_account_details(idx);
                        update_tag_filter_dropdown();
                    } else {
                        g_warning("MainWindow: Could not find account with id: %s", account_id.c_str());
                    }
                }
            )
        );

        m_signal_connections.push_back(
            m_account_tree_widget->signal_group_selected().connect(
                [this](const std::string& group_id) {
                    filter_accounts_by_group(group_id);
                }
            )
        );

        m_signal_connections.push_back(
            m_account_tree_widget->signal_favorite_toggled().connect(
                [this](const std::string& account_id) {
                    int idx = find_account_index_by_id(account_id);
                    if (idx >= 0) {
                        on_favorite_toggled(idx);
                    }
                }
            )
        );

        m_signal_connections.push_back(
            m_account_tree_widget->signal_account_right_click().connect(
                [this](const std::string& account_id, Gtk::Widget* widget, double x, double y) {
                    show_account_context_menu(account_id, widget, x, y);
                }
            )
        );

        m_signal_connections.push_back(
            m_account_tree_widget->signal_group_right_click().connect(
                [this](const std::string& group_id, Gtk::Widget* widget, double x, double y) {
                    show_group_context_menu(group_id, widget, x, y);
                }
            )
        );

        m_signal_connections.push_back(
            m_account_tree_widget->signal_account_reordered().connect(
                [this](const std::string& account_id, const std::string& target_group_id, int new_index) {
                    on_account_reordered(account_id, target_group_id, new_index);
                }
            )
        );
        m_signal_connections.push_back(
            m_account_tree_widget->signal_group_reordered().connect(
                [this](const std::string& group_id, int new_index) {
                    on_group_reordered(group_id, new_index);
                }
            )
        );
    }
        // [REMOVED] Legacy TreeView star column click logic (migrated to AccountTreeWidget)

    // Setup activity monitoring for auto-lock
    setup_activity_monitoring();

    // Initially disable search and details
    m_search_entry.set_sensitive(false);
    clear_account_details();
}

MainWindow::~MainWindow() {
    // Disconnect all persistent widget signal connections
    for (auto& conn : m_signal_connections) {
        if (conn.connected()) {
            conn.disconnect();
        }
    }
    m_signal_connections.clear();

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
            // Show combined username + password creation dialog
            auto pwd_dialog = Gtk::make_managed<CreatePasswordDialog>(*this);
            Glib::ustring vault_path = dialog->get_file()->get_path();

            pwd_dialog->signal_response().connect([this, pwd_dialog, vault_path](int pwd_response) {
                if (pwd_response == Gtk::ResponseType::OK) {
                    Glib::ustring admin_username = pwd_dialog->get_username();
                    Glib::ustring password = pwd_dialog->get_password();
                    bool require_yubikey = pwd_dialog->get_yubikey_enabled();

                    // Load default FEC preferences for new vault
                    auto settings = Gio::Settings::create("com.tjdeveng.keeptower");
                    bool use_rs = settings->get_boolean("use-reed-solomon");
                    int rs_redundancy = settings->get_int("rs-redundancy-percent");
                    m_vault_manager->apply_default_fec_preferences(use_rs, rs_redundancy);

                    // Load vault user password history default setting
                    int vault_password_history_depth = settings->get_int("vault-user-password-history-depth");
                    vault_password_history_depth = std::clamp(vault_password_history_depth, 0, 24);

                    KeepTower::VaultSecurityPolicy policy;
                    policy.min_password_length = 8;  // NIST minimum
                    policy.pbkdf2_iterations = 100000;  // Default iterations
                    policy.password_history_depth = vault_password_history_depth;
                    policy.require_yubikey = require_yubikey;

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

                    // Create V2 vault with admin account
                    auto result = m_vault_manager->create_vault_v2(
                        safe_ustring_to_string(vault_path, "vault_path"),
                        admin_username,
                        password,
                        policy
                    );

#ifdef HAVE_YUBIKEY_SUPPORT
                    if (touch_dialog) {
                        touch_dialog->hide();
                    }
#endif
                        if (result) {
                            // Apply default preferences from GSettings to new vault
                            auto settings = Gio::Settings::create("com.tjdeveng.keeptower");

                            // Apply auto-lock settings
                            bool auto_lock_enabled = SettingsValidator::is_auto_lock_enabled(settings);
                            int auto_lock_timeout = SettingsValidator::get_auto_lock_timeout(settings);
                            m_vault_manager->set_auto_lock_enabled(auto_lock_enabled);
                            m_vault_manager->set_auto_lock_timeout(auto_lock_timeout);

                            // Apply clipboard timeout
                            int clipboard_timeout = SettingsValidator::get_clipboard_timeout(settings);
                            m_vault_manager->set_clipboard_timeout(clipboard_timeout);

                            // Apply undo/redo settings
                            bool undo_redo_enabled = settings->get_boolean("undo-redo-enabled");
                            int undo_history_limit = settings->get_int("undo-history-limit");
                            m_vault_manager->set_undo_redo_enabled(undo_redo_enabled);
                            m_vault_manager->set_undo_history_limit(undo_history_limit);

                            // Apply account password history settings
                            bool account_pwd_history_enabled = settings->get_boolean("password-history-enabled");
                            int account_pwd_history_limit = settings->get_int("password-history-limit");
                            m_vault_manager->set_account_password_history_enabled(account_pwd_history_enabled);
                            m_vault_manager->set_account_password_history_limit(account_pwd_history_limit);

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

                            // Update menu for V2 vault (enable user management, etc.)
                            update_menu_for_role();
                            update_session_display();

                            // Start activity monitoring for auto-lock
                            on_user_activity();

                            // Show success dialog with username reminder
                            auto info_dialog = Gtk::make_managed<Gtk::MessageDialog>(
                                *this,
                                "Vault Created Successfully",
                                false,
                                Gtk::MessageType::INFO,
                                Gtk::ButtonsType::OK,
                                true
                            );
                            info_dialog->set_secondary_text(
                                "Your vault has been created successfully.\n\n"
                                "Username: " + admin_username + "\n\n"
                                "Remember this username - you will need it to reopen the vault. "
                                "You can add additional users through the User Management dialog (Tools → Manage Users)."
                            );
                            info_dialog->signal_response().connect([info_dialog](int) {
                                info_dialog->hide();
                            });
                            info_dialog->show();
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
            std::string vault_path_str = safe_ustring_to_string(vault_path, "vault_path");

            // STEP 1: Detect vault version
            auto version_opt = detect_vault_version(vault_path_str);
            if (!version_opt) {
                show_error_dialog("Unable to read vault file or invalid format");
                dialog->hide();
                return;
            }

            uint32_t version = *version_opt;

            // STEP 2: Route to appropriate authentication method
            if (version == 2) {
                // V2 multi-user vault - use new authentication flow
                dialog->hide();
                handle_v2_vault_open(vault_path_str);
                return;
            }

            // STEP 3: V1 vault - use legacy password dialog authentication
            // (Original code path below)

#ifdef HAVE_YUBIKEY_SUPPORT
            // Check if vault requires YubiKey
            std::string yubikey_serial;
            bool yubikey_required = m_vault_manager->check_vault_requires_yubikey(safe_ustring_to_string(vault_path, "vault_path"), yubikey_serial);

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
                    bool yubikey_required = m_vault_manager->check_vault_requires_yubikey(safe_ustring_to_string(vault_path, "vault_path"), yubikey_serial);

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

                    auto result = m_vault_manager->open_vault(safe_ustring_to_string(vault_path, "vault_path"), password);

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
                        m_cached_master_password = safe_ustring_to_string(password, "master_password");

                        update_account_list();
                        update_tag_filter_dropdown();

                        // Initialize undo/redo state
                        update_undo_redo_sensitivity(false, false);

                        // Update menu for V2 vault if applicable
                        update_menu_for_role();
                        update_session_display();

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
    KeepTower::Log::info("MainWindow: on_close_vault() called - m_vault_open={}", m_vault_open);
    if (!m_vault_open) {
        KeepTower::Log::info("MainWindow: Vault not open, returning early");
        return;
    }

    KeepTower::Log::info("MainWindow: Proceeding with vault close");

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

    // Phase 4: Reset V2 UI elements
    m_session_label.set_visible(false);
    update_menu_for_role();  // Disable V2-specific menu items

    // Clear widget-based UI
    if (m_account_tree_widget) {
        m_account_tree_widget->set_data({}, {});
    }
    clear_account_details();
    m_status_label.set_text("No vault open");
}

void MainWindow::on_migrate_v1_to_v2() {
    // Validation: Must have V1 vault open
    if (!m_vault_open) {
        show_error_dialog("No vault is currently open.\nPlease open a vault first.");
        return;
    }

    // Check if already V2 (V2 vaults have multi-user support)
    // V1 vaults don't have the is_v2_vault flag set
    // We can detect this by checking if we have any V2-specific features
    auto session = m_vault_manager->get_current_user_session();
    if (session.has_value()) {
        show_error_dialog("This vault is already in V2 multi-user format.\nNo migration needed.");
        return;
    }

    // Show migration dialog
    auto* migration_dialog = Gtk::make_managed<VaultMigrationDialog>(*this, m_current_vault_path.raw());

    migration_dialog->signal_response().connect([this, migration_dialog](int response) {
        if (response == Gtk::ResponseType::OK) {
            // Get migration parameters
            auto admin_username = migration_dialog->get_admin_username();
            auto admin_password = migration_dialog->get_admin_password();
            auto min_length = migration_dialog->get_min_password_length();
            auto iterations = migration_dialog->get_pbkdf2_iterations();

            // Load vault user password history default setting
            auto settings = Gio::Settings::create("com.tjdeveng.keeptower");
            int vault_password_history_depth = settings->get_int("vault-user-password-history-depth");
            vault_password_history_depth = std::clamp(vault_password_history_depth, 0, 24);

            // Create security policy
            KeepTower::VaultSecurityPolicy policy;
            policy.min_password_length = min_length;
            policy.pbkdf2_iterations = iterations;
            policy.password_history_depth = vault_password_history_depth;
            policy.require_yubikey = false;

            // Perform migration
            auto result = m_vault_manager->convert_v1_to_v2(admin_username, admin_password, policy);

            if (result) {
                // Success - update UI to V2 mode
                update_session_display();

                // Show success dialog
                auto* success_dialog = Gtk::make_managed<Gtk::MessageDialog>(
                    *this,
                    "Migration Successful",
                    false,
                    Gtk::MessageType::INFO,
                    Gtk::ButtonsType::OK,
                    true
                );
                success_dialog->set_secondary_text(
                    "Your vault has been successfully upgraded to V2 multi-user format.\n\n"
                    "• Administrator account: " + admin_username.raw() + "\n"
                    "• Backup created: " + m_current_vault_path.raw() + ".v1.backup\n"
                    "• You can now add additional users via Tools → Manage Users"
                );
                success_dialog->set_modal(true);
                success_dialog->set_hide_on_close(true);

                success_dialog->signal_response().connect([success_dialog](int) {
                    success_dialog->hide();
                });

                success_dialog->show();

                // Enable V2-only features
                if (m_manage_users_action) {
                    m_manage_users_action->set_enabled(true);
                }

            } else {
                // Migration failed - show error
                std::string error_message = "Failed to migrate vault to V2 format.\n\n";

                switch (result.error()) {
                    case KeepTower::VaultError::VaultNotOpen:
                        error_message += "Vault is not open.";
                        break;
                    case KeepTower::VaultError::InvalidUsername:
                        error_message += "Invalid username format.";
                        break;
                    case KeepTower::VaultError::InvalidPassword:
                        error_message += "Invalid password format.";
                        break;
                    case KeepTower::VaultError::WeakPassword:
                        error_message += "Password does not meet minimum length requirement.";
                        break;
                    case KeepTower::VaultError::FileWriteError:
                        error_message += "Failed to create backup file.";
                        break;
                    case KeepTower::VaultError::CryptoError:
                        error_message += "Cryptographic operation failed.";
                        break;
                    default:
                        error_message += "Unknown error occurred.";
                        break;
                }

                show_error_dialog(error_message);
            }
        }

        migration_dialog->hide();
    });

    migration_dialog->show();
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

        // Update the display first
        update_account_list();

        // Select the newly added account (it will be at the end)
        auto accounts = m_vault_manager->get_all_accounts();
        if (!accounts.empty()) {
            int new_index = accounts.size() - 1;

            // Display the account details
            display_account_details(new_index);

            // Focus the name field and select all text for easy editing (only on first add, not redo)
            Glib::signal_idle().connect_once([this]() {
                if (m_account_detail_widget && m_selected_account_index >= 0) {
                    m_account_detail_widget->focus_account_name_entry();
                }
            });
        } else {
            clear_account_details();
        }

        m_status_label.set_text("Account added");
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
}

void MainWindow::on_copy_password() {
    const std::string password = m_account_detail_widget->get_password();

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
    // This functionality is now handled by AccountDetailWidget internally
}

void MainWindow::on_star_column_clicked(const Gtk::TreeModel::Path& /*path*/) {
    // [LEGACY METHOD - REPLACED BY AccountTreeWidget signal handlers]
    // This method handled TreeView star column clicks
    // Now handled by AccountRowWidget's favorite_toggled signal via on_favorite_toggled()
    return;
}

void MainWindow::on_favorite_toggled(int account_index) {
    if (!m_vault_open) {
        return;
    }

    // Create command with UI callback
    auto ui_callback = [this]() {
        // Refresh the account list to show updated favorite status
        update_account_list();
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

    // [LEGACY METHOD - REPLACED BY AccountTreeWidget signal handlers]
    // This method handled TreeView selection changes
    // Now handled by m_account_tree_widget->signal_account_selected()
    return;
}

// [REMOVED] Legacy on_account_selected (migrated to AccountTreeWidget)

// [REMOVED] Legacy on_account_right_click (migrated to AccountTreeWidget)

void MainWindow::update_account_list() {
    // Widget-based UI update
    if (!m_vault_manager || !m_account_tree_widget) {
        return;
    }

    auto all_accounts = m_vault_manager->get_all_accounts();
    auto groups = m_vault_manager->get_all_groups();

    // Filter accounts based on user permissions (V2 multi-user vaults)
    std::vector<keeptower::AccountRecord> viewable_accounts;
    viewable_accounts.reserve(all_accounts.size());

    for (size_t i = 0; i < all_accounts.size(); ++i) {
        if (m_vault_manager->can_view_account(i)) {
            viewable_accounts.push_back(all_accounts[i]);
        }
    }

    m_account_tree_widget->set_data(groups, viewable_accounts);

    m_status_label.set_text("Vault opened: " + m_current_vault_path +
                           " (" + std::to_string(viewable_accounts.size()) + " accounts)");
    return;

    // [LEGACY TreeView code below - kept for reference, no longer executed]
    /*
    m_account_list_store->clear();
    m_filtered_indices.clear();

    // Add "Favorites" group with favorited accounts
    std::vector<size_t> favorite_indices;
    for (size_t i = 0; i < accounts.size(); i++) {
        if (accounts[i].is_favorite()) {
            favorite_indices.push_back(i);
        }
    }

    if (!favorite_indices.empty()) {
        auto favorites_group_row = *(m_account_list_store->append());
        favorites_group_row[m_columns.m_col_is_group] = true;
        favorites_group_row[m_columns.m_col_account_name] = "⭐ Favorites";
        favorites_group_row[m_columns.m_col_user_name] = "";
        favorites_group_row[m_columns.m_col_index] = -1;
        favorites_group_row[m_columns.m_col_group_id] = "favorites";
        favorites_group_row[m_columns.m_col_is_favorite] = false;

        // Sort favorites alphabetically
        std::sort(favorite_indices.begin(), favorite_indices.end(),
            [&accounts](size_t a, size_t b) {
                return accounts[a].account_name() < accounts[b].account_name();
            });

        for (size_t index : favorite_indices) {
            auto child_row = *(m_account_list_store->append(favorites_group_row.children()));
            child_row[m_columns.m_col_is_group] = false;
            child_row[m_columns.m_col_is_favorite] = true;
            child_row[m_columns.m_col_account_name] = accounts[index].account_name();
            child_row[m_columns.m_col_user_name] = accounts[index].user_name();
            child_row[m_columns.m_col_index] = index;
            child_row[m_columns.m_col_group_id] = "";
        }
    }

    // Add user-created groups
    for (const auto& group : groups) {
        // Skip system group (favorites)
        if (group.group_id() == "favorites") continue;

        // Get accounts in this group
        std::vector<size_t> group_account_indices;
        for (size_t i = 0; i < accounts.size(); i++) {
            if (m_vault_manager->is_account_in_group(i, group.group_id())) {
                group_account_indices.push_back(i);
            }
        }

        // Only show group if it has accounts
        if (group_account_indices.empty()) continue;

        auto group_row = *(m_account_list_store->append());
        group_row[m_columns.m_col_is_group] = true;
        group_row[m_columns.m_col_account_name] = group.group_name();
        group_row[m_columns.m_col_user_name] = "";
        group_row[m_columns.m_col_index] = -1;
        group_row[m_columns.m_col_group_id] = group.group_id();
        group_row[m_columns.m_col_is_favorite] = false;

        // Sort accounts alphabetically
        std::sort(group_account_indices.begin(), group_account_indices.end(),
            [&accounts](size_t a, size_t b) {
                return accounts[a].account_name() < accounts[b].account_name();
            });

        for (size_t index : group_account_indices) {
            auto child_row = *(m_account_list_store->append(group_row.children()));
            child_row[m_columns.m_col_is_group] = false;
            child_row[m_columns.m_col_is_favorite] = accounts[index].is_favorite();
            child_row[m_columns.m_col_account_name] = accounts[index].account_name();
            child_row[m_columns.m_col_user_name] = accounts[index].user_name();
            child_row[m_columns.m_col_index] = index;
            child_row[m_columns.m_col_group_id] = "";
        }
    }

    // Add "All Accounts" group with all ungrouped accounts
    std::vector<size_t> ungrouped_indices;
    for (size_t i = 0; i < accounts.size(); i++) {
        bool is_in_any_group = false;
        for (const auto& group : groups) {
            if (m_vault_manager->is_account_in_group(i, group.group_id())) {
                is_in_any_group = true;
                break;
            }
        }
        if (!is_in_any_group) {
            ungrouped_indices.push_back(i);
        }
    }

    if (!ungrouped_indices.empty()) {
        auto all_accounts_row = *(m_account_list_store->append());
        all_accounts_row[m_columns.m_col_is_group] = true;
        all_accounts_row[m_columns.m_col_account_name] = "All Accounts";
        all_accounts_row[m_columns.m_col_user_name] = "";
        all_accounts_row[m_columns.m_col_index] = -1;
        all_accounts_row[m_columns.m_col_group_id] = "all";
        all_accounts_row[m_columns.m_col_is_favorite] = false;

        // Sort ungrouped accounts alphabetically
        std::sort(ungrouped_indices.begin(), ungrouped_indices.end(),
            [&accounts](size_t a, size_t b) {
                return accounts[a].account_name() < accounts[b].account_name();
            });

        for (size_t index : ungrouped_indices) {
            auto child_row = *(m_account_list_store->append(all_accounts_row.children()));
            child_row[m_columns.m_col_is_group] = false;
            child_row[m_columns.m_col_is_favorite] = accounts[index].is_favorite();
            child_row[m_columns.m_col_account_name] = accounts[index].account_name();
            child_row[m_columns.m_col_user_name] = accounts[index].user_name();
            child_row[m_columns.m_col_index] = index;
            child_row[m_columns.m_col_group_id] = "";
        }
    }

    // Expand all groups by default
    m_account_tree_view.expand_all();

    m_status_label.set_text("Vault opened: " + m_current_vault_path +
                           " (" + std::to_string(accounts.size()) + " accounts)");
    */
}

void MainWindow::filter_accounts(const Glib::ustring& search_text) {
    if (!m_account_tree_widget) {
        return;
    }

    // Get current field filter selection
    const guint field_filter = m_field_filter_dropdown.get_selected();
    // 0=All, 1=Account Name, 2=Username, 3=Email, 4=Website, 5=Notes, 6=Tags

    // Apply filters to the tree widget
    m_account_tree_widget->set_filters(
        safe_ustring_to_string(search_text, "search_text"),
        m_selected_tag_filter,
        static_cast<int>(field_filter)
    );
}

void MainWindow::clear_account_details() {
    m_account_detail_widget->clear();
    m_selected_account_index = -1;
}

void MainWindow::display_account_details(int index) {
    // Bounds checking for safety
    if (index < 0) {
        m_account_detail_widget->clear();
        return;
    }

    m_selected_account_index = index;

    // Load account from VaultManager
    const auto* account = m_vault_manager->get_account(index);
    if (!account) {
        g_warning("MainWindow::display_account_details - account is null at index %d", index);
        m_account_detail_widget->clear();
        return;
    }

    // Display in the detail widget
    m_account_detail_widget->display_account(account);

    // Check user role for permissions (V2 multi-user vaults)
    bool is_admin = is_current_user_admin();

    // Control privacy checkboxes - only admins can modify them
    m_account_detail_widget->set_privacy_controls_editable(is_admin);

    // Check if account is admin-only-deletable
    // Standard users get read-only access to prevent circumventing deletion protection
    if (!is_admin && account->is_admin_only_deletable()) {
        // Standard user viewing admin-protected account: read-only mode
        // User can view/copy password but cannot edit or delete
        m_account_detail_widget->set_editable(false);
        m_account_detail_widget->set_delete_button_sensitive(false);
    } else {
        // Normal edit mode (admin or non-protected account)
        m_account_detail_widget->set_editable(true);
        m_account_detail_widget->set_delete_button_sensitive(true);
    }
}

bool MainWindow::save_current_account() {
    // Only save if we have a valid account selected
    if (m_selected_account_index < 0 || !m_vault_open) {
        return true;  // Nothing to save, allow continue
    }

    // Validate the index is within bounds
    const auto accounts = m_vault_manager->get_all_accounts();
    if (m_selected_account_index >= static_cast<int>(accounts.size())) {
        g_warning("Invalid account index %d (total accounts: %zu)",
                  m_selected_account_index, accounts.size());
        return true;  // Invalid state, but don't block navigation
    }

    // Get values from detail widget
    const auto account_name = m_account_detail_widget->get_account_name();
    const auto user_name = m_account_detail_widget->get_user_name();
    const auto password = m_account_detail_widget->get_password();
    const auto email = m_account_detail_widget->get_email();
    const auto website = m_account_detail_widget->get_website();
    const auto notes = m_account_detail_widget->get_notes();

    // Convert to Glib::ustring for validation functions
    const Glib::ustring account_name_u(account_name);
    const Glib::ustring user_name_u(user_name);
    const Glib::ustring password_u(password);
    const Glib::ustring email_u(email);
    const Glib::ustring website_u(website);
    const Glib::ustring notes_u(notes);

    // Validate field lengths before saving
    if (!validate_field_length("Account Name", account_name_u, UI::MAX_ACCOUNT_NAME_LENGTH)) {
        return false;
    }
    if (!validate_field_length("Username", user_name_u, UI::MAX_USERNAME_LENGTH)) {
        return false;
    }
    if (!validate_field_length("Password", password_u, UI::MAX_PASSWORD_LENGTH)) {
        return false;
    }

    // Validate email format if not empty
    if (!email.empty() && !validate_email_format(email_u)) {
        return false;
    }

    if (!validate_field_length("Email", email_u, UI::MAX_EMAIL_LENGTH)) {
        return false;
    }
    if (!validate_field_length("Website", website_u, UI::MAX_WEBSITE_LENGTH)) {
        return false;
    }

    // Validate notes length
    if (!validate_field_length("Notes", notes_u, UI::MAX_NOTES_LENGTH)) {
        return false;
    }

    // Get the current account from VaultManager
    auto* account = m_vault_manager->get_account_mutable(m_selected_account_index);
    if (!account) {
        g_warning("Failed to get account at index %d", m_selected_account_index);
        return true;  // Allow navigation even if account not found
    }

    // Check if user has permission to edit this account (V2 multi-user vaults)
    // Standard users cannot edit admin-only-deletable accounts
    bool is_admin = is_current_user_admin();
    if (!is_admin && account->is_admin_only_deletable()) {
        // Only block save if account was actually modified
        if (m_account_detail_widget->is_modified()) {
            show_error_dialog(
                "You do not have permission to edit this account.\n\n"
                "This account is marked as admin-only-deletable.\n"
                "Only administrators can modify protected accounts."
            );
            // Reload the original account data to discard any changes
            m_account_detail_widget->display_account(account);
            return false;  // Prevent save and navigation
        }
        // Not modified, allow navigation without error
        return true;
    }

    // Store the old account name to detect if it changed
    const std::string old_name = account->account_name();
    const std::string old_password = account->password();
    const std::string new_password = password;

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
    account->set_account_name(account_name);
    account->set_user_name(user_name);
    account->set_password(new_password);
    account->set_email(email);
    account->set_website(website);
    account->set_notes(notes);

    // Update tags
    account->clear_tags();
    auto current_tags = m_account_detail_widget->get_all_tags();
    for (const auto& tag : current_tags) {
        account->add_tags(tag);
    }

    // Update privacy controls (V2 multi-user vaults)
    account->set_is_admin_only_viewable(m_account_detail_widget->get_admin_only_viewable());
    account->set_is_admin_only_deletable(m_account_detail_widget->get_admin_only_deletable());

    // Update modification timestamp
    account->set_modified_at(std::time(nullptr));

    // Refresh the account list if the name changed
    if (old_name != account->account_name()) {
        update_account_list();
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
        if (!std::regex_match(safe_ustring_to_string(email, "email"), email_pattern)) {
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
        m_selected_tag_filter = safe_ustring_to_string(item, "tag_filter");
    }

    // Re-apply current search with new tag filter
    filter_accounts(m_search_entry.get_text());
}

void MainWindow::on_field_filter_changed() {
    // Re-apply current search with new field filter
    filter_accounts(m_search_entry.get_text());
}

void MainWindow::on_sort_button_clicked() {
    if (!m_account_tree_widget) {
        return;
    }

    // Toggle sort direction
    m_account_tree_widget->toggle_sort_direction();

    // Update button icon and tooltip based on new direction
    SortDirection direction = m_account_tree_widget->get_sort_direction();
    if (direction == SortDirection::ASCENDING) {
        m_sort_button.set_icon_name("view-sort-ascending-symbolic");
        m_sort_button.set_tooltip_text("Sort accounts A-Z");
    } else {
        m_sort_button.set_icon_name("view-sort-descending-symbolic");
        m_sort_button.set_tooltip_text("Sort accounts Z-A");
    }

    // Save preference to GSettings
    auto settings = Gio::Settings::create("com.tjdeveng.keeptower");
    settings->set_string("sort-direction",
        (direction == SortDirection::ASCENDING) ? "ascending" : "descending");
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
    // This functionality is now handled by AccountDetailWidget internally
}

void MainWindow::add_tag_chip(const std::string& /*tag*/) {
    // This functionality is now handled by AccountDetailWidget internally
}

void MainWindow::remove_tag_chip(const std::string& /*tag*/) {
    // This functionality is now handled by AccountDetailWidget internally
}

void MainWindow::update_tags_display() {
    // This functionality is now handled by AccountDetailWidget internally
}

std::vector<std::string> MainWindow::get_current_tags() {
    // Tags are now managed by AccountDetailWidget
    // This is a compatibility stub - consider refactoring callers
    return {};
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
    // Create preferences dialog (as managed widget)
    auto* dialog = Gtk::make_managed<PreferencesDialog>(*this, m_vault_manager.get());

    // Connect to close signal to reload settings when dialog is dismissed
    dialog->signal_close_request().connect([this]() {
        // Reload undo/redo settings when preferences closes
        Glib::signal_idle().connect_once([this]() {
            auto settings = Gio::Settings::create("com.tjdeveng.keeptower");
            bool undo_redo_enabled = settings->get_boolean("undo-redo-enabled");
            int undo_history_limit = settings->get_int("undo-history-limit");
            undo_history_limit = std::clamp(undo_history_limit, 1, 100);

            m_undo_manager.set_max_history(undo_history_limit);

            if (!undo_redo_enabled) {
                m_undo_manager.clear();
                update_undo_redo_sensitivity(false, false);
            } else {
                update_undo_redo_sensitivity(m_undo_manager.can_undo(), m_undo_manager.can_redo());
            }
        });
        return false;  // Allow the dialog to close
    }, false);

    dialog->show();
}

void MainWindow::on_delete_account() {
    // Determine which account to delete: context menu or selected account
    int account_index = -1;

    if (!m_context_menu_account_id.empty()) {
        // Context menu delete
        account_index = find_account_index_by_id(m_context_menu_account_id);
    } else if (m_selected_account_index >= 0) {
        // Button/keyboard delete
        account_index = m_selected_account_index;
    }

    if (account_index < 0 || !m_vault_open) {
        return;
    }

    // Check delete permissions (V2 multi-user vaults)
    if (!m_vault_manager->can_delete_account(account_index)) {
        show_error_dialog(
            "You do not have permission to delete this account.\n\n"
            "Only administrators can delete admin-protected accounts."
        );
        m_context_menu_account_id.clear();
        return;
    }

    // Get account name for confirmation dialog
    const auto* account = m_vault_manager->get_account(account_index);
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

    dialog->signal_response().connect([this, dialog, account_index](int response) {
        if (response == Gtk::ResponseType::OK) {
            // Create command with UI callback
            auto ui_callback = [this]() {
                clear_account_details();
                update_account_list();
                filter_accounts(m_search_entry.get_text());
            };

            auto command = std::make_unique<DeleteAccountCommand>(
                m_vault_manager.get(),
                account_index,
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

        // Clear context menu state
        m_context_menu_account_id.clear();
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
                Glib::ustring password = password_dialog->get_password();

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
                        // Securely clear password before returning (Glib::ustring)
                        if (!password.empty()) {
                            volatile char* p = const_cast<char*>(password.data());
                            std::memset(const_cast<char*>(p), 0, password.bytes());
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
                        // Securely clear password before returning (Glib::ustring)
                        if (!password.empty()) {
                            volatile char* p = const_cast<char*>(password.data());
                            std::memset(const_cast<char*>(p), 0, password.bytes());
                            password.clear();
                        }
                        show_error_dialog("Authentication failed. Export cancelled.");
                        return;
                    }
                }

                // Securely clear password after successful authentication (Glib::ustring)
                if (!password.empty()) {
                    volatile char* p = const_cast<char*>(password.data());
                    std::memset(const_cast<char*>(p), 0, password.bytes());
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

            m_account_detail_widget->set_password(password);
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

    // Check if auto-lock is enabled
    // CRITICAL: Read from vault if open (security policy), otherwise from GSettings (user preference)
    bool auto_lock_enabled;
    if (m_vault_manager && m_vault_manager->is_vault_open()) {
        auto_lock_enabled = m_vault_manager->get_auto_lock_enabled();
    } else {
        static const auto settings = Gio::Settings::create("com.tjdeveng.keeptower");
        auto_lock_enabled = SettingsValidator::is_auto_lock_enabled(settings);
    }

    if (!auto_lock_enabled) {
        return;
    }

    // Cancel previous timeout if exists
    if (m_auto_lock_timeout.connected()) {
        m_auto_lock_timeout.disconnect();
    }

    // Get timeout from vault if open, otherwise from defaults
    int timeout_seconds;
    if (m_vault_manager && m_vault_manager->is_vault_open()) {
        timeout_seconds = m_vault_manager->get_auto_lock_timeout();
        // Clamp to valid range (60-3600 seconds)
        timeout_seconds = std::clamp(timeout_seconds, 60, 3600);
    } else {
        static const auto settings = Gio::Settings::create("com.tjdeveng.keeptower");
        timeout_seconds = SettingsValidator::get_auto_lock_timeout(settings);
    }

    // Schedule auto-lock after validated timeout
    m_auto_lock_timeout = Glib::signal_timeout().connect(
        sigc::mem_fun(*this, &MainWindow::on_auto_lock_timeout),
        timeout_seconds * 1000  // Convert seconds to milliseconds
    );
}

bool MainWindow::on_auto_lock_timeout() {
    if (!m_vault_open || m_is_locked) {
        return false;
    }

    // For V2 vaults, auto-lock means automatic logout (no cached password)
    if (is_v2_vault_open()) {
        KeepTower::Log::info("MainWindow: Auto-lock timeout triggered for V2 vault, forcing logout");

        // Auto-save only if vault has been modified (security timeout)
        bool had_unsaved_changes = false;
        if (m_vault_manager && m_vault_manager->is_modified()) {
            had_unsaved_changes = true;
            save_current_account();
            m_vault_manager->save_vault();
        }

        // Force logout without allowing cancellation (security timeout)
        std::string vault_path = m_current_vault_path;  // Save path before close
        on_close_vault();

        // Show notification that auto-lock occurred
        auto info_dialog = Gtk::make_managed<Gtk::MessageDialog>(*this,
            "Session Timeout", false, Gtk::MessageType::INFO, Gtk::ButtonsType::OK, true);

        if (had_unsaved_changes) {
            info_dialog->set_secondary_text("Your session has been automatically logged out due to inactivity.\nAny unsaved changes have been saved.");
        } else {
            info_dialog->set_secondary_text("Your session has been automatically logged out due to inactivity.");
        }

        info_dialog->signal_response().connect([info_dialog, this, vault_path](int) {
            info_dialog->hide();
            // Reopen the same vault file (will show login dialog)
            if (!vault_path.empty()) {
                handle_v2_vault_open(vault_path);
            }
        });
        info_dialog->show();
    } else {
        // For V1 vaults, use traditional lock/unlock mechanism
        lock_vault();
    }
    return false;  // Don't repeat
}

void MainWindow::lock_vault() {
    if (!m_vault_open || m_is_locked) {
        return;
    }

    // This should only be called for V1 vaults
    if (is_v2_vault_open()) {
        KeepTower::Log::warning("MainWindow: lock_vault() called for V2 vault, use logout instead");
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

    // Clear widget-based UI
    if (m_account_tree_widget) {
        m_account_tree_widget->set_data({}, {});
    }

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
        const std::string entered_password{safe_ustring_to_string(password_entry->get_text(), "unlock_password")};

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
#endif

#ifdef HAVE_YUBIKEY_SUPPORT
void MainWindow::on_manage_yubikeys() {
    // Check if vault is open
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

    // Show YubiKey manager dialog (managed by GTK)
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
// [REMOVED] Legacy setup_drag_and_drop (migrated to AccountTreeWidget)

// Account Groups Implementation
// ============================================================================

void MainWindow::on_create_group() {
    if (!m_vault_open) {
        return;
    }

    auto* dialog = Gtk::make_managed<GroupCreateDialog>(*this);
    dialog->set_modal(true);
    dialog->set_hide_on_close(true);

    dialog->signal_response().connect([this, dialog](int result) {
        dialog->hide();

        if (result != static_cast<int>(Gtk::ResponseType::OK)) {
            return;
        }

        auto group_name = dialog->get_group_name();
        if (group_name.empty()) {
            return;
        }

        // Create the group
        std::string group_id = m_vault_manager->create_group(safe_ustring_to_string(group_name, "group_name"));
        if (group_id.empty()) {
            show_error_dialog("Failed to create group. The name may already exist or be invalid.");
            return;
        }

        m_status_label.set_text("Group created: " + group_name);
        update_account_list();
    });

    dialog->present();
}

void MainWindow::on_rename_group(const std::string& group_id, const Glib::ustring& current_name) {
    if (!m_vault_open || group_id.empty()) {
        return;
    }

    auto* dialog = Gtk::make_managed<GroupRenameDialog>(*this, current_name);
    dialog->set_modal(true);
    dialog->set_hide_on_close(true);

    dialog->signal_response().connect([this, dialog, group_id, current_name](int result) {
        dialog->hide();

        if (result != static_cast<int>(Gtk::ResponseType::OK)) {
            return;
        }

        auto new_name = dialog->get_group_name();
        // [REMOVED] Legacy account list update logic (migrated to AccountTreeWidget)

        // Rename the group in vault manager
        if (m_vault_manager->rename_group(group_id, safe_ustring_to_string(new_name, "group_name"))) {
            m_status_label.set_text("Group renamed");
            update_account_list();
        } else {
            show_error_dialog("Failed to rename group");
        }
    });

    dialog->present();
}

void MainWindow::on_delete_group(const std::string& group_id) {
    if (!m_vault_open || group_id.empty()) {
        return;
    }

    // Confirm deletion
    auto dialog = std::make_unique<Gtk::MessageDialog>(
        *this,
        "Delete this group?",
        false,
        Gtk::MessageType::QUESTION,
        Gtk::ButtonsType::YES_NO,
        true
    );
    dialog->set_secondary_text("Accounts in this group will not be deleted.");

    dialog->signal_response().connect([this, dialog_ptr = dialog.get(), group_id](int response) {
        if (response == static_cast<int>(Gtk::ResponseType::YES)) {
            if (m_vault_manager->delete_group(group_id)) {
                m_status_label.set_text("Group deleted");
                update_account_list();
            } else {
                show_error_dialog("Failed to delete group");
            }
        }
        dialog_ptr->hide();
    });

    dialog->set_modal(true);
    dialog->show();
    dialog.release(); // Dialog will delete itself
}

// Helper methods for widget-based UI
int MainWindow::find_account_index_by_id(const std::string& account_id) const {
    if (!m_vault_manager) return -1;
    const auto& accounts = m_vault_manager->get_all_accounts();
    for (size_t i = 0; i < accounts.size(); ++i) {
        if (accounts[i].id() == account_id) return static_cast<int>(i);
    }
    return -1;
}

void MainWindow::filter_accounts_by_group(const std::string& group_id) {
    if (!m_vault_manager) return;
    const auto& groups = m_vault_manager->get_all_groups();
    const auto& accounts = m_vault_manager->get_all_accounts();
    if (group_id.empty()) {
        // Show all accounts
        m_account_tree_widget->set_data(groups, accounts);
        return;
    }
    // Filter accounts belonging to the selected group
    std::vector<keeptower::AccountRecord> filtered_accounts;
    for (const auto& account : accounts) {
        for (int i = 0; i < account.groups_size(); ++i) {
            if (account.groups(i).group_id() == group_id) {
                filtered_accounts.push_back(account);
                break;
            }
        }
    }
    m_account_tree_widget->set_data(groups, filtered_accounts);
}

// Handle account drag-and-drop reorder
void MainWindow::on_account_reordered(const std::string& account_id, const std::string& target_group_id, int new_index) {
    if (!m_vault_manager) return;
    int idx = find_account_index_by_id(account_id);
    if (idx < 0) return;

    g_debug("MainWindow::on_account_reordered - account_id=%s, target_group_id='%s', index=%d",
            account_id.c_str(), target_group_id.c_str(), new_index);

    // Handle group membership changes
    if (target_group_id.empty()) {
        // Empty group_id means dropped into "All Accounts" view
        // This is just a view of all accounts, not a group container
        // Don't change group membership - use context menu to remove from groups
        g_debug("  Dropped into All Accounts - no group membership changes");
        return;  // No-op
    } else {
        // Adding to a group - just add without removing from other groups
        // This allows accounts to be members of multiple groups
        if (!m_vault_manager->is_account_in_group(idx, target_group_id)) {
            m_vault_manager->add_account_to_group(idx, target_group_id);
        }
    }

    // Defer UI refresh until after drag operation completes (next idle cycle)
    // This prevents destroying widgets while drag is still in progress
    Glib::signal_idle().connect_once([this]() {
        if (m_vault_manager && m_account_tree_widget) {
            const auto& groups = m_vault_manager->get_all_groups();
            const auto& accounts = m_vault_manager->get_all_accounts();
            m_account_tree_widget->set_data(groups, accounts);
        }
    });
}

// Handle group drag-and-drop reorder
void MainWindow::on_group_reordered(const std::string& group_id, int new_index) {
    if (!m_vault_manager) return;
    m_vault_manager->reorder_group(group_id, new_index);

    // Defer UI refresh until after drag operation completes
    Glib::signal_idle().connect_once([this]() {
        if (m_vault_manager && m_account_tree_widget) {
            const auto& groups = m_vault_manager->get_all_groups();
            const auto& accounts = m_vault_manager->get_all_accounts();
            m_account_tree_widget->set_data(groups, accounts);
        }
    });
}

void MainWindow::show_account_context_menu(const std::string& account_id, Gtk::Widget* widget, double x, double y) {
    // Find the account index
    int account_index = find_account_index_by_id(account_id);
    if (account_index < 0) {
        return;
    }

    // Store account_id for use by action handlers
    m_context_menu_account_id = account_id;

    // Create main menu
    auto menu = Gio::Menu::create();

    // Add "Add to Group" submenu if there are groups
    if (m_vault_manager) {
        auto groups = m_vault_manager->get_all_groups();
        auto accounts = m_vault_manager->get_all_accounts();

        if (account_index < accounts.size()) {
            const auto& account = accounts[account_index];

            // Build "Add to Group" submenu
            if (!groups.empty()) {
                auto groups_menu = Gio::Menu::create();
                for (const auto& group : groups) {
                    if (group.group_id() != "favorites") {  // Skip system groups
                        // Create a simple action for this specific group
                        std::string action_name = "add-to-group-" + group.group_id();
                        remove_action(action_name);  // Remove if it exists
                        add_action(action_name, [this, gid = group.group_id()]() {
                            if (!m_context_menu_account_id.empty() && m_vault_manager) {
                                int idx = find_account_index_by_id(m_context_menu_account_id);
                                if (idx >= 0) {
                                    m_vault_manager->add_account_to_group(idx, gid);
                                    update_account_list();
                                }
                            }
                        });
                        groups_menu->append(group.group_name(), "win." + action_name);
                    }
                }
                if (groups_menu->get_n_items() > 0) {
                    menu->append_submenu("Add to Group", groups_menu);
                }
            }

            // Build "Remove from Group" submenu if account belongs to any groups
            std::vector<std::string> account_groups;
            for (int i = 0; i < account.groups_size(); ++i) {
                account_groups.push_back(account.groups(i).group_id());
            }

            if (!account_groups.empty()) {
                auto remove_groups_menu = Gio::Menu::create();
                for (const auto& gid : account_groups) {
                    // Find the group name
                    std::string group_name = gid;
                    for (const auto& group : groups) {
                        if (group.group_id() == gid) {
                            group_name = group.group_name();
                            break;
                        }
                    }

                    // Create action for removing from this group
                    std::string action_name = "remove-from-group-" + gid;
                    remove_action(action_name);  // Remove if it exists
                    add_action(action_name, [this, gid]() {
                        if (!m_context_menu_account_id.empty() && m_vault_manager) {
                            int idx = find_account_index_by_id(m_context_menu_account_id);
                            if (idx >= 0) {
                                m_vault_manager->remove_account_from_group(idx, gid);
                                update_account_list();
                            }
                        }
                    });
                    remove_groups_menu->append(group_name, "win." + action_name);
                }
                menu->append_submenu("Remove from Group", remove_groups_menu);
            }
        }
    }

    // Add separator and destructive delete action
    auto delete_section = Gio::Menu::create();
    delete_section->append("Delete Account", "win.delete-account");
    menu->append_section(delete_section);

    // Create managed popover that persists until hidden
    auto popover = Gtk::make_managed<Gtk::PopoverMenu>();
    popover->set_menu_model(menu);
    popover->set_parent(*widget);
    popover->set_has_arrow(true);
    popover->set_autohide(true);

    // Position at click location (widget-relative coordinates)
    Gdk::Rectangle rect;
    rect.set_x(static_cast<int>(x));
    rect.set_y(static_cast<int>(y));
    rect.set_width(1);
    rect.set_height(1);
    popover->set_pointing_to(rect);

    popover->popup();
}

void MainWindow::show_group_context_menu(const std::string& group_id, Gtk::Widget* widget, double x, double y) {
    // Don't show menu for Favorites (it's fully system-managed)
    if (group_id == "favorites") {
        return;
    }

    // Store group_id for use by action handlers
    m_context_menu_group_id = group_id;

    // Create a popover menu with group actions
    auto menu = Gio::Menu::create();

    // For "All Accounts", only show "New Group" option
    // For user groups, show all options
    auto actions_section = Gio::Menu::create();
    actions_section->append("New Group...", "win.create-group");

    if (group_id != "all") {
        // Only show rename/delete for user-created groups
        actions_section->append("Rename Group...", "win.rename-group");
        menu->append_section(actions_section);

        // Destructive delete action in separate section
        auto delete_section = Gio::Menu::create();
        delete_section->append("Delete Group...", "win.delete-group");
        menu->append_section(delete_section);
    } else {
        // For "All Accounts", just show the create option
        menu->append_section(actions_section);
    }

    // Create managed popover that persists until hidden
    auto popover = Gtk::make_managed<Gtk::PopoverMenu>();
    popover->set_menu_model(menu);
    popover->set_parent(*widget);
    popover->set_has_arrow(true);
    popover->set_autohide(true);

    // Position at click location (widget-relative coordinates)
    Gdk::Rectangle rect;
    rect.set_x(static_cast<int>(x));
    rect.set_y(static_cast<int>(y));
    rect.set_width(1);
    rect.set_height(1);
    popover->set_pointing_to(rect);

    popover->popup();
}

// ============================================================================
// V2 Multi-User Vault Support
// ============================================================================

std::optional<uint32_t> MainWindow::detect_vault_version(const std::string& vault_path) {
    // Read vault file header to detect version
    std::ifstream file(vault_path, std::ios::binary);
    if (!file) {
        return std::nullopt;
    }

    // Read enough data for header detection (magic + version)
    std::vector<uint8_t> header_data(1024);  // Plenty for header
    file.read(reinterpret_cast<char*>(header_data.data()), header_data.size());
    if (!file) {
        // File too small or error
        size_t bytes_read = file.gcount();
        header_data.resize(bytes_read);
    }

    // Use VaultFormatV2::detect_version to determine format
    auto version_result = KeepTower::VaultFormatV2::detect_version(header_data);
    if (!version_result) {
        return std::nullopt;  // Invalid vault or error
    }

    return *version_result;
}

void MainWindow::handle_v2_vault_open(const std::string& vault_path) {
    // Check if YubiKey is required for this vault
    std::string yubikey_serial;
    bool yubikey_required = m_vault_manager->check_vault_requires_yubikey(vault_path, yubikey_serial);

#ifdef HAVE_YUBIKEY_SUPPORT
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
                    // User clicked Retry
                    yk_dialog->hide();
                    handle_v2_vault_open(vault_path);  // Retry
                    return;
                }
                yk_dialog->hide();
            });

            yk_dialog->show();
            return;
        }
    }
#endif

    // Show V2 user login dialog (username + password)
    auto login_dialog = Gtk::make_managed<V2UserLoginDialog>(*this, yubikey_required);

    login_dialog->signal_response().connect([this, login_dialog, vault_path, yubikey_required](int response) {
        if (response != Gtk::ResponseType::OK) {
            login_dialog->hide();
            return;
        }

        // Get credentials from dialog
        auto creds = login_dialog->get_credentials();
        login_dialog->hide();

#ifdef HAVE_YUBIKEY_SUPPORT
        // Show YubiKey touch prompt if required
        YubiKeyPromptDialog* touch_dialog = nullptr;
        if (yubikey_required) {
            touch_dialog = Gtk::make_managed<YubiKeyPromptDialog>(*this,
                YubiKeyPromptDialog::PromptType::TOUCH);
            touch_dialog->present();

            // Force GTK to process events and render the dialog
            auto context = Glib::MainContext::get_default();
            while (context->pending()) {
                context->iteration(false);
            }
            g_usleep(150000);  // 150ms delay for dialog rendering
        }
#endif

        // Attempt V2 vault authentication
        auto result = m_vault_manager->open_vault_v2(vault_path, creds.username, creds.password);

        // Clear credentials immediately after use
        creds.clear();

#ifdef HAVE_YUBIKEY_SUPPORT
        // Hide touch prompt if shown
        if (touch_dialog) {
            touch_dialog->hide();
        }
#endif

        if (!result) {
            // Authentication failed - show error
            std::string error_message = "Authentication failed";
            if (result.error() == KeepTower::VaultError::AuthenticationFailed) {
                error_message = "Invalid username or password";
            } else if (result.error() == KeepTower::VaultError::UserNotFound) {
                error_message = "User not found";
            }

            show_error_dialog(error_message);
            return;
        }

        // Successfully authenticated - check for password change requirement
        KeepTower::Log::info("MainWindow: Authentication succeeded, getting user session");
        auto session_opt = m_vault_manager->get_current_user_session();
        if (!session_opt) {
            KeepTower::Log::error("MainWindow: No session after successful authentication!");
            show_error_dialog("Internal error: No session after successful authentication");
            return;
        }

        const auto& session = *session_opt;
        KeepTower::Log::info("MainWindow: Session obtained - username='{}', password_change_required={}",
            session.username, session.password_change_required);

        // Check if password change is required (first login with temporary password)
        if (session.password_change_required) {
            KeepTower::Log::info("MainWindow: Password change required, calling handler");
            handle_password_change_required(session.username);
            return;  // Stop here - vault setup will complete after password change
        }

        // Complete vault opening
        KeepTower::Log::info("MainWindow: About to call complete_vault_opening()");
        complete_vault_opening(vault_path, session.username);
        KeepTower::Log::info("MainWindow: Returned from complete_vault_opening()");
    });

    login_dialog->show();
}

void MainWindow::handle_password_change_required(const std::string& username) {
    // Get vault security policy for min password length
    auto policy_opt = m_vault_manager->get_vault_security_policy();
    const uint32_t min_length = policy_opt ? policy_opt->min_password_length : 12;

    // Show password change dialog in forced mode
    auto change_dialog = Gtk::make_managed<ChangePasswordDialog>(*this, min_length, true);

    change_dialog->signal_response().connect([this, change_dialog, username, min_length](int response) {
        if (response != Gtk::ResponseType::OK) {
            // User cancelled - close vault (cannot use without changing password)
            change_dialog->hide();
            on_close_vault();
            show_error_dialog("Password change is required to access this vault.\nVault has been closed.");
            return;
        }

        // Get new password
        auto req = change_dialog->get_request();
        change_dialog->hide();

        // Validate password BEFORE showing YubiKey prompt
        // This allows fail-fast for invalid passwords without YubiKey interaction
        auto validation = m_vault_manager->validate_new_password(username, req.new_password);
        if (!validation) {
            // Validation failed - show error and retry
            std::string error_msg = "Failed to validate password";
            if (validation.error() == KeepTower::VaultError::WeakPassword) {
                error_msg = "New password must be at least " + std::to_string(min_length) + " characters";
            } else if (validation.error() == KeepTower::VaultError::PasswordReused) {
                error_msg = "This password was used previously. Please choose a different password.";
            }

            auto error_dialog = Gtk::make_managed<Gtk::MessageDialog>(
                *this, error_msg, false, Gtk::MessageType::ERROR, Gtk::ButtonsType::OK, true);
            error_dialog->signal_response().connect([this, username](int) {
                // Retry after error
                handle_password_change_required(username);
            });
            error_dialog->present();

            req.clear();
            return;
        }

#ifdef HAVE_YUBIKEY_SUPPORT
        // Validation passed - now show YubiKey prompt if enrolled
        YubiKeyPromptDialog* touch_dialog = nullptr;
        auto users = m_vault_manager->list_users();
        for (const auto& user : users) {
            if (user.username == username && user.yubikey_enrolled) {
                touch_dialog = Gtk::make_managed<YubiKeyPromptDialog>(*this,
                    YubiKeyPromptDialog::PromptType::TOUCH);
                touch_dialog->present();

                // Force GTK to process events and render the dialog
                auto context = Glib::MainContext::get_default();
                while (context->pending()) {
                    context->iteration(false);
                }
                g_usleep(150000);  // 150ms to ensure dialog is visible
                break;
            }
        }
#endif

        // Attempt password change (password already validated, just needs YubiKey operations)
        auto result = m_vault_manager->change_user_password(username, req.current_password, req.new_password);

#ifdef HAVE_YUBIKEY_SUPPORT
        if (touch_dialog) {
            touch_dialog->hide();
        }
#endif

        // Clear passwords immediately
        req.clear();

        if (!result) {
            // Password change failed
            std::string error_msg = "Failed to change password";
            if (result.error() == KeepTower::VaultError::WeakPassword) {
                error_msg = "New password must be at least " + std::to_string(min_length) + " characters";
            } else if (result.error() == KeepTower::VaultError::PasswordReused) {
                error_msg = "This password was used previously. Please choose a different password.";
            }

            // Show error dialog, then retry after it's dismissed
            auto error_dialog = Gtk::make_managed<Gtk::MessageDialog>(
                *this, error_msg, false, Gtk::MessageType::ERROR, Gtk::ButtonsType::OK, true);
            error_dialog->set_title("Password Change Failed");
            error_dialog->signal_response().connect([this, error_dialog, username](int) {
                error_dialog->hide();
                // Retry password change after error dialog is dismissed
                handle_password_change_required(username);
            });
            error_dialog->show();
            return;
        }

        // Password changed successfully - save vault
        on_save_vault();

        // Check if YubiKey enrollment is now required
        auto session_opt = m_vault_manager->get_current_user_session();
        if (session_opt && session_opt->requires_yubikey_enrollment) {
            // Show YubiKey enrollment dialog (required by policy)
            handle_yubikey_enrollment_required(username);
            return;
        }

        // Complete vault opening
        complete_vault_opening(m_current_vault_path, username);
    });

    change_dialog->show();
}

void MainWindow::handle_yubikey_enrollment_required(const std::string& username) {
#ifdef HAVE_YUBIKEY_SUPPORT
    // Show message dialog explaining YubiKey enrollment requirement
    auto info_dialog = Gtk::make_managed<Gtk::MessageDialog>(
        *this,
        "YubiKey enrollment is required by vault policy.\n\n"
        "You must enroll your YubiKey to access this vault.\n\n"
        "Please ensure your YubiKey is connected, then click OK to continue.",
        false,
        Gtk::MessageType::INFO,
        Gtk::ButtonsType::OK_CANCEL,
        true);
    info_dialog->set_title("YubiKey Enrollment Required");

    info_dialog->signal_response().connect([this, info_dialog, username](int response) {
        info_dialog->hide();

        if (response != Gtk::ResponseType::OK) {
            // User cancelled - close vault
            on_close_vault();
            show_error_dialog("YubiKey enrollment is required.\nVault has been closed.");
            return;
        }

        // Get user's current password (they just changed it)
        auto pwd_dialog = Gtk::make_managed<Gtk::MessageDialog>(
            *this,
            "Enter your password to enroll YubiKey:",
            false,
            Gtk::MessageType::QUESTION,
            Gtk::ButtonsType::OK_CANCEL,
            true);
        pwd_dialog->set_title("Password Required");

        auto* entry = Gtk::make_managed<Gtk::Entry>();
        entry->set_visibility(false);
        entry->set_activates_default(true);
        pwd_dialog->get_content_area()->append(*entry);
        pwd_dialog->set_default_response(Gtk::ResponseType::OK);

        pwd_dialog->signal_response().connect([this, pwd_dialog, entry, username](int pwd_response) {
            if (pwd_response != Gtk::ResponseType::OK) {
                pwd_dialog->hide();
                on_close_vault();
                show_error_dialog("YubiKey enrollment cancelled.\nVault has been closed.");
                return;
            }

            auto password = entry->get_text();
            pwd_dialog->hide();

            // Show YubiKey touch prompt
            auto touch_dialog = Gtk::make_managed<YubiKeyPromptDialog>(*this,
                YubiKeyPromptDialog::PromptType::TOUCH);
            touch_dialog->present();

            // Force GTK to process events
            auto context = Glib::MainContext::get_default();
            while (context->pending()) {
                context->iteration(false);
            }
            g_usleep(150000);  // 150ms delay

            // Attempt YubiKey enrollment
            auto result = m_vault_manager->enroll_yubikey_for_user(username, password);

            // Clear password
            entry->set_text("");
            password = "";

            touch_dialog->hide();

            if (!result) {
                std::string error_msg = "Failed to enroll YubiKey";
                if (result.error() == KeepTower::VaultError::YubiKeyNotPresent) {
                    error_msg = "YubiKey not detected. Please connect your YubiKey and try again.";
                } else if (result.error() == KeepTower::VaultError::AuthenticationFailed) {
                    error_msg = "Incorrect password.";
                }

                // Show error and retry
                auto error_dialog = Gtk::make_managed<Gtk::MessageDialog>(
                    *this, error_msg, false, Gtk::MessageType::ERROR, Gtk::ButtonsType::OK, true);
                error_dialog->set_title("Enrollment Failed");
                error_dialog->signal_response().connect([this, error_dialog, username](int) {
                    error_dialog->hide();
                    handle_yubikey_enrollment_required(username);  // Retry
                });
                error_dialog->show();
                return;
            }

            // YubiKey enrolled successfully
            on_save_vault();
            complete_vault_opening(m_current_vault_path, username);

            // Show success message
            auto success_dialog = Gtk::make_managed<Gtk::MessageDialog>(
                *this,
                "YubiKey enrolled successfully!\n\nYour YubiKey will be required for all future logins.",
                false,
                Gtk::MessageType::INFO,
                Gtk::ButtonsType::OK,
                true);
            success_dialog->set_title("Enrollment Complete");
            success_dialog->signal_response().connect([success_dialog](int) {
                success_dialog->hide();
            });
            success_dialog->show();
        });

        pwd_dialog->show();
    });

    info_dialog->show();
#else
    // YubiKey support not compiled - close vault
    on_close_vault();
    show_error_dialog("YubiKey enrollment required but YubiKey support is not available.\nVault has been closed.");
#endif
}

void MainWindow::complete_vault_opening(const std::string& vault_path, const std::string& username) {
    KeepTower::Log::info("MainWindow: complete_vault_opening() called - vault_path='{}', username='{}'", vault_path, username);

    // Vault opened successfully
    KeepTower::Log::info("MainWindow: Setting member variables");
    m_current_vault_path = vault_path;
    m_vault_open = true;
    m_is_locked = false;

    KeepTower::Log::info("MainWindow: Setting button sensitivities");
    m_save_button.set_sensitive(true);
    m_close_button.set_sensitive(true);
    m_add_account_button.set_sensitive(true);
    m_search_entry.set_sensitive(true);

    // Update UI with session information
    KeepTower::Log::info("MainWindow: About to call update_session_display()");
    update_session_display();
    KeepTower::Log::info("MainWindow: Returned from update_session_display()");

    // Load vault data
    KeepTower::Log::info("MainWindow: About to call update_account_list()");
    update_account_list();
    KeepTower::Log::info("MainWindow: About to call update_tag_filter_dropdown()");
    update_tag_filter_dropdown();

    // Initialize undo/redo state
    KeepTower::Log::info("MainWindow: Setting undo/redo sensitivity");
    update_undo_redo_sensitivity(false, false);

    // Start activity monitoring for auto-lock
    KeepTower::Log::info("MainWindow: Starting activity monitoring");
    on_user_activity();

    KeepTower::Log::info("MainWindow: Setting status label");
    m_status_label.set_text("Vault opened: " + vault_path + " (User: " + username + ")");
    KeepTower::Log::info("MainWindow: complete_vault_opening() completed successfully");
}

void MainWindow::update_session_display() {
    KeepTower::Log::info("MainWindow: update_session_display() called");

    auto session_opt = m_vault_manager->get_current_user_session();
    if (!session_opt) {
        // No active session (V1 vault or not logged in)
        KeepTower::Log::info("MainWindow: No active session, hiding session label");
        m_session_label.set_visible(false);
        return;
    }

    const auto& session = *session_opt;
    KeepTower::Log::info("MainWindow: Session found: username='{}', role={}, password_change_required={}",
        session.username, static_cast<int>(session.role), session.password_change_required);

    // Format session info: "User: alice (Admin)"
    std::string role_str = (session.role == KeepTower::UserRole::ADMINISTRATOR) ? "Admin" : "User";
    std::string session_text;

    try {
        session_text = "User: " + session.username + " (" + role_str + ")";

        if (session.password_change_required) {
            session_text += " [Password change required]";
        }

        KeepTower::Log::info("MainWindow: Setting session label text: '{}'", session_text);
        m_session_label.set_text(session_text);
        m_session_label.set_visible(true);
        KeepTower::Log::info("MainWindow: Session label updated successfully");
    } catch (const std::exception& e) {
        KeepTower::Log::error("MainWindow: Error updating session display: {}", e.what());
        m_session_label.set_text("User: " + session.username);
        m_session_label.set_visible(true);
    }

    // Phase 4: Update menu visibility based on role
    KeepTower::Log::info("MainWindow: Calling update_menu_for_role()");
    update_menu_for_role();
    KeepTower::Log::info("MainWindow: update_session_display() completed");
}

// ============================================================================
// Phase 4: Permissions & Role-Based UI
// ============================================================================

void MainWindow::on_change_my_password() {
    if (!m_vault_manager || !is_v2_vault_open()) {
        show_error_dialog("No V2 vault is open");
        return;
    }

    auto session_opt = m_vault_manager->get_current_user_session();
    if (!session_opt) {
        show_error_dialog("No active user session");
        return;
    }

    const auto& session = *session_opt;

    // Get vault security policy for password requirements
    auto policy_opt = m_vault_manager->get_vault_security_policy();
    const uint32_t min_length = policy_opt ? policy_opt->min_password_length : 12;

    // Show password change dialog (voluntary mode)
    auto* change_dialog = new ChangePasswordDialog(*this, min_length, false);  // false = voluntary

    change_dialog->signal_response().connect([this, change_dialog, username = session.username, min_length](int response) {
        if (response != Gtk::ResponseType::OK) {
            change_dialog->hide();
            delete change_dialog;
            return;
        }

        // Get new password
        auto req = change_dialog->get_request();
        change_dialog->hide();
        delete change_dialog;

        // Validate password BEFORE showing YubiKey prompt
        // This allows fail-fast for invalid passwords without YubiKey interaction
        auto validation = m_vault_manager->validate_new_password(username, req.new_password);
        if (!validation) {
            // Validation failed - show error
            std::string error_msg = "Failed to validate password";
            if (validation.error() == KeepTower::VaultError::WeakPassword) {
                error_msg = "New password must be at least " + std::to_string(min_length) + " characters";
            } else if (validation.error() == KeepTower::VaultError::PasswordReused) {
                error_msg = "This password was used previously. Please choose a different password.";
            }

            show_error_dialog(error_msg);
            req.clear();
            return;
        }

#ifdef HAVE_YUBIKEY_SUPPORT
        // Validation passed - now show YubiKey prompt if enrolled
        YubiKeyPromptDialog* touch_dialog = nullptr;
        auto users = m_vault_manager->list_users();
        for (const auto& user : users) {
            if (user.username == username && user.yubikey_enrolled) {
                touch_dialog = Gtk::make_managed<YubiKeyPromptDialog>(*this,
                    YubiKeyPromptDialog::PromptType::TOUCH);
                touch_dialog->present();

                // Force GTK to process events and render the dialog
                auto context = Glib::MainContext::get_default();
                while (context->pending()) {
                    context->iteration(false);
                }
                g_usleep(150000);  // 150ms to ensure dialog is visible
                break;
            }
        }
#endif

        // Attempt password change (password already validated, just needs YubiKey operations)
        auto result = m_vault_manager->change_user_password(username, req.current_password, req.new_password);

#ifdef HAVE_YUBIKEY_SUPPORT
        if (touch_dialog) {
            touch_dialog->hide();
        }
#endif

        // Clear passwords immediately
        req.clear();

        if (!result) {
            // Password change failed
            std::string error_msg = "Failed to change password";
            if (result.error() == KeepTower::VaultError::AuthenticationFailed) {
                error_msg = "Current password is incorrect";
            } else if (result.error() == KeepTower::VaultError::WeakPassword) {
                error_msg = "New password must be at least " + std::to_string(min_length) + " characters";
            } else if (result.error() == KeepTower::VaultError::PasswordReused) {
                error_msg = "This password was used previously. Please choose a different password.";
            }

            show_error_dialog(error_msg);
            return;
        }

        // Password changed successfully
        m_status_label.set_text("Password changed successfully");

        auto* success_dlg = new Gtk::MessageDialog(
            *this,
            "Password changed successfully",
            false,
            Gtk::MessageType::INFO
        );
        success_dlg->set_modal(true);
        success_dlg->signal_response().connect([success_dlg](int) {
            success_dlg->hide();
            delete success_dlg;
        });
        success_dlg->show();
    });

    change_dialog->show();
}

void MainWindow::on_logout() {
    if (!m_vault_manager || !is_v2_vault_open()) {
        return;
    }

    // Prompt to save if modified
    if (!prompt_save_if_modified()) {
        return;  // User cancelled
    }

    // Save vault before logout (prompt_save_if_modified already handles saving)

    // Close vault (this logs out the user)
    std::string vault_path = m_current_vault_path;  // Save path before close
    on_close_vault();

    // Reopen the same vault file (will show login dialog)
    if (!vault_path.empty()) {
        handle_v2_vault_open(vault_path);
    }
}

void MainWindow::on_manage_users() {
    if (!m_vault_manager || !is_v2_vault_open()) {
        show_error_dialog("No V2 vault is open");
        return;
    }

    // Check if current user is administrator
    if (!is_current_user_admin()) {
        show_error_dialog("Only administrators can manage users");
        return;
    }

    auto session_opt = m_vault_manager->get_current_user_session();
    if (!session_opt) {
        show_error_dialog("No active user session");
        return;
    }

    // Show user management dialog
    auto* dialog = new UserManagementDialog(*this, *m_vault_manager, session_opt->username);

    // Handle relogin request
    dialog->m_signal_request_relogin.connect([this](const std::string& new_username) {
        // Store vault path before closing
        std::string vault_path = m_current_vault_path;

        // Close current vault (logout)
        on_close_vault();

        // Reopen vault with login dialog (it will default to the last username)
        handle_v2_vault_open(vault_path);
    });

    dialog->signal_response().connect([dialog](int) {
        dialog->hide();
        delete dialog;
    });

    dialog->show();
}

void MainWindow::update_menu_for_role() {
    KeepTower::Log::info("MainWindow: update_menu_for_role() called");

    // Only update if V2 vault is open
    if (!is_v2_vault_open()) {
        KeepTower::Log::info("MainWindow: No V2 vault open, disabling V2-specific menu actions");
        // Disable all V2-specific actions for V1 vaults or when no vault is open
        m_change_password_action->set_enabled(false);
        m_logout_action->set_enabled(false);
        m_manage_users_action->set_enabled(false);
        // For V1 vaults, export is allowed (single-user)
        m_export_action->set_enabled(true);
        KeepTower::Log::info("MainWindow: update_menu_for_role() completed (no V2 vault)");
        return;
    }

    KeepTower::Log::info("MainWindow: V2 vault detected, updating menu");
    // Enable change password and logout for all V2 users
    m_change_password_action->set_enabled(true);
    m_logout_action->set_enabled(true);

    // Enable user management and export only for administrators
    bool is_admin = is_current_user_admin();
    KeepTower::Log::info("MainWindow: User is admin: {}", is_admin);
    m_manage_users_action->set_enabled(is_admin);
    m_export_action->set_enabled(is_admin);
    KeepTower::Log::info("MainWindow: update_menu_for_role() completed (V2 mode)");
}

bool MainWindow::is_v2_vault_open() const noexcept {
    bool has_manager = (m_vault_manager != nullptr);
    bool vault_open_flag = m_vault_open;
    bool is_v2 = has_manager && m_vault_manager->is_v2_vault();

    KeepTower::Log::info("MainWindow: is_v2_vault_open() check - manager={}, m_vault_open={}, is_v2_vault()={}",
        has_manager, vault_open_flag, is_v2);

    if (!m_vault_manager || !m_vault_open) {
        return false;
    }

    // Check vault format directly - more reliable than session check
    return m_vault_manager->is_v2_vault();
}

bool MainWindow::is_current_user_admin() const noexcept {
    if (!m_vault_manager) {
        return false;
    }

    auto session_opt = m_vault_manager->get_current_user_session();
    if (!session_opt) {
        return false;
    }

    return session_opt->is_admin();
}
