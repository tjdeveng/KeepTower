// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

/**
 * @file AccountSelectionCoordinator.h
 * @brief Coordinates account selection and detail-panel synchronization.
 */

#pragma once

#include <functional>
#include <string>

class VaultManager;
class AccountDetailWidget;

class AccountSelectionCoordinator {
public:
    using SaveCurrentAccountFn = std::function<bool()>;
    using FindAccountIndexByIdFn = std::function<int(const std::string&)>;
    using UpdateTagFilterFn = std::function<void()>;
    using IsCurrentUserAdminFn = std::function<bool()>;

    AccountSelectionCoordinator(
        VaultManager* vault_manager,
        AccountDetailWidget* account_detail_widget,
        int& selected_account_index,
        SaveCurrentAccountFn save_current_account,
        FindAccountIndexByIdFn find_account_index_by_id,
        UpdateTagFilterFn update_tag_filter,
        IsCurrentUserAdminFn is_current_user_admin);

    AccountSelectionCoordinator(const AccountSelectionCoordinator&) = delete;
    AccountSelectionCoordinator& operator=(const AccountSelectionCoordinator&) = delete;
    AccountSelectionCoordinator(AccountSelectionCoordinator&&) = delete;
    AccountSelectionCoordinator& operator=(AccountSelectionCoordinator&&) = delete;

    void handle_account_selected(const std::string& account_id, bool vault_open);
    void clear_account_details();
    void display_account_details(int index);

private:
    VaultManager* m_vault_manager;
    AccountDetailWidget* m_account_detail_widget;
    int& m_selected_account_index;

    SaveCurrentAccountFn m_save_current_account;
    FindAccountIndexByIdFn m_find_account_index_by_id;
    UpdateTagFilterFn m_update_tag_filter;
    IsCurrentUserAdminFn m_is_current_user_admin;
};