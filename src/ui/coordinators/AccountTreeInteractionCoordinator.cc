// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#include "AccountTreeInteractionCoordinator.h"

#include "../../core/VaultManager.h"
#include "../../utils/Log.h"
#include "../managers/AccountEditHandler.h"
#include "../managers/GroupHandler.h"
#include "../managers/MenuManager.h"

#include <gdkmm/rectangle.h>
#include <glibmm/main.h>

AccountTreeInteractionCoordinator::AccountTreeInteractionCoordinator(
    VaultManager* vault_manager,
    UI::MenuManager* menu_manager,
    UI::GroupHandler* group_handler,
    UI::AccountEditHandler* account_edit_handler,
    FindAccountIndexByIdFn find_account_index_by_id,
    UpdateAccountListFn update_account_list)
    : m_vault_manager(vault_manager),
      m_menu_manager(menu_manager),
      m_group_handler(group_handler),
      m_account_edit_handler(account_edit_handler),
      m_find_account_index_by_id(std::move(find_account_index_by_id)),
      m_update_account_list(std::move(update_account_list)) {}

void AccountTreeInteractionCoordinator::show_account_context_menu(
    const std::string& account_id,
    Gtk::Widget* widget,
    double x,
    double y) {
    if (!m_menu_manager || !m_find_account_index_by_id) {
        return;
    }

    const int account_index = m_find_account_index_by_id(account_id);
    if (account_index < 0) {
        return;
    }

    m_context_menu_account_id = account_id;
    m_context_menu_group_id.clear();

    auto* popover = m_menu_manager->create_account_context_menu(
        account_id,
        account_index,
        widget,
        [this](const std::string& group_id) {
            handle_add_account_to_group(group_id);
        },
        [this](const std::string& group_id) {
            handle_remove_account_from_group(group_id);
        });

    popup_menu(popover, x, y);
}

void AccountTreeInteractionCoordinator::show_group_context_menu(
    const std::string& group_id,
    Gtk::Widget* widget,
    double x,
    double y) {
    if (!m_menu_manager) {
        return;
    }

    if (group_id == "favorites") {
        return;
    }

    m_context_menu_group_id = group_id;
    m_context_menu_account_id.clear();

    auto* popover = m_menu_manager->create_group_context_menu(group_id, widget);
    popup_menu(popover, x, y);
}

void AccountTreeInteractionCoordinator::handle_account_reordered(
    const std::string& account_id,
    const std::string& target_group_id,
    int new_index) {
    if (!m_vault_manager || !m_find_account_index_by_id) {
        return;
    }

    const int account_index = m_find_account_index_by_id(account_id);
    if (account_index < 0) {
        return;
    }

    g_debug(
        "AccountTreeInteractionCoordinator::handle_account_reordered - account_id=%s, target_group_id='%s', index=%d",
        account_id.c_str(),
        target_group_id.c_str(),
        new_index);

    if (target_group_id.empty()) {
        g_debug("  Dropped into All Accounts - no group membership changes");
        return;
    }

    if (!m_vault_manager->is_account_in_group(account_index, target_group_id)
        && !m_vault_manager->add_account_to_group(account_index, target_group_id)) {
        KeepTower::Log::warning("Failed to add account to group");
        return;
    }

    if (m_update_account_list) {
        Glib::signal_idle().connect_once([update = m_update_account_list]() {
            update();
        });
    }
}

void AccountTreeInteractionCoordinator::handle_group_reordered(const std::string& group_id, int new_index) {
    if (!m_vault_manager) {
        return;
    }

    if (!m_vault_manager->reorder_group(group_id, new_index)) {
        KeepTower::Log::warning("Failed to reorder group");
        return;
    }

    if (m_update_account_list) {
        Glib::signal_idle().connect_once([update = m_update_account_list]() {
            update();
        });
    }
}

void AccountTreeInteractionCoordinator::handle_delete_account_action() {
    if (!m_account_edit_handler) {
        return;
    }

    m_account_edit_handler->handle_delete(m_context_menu_account_id);
    m_context_menu_account_id.clear();
    m_context_menu_group_id.clear();
}

void AccountTreeInteractionCoordinator::handle_rename_group_action() {
    if (!m_group_handler || !m_vault_manager || m_context_menu_group_id.empty()) {
        return;
    }

    const auto groups = m_vault_manager->get_all_groups_view();
    for (const auto& group : groups) {
        if (group.group_id == m_context_menu_group_id) {
            m_group_handler->handle_rename(m_context_menu_group_id, group.group_name);
            m_context_menu_group_id.clear();
            m_context_menu_account_id.clear();
            return;
        }
    }

    m_context_menu_group_id.clear();
}

void AccountTreeInteractionCoordinator::handle_delete_group_action() {
    if (!m_group_handler || m_context_menu_group_id.empty()) {
        return;
    }

    m_group_handler->handle_delete(m_context_menu_group_id);
    m_context_menu_group_id.clear();
    m_context_menu_account_id.clear();
}

void AccountTreeInteractionCoordinator::popup_menu(Gtk::PopoverMenu* popover, double x, double y) {
    if (!popover) {
        return;
    }

    Gdk::Rectangle rect;
    rect.set_x(static_cast<int>(x));
    rect.set_y(static_cast<int>(y));
    rect.set_width(1);
    rect.set_height(1);
    popover->set_pointing_to(rect);
    popover->popup();
}

void AccountTreeInteractionCoordinator::handle_add_account_to_group(const std::string& group_id) {
    if (!m_vault_manager || !m_find_account_index_by_id || m_context_menu_account_id.empty()) {
        return;
    }

    const int account_index = m_find_account_index_by_id(m_context_menu_account_id);
    if (account_index < 0) {
        return;
    }

    if (m_vault_manager->add_account_to_group(account_index, group_id) && m_update_account_list) {
        m_update_account_list();
    }
}

void AccountTreeInteractionCoordinator::handle_remove_account_from_group(const std::string& group_id) {
    if (!m_vault_manager || !m_find_account_index_by_id || m_context_menu_account_id.empty()) {
        return;
    }

    const int account_index = m_find_account_index_by_id(m_context_menu_account_id);
    if (account_index < 0) {
        return;
    }

    if (m_vault_manager->remove_account_from_group(account_index, group_id) && m_update_account_list) {
        m_update_account_list();
    }
}