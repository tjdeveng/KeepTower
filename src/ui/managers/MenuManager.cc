// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "MenuManager.h"
#include "../../core/VaultManager.h"
#include "../../utils/helpers/HelpManager.h"
#include <config.h>

namespace UI {

MenuManager::MenuManager(Gtk::ApplicationWindow& parent, VaultManager* vault_manager)
    : m_parent(parent)
    , m_vault_manager(vault_manager)
{
}

Glib::RefPtr<Gio::SimpleAction> MenuManager::add_action(
    const std::string& name,
    const std::function<void()>& callback)
{
    auto action = Gio::SimpleAction::create(name);
    action->signal_activate().connect([callback](const Glib::VariantBase&) {
        callback();
    });
    m_parent.add_action(action);
    return action;
}

void MenuManager::remove_action(const std::string& name) {
    m_parent.remove_action(name);
}

void MenuManager::setup_actions(
    const std::map<std::string, std::function<void()>>& callbacks)
{
    for (const auto& [name, callback] : callbacks) {
        add_action(name, callback);
    }
}

void MenuManager::setup_help_actions() {
    auto& help = Utils::HelpManager::get_instance();

    // User Guide
    add_action("help-user-guide", [this, &help]() {
        help.open_help(Utils::HelpTopic::UserGuide, m_parent);
    });

    // Getting Started
    add_action("help-getting-started", [this, &help]() {
        help.open_help(Utils::HelpTopic::GettingStarted, m_parent);
    });

    // FAQ
    add_action("help-faq", [this, &help]() {
        help.open_help(Utils::HelpTopic::FAQ, m_parent);
    });

    // Security
    add_action("help-security", [this, &help]() {
        help.open_help(Utils::HelpTopic::Security, m_parent);
    });
}

Glib::RefPtr<Gio::Menu> MenuManager::create_primary_menu() {
    auto menu = Gio::Menu::create();

    // Edit section
    auto edit_section = Gio::Menu::create();
    edit_section->append("_Undo", "win.undo");
    edit_section->append("_Redo", "win.redo");
    menu->append_section(edit_section);

    // Actions section
    auto actions_section = Gio::Menu::create();
    actions_section->append("_Preferences", "win.preferences");
    actions_section->append("_Import Accounts...", "win.import-csv");
    actions_section->append("_Export Accounts...", "win.export-csv");
#ifdef HAVE_YUBIKEY_SUPPORT
    actions_section->append("Manage _YubiKeys", "win.manage-yubikeys");
    actions_section->append("Test _YubiKey", "win.test-yubikey");
#endif
    menu->append_section(actions_section);

    // V2 vault user section
    auto user_section = Gio::Menu::create();
    user_section->append("_Change My Password", "win.change-password");
    user_section->append("Manage _Users", "win.manage-users");
    user_section->append("_Logout", "win.logout");
    menu->append_section(user_section);

    // Help section with submenu
    auto help_section = Gio::Menu::create();

    // Create Help submenu
    auto help_submenu = Gio::Menu::create();
    help_submenu->append("_User Guide", "win.help-user-guide");
    help_submenu->append("_Getting Started", "win.help-getting-started");
    help_submenu->append("_FAQ", "win.help-faq");
    help_submenu->append("_Security", "win.help-security");

    help_section->append_submenu("_Help", help_submenu);
    help_section->append("_Keyboard Shortcuts", "win.show-help-overlay");
    help_section->append("_About KeepTower", "app.about");
    menu->append_section(help_section);

    return menu;
}

Gtk::PopoverMenu* MenuManager::create_account_context_menu(
    const std::string& account_id,
    int account_index,
    Gtk::Widget* widget,
    const std::function<void(const std::string&)>& add_to_group_callback,
    const std::function<void(const std::string&)>& remove_from_group_callback)
{
    m_context_menu_account_id = account_id;
    auto menu = Gio::Menu::create();

    if (m_vault_manager) {
        auto groups = m_vault_manager->get_all_groups();
        auto accounts = m_vault_manager->get_all_accounts();

        if (account_index < static_cast<int>(accounts.size())) {
            const auto& account = accounts[account_index];

            // Build "Add to Group" submenu
            if (!groups.empty()) {
                auto groups_menu = Gio::Menu::create();
                for (const auto& group : groups) {
                    if (group.group_id() != "favorites") {
                        std::string action_name = "add-to-group-" + group.group_id();
                        remove_action(action_name);
                        add_action(action_name, [add_to_group_callback, gid = group.group_id()]() {
                            add_to_group_callback(gid);
                        });
                        groups_menu->append(group.group_name(), "win." + action_name);
                    }
                }
                if (groups_menu->get_n_items() > 0) {
                    menu->append_submenu("Add to Group", groups_menu);
                }
            }

            // Build "Remove from Group" submenu
            std::vector<std::string> account_groups;
            account_groups.reserve(account.groups_size());
            for (int i = 0; i < account.groups_size(); ++i) {
                account_groups.push_back(account.groups(i).group_id());
            }

            if (!account_groups.empty()) {
                auto remove_groups_menu = Gio::Menu::create();
                for (const auto& gid : account_groups) {
                    std::string group_name = gid;
                    for (const auto& group : groups) {
                        if (group.group_id() == gid) {
                            group_name = group.group_name();
                            break;
                        }
                    }

                    std::string action_name = "remove-from-group-" + gid;
                    remove_action(action_name);
                    add_action(action_name, [remove_from_group_callback, gid]() {
                        remove_from_group_callback(gid);
                    });
                    remove_groups_menu->append(group_name, "win." + action_name);
                }
                menu->append_submenu("Remove from Group", remove_groups_menu);
            }
        }
    }

    // Delete action
    auto delete_section = Gio::Menu::create();
    delete_section->append("Delete Account", "win.delete-account");
    menu->append_section(delete_section);

    auto popover = Gtk::make_managed<Gtk::PopoverMenu>();
    popover->set_menu_model(menu);
    popover->set_parent(*widget);
    return popover;
}

Gtk::PopoverMenu* MenuManager::create_group_context_menu(
    const std::string& group_id,
    Gtk::Widget* widget)
{
    m_context_menu_group_id = group_id;
    auto menu = Gio::Menu::create();

    // Actions section
    auto actions_section = Gio::Menu::create();
    actions_section->append("Rename Group", "win.rename-group");
    menu->append_section(actions_section);

    // Delete section
    if (group_id != "favorites") {
        auto delete_section = Gio::Menu::create();
        delete_section->append("Delete Group", "win.delete-group");
        menu->append_section(delete_section);
    }

    auto popover = Gtk::make_managed<Gtk::PopoverMenu>();
    popover->set_menu_model(menu);
    popover->set_parent(*widget);
    return popover;
}

void MenuManager::update_menu_for_role(
    bool is_v2_vault,
    bool is_admin,
    bool vault_open)
{
    if (!is_v2_vault) {
        // V1 vaults or no vault - disable V2-specific actions
        if (m_change_password_action) m_change_password_action->set_enabled(false);
        if (m_logout_action) m_logout_action->set_enabled(false);
        if (m_manage_users_action) m_manage_users_action->set_enabled(false);
        // V1 allows export (single-user)
        if (m_export_action) m_export_action->set_enabled(vault_open);
        return;
    }

    // V2 vault - enable user actions
    if (m_change_password_action) m_change_password_action->set_enabled(true);
    if (m_logout_action) m_logout_action->set_enabled(true);

    // Admin-only actions
    if (m_manage_users_action) m_manage_users_action->set_enabled(is_admin);
    if (m_export_action) m_export_action->set_enabled(is_admin);
}

void MenuManager::setup_keyboard_shortcuts(const Glib::RefPtr<Gtk::Application>& app) {
    if (!app) return;

    app->set_accel_for_action("win.preferences", "<Ctrl>comma");
    app->set_accel_for_action("win.undo", "<Ctrl>Z");
    app->set_accel_for_action("win.redo", "<Ctrl><Shift>Z");
}

void MenuManager::set_action_references(
    Glib::RefPtr<Gio::SimpleAction> export_action,
    Glib::RefPtr<Gio::SimpleAction> change_password_action,
    Glib::RefPtr<Gio::SimpleAction> logout_action,
    Glib::RefPtr<Gio::SimpleAction> manage_users_action)
{
    m_export_action = std::move(export_action);
    m_change_password_action = std::move(change_password_action);
    m_logout_action = std::move(logout_action);
    m_manage_users_action = std::move(manage_users_action);
}

} // namespace UI
