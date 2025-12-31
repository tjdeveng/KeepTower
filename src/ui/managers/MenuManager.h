// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file MenuManager.h
 * @brief Centralized menu management for MainWindow
 *
 * Part of Phase 5 refactoring: MainWindow size reduction
 * Extracts menu creation, action setup, and menu update logic from MainWindow
 */

#pragma once

#include <gtkmm.h>
#include <giomm/menu.h>
#include <giomm/simpleaction.h>
#include <string>
#include <functional>
#include <memory>

// Forward declarations
class VaultManager;

namespace UI {

/**
 * @brief Manages menu and action creation for MainWindow
 *
 * Centralizes menu building, action setup, and role-based updates
 * to reduce MainWindow complexity.
 *
 * Design Goals:
 * - Reduce MainWindow size by ~150-200 lines
 * - Centralize menu patterns
 * - Handle role-based menu updates (V2 multi-user vaults)
 * - Maintain action lifecycle management
 *
 * @note This class does not own the parent window, just references it
 */
class MenuManager {
public:
    /**
     * @brief Construct menu manager for a parent window
     * @param parent Reference to parent Gtk::ApplicationWindow (MainWindow)
     * @param vault_manager Pointer to VaultManager for vault state
     */
    explicit MenuManager(Gtk::ApplicationWindow& parent, VaultManager* vault_manager);

    ~MenuManager() = default;

    // Delete copy and move to ensure parent reference validity
    MenuManager(const MenuManager&) = delete;
    MenuManager& operator=(const MenuManager&) = delete;
    MenuManager(MenuManager&&) = delete;
    MenuManager& operator=(MenuManager&&) = delete;

    /**
     * @brief Setup all window actions
     * @param callbacks Map of action names to callback functions
     */
    void setup_actions(const std::map<std::string, std::function<void()>>& callbacks);

    /**
     * @brief Create primary menu (hamburger menu)
     * @return Shared pointer to the menu model
     */
    Glib::RefPtr<Gio::Menu> create_primary_menu();

    /**
     * @brief Create account context menu
     * @param account_id ID of the account
     * @param account_index Index of the account
     * @param widget Widget to attach popover to
     * @param add_to_group_callback Callback for add to group action
     * @param remove_from_group_callback Callback for remove from group action
     * @return Managed popover menu
     */
    Gtk::PopoverMenu* create_account_context_menu(
        const std::string& account_id,
        int account_index,
        Gtk::Widget* widget,
        const std::function<void(const std::string&)>& add_to_group_callback,
        const std::function<void(const std::string&)>& remove_from_group_callback
    );

    /**
     * @brief Create group context menu
     * @param group_id ID of the group
     * @param widget Widget to attach popover to
     * @return Managed popover menu
     */
    Gtk::PopoverMenu* create_group_context_menu(
        const std::string& group_id,
        Gtk::Widget* widget
    );

    /**
     * @brief Update menu items based on vault version and user role
     * @param is_v2_vault Whether a V2 vault is open
     * @param is_admin Whether current user is admin (V2 only)
     * @param vault_open Whether any vault is open
     */
    void update_menu_for_role(bool is_v2_vault, bool is_admin, bool vault_open);

    /**
     * @brief Set keyboard shortcuts for actions
     * @param app Application to set shortcuts on
     */
    void setup_keyboard_shortcuts(const Glib::RefPtr<Gtk::Application>& app);

    /**
     * @brief Store action references for enable/disable operations
     */
    void set_action_references(
        Glib::RefPtr<Gio::SimpleAction> export_action,
        Glib::RefPtr<Gio::SimpleAction> change_password_action,
        Glib::RefPtr<Gio::SimpleAction> logout_action,
        Glib::RefPtr<Gio::SimpleAction> manage_users_action
    );

private:
    Gtk::ApplicationWindow& m_parent;
    VaultManager* m_vault_manager;

    // Cached action references for enable/disable
    Glib::RefPtr<Gio::SimpleAction> m_export_action;
    Glib::RefPtr<Gio::SimpleAction> m_change_password_action;
    Glib::RefPtr<Gio::SimpleAction> m_logout_action;
    Glib::RefPtr<Gio::SimpleAction> m_manage_users_action;

    // Context menu state
    std::string m_context_menu_account_id;
    std::string m_context_menu_group_id;

    /**
     * @brief Helper to add an action to the window
     */
    Glib::RefPtr<Gio::SimpleAction> add_action(
        const std::string& name,
        const std::function<void()>& callback
    );

    /**
     * @brief Helper to remove an action from the window
     */
    void remove_action(const std::string& name);
};

} // namespace UI
