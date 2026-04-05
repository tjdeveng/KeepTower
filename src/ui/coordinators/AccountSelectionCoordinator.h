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

/**
 * @brief Synchronizes account selection with the detail panel.
 *
 * This coordinator owns the selection-side workflow of saving pending edits,
 * resolving IDs to vault indices, updating admin-only detail visibility, and
 * refreshing related tag-filter state.
 */
class AccountSelectionCoordinator {
public:
    /** @brief Callback used to persist in-progress edits before switching selection. */
    using SaveCurrentAccountFn = std::function<bool()>;

    /** @brief Callback used to resolve account IDs to vault indices. */
    using FindAccountIndexByIdFn = std::function<int(const std::string&)>;

    /** @brief Callback used to refresh the tag-filter UI after selection changes. */
    using UpdateTagFilterFn = std::function<void()>;

    /** @brief Callback used to determine whether admin-only fields may be shown. */
    using IsCurrentUserAdminFn = std::function<bool()>;

    /**
     * @brief Construct the selection coordinator.
     * @param vault_manager Vault manager used to load account details.
     * @param account_detail_widget Detail widget to update.
     * @param selected_account_index Shared selected-index state owned by MainWindow.
     * @param save_current_account Callback used before switching selection.
     * @param find_account_index_by_id Callback used to resolve account IDs.
     * @param update_tag_filter Callback used to refresh tag filters.
     * @param is_current_user_admin Callback used to gate admin-only detail fields.
     */
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

    /**
     * @brief Handle user or programmatic account selection.
     * @param account_id Selected account identifier.
     * @param vault_open True when the vault is currently open.
     */
    void handle_account_selected(const std::string& account_id, bool vault_open);

    /** @brief Clear the detail widget and reset selection state. */
    void clear_account_details();

    /**
     * @brief Display account details for the given vault index.
     * @param index Vault index to display.
     */
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