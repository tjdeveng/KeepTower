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

/**
 * @brief Coordinates account-tree context menus and reorder actions.
 *
 * This coordinator translates widget-level context menu and drag-and-drop
 * events into higher-level account/group operations using injected handlers.
 */
class AccountTreeInteractionCoordinator {
public:
    /** @brief Callback used to resolve vault indices from account IDs. */
    using FindAccountIndexByIdFn = std::function<int(const std::string&)>;

    /** @brief Callback used to refresh the account list after reorder actions. */
    using UpdateAccountListFn = std::function<void()>;

    /**
     * @brief Construct the coordinator with injected collaborators.
     * @param vault_manager Vault manager used for move/delete operations.
     * @param menu_manager Menu manager that owns popover menus.
     * @param group_handler Group handler for group operations.
     * @param account_edit_handler Account edit handler for account operations.
     * @param find_account_index_by_id Callback that resolves account IDs to vault indices.
     * @param update_account_list Callback that refreshes the account list after mutations.
     */
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

    /**
     * @brief Show the account context menu at the requested widget position.
     * @param account_id Account under the cursor.
     * @param widget Anchor widget for the popover.
     * @param x X coordinate within the widget.
     * @param y Y coordinate within the widget.
     */
    void show_account_context_menu(const std::string& account_id, Gtk::Widget* widget, double x, double y);

    /**
     * @brief Show the group context menu at the requested widget position.
     * @param group_id Group under the cursor.
     * @param widget Anchor widget for the popover.
     * @param x X coordinate within the widget.
     * @param y Y coordinate within the widget.
     */
    void show_group_context_menu(const std::string& group_id, Gtk::Widget* widget, double x, double y);

    /**
     * @brief Persist an account reorder initiated from drag-and-drop.
     * @param account_id Account being moved.
     * @param target_group_id Destination group identifier.
     * @param new_index New index within the destination scope.
     */
    void handle_account_reordered(const std::string& account_id, const std::string& target_group_id, int new_index);

    /**
     * @brief Persist a group reorder initiated from drag-and-drop.
     * @param group_id Group being moved.
     * @param new_index New index among groups.
     */
    void handle_group_reordered(const std::string& group_id, int new_index);

    /** @brief Delete the account currently captured for the context menu. */
    void handle_delete_account_action();

    /** @brief Rename the group currently captured for the context menu. */
    void handle_rename_group_action();

    /** @brief Delete the group currently captured for the context menu. */
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