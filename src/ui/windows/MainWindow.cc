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
#include "../dialogs/PreferencesDialog.h"
#include "../dialogs/GroupCreateDialog.h"
#include "../dialogs/GroupRenameDialog.h"
#include "../dialogs/YubiKeyPromptDialog.h"
#include "../../core/VaultError.h"
#include "../../core/services/VaultFileService.h"
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

using KeepTower::safe_ustring_to_string;
#ifdef HAVE_YUBIKEY_SUPPORT
#include "../../lib/yubikey/YubiKeyManager.h"
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
      m_updating_selection(false),
      m_selected_account_index(-1),
      m_vault_manager(std::make_unique<VaultManager>()),
      m_account_controller(nullptr),  // Initialized after vault_manager
      m_search_controller(std::make_unique<SearchController>()) {

    // Set window properties
    set_title(PROJECT_NAME);
    set_default_size(1000, 700);

    // Load settings from GSettings
    auto settings = Gio::Settings::create("com.tjdeveng.keeptower");

    // Apply and monitor theme preference
    m_theme_controller = std::make_unique<ThemeController>(settings, Gtk::Settings::get_default());
    m_theme_controller->start();

    // Load Reed-Solomon settings as defaults for NEW vaults
    // Note: Opened vaults preserve their own FEC settings
    bool use_rs = settings->get_boolean("use-reed-solomon");
    int rs_redundancy = settings->get_int("rs-redundancy-percent");
    m_vault_manager->apply_default_fec_preferences(use_rs, rs_redundancy);

    // Load backup settings and apply to VaultManager
    const SettingsValidator::BackupPreferences backup_prefs =
        SettingsValidator::get_backup_preferences(settings);
    const VaultManager::BackupSettings backup_settings{
        backup_prefs.enabled,
        backup_prefs.count,
        backup_prefs.path
    };
    if (!m_vault_manager->apply_backup_settings(backup_settings)) {
        KeepTower::Log::warning("MainWindow: Invalid backup settings in preferences; using policy defaults");
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
    m_account_selection_coordinator = std::make_unique<AccountSelectionCoordinator>(
        m_vault_manager.get(),
        m_account_detail_widget.get(),
        m_selected_account_index,
        [this]() { return save_current_account(); },
        [this](const std::string& account_id) { return find_account_index_by_id(account_id); },
        [this]() { update_tag_filter_dropdown(); },
        [this]() { return is_current_user_admin(); }
    );

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
            const bool vault_open = m_vault_ui_coordinator && m_vault_ui_coordinator->vault_open();
            const Glib::ustring vault_path = (m_vault_ui_coordinator ? m_vault_ui_coordinator->current_vault_path() : Glib::ustring{});
            std::string status = vault_open ?
                ("Vault opened: " + vault_path + " (" + std::to_string(accounts.size()) + " accounts)") :
                "No vault open";
            m_status_label.set_text(KeepTower::make_valid_utf8(status, "status"));
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

    // Phase 5: Initialize VaultUiStateApplier
    UI::VaultUiStateApplier::UIWidgets widgets{
        &m_save_button,
        &m_close_button,
        &m_add_account_button,
        &m_search_entry,
        &m_status_label,
        &m_session_label
    };
    m_vault_ui_state_applier = std::make_unique<UI::VaultUiStateApplier>(widgets);

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
        [this](const std::string& message) { m_status_label.set_text(KeepTower::make_valid_utf8(message, "status")); },
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
        [this](const std::string& message) { m_status_label.set_text(KeepTower::make_valid_utf8(message, "status")); },
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

    // Issue #5: Vault UI coordinator (owns vault state + vault open wiring)
    m_vault_ui_coordinator = std::make_unique<VaultUiCoordinator>(
        *this,
        m_vault_manager.get(),
        m_dialog_manager.get(),
        m_menu_manager.get(),
        m_vault_ui_state_applier.get(),
        m_v2_auth_handler.get(),
        [this](const std::string& path) { return detect_vault_version(path); },
        [this]() { return save_current_account(); },
        [this]() { return prompt_save_if_modified(); },
        [this]() { initialize_repositories(); },
        [this]() { reset_repositories(); },
        [this]() { initialize_services(); },
        [this]() { reset_services(); },
        [this]() { update_account_list(); },
        [this]() { update_tag_filter_dropdown(); },
        [this]() { clear_account_details(); },
        [this]() {
            if (m_account_tree_widget) {
                m_account_tree_widget->set_data(
                    std::vector<KeepTower::GroupView>{},
                    std::vector<KeepTower::AccountListItem>{});
            }
        },
        [this](bool can_undo, bool can_redo) { update_undo_redo_sensitivity(can_undo, can_redo); },
        [this]() { m_undo_manager.clear(); },
        [this]() { on_user_activity(); },
        [this](const std::string& text) { m_status_label.set_text(KeepTower::make_valid_utf8(text, "status")); },
        [this]() {
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
        }
    );

    // Phase 5k: Initialize AutoLockHandler (binds to coordinator-owned vault state)
    m_auto_lock_handler = std::make_unique<UI::AutoLockHandler>(
        *this,
        m_vault_manager.get(),
        m_auto_lock_manager.get(),
        m_dialog_manager.get(),
        m_vault_ui_coordinator->vault_open_ref(),
        m_vault_ui_coordinator->is_locked_ref(),
        m_vault_ui_coordinator->current_vault_path_ref(),
        m_vault_ui_coordinator->cached_master_password_ref(),
        [this](bool locked, const std::string& status) {
            if (m_vault_ui_coordinator) {
                m_vault_ui_coordinator->apply_lock_state_ui(locked, status);
            }
        },
        [this]() { save_current_account(); },
        [this]() { on_close_vault(); },
        [this]() { update_account_list(); },
        [this](const Glib::ustring& text) { filter_accounts(text); },
        [this](const std::string& path) { handle_v2_vault_open(path); },
        [this]() { return is_v2_vault_open(); },
        [this]() { return m_vault_manager && m_vault_manager->is_modified(); },
        [this]() { return m_search_entry.get_text(); }
    );

    // Phase 5l: Initialize UserAccountHandler (binds to coordinator-owned vault state)
    m_user_account_handler = std::make_unique<UI::UserAccountHandler>(
        *this,
        m_vault_manager.get(),
        m_dialog_manager.get(),
        m_clipboard_manager.get(),
        m_vault_ui_coordinator->current_vault_path_ref(),
        [this](const std::string& message) { m_status_label.set_text(KeepTower::make_valid_utf8(message, "status")); },
        [this](const std::string& message) { show_error_dialog(message); },
        [this]() { on_close_vault(); },
        [this](const std::string& path) { handle_v2_vault_open(path); },
        [this]() { return is_v2_vault_open(); },
        [this]() { return is_current_user_admin(); },
        [this]() { return prompt_save_if_modified(); }
    );

    // Phase 5: Setup window actions via MenuManager (after MenuManager is initialized)
    std::map<std::string, std::function<void()>> action_callbacks = {
        {"preferences", [this]() { on_preferences(); }},
        {"import-csv", [this]() { on_import_from_csv(); }},
        {"delete-account", [this]() { on_delete_account(); }},
        {"create-group", [this]() { on_create_group(); }},
        {"rename-group", [this]() {
            if (!m_context_menu_group_id.empty() && m_vault_manager) {
                auto groups = m_vault_manager->get_all_groups_view();
                for (const auto& group : groups) {
                    if (group.group_id == m_context_menu_group_id) {
                        on_rename_group(m_context_menu_group_id, group.group_name);
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
            if (m_vault_ui_coordinator && m_vault_ui_coordinator->vault_open()) {
                m_status_label.set_text("Clipboard cleared");
            }
        });

    // Load and apply sort direction from settings
    {
        auto sort_settings = Gio::Settings::create("com.tjdeveng.keeptower");
        Glib::ustring sort_dir = sort_settings->get_string("sort-direction");
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
                    if (m_account_selection_coordinator) {
                        m_account_selection_coordinator->handle_account_selected(
                            account_id,
                            has_open_vault());
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

}

void MainWindow::on_new_vault() {
    if (m_vault_ui_coordinator) {
        m_vault_ui_coordinator->on_new_vault();
    }
}

void MainWindow::on_open_vault() {
    if (m_vault_ui_coordinator) {
        m_vault_ui_coordinator->on_open_vault();
    }
}

void MainWindow::on_save_vault() {
    if (m_vault_ui_coordinator) {
        m_vault_ui_coordinator->on_save_vault();
    }
}

void MainWindow::on_close_vault() {
    if (m_vault_ui_coordinator) {
        m_vault_ui_coordinator->on_close_vault();
    }
}

bool MainWindow::has_open_vault() const noexcept {
    return m_vault_ui_coordinator && m_vault_ui_coordinator->vault_open();
}

bool MainWindow::has_selected_account() const noexcept {
    return has_open_vault() && m_selected_account_index >= 0;
}

bool MainWindow::require_open_vault_status(std::string_view message) {
    if (has_open_vault()) {
        return true;
    }

    if (!message.empty()) {
        m_status_label.set_text(std::string(message));
    }
    return false;
}

bool MainWindow::require_open_vault_error(const Glib::ustring& message) {
    if (has_open_vault()) {
        return true;
    }

    show_error_dialog(message);
    return false;
}

bool MainWindow::require_open_vault_alert(
    const Glib::ustring& title,
    const Glib::ustring& detail) {
    if (has_open_vault()) {
        return true;
    }

    auto dialog = Gtk::AlertDialog::create(title);
    dialog->set_detail(detail);
    dialog->set_buttons({"OK"});
    dialog->choose(*this, {});
    return false;
}

void MainWindow::on_add_account() {
    if (!require_open_vault_status("Please open or create a vault first")) {
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
    if (!has_open_vault()) {
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
    auto detail_opt = m_vault_manager->get_account_view(index);
    if (!detail_opt) {
        KeepTower::Log::warning("MainWindow::display_account_details - account is null at index {}", index);
        m_account_detail_widget->clear();
        return;
    }
    const KeepTower::AccountDetail& account = *detail_opt;

    // Display in the detail widget
    m_account_detail_widget->display_account(account);

    // Check user role for permissions (V2 multi-user vaults)
    bool is_admin = is_current_user_admin();

    // Control privacy checkboxes - only admins can modify them
    m_account_detail_widget->set_privacy_controls_editable(is_admin);

    // Check if account is admin-only-deletable
    // Standard users get read-only access to prevent circumventing deletion protection
    if (!is_admin && account.is_admin_only_deletable) {
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
    if (!has_selected_account()) {
        return true;  // Nothing to save, allow continue
    }

    // Validate the index is within bounds
    const auto accounts = m_vault_manager->get_all_accounts_view();
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

    // Read current account via boundary type — no protobuf in this function
    auto detail_opt = m_vault_manager->get_account_view(m_selected_account_index);
    if (!detail_opt) {
        KeepTower::Log::warning("Failed to get account at index {}", m_selected_account_index);
        return true;  // Allow navigation even if account not found
    }
    KeepTower::AccountDetail detail = std::move(*detail_opt);

    // Save old values before overwrite (needed for history and change detection)
    const std::string old_name = detail.account_name;
    const std::string old_password = detail.password;

    // Apply new field values
    detail.account_name = account_name;
    detail.user_name = user_name;
    detail.password = password;
    detail.email = email;
    detail.website = website;
    detail.notes = notes;

    // Phase 3: Use AccountService for validation
    if (m_account_service) {
        auto validation_result = m_account_service->validate_account(detail);
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
    if (!is_admin && detail.is_admin_only_deletable) {
        // Only block save if account was actually modified
        if (m_account_detail_widget->is_modified()) {
            show_error_dialog(
                "You do not have permission to edit this account.\n\n"
                "This account is marked as admin-only-deletable.\n"
                "Only administrators can modify protected accounts."
            );
            // Reload the original account data to discard any changes
            m_account_detail_widget->display_account(detail);
            return false;  // Prevent save and navigation
        }
        // Not modified, allow navigation without error
        return true;
    }

    // Check password history settings
    auto settings = Gio::Settings::create("com.tjdeveng.keeptower");
    const bool history_enabled = SettingsValidator::is_password_history_enabled(settings);
    const int history_limit = SettingsValidator::get_password_history_limit(settings);

    // Check if password changed and prevent reuse
    if (password != old_password && history_enabled) {
        // Check against previous passwords to prevent reuse
        for (const auto& hist_entry : detail.password_history) {
            if (hist_entry == password) {
                show_error_dialog("Password reuse detected!\n\n"
                    "This password was used previously. Please choose a different password.\n\n"
                    "Using unique passwords for each change improves security.");
                return false;
            }
        }

        // Add old password to history if it's not empty
        if (!old_password.empty()) {
            detail.password_history.push_back(old_password);

            // Enforce history limit - remove oldest entries if exceeded
            while (static_cast<int>(detail.password_history.size()) > history_limit) {
                detail.password_history.erase(detail.password_history.begin());
            }
        }

        // Update password_changed_at timestamp when password changes
        detail.password_changed_at = std::time(nullptr);
    }

    // Update tags
    detail.tags = m_account_detail_widget->get_all_tags();

    // Update privacy controls (V2 multi-user vaults)
    detail.is_admin_only_viewable = m_account_detail_widget->get_admin_only_viewable();
    detail.is_admin_only_deletable = m_account_detail_widget->get_admin_only_deletable();

    // Update modification timestamp
    detail.modified_at = std::time(nullptr);

    // Persist the updated account back to the vault atomically
    if (!m_vault_manager->update_account(m_selected_account_index, detail)) {
        show_error_dialog("Failed to save account changes.");
        return false;
    }

    // Refresh the account list if the name changed
    if (old_name != detail.account_name) {
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
        const auto accounts = m_vault_manager->get_all_accounts_view();
        for (const auto& account : accounts) {
            for (const auto& tag : account.tags) {
                all_tags.insert(tag);
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
    if (!has_open_vault()) {
        return;
    }

    // Phase 5j: Delegate to AccountEditHandler
    m_account_edit_handler->handle_delete(m_context_menu_account_id);

    // Clear context menu state
    m_context_menu_account_id.clear();
}

void MainWindow::on_import_from_csv() {
    if (!require_open_vault_error("Please open a vault first before importing accounts.")) {
        return;
    }

    // Phase 5g: Delegate to VaultIOHandler
    m_vault_io_handler->handle_import([this]() {
        update_account_list();
        filter_accounts(m_search_entry.get_text());
    });
}

void MainWindow::on_export_to_csv() {
    if (!require_open_vault_error("Please open a vault first before exporting accounts.")) {
        return;
    }

    // Phase 5g: Delegate to VaultIOHandler
    m_vault_io_handler->handle_export(
        std::string{m_vault_ui_coordinator->current_vault_path()},
        m_vault_ui_coordinator->vault_open()
    );
}


void MainWindow::on_generate_password() {
    if (!has_selected_account()) {
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
    if (!require_open_vault_alert("No Vault Open", "Please open a vault first.")) {
        return;
    }

    // Phase 5h: Delegate to YubiKeyHandler
    m_yubikey_handler->handle_manage();
}
#endif

void MainWindow::on_undo() {
    if (!has_open_vault()) {
        return;
    }

    if (m_undo_manager.undo()) {
        const std::string msg = "Undid: " + m_undo_manager.get_redo_description();
        m_status_label.set_text(KeepTower::make_valid_utf8(msg, "status"));
    } else {
        m_status_label.set_text("Nothing to undo");
    }
}

void MainWindow::on_redo() {
    if (!has_open_vault()) {
        return;
    }

    if (m_undo_manager.redo()) {
        const std::string msg = "Redid: " + m_undo_manager.get_undo_description();
        m_status_label.set_text(KeepTower::make_valid_utf8(msg, "status"));
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
    const bool vault_open = m_vault_ui_coordinator && m_vault_ui_coordinator->vault_open();

    if (undo_action) {
        bool should_enable = can_undo && vault_open && undo_redo_enabled;
        undo_action->set_enabled(should_enable);
    }

    if (redo_action) {
        bool should_enable = can_redo && vault_open && undo_redo_enabled;
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
    if (!has_open_vault()) {
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
    if (!has_open_vault() || group_id.empty()) {
        return;
    }

    // Phase 5i: Delegate to GroupHandler
    m_group_handler->handle_rename(group_id, current_name);
}

void MainWindow::on_delete_group(const std::string& group_id) {
    if (!has_open_vault() || group_id.empty()) {
        return;
    }

    // Phase 5i: Delegate to GroupHandler
    m_group_handler->handle_delete(group_id);
}

// Helper methods for widget-based UI
int MainWindow::find_account_index_by_id(const std::string& account_id) const {
    if (!m_vault_manager) return -1;
    const auto accounts = m_vault_manager->get_all_accounts_view();
    for (size_t i = 0; i < accounts.size(); ++i) {
        if (accounts[i].id == account_id) return static_cast<int>(i);
    }
    return -1;
}

void MainWindow::filter_accounts_by_group(const std::string& group_id) {
    if (!m_vault_manager) return;
    const auto groups = m_vault_manager->get_all_groups_view();
    const auto accounts = m_vault_manager->get_all_accounts_view();
    if (group_id.empty()) {
        // Show all accounts
        m_account_tree_widget->set_data(groups, accounts);
        return;
    }
    // Filter accounts belonging to the selected group
    std::vector<KeepTower::AccountListItem> filtered_accounts;
    for (const auto& account : accounts) {
        for (const auto& membership : account.groups) {
            if (membership.group_id == group_id) {
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
    return KeepTower::VaultFileService::detect_vault_version_from_file(vault_path);
}

void MainWindow::handle_v2_vault_open(const std::string& vault_path) {
    if (m_vault_ui_coordinator) {
        m_vault_ui_coordinator->handle_v2_vault_open(vault_path);
    }
}





void MainWindow::complete_vault_opening(const std::string& vault_path, const std::string& username) {
    if (m_vault_ui_coordinator) {
        m_vault_ui_coordinator->complete_vault_opening(vault_path, username);
    }
}

void MainWindow::update_session_display() {
    if (m_vault_ui_coordinator) {
        m_vault_ui_coordinator->update_session_display();
    }
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
    if (m_vault_ui_coordinator) {
        m_vault_ui_coordinator->update_menu_for_role();
        return;
    }

    KeepTower::Log::info("MainWindow: update_menu_for_role() called (fallback)");

    // Fallback: Delegate to MenuManager
    const bool is_v2 = is_v2_vault_open();
    const bool is_admin = is_v2 && is_current_user_admin();
    const bool vault_open = m_vault_manager && m_vault_manager->is_vault_open();
    m_menu_manager->update_menu_for_role(is_v2, is_admin, vault_open);
}

bool MainWindow::is_v2_vault_open() const noexcept {
    if (m_vault_ui_coordinator) {
        return m_vault_ui_coordinator->is_v2_vault_open();
    }

    const bool has_manager = (m_vault_manager != nullptr);
    const bool vault_open_flag = has_manager && m_vault_manager->is_vault_open();
    const bool is_v2 = has_manager && m_vault_manager->is_v2_vault();

    KeepTower::Log::info("MainWindow: is_v2_vault_open() check - manager={}, m_vault_open={}, is_v2_vault()={}",
        has_manager, vault_open_flag, is_v2);

    if (!m_vault_manager || !vault_open_flag) {
        return false;
    }

    // Check vault format directly - more reliable than session check
    return m_vault_manager->is_v2_vault();
}

bool MainWindow::is_current_user_admin() const noexcept {
    if (m_vault_ui_coordinator) {
        return m_vault_ui_coordinator->is_current_user_admin();
    }

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
 * @note Should be called before clearing the active vault state
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
