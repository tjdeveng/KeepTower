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
#include "../../core/repositories/AccountRepository.h"
#include "../../core/repositories/GroupRepository.h"
#include "../../core/services/AccountService.h"
#include "../../core/services/GroupService.h"
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
#include "../../core/managers/YubiKeyManager.h"
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
      m_vault_manager(std::make_unique<VaultManager>()),
      m_account_controller(nullptr),  // Initialized after vault_manager
      m_search_controller(std::make_unique<SearchController>()) {

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
                    [this, gtk_settings]([[maybe_unused]] const Glib::ustring& key) {
                        auto system_color_scheme = m_desktop_settings->get_string("color-scheme");
                        gtk_settings->property_gtk_application_prefer_dark_theme() = (system_color_scheme == "prefer-dark");
                    }
                );
            } catch (const std::exception& e) {
                KeepTower::Log::debug("MainWindow: Could not monitor theme changes: {}", e.what());
            } catch (...) {
                KeepTower::Log::debug("MainWindow: Could not monitor theme changes (unknown error)");
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
    settings->signal_changed("color-scheme").connect([this]([[maybe_unused]] const Glib::ustring& key) {
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

    // Phase 5: Create primary menu via MenuManager
    m_primary_menu = m_menu_manager->create_primary_menu();

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

    // Phase 1: Initialize view controllers
    m_account_controller = std::make_unique<AccountViewController>(m_vault_manager.get());

    // Connect AccountViewController signals
    m_account_controller->signal_list_updated().connect(
        [this](const auto& accounts, const auto& groups, [[maybe_unused]] size_t total) {
            // Update the tree widget with filtered accounts
            if (m_account_tree_widget) {
                m_account_tree_widget->set_data(groups, accounts);
            }
            // Update status label
            std::string status = m_vault_open ?
                ("Vault opened: " + m_current_vault_path + " (" + std::to_string(accounts.size()) + " accounts)") :
                "No vault open";
            m_status_label.set_text(status);
        });

    m_account_controller->signal_error().connect(
        [this](const std::string& error_msg) {
            show_error_dialog(error_msg);
        });

    // Phase 1.3: Initialize security controllers
    m_auto_lock_manager = std::make_unique<KeepTower::AutoLockManager>();
    m_clipboard_manager = std::make_unique<KeepTower::ClipboardManager>(get_clipboard());

    // Phase 5: Initialize DialogManager
    m_dialog_manager = std::make_unique<UI::DialogManager>(*this, m_vault_manager.get());

    // Phase 5: Initialize MenuManager
    m_menu_manager = std::make_unique<UI::MenuManager>(*this, m_vault_manager.get());

    // Phase 5: Initialize UIStateManager
    UI::UIStateManager::UIWidgets widgets{
        &m_save_button,
        &m_close_button,
        &m_add_account_button,
        &m_search_entry,
        &m_status_label,
        &m_session_label
    };
    m_ui_state_manager = std::make_unique<UI::UIStateManager>(widgets, m_vault_manager.get());

    // Phase 5: Initialize V2AuthenticationHandler
    m_v2_auth_handler = std::make_unique<UI::V2AuthenticationHandler>(
        *this, m_vault_manager.get(), m_dialog_manager.get(), m_clipboard_manager.get());

    // Phase 5: Initialize VaultIOHandler
    m_vault_io_handler = std::make_unique<UI::VaultIOHandler>(
        *this, m_vault_manager.get(), m_dialog_manager.get());

    // Phase 5h: Initialize YubiKeyHandler
    m_yubikey_handler = std::make_unique<UI::YubiKeyHandler>(
        *this, m_vault_manager.get());

    // Phase 5i: Initialize GroupHandler
    m_group_handler = std::make_unique<UI::GroupHandler>(
        *this,
        m_vault_manager.get(),
        m_group_service.get(),
        m_dialog_manager.get(),
        [this](const std::string& message) { m_status_label.set_text(message); },
        [this]() { update_account_list(); }
    );

    // Phase 5j: Initialize AccountEditHandler
    m_account_edit_handler = std::make_unique<UI::AccountEditHandler>(
        *this,
        m_vault_manager.get(),
        &m_undo_manager,
        m_dialog_manager.get(),
        m_account_detail_widget.get(),
        &m_search_entry,
        [this](const std::string& message) { m_status_label.set_text(message); },
        [this]() {
            clear_account_details();
            update_account_list();
            filter_accounts(m_search_entry.get_text());
        },
        [this]() { return m_selected_account_index; },
        [this]() { return is_undo_redo_enabled(); },
        [this](const std::string& account_id) {
            // Select account by ID in the tree widget
            if (m_account_tree_widget) {
                m_account_tree_widget->select_account_by_id(account_id);
            }
        }
    );

    // Phase 5k: Initialize AutoLockHandler
    m_auto_lock_handler = std::make_unique<UI::AutoLockHandler>(
        *this,
        m_vault_manager.get(),
        m_auto_lock_manager.get(),
        m_dialog_manager.get(),
        m_ui_state_manager.get(),
        m_vault_open,
        m_is_locked,
        m_current_vault_path,
        m_cached_master_password,
        [this]() { save_current_account(); },
        [this]() { on_close_vault(); },
        [this]() { update_account_list(); },
        [this](const Glib::ustring& text) { filter_accounts(text); },
        [this](const std::string& path) { handle_v2_vault_open(path); },
        [this]() { return is_v2_vault_open(); },
        [this]() { return m_vault_manager && m_vault_manager->is_modified(); },
        [this]() { return m_search_entry.get_text(); }
    );

    // Phase 5l: Initialize UserAccountHandler
    m_user_account_handler = std::make_unique<UI::UserAccountHandler>(
        *this,
        m_vault_manager.get(),
        m_dialog_manager.get(),
        m_clipboard_manager.get(),
        m_current_vault_path,
        [this](const std::string& message) { m_status_label.set_text(message); },
        [this](const std::string& message) { show_error_dialog(message); },
        [this]() { on_close_vault(); },
        [this](const std::string& path) { handle_v2_vault_open(path); },
        [this]() { return is_v2_vault_open(); },
        [this]() { return is_current_user_admin(); },
        [this]() { return prompt_save_if_modified(); }
    );

    // Phase 5l: Initialize VaultOpenHandler
    m_vault_open_handler = std::make_unique<UI::VaultOpenHandler>(
        *this,
        m_vault_manager.get(),
        m_dialog_manager.get(),
        m_ui_state_manager.get(),
        m_vault_open,
        m_is_locked,
        m_current_vault_path,
        m_cached_master_password,
        [this](const std::string& message) { show_error_dialog(message); },
        [this](const std::string& message, const std::string& title) {
            m_dialog_manager->show_info_dialog(message, title);
        },
        [this](const std::string& path) { return detect_vault_version(path); },
        [this](const std::string& path) { handle_v2_vault_open(path); },
        [this]() { initialize_repositories(); },
        [this]() { update_account_list(); },
        [this]() { update_tag_filter_dropdown(); },
        [this]() { clear_account_details(); },
        [this](bool can_undo, bool can_redo) { update_undo_redo_sensitivity(can_undo, can_redo); },
        [this]() { update_menu_for_role(); },
        [this]() { update_session_display(); },
        [this]() { on_user_activity(); }
    );

    // Phase 5: Setup window actions via MenuManager (after MenuManager is initialized)
    std::map<std::string, std::function<void()>> action_callbacks = {
        {"preferences", [this]() { on_preferences(); }},
        {"import-csv", [this]() { on_import_from_csv(); }},
        {"migrate-v1-to-v2", [this]() { on_migrate_v1_to_v2(); }},
        {"delete-account", [this]() { on_delete_account(); }},
        {"create-group", [this]() { on_create_group(); }},
        {"rename-group", [this]() {
            if (!m_context_menu_group_id.empty() && m_vault_manager) {
                auto groups = m_vault_manager->get_all_groups();
                for (const auto& group : groups) {
                    if (group.group_id() == m_context_menu_group_id) {
                        on_rename_group(m_context_menu_group_id, group.group_name());
                        break;
                    }
                }
            }
        }},
        {"delete-group", [this]() {
            if (!m_context_menu_group_id.empty()) {
                on_delete_group(m_context_menu_group_id);
            }
        }},
        {"undo", [this]() { on_undo(); }},
        {"redo", [this]() { on_redo(); }},
#ifdef HAVE_YUBIKEY_SUPPORT
        {"test-yubikey", [this]() { on_test_yubikey(); }},
        {"manage-yubikeys", [this]() { on_manage_yubikeys(); }},
#endif
    };
    m_menu_manager->setup_actions(action_callbacks);

    // Setup help menu actions
    m_menu_manager->setup_help_actions();

    // Setup V2-specific actions separately to capture return values
    m_export_action = add_action("export-csv", sigc::mem_fun(*this, &MainWindow::on_export_to_csv));
    m_change_password_action = add_action("change-password", sigc::mem_fun(*this, &MainWindow::on_change_my_password));
    m_logout_action = add_action("logout", sigc::mem_fun(*this, &MainWindow::on_logout));
    m_manage_users_action = add_action("manage-users", sigc::mem_fun(*this, &MainWindow::on_manage_users));

    // Pass action references to MenuManager for enable/disable
    m_menu_manager->set_action_references(
        m_export_action,
        m_change_password_action,
        m_logout_action,
        m_manage_users_action
    );

    // Initially disable V2-only actions
    m_change_password_action->set_enabled(false);
    m_logout_action->set_enabled(false);
    m_manage_users_action->set_enabled(false);

    // Setup keyboard shortcuts via MenuManager
    m_menu_manager->setup_keyboard_shortcuts(get_application());

    // Connect AutoLockManager signals
    m_auto_lock_manager->signal_auto_lock_triggered().connect(
        [this]() {
            on_auto_lock_timeout();  // Returns bool but signal is void
        });

    // Connect ClipboardManager signals
    m_clipboard_manager->signal_copied().connect(
        [this]() {
            // Update status to show password copied (don't show the password itself)
            m_status_label.set_text("Password copied to clipboard");
        });

    m_clipboard_manager->signal_cleared().connect(
        [this]() {
            // Update status when clipboard is cleared
            if (m_vault_open) {
                m_status_label.set_text("Clipboard cleared");
            }
        });

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
                        KeepTower::Log::warning("MainWindow: Could not find account with id: {}", account_id);
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

    // Phase 5k: Setup activity monitoring for auto-lock via AutoLockHandler
    m_auto_lock_handler->setup_activity_monitoring();

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

    // Phase 1.3: Clear clipboard and stop auto-lock using controllers
    if (m_clipboard_manager) {
        m_clipboard_manager->clear_immediately();
    }

    if (m_auto_lock_manager) {
        m_auto_lock_manager->stop();
    }

    // Clear cached password
    if (!m_cached_master_password.empty()) {
        std::fill(m_cached_master_password.begin(), m_cached_master_password.end(), '\0');
        m_cached_master_password.clear();
    }
}

void MainWindow::on_new_vault() {
    // Phase 5l: Delegate to VaultOpenHandler
    if (m_vault_open_handler) {
        m_vault_open_handler->handle_new_vault();
    }
}

void MainWindow::on_open_vault() {
    // Phase 5l: Delegate to VaultOpenHandler
    if (m_vault_open_handler) {
        m_vault_open_handler->handle_open_vault();
    }
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

    // Phase 1.3: Clear clipboard and stop auto-lock using controllers
    if (m_clipboard_manager) {
        m_clipboard_manager->clear_immediately();
    }

    if (m_auto_lock_manager) {
        m_auto_lock_manager->stop();
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

    // Phase 2: Reset repositories
    reset_repositories();

    // Phase 5: Use UIStateManager for state management
    m_ui_state_manager->set_vault_closed();

    // Reset local state cache to maintain consistency
    m_vault_open = false;
    m_is_locked = false;
    m_current_vault_path.clear();

    // Phase 4: Reset V2 UI elements
    update_menu_for_role();  // Disable V2-specific menu items

    // Clear widget-based UI
    if (m_account_tree_widget) {
        m_account_tree_widget->set_data({}, {});
    }
    clear_account_details();
}

void MainWindow::on_migrate_v1_to_v2() {
    // Validation: Must have V1 vault open
    if (!m_vault_open) {
        show_error_dialog("No vault is currently open.\nPlease open a vault first.");
        return;
    }

    // Phase 5g: Delegate to VaultIOHandler
    m_vault_io_handler->handle_migration(m_current_vault_path, m_vault_open, [this]() {
        update_session_display();
        if (m_manage_users_action) {
            m_manage_users_action->set_enabled(true);
        }
    });
}

void MainWindow::on_add_account() {
    if (!m_vault_open) {
        m_status_label.set_text("Please open or create a vault first");
        return;
    }

    // Save the current account before creating a new one
    save_current_account();

    // Phase 5j: Delegate to AccountEditHandler
    m_account_edit_handler->handle_add();
}

void MainWindow::on_copy_password() {
    const std::string password = m_account_detail_widget->get_password();

    if (password.empty()) {
        constexpr std::string_view no_password_msg{"No password to copy"};
        m_status_label.set_text(std::string{no_password_msg});
        return;
    }

    // Phase 1.3: Use ClipboardManager for secure clipboard handling
    if (m_clipboard_manager) {
        // Get validated clipboard timeout from settings
        auto settings = Gio::Settings::create("com.tjdeveng.keeptower");
        const int timeout_seconds = SettingsValidator::get_clipboard_timeout(settings);
        m_clipboard_manager->set_clear_timeout_seconds(timeout_seconds);

        // Copy password (will auto-clear after timeout)
        m_clipboard_manager->copy_text(password);

        // Status updated via signal handler
        const std::string copied_msg = std::format("Password copied to clipboard (will clear in {}s)", timeout_seconds);
        m_status_label.set_text(copied_msg);
    }
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
    // Phase 1: Delegate to AccountViewController
    if (!m_account_controller) {
        return;
    }

    // Refresh account list through controller
    // The controller will emit signal_list_updated which we connected to update the UI
    m_account_controller->refresh_account_list();

    // Update tag filter dropdown with current tags
    update_tag_filter_dropdown();
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
        KeepTower::Log::warning("MainWindow::display_account_details - account is null at index {}", index);
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

/**
 * @brief Save changes to the currently selected account
 * @return true if save succeeded or nothing to save, false if validation failed
 *
 * Phase 3: Uses AccountService for comprehensive validation including:
 * - Empty account name check
 * - Field length limits (name, username, password, email, website, notes)
 * - Email format validation (if email provided)
 *
 * Updates account fields and maintains password history if configured.
 * Displays user-friendly error dialogs for validation failures.
 *
 * @note Does nothing if no account selected or vault closed
 */
bool MainWindow::save_current_account() {
    // Only save if we have a valid account selected
    if (m_selected_account_index < 0 || !m_vault_open) {
        return true;  // Nothing to save, allow continue
    }

    // Validate the index is within bounds
    const auto accounts = m_vault_manager->get_all_accounts();
    if (m_selected_account_index >= static_cast<int>(accounts.size())) {
        KeepTower::Log::warning("Invalid account index {} (total accounts: {})",
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

    // Get the current account from VaultManager
    auto* account = m_vault_manager->get_account_mutable(m_selected_account_index);
    if (!account) {
        KeepTower::Log::warning("Failed to get account at index {}", m_selected_account_index);
        return true;  // Allow navigation even if account not found
    }

    // Create a temporary account record with new values for validation
    keeptower::AccountRecord temp_account = *account;
    temp_account.set_account_name(account_name);
    temp_account.set_user_name(user_name);
    temp_account.set_password(password);
    temp_account.set_email(email);
    temp_account.set_website(website);
    temp_account.set_notes(notes);

    // Phase 3: Use AccountService for validation
    if (m_account_service) {
        auto validation_result = m_account_service->validate_account(temp_account);
        if (!validation_result) {
            // Convert service error to user-friendly message
            std::string error_msg;
            switch (validation_result.error()) {
                case KeepTower::ServiceError::VALIDATION_FAILED:
                    error_msg = "Account name cannot be empty.";
                    break;
                case KeepTower::ServiceError::FIELD_TOO_LONG:
                    error_msg = "One or more fields exceed maximum length.\n\n"
                               "Maximum lengths:\n"
                               "• Account Name: " + std::to_string(UI::MAX_ACCOUNT_NAME_LENGTH) + "\n"
                               "• Username: " + std::to_string(UI::MAX_USERNAME_LENGTH) + "\n"
                               "• Password: " + std::to_string(UI::MAX_PASSWORD_LENGTH) + "\n"
                               "• Email: " + std::to_string(UI::MAX_EMAIL_LENGTH) + "\n"
                               "• Website: " + std::to_string(UI::MAX_WEBSITE_LENGTH) + "\n"
                               "• Notes: " + std::to_string(UI::MAX_NOTES_LENGTH);
                    break;
                case KeepTower::ServiceError::INVALID_EMAIL:
                    error_msg = "Invalid email format.\n\n"
                               "Email must be in the format: user@domain.ext\n\n"
                               "Examples:\n"
                               "  • john@example.com\n"
                               "  • jane.doe@company.co.uk\n"
                               "  • user+tag@mail.example.org";
                    break;
                default:
                    error_msg = "Validation error: " + std::string(KeepTower::to_string(validation_result.error()));
                    break;
            }
            show_error_dialog(error_msg);
            return false;
        }
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

    // Check password history settings
    auto settings = Gio::Settings::create("com.tjdeveng.keeptower");
    const bool history_enabled = SettingsValidator::is_password_history_enabled(settings);
    const int history_limit = SettingsValidator::get_password_history_limit(settings);

    // Check if password changed and prevent reuse
    if (password != old_password && history_enabled) {
        // Check against previous passwords to prevent reuse
        for (int i = 0; i < account->password_history_size(); ++i) {
            if (account->password_history(i) == password) {
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
    account->set_password(password);
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
    auto current_length = static_cast<int>(value.length());

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
        KeepTower::Log::warning("Email validation regex error: {}", e.what());
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

// Phase 5: Delegate to DialogManager for consistent dialog handling
void MainWindow::show_error_dialog(const Glib::ustring& message) {
    if (m_dialog_manager) {
        m_dialog_manager->show_error_dialog(message.raw());
    }
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
    if (!m_vault_open) {
        return;
    }

    // Phase 5j: Delegate to AccountEditHandler
    m_account_edit_handler->handle_delete(m_context_menu_account_id);

    // Clear context menu state
    m_context_menu_account_id.clear();
}

void MainWindow::on_import_from_csv() {
    if (!m_vault_open) {
        show_error_dialog("Please open a vault first before importing accounts.");
        return;
    }

    // Phase 5g: Delegate to VaultIOHandler
    m_vault_io_handler->handle_import([this]() {
        update_account_list();
        filter_accounts(m_search_entry.get_text());
    });
}

void MainWindow::on_export_to_csv() {
    if (!m_vault_open) {
        show_error_dialog("Please open a vault first before exporting accounts.");
        return;
    }

    // Phase 5g: Delegate to VaultIOHandler
    m_vault_io_handler->handle_export(m_current_vault_path, m_vault_open);
}


void MainWindow::on_generate_password() {
    if (!m_vault_open || m_selected_account_index < 0) {
        return;
    }

    // Phase 5j: Delegate to AccountEditHandler
    m_account_edit_handler->handle_generate_password();
}

void MainWindow::on_user_activity() {
    // Phase 5k: Delegate to AutoLockHandler
    if (m_auto_lock_handler) {
        m_auto_lock_handler->handle_user_activity();
    }
}

bool MainWindow::on_auto_lock_timeout() {
    // Phase 5k: Delegate to AutoLockHandler
    if (m_auto_lock_handler) {
        return m_auto_lock_handler->handle_auto_lock_timeout();
    }
    return false;
}

void MainWindow::lock_vault() {
    // Phase 5k: Delegate to AutoLockHandler
    if (m_auto_lock_handler) {
        m_auto_lock_handler->lock_vault();
    }
}



#ifdef HAVE_YUBIKEY_SUPPORT
void MainWindow::on_test_yubikey() {
    // Phase 5h: Delegate to YubiKeyHandler
    m_yubikey_handler->handle_test();
}
#endif

#ifdef HAVE_YUBIKEY_SUPPORT
void MainWindow::on_manage_yubikeys() {
    // Check if vault is open first
    if (!m_vault_open) {
        auto dialog = Gtk::AlertDialog::create("No Vault Open");
        dialog->set_detail("Please open a vault first.");
        dialog->set_buttons({"OK"});
        dialog->choose(*this, {});
        return;
    }

    // Phase 5h: Delegate to YubiKeyHandler
    m_yubikey_handler->handle_manage();
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

/**
 * @brief Create a new account group with validation
 *
 * Phase 3: Uses GroupService for business logic validation:
 * - Empty name check
 * - Length limit (100 characters)
 * - Duplicate name detection
 *
 * Displays user-friendly error messages for validation failures.
 * On success, updates the account tree view and status label.
 *
 * @note Does nothing if vault is not open
 */
void MainWindow::on_create_group() {
    if (!m_vault_open) {
        return;
    }

    // Phase 5i: Delegate to GroupHandler
    m_group_handler->handle_create();
}

/**
 * @brief Rename an existing account group with validation
 * @param group_id Unique identifier of the group to rename
 * @param current_name Current name of the group (for dialog display)
 *
 * Phase 3: Uses GroupService for business logic validation:
 * - Empty name check
 * - Length limit (100 characters)
 * - Duplicate name detection
 * - Group existence verification
 *
 * Displays user-friendly error messages for validation failures.
 * On success, updates the account tree view and status label.
 *
 * @note Does nothing if vault is not open or group_id is empty
 */
void MainWindow::on_rename_group(const std::string& group_id, const Glib::ustring& current_name) {
    if (!m_vault_open || group_id.empty()) {
        return;
    }

    // Phase 5i: Delegate to GroupHandler
    m_group_handler->handle_rename(group_id, current_name);
}

void MainWindow::on_delete_group(const std::string& group_id) {
    if (!m_vault_open || group_id.empty()) {
        return;
    }

    // Phase 5i: Delegate to GroupHandler
    m_group_handler->handle_delete(group_id);
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
            if (!m_vault_manager->add_account_to_group(idx, target_group_id)) {
                KeepTower::Log::warning("Failed to add account to group");
                return;
            }
        }
    }

    // Defer UI refresh until after drag operation completes (next idle cycle)
    // This prevents destroying widgets while drag is still in progress
    Glib::signal_idle().connect_once([this]() {
        update_account_list();
    });
}

// Handle group drag-and-drop reorder
void MainWindow::on_group_reordered(const std::string& group_id, int new_index) {
    if (!m_vault_manager) return;
    if (!m_vault_manager->reorder_group(group_id, new_index)) {
        KeepTower::Log::warning("Failed to reorder group");
        return;
    }

    // Defer UI refresh until after drag operation completes
    Glib::signal_idle().connect_once([this]() {
        update_account_list();
    });
}

void MainWindow::show_account_context_menu(const std::string& account_id, Gtk::Widget* widget, double x, double y) {
    // Find the account index
    int account_index = find_account_index_by_id(account_id);
    if (account_index < 0) {
        return;
    }

    // Store account_id for use in callbacks
    m_context_menu_account_id = account_id;

    // Phase 5: Use MenuManager to create context menu
    auto popover = m_menu_manager->create_account_context_menu(
        account_id,
        account_index,
        widget,
        [this](const std::string& gid) {
            if (!m_context_menu_account_id.empty() && m_vault_manager) {
                int idx = find_account_index_by_id(m_context_menu_account_id);
                if (idx >= 0) {
                    if (m_vault_manager->add_account_to_group(idx, gid)) {
                        update_account_list();
                    }
                }
            }
        },
        [this](const std::string& gid) {
            if (!m_context_menu_account_id.empty() && m_vault_manager) {
                int idx = find_account_index_by_id(m_context_menu_account_id);
                if (idx >= 0) {
                    if (m_vault_manager->remove_account_from_group(idx, gid)) {
                        update_account_list();
                    }
                }
            }
        }
    );

    // Position at click location
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

    // Phase 5: Use MenuManager to create context menu
    auto popover = m_menu_manager->create_group_context_menu(group_id, widget);

    // Position at click location
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
    // Phase 5f: Delegate V2 authentication to handler
    m_v2_auth_handler->handle_vault_open(vault_path, [this](const std::string& path, const std::string& username) {
        // Save vault after successful authentication (for password changes, etc.)
        on_save_vault();
        // Complete vault opening
        complete_vault_opening(path, username);
    });
}





void MainWindow::complete_vault_opening(const std::string& vault_path, const std::string& username) {
    KeepTower::Log::info("MainWindow: complete_vault_opening() called - vault_path='{}', username='{}'", vault_path, username);

    // Phase 5: Use UIStateManager for state management
    KeepTower::Log::info("MainWindow: Setting vault opened state");
    m_ui_state_manager->set_vault_opened(vault_path, username);

    // Maintain local state cache for quick access without manager queries
    m_current_vault_path = vault_path;
    m_vault_open = true;
    m_is_locked = false;

    // Phase 2: Initialize repositories for data access
    KeepTower::Log::info("MainWindow: Initializing repositories");
    initialize_repositories();

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
    m_ui_state_manager->set_status("Vault opened: " + vault_path + " (User: " + username + ")");
    KeepTower::Log::info("MainWindow: complete_vault_opening() completed successfully");
}

void MainWindow::update_session_display() {
    KeepTower::Log::info("MainWindow: update_session_display() called");

    // Phase 5: Delegate to UIStateManager
    m_ui_state_manager->update_session_display([this]() {
        KeepTower::Log::info("MainWindow: Calling update_menu_for_role() from UIStateManager callback");
        update_menu_for_role();
    });

    KeepTower::Log::info("MainWindow: update_session_display() completed");
}

// ============================================================================
// Phase 4: Permissions & Role-Based UI
// ============================================================================

void MainWindow::on_change_my_password() {
    // Phase 5l: Delegate to UserAccountHandler
    if (m_user_account_handler) {
        m_user_account_handler->handle_change_password();
    }
}

void MainWindow::on_logout() {
    // Phase 5l: Delegate to UserAccountHandler
    if (m_user_account_handler) {
        m_user_account_handler->handle_logout();
    }
}

void MainWindow::on_manage_users() {
    // Phase 5l: Delegate to UserAccountHandler
    if (m_user_account_handler) {
        m_user_account_handler->handle_manage_users();
    }
}

void MainWindow::update_menu_for_role() {
    KeepTower::Log::info("MainWindow: update_menu_for_role() called");

    // Phase 5: Delegate to MenuManager
    bool is_v2 = is_v2_vault_open();
    bool is_admin = is_v2 && is_current_user_admin();
    m_menu_manager->update_menu_for_role(is_v2, is_admin, m_vault_open);

    KeepTower::Log::info("MainWindow: update_menu_for_role() completed (V2={}, Admin={})", is_v2, is_admin);
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

/**
 * @brief Initialize repositories after vault opening
 *
 * Creates AccountRepository and GroupRepository instances that provide
 * a data access abstraction layer over VaultManager. This is part of the
 * Phase 2 refactoring to introduce the Repository Pattern.
 *
 * The repositories provide:
 * - Clean separation between data access and business logic
 * - Consistent error handling with std::expected
 * - Testability through interface-based design
 * - Foundation for future service layer (Phase 3)
 *
 * @note Should be called immediately after successful vault opening
 * @note Logs warning and returns early if VaultManager is null
 */
void MainWindow::initialize_repositories() {
    if (!m_vault_manager) {
        KeepTower::Log::warning("Cannot initialize repositories: VaultManager is null");
        return;
    }

    KeepTower::Log::info("Initializing repositories for data access");
    m_account_repo = std::make_unique<KeepTower::AccountRepository>(m_vault_manager.get());
    m_group_repo = std::make_unique<KeepTower::GroupRepository>(m_vault_manager.get());

    // Phase 3: Initialize services after repositories
    initialize_services();
}

/**
 * @brief Reset repositories when vault is closed
 *
 * Destroys the repository instances to free resources and ensure that
 * no data access operations can be attempted on a closed vault.
 * Part of the Phase 2 refactoring cleanup process.
 *
 * @note Should be called before setting m_vault_open to false
 */
void MainWindow::reset_repositories() {
    KeepTower::Log::info("Resetting repositories (vault closed)");

    // Phase 3: Reset services before repositories
    reset_services();

    m_account_repo.reset();
    m_group_repo.reset();
}

/**
 * @brief Initialize services after repositories are created
 *
 * Creates AccountService and GroupService instances that wrap repositories
 * to provide business logic validation. Part of Phase 3 refactoring.
 *
 * @note Requires repositories to be initialized first
 */
void MainWindow::initialize_services() {
    if (!m_account_repo || !m_group_repo) {
        KeepTower::Log::warning("Cannot initialize services: repositories are null");
        return;
    }

    KeepTower::Log::info("Initializing services for business logic");
    m_account_service = std::make_unique<KeepTower::AccountService>(m_account_repo.get());
    m_group_service = std::make_unique<KeepTower::GroupService>(m_group_repo.get());
}

/**
 * @brief Reset services when vault is closed
 *
 * Destroys service instances to free resources.
 * Part of Phase 3 refactoring cleanup process.
 */
void MainWindow::reset_services() {
    KeepTower::Log::info("Resetting services (vault closed)");
    m_account_service.reset();
    m_group_service.reset();
}
