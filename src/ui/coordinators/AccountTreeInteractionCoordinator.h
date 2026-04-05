// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

/**
 * @file AccountTreeInteractionCoordinator.h
 * @brief Coordinates account-tree context menus and reorder interactions.
 */

#pragma once

#include <functional>
#include <string>

#include <glibmm/ustring.h>

class VaultManager;

namespace Gtk {
class PopoverMenu;
class Widget;
}

namespace UI {
class AccountEditHandler;
class GroupHandler;
class MenuManager;
}

class AccountTreeInteractionCoordinator {
public:
    using FindAccountIndexByIdFn = std::function<int(const std::string&)>;
    using UpdateAccountListFn = std::function<void()>;

    AccountTreeInteractionCoordinator(
        VaultManager* vault_manager,
        UI::MenuManager* menu_manager,
        UI::GroupHandler* group_handler,
        UI::AccountEditHandler* account_edit_handler,
        FindAccountIndexByIdFn find_account_index_by_id,
        UpdateAccountListFn update_account_list);

    AccountTreeInteractionCoordinator(const AccountTreeInteractionCoordinator&) = delete;
    AccountTreeInteractionCoordinator& operator=(const AccountTreeInteractionCoordinator&) = delete;
    AccountTreeInteractionCoordinator(AccountTreeInteractionCoordinator&&) = delete;
    AccountTreeInteractionCoordinator& operator=(AccountTreeInteractionCoordinator&&) = delete;

    void show_account_context_menu(const std::string& account_id, Gtk::Widget* widget, double x, double y);
    void show_group_context_menu(const std::string& group_id, Gtk::Widget* widget, double x, double y);

    void handle_account_reordered(const std::string& account_id, const std::string& target_group_id, int new_index);
    void handle_group_reordered(const std::string& group_id, int new_index);

    void handle_delete_account_action();
    void handle_rename_group_action();
    void handle_delete_group_action();

private:
    void popup_menu(Gtk::PopoverMenu* popover, double x, double y);
    void handle_add_account_to_group(const std::string& group_id);
    void handle_remove_account_from_group(const std::string& group_id);

    VaultManager* m_vault_manager;
    UI::MenuManager* m_menu_manager;
    UI::GroupHandler* m_group_handler;
    UI::AccountEditHandler* m_account_edit_handler;

    FindAccountIndexByIdFn m_find_account_index_by_id;
    UpdateAccountListFn m_update_account_list;

    std::string m_context_menu_account_id;
    std::string m_context_menu_group_id;
};