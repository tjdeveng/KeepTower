// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file AccountViewController.cc
 * @brief Implementation of AccountViewController
 *
 * Part of Phase 2 refactoring: Now uses Repository Pattern for data access
 * instead of direct VaultManager calls.
 */

#include "AccountViewController.h"
#include "../../core/repositories/AccountRepository.h"
#include "../../core/repositories/GroupRepository.h"
#include <algorithm>
#include <stdexcept>

AccountViewController::AccountViewController(VaultManager* vault_manager)
    : m_vault_manager(vault_manager),
      m_account_repo(std::make_unique<KeepTower::AccountRepository>(vault_manager)),
      m_group_repo(std::make_unique<KeepTower::GroupRepository>(vault_manager)) {
    if (!m_vault_manager) {
        throw std::invalid_argument("VaultManager cannot be null");
    }
}

void AccountViewController::refresh_account_list() {
    if (!is_vault_open()) {
        // Clear cached data if vault is not open
        m_viewable_accounts.clear();
        m_groups.clear();
        m_signal_list_updated.emit(m_viewable_accounts, m_groups, 0);
        return;
    }

    try {
        // Retrieve all accounts and groups from repositories
        auto all_accounts_result = m_account_repo->get_all();
        auto groups_result = m_group_repo->get_all();

        if (!all_accounts_result) {
            throw std::runtime_error("Failed to get accounts: " +
                std::string(KeepTower::to_string(all_accounts_result.error())));
        }

        if (!groups_result) {
            throw std::runtime_error("Failed to get groups: " +
                std::string(KeepTower::to_string(groups_result.error())));
        }

        auto all_accounts = std::move(all_accounts_result.value());
        m_groups = std::move(groups_result.value());

        // Apply permission filtering for V2 multi-user vaults
        m_viewable_accounts = filter_by_permissions(all_accounts);

        // Emit signal with updated data
        m_signal_list_updated.emit(m_viewable_accounts, m_groups, all_accounts.size());
    } catch (const std::exception& e) {
        m_signal_error.emit(std::string("Failed to refresh account list: ") + e.what());
        // Clear cached data on error
        m_viewable_accounts.clear();
        m_groups.clear();
    }
}

const std::vector<keeptower::AccountRecord>&
AccountViewController::get_viewable_accounts() const {
    return m_viewable_accounts;
}

const std::vector<keeptower::AccountGroup>&
AccountViewController::get_groups() const {
    return m_groups;
}

size_t AccountViewController::get_viewable_account_count() const {
    return m_viewable_accounts.size();
}

bool AccountViewController::can_view_account(size_t account_index) const {
    if (!is_vault_open()) {
        return false;
    }
    return m_account_repo->can_view(account_index);
}

int AccountViewController::find_account_index_by_id(const std::string& account_id) const {
    if (!is_vault_open()) {
        return -1;
    }

    auto index_opt = m_account_repo->find_index_by_id(account_id);
    if (index_opt) {
        return static_cast<int>(index_opt.value());
    }
    return -1;
}

bool AccountViewController::toggle_favorite(size_t account_index) {
    if (!is_vault_open()) {
        m_signal_error.emit("Cannot toggle favorite: vault is not open");
        return false;
    }

    try {
        // Get account using repository
        auto account_result = m_account_repo->get(account_index);
        if (!account_result) {
            std::string error_msg = "Failed to get account: " +
                std::string(KeepTower::to_string(account_result.error()));
            m_signal_error.emit(error_msg);
            return false;
        }

        auto account = std::move(account_result.value());
        bool current_favorite = account.is_favorite();
        bool new_favorite = !current_favorite;

        // Update the account's favorite status
        account.set_is_favorite(new_favorite);

        // Update via repository
        auto update_result = m_account_repo->update(account_index, account);
        if (!update_result) {
            std::string error_msg = "Failed to update account: " +
                std::string(KeepTower::to_string(update_result.error()));
            m_signal_error.emit(error_msg);
            return false;
        }

        // Emit signal on success
        m_signal_favorite_toggled.emit(account_index, new_favorite);
        return true;
    } catch (const std::exception& e) {
        m_signal_error.emit(std::string("Error toggling favorite: ") + e.what());
        return false;
    }
}

bool AccountViewController::is_vault_open() const {
    return m_account_repo && m_account_repo->is_vault_open();
}

sigc::signal<void(const std::vector<keeptower::AccountRecord>&,
                  const std::vector<keeptower::AccountGroup>&,
                  size_t)>&
AccountViewController::signal_list_updated() {
    return m_signal_list_updated;
}

sigc::signal<void(size_t, bool)>&
AccountViewController::signal_favorite_toggled() {
    return m_signal_favorite_toggled;
}

sigc::signal<void(const std::string&)>&
AccountViewController::signal_error() {
    return m_signal_error;
}

std::vector<keeptower::AccountRecord>
AccountViewController::filter_by_permissions(
    const std::vector<keeptower::AccountRecord>& all_accounts) {

    std::vector<keeptower::AccountRecord> filtered;
    filtered.reserve(all_accounts.size());

    for (size_t i = 0; i < all_accounts.size(); ++i) {
        if (can_view_account(i)) {
            filtered.push_back(all_accounts[i]);
        }
    }

    return filtered;
}
