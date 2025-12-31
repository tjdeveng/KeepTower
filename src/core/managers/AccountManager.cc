// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 TJDev

#include "AccountManager.h"
#include "../../utils/Cpp23Compat.h"
#include <algorithm>

#if KEEPTOWER_HAS_RANGES
#include <ranges>
#endif

namespace KeepTower {

AccountManager::AccountManager(keeptower::VaultData& vault_data, bool& modified_flag)
    : m_vault_data(vault_data), m_modified_flag(modified_flag) {}

bool AccountManager::add_account(const keeptower::AccountRecord& account) {
    auto* new_account = m_vault_data.add_accounts();
    new_account->CopyFrom(account);
    m_modified_flag = true;
    return true;
}

std::vector<keeptower::AccountRecord> AccountManager::get_all_accounts() const {
    std::vector<keeptower::AccountRecord> accounts;
    const auto account_count = compat::to_size(m_vault_data.accounts_size());
    accounts.reserve(account_count);

#if KEEPTOWER_HAS_RANGES
    // Modern C++23: Use ranges for cleaner iteration
    for (const auto& account : m_vault_data.accounts()) {
        accounts.push_back(account);
    }
#else
    // GCC 13 fallback: Traditional index-based loop
    for (size_t i = 0; i < account_count; ++i) {
        accounts.push_back(m_vault_data.accounts(static_cast<int>(i)));
    }
#endif

    return accounts;
}

bool AccountManager::update_account(size_t index, const keeptower::AccountRecord& account) {
    if (!compat::is_valid_index(index, m_vault_data.accounts_size())) {
        return false;
    }

    m_vault_data.mutable_accounts(static_cast<int>(index))->CopyFrom(account);
    m_modified_flag = true;
    return true;
}

bool AccountManager::delete_account(size_t index) {
    if (!compat::is_valid_index(index, m_vault_data.accounts_size())) {
        return false;
    }

    // Remove account by shifting
    auto* accounts = m_vault_data.mutable_accounts();
    accounts->erase(accounts->begin() + static_cast<std::ptrdiff_t>(index));
    m_modified_flag = true;
    return true;
}

const keeptower::AccountRecord* AccountManager::get_account(size_t index) const {
    if (!compat::is_valid_index(index, m_vault_data.accounts_size())) {
        return nullptr;
    }
    return &m_vault_data.accounts(static_cast<int>(index));
}

keeptower::AccountRecord* AccountManager::get_account_mutable(size_t index) {
    if (!compat::is_valid_index(index, m_vault_data.accounts_size())) {
        return nullptr;
    }
    return m_vault_data.mutable_accounts(static_cast<int>(index));
}

size_t AccountManager::get_account_count() const {
    return compat::to_size(m_vault_data.accounts_size());
}

bool AccountManager::reorder_account(size_t old_index, size_t new_index) {
    const size_t account_count = get_account_count();

    // Validate indices are within bounds
    if (old_index >= account_count || new_index >= account_count) {
        return false;
    }

    // Optimization: No-op if source and destination are the same
    if (old_index == new_index) {
        return true;
    }

    // Initialize global_display_order for all accounts if not already set
    bool has_custom_ordering = false;
    if (account_count > 0) {
        for (size_t i = 0; i < account_count; ++i) {
            if (m_vault_data.accounts(static_cast<int>(i)).global_display_order() >= 0) {
                has_custom_ordering = true;
                break;
            }
        }
    }

    if (!has_custom_ordering) {
        for (size_t i = 0; i < account_count; i++) {
            m_vault_data.mutable_accounts(static_cast<int>(i))->set_global_display_order(static_cast<int32_t>(i));
        }
    }

    // Get the account being moved
    auto* account_to_move = m_vault_data.mutable_accounts(static_cast<int>(old_index));

    if (old_index < new_index) {
        // Moving down: shift accounts up in the range [old_index+1, new_index]
        for (size_t i = old_index + 1; i <= new_index; i++) {
            auto* acc = m_vault_data.mutable_accounts(static_cast<int>(i));
            acc->set_global_display_order(acc->global_display_order() - 1);
        }
        // Place the moved account at the end of the shifted range
        account_to_move->set_global_display_order(
            m_vault_data.accounts(static_cast<int>(new_index)).global_display_order()
        );
    } else {
        // Moving up: shift accounts down in the range [new_index, old_index-1]
        for (size_t i = new_index; i < old_index; i++) {
            auto* acc = m_vault_data.mutable_accounts(static_cast<int>(i));
            acc->set_global_display_order(acc->global_display_order() + 1);
        }
        // Place the moved account at the start of the shifted range
        account_to_move->set_global_display_order(
            m_vault_data.accounts(static_cast<int>(new_index)).global_display_order()
        );
    }

    // Normalize display orders to ensure they're sequential (0, 1, 2, ...)
    // This prevents gaps and keeps the logic simple
    std::vector<std::pair<int32_t, size_t>> order_index_pairs;
    order_index_pairs.reserve(account_count);

    for (size_t i = 0; i < account_count; i++) {
        order_index_pairs.emplace_back(
            m_vault_data.accounts(static_cast<int>(i)).global_display_order(),
            i
        );
    }

    std::sort(order_index_pairs.begin(), order_index_pairs.end());

    for (size_t i = 0; i < account_count; i++) {
        size_t account_idx = order_index_pairs[i].second;
        m_vault_data.mutable_accounts(static_cast<int>(account_idx))->set_global_display_order(static_cast<int32_t>(i));
    }

    m_modified_flag = true;
    return true;
}

bool AccountManager::can_delete_account(size_t account_index) const noexcept {
    // Basic validation: check if index is valid
    return account_index < get_account_count();
}

}  // namespace KeepTower
