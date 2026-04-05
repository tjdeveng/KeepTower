// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#include "AccountSelectionCoordinator.h"

#include "../../core/VaultManager.h"
#include "../../utils/Log.h"
#include "../widgets/AccountDetailWidget.h"

AccountSelectionCoordinator::AccountSelectionCoordinator(
    VaultManager* vault_manager,
    AccountDetailWidget* account_detail_widget,
    int& selected_account_index,
    SaveCurrentAccountFn save_current_account,
    FindAccountIndexByIdFn find_account_index_by_id,
    UpdateTagFilterFn update_tag_filter,
    IsCurrentUserAdminFn is_current_user_admin)
    : m_vault_manager(vault_manager),
      m_account_detail_widget(account_detail_widget),
      m_selected_account_index(selected_account_index),
      m_save_current_account(std::move(save_current_account)),
      m_find_account_index_by_id(std::move(find_account_index_by_id)),
      m_update_tag_filter(std::move(update_tag_filter)),
      m_is_current_user_admin(std::move(is_current_user_admin)) {}

void AccountSelectionCoordinator::handle_account_selected(
    const std::string& account_id,
    bool vault_open) {
    if (m_selected_account_index >= 0 && vault_open && m_save_current_account) {
        (void)m_save_current_account();
    }

    if (!m_find_account_index_by_id) {
        return;
    }

    const int index = m_find_account_index_by_id(account_id);
    if (index < 0) {
        KeepTower::Log::warning("AccountSelectionCoordinator: Could not find account with id: {}", account_id);
        return;
    }

    display_account_details(index);
    if (m_update_tag_filter) {
        m_update_tag_filter();
    }
}

void AccountSelectionCoordinator::clear_account_details() {
    if (m_account_detail_widget) {
        m_account_detail_widget->clear();
    }
    m_selected_account_index = -1;
}

void AccountSelectionCoordinator::display_account_details(int index) {
    if (!m_account_detail_widget || !m_vault_manager) {
        return;
    }

    if (index < 0) {
        m_account_detail_widget->clear();
        m_selected_account_index = -1;
        return;
    }

    m_selected_account_index = index;

    auto detail_opt = m_vault_manager->get_account_view(index);
    if (!detail_opt) {
        KeepTower::Log::warning(
            "AccountSelectionCoordinator::display_account_details - account is null at index {}",
            index);
        m_account_detail_widget->clear();
        m_selected_account_index = -1;
        return;
    }

    const KeepTower::AccountDetail& account = *detail_opt;
    m_account_detail_widget->display_account(account);

    const bool is_admin = m_is_current_user_admin ? m_is_current_user_admin() : false;
    m_account_detail_widget->set_privacy_controls_editable(is_admin);

    if (!is_admin && account.is_admin_only_deletable) {
        m_account_detail_widget->set_editable(false);
        m_account_detail_widget->set_delete_button_sensitive(false);
    } else {
        m_account_detail_widget->set_editable(true);
        m_account_detail_widget->set_delete_button_sensitive(true);
    }
}