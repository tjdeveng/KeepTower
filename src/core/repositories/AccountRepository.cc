// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file AccountRepository.cc
 * @brief Implementation of AccountRepository
 *
 * Part of Phase 2 refactoring: Repository Pattern
 *
 * This is a transitional implementation that wraps VaultManager.
 * Future refactoring will move account-specific logic from VaultManager
 * into this repository, making VaultManager a thin coordinator.
 *
 * Error Handling Strategy:
 * - Checks vault state first (VAULT_CLOSED)
 * - Validates indices/IDs (INVALID_INDEX, ACCOUNT_NOT_FOUND)
 * - Checks permissions (PERMISSION_DENIED)
 * - Reports operation failures (SAVE_FAILED)
 */

#include "AccountRepository.h"
#include <stdexcept>
#include <algorithm>

namespace KeepTower {

AccountRepository::AccountRepository(VaultManager* vault_manager)
    : m_vault_manager(vault_manager) {
    if (!m_vault_manager) {
        throw std::invalid_argument("AccountRepository: vault_manager cannot be null");
    }
}

std::expected<void, RepositoryError>
AccountRepository::add(const keeptower::AccountRecord& account) {
    if (!m_vault_manager->is_vault_open()) {
        return std::unexpected(RepositoryError::VAULT_CLOSED);
    }

    // Delegate to VaultManager
    if (m_vault_manager->add_account(account)) {
        return {};
    }

    // VaultManager::add_account returns false on failure
    // Note: VaultManager does not provide specific error codes yet
    return std::unexpected(RepositoryError::SAVE_FAILED);
}

std::expected<keeptower::AccountRecord, RepositoryError>
AccountRepository::get(size_t index) const {
    if (!m_vault_manager->is_vault_open()) {
        return std::unexpected(RepositoryError::VAULT_CLOSED);
    }

    // Check bounds first
    if (index >= m_vault_manager->get_account_count()) {
        return std::unexpected(RepositoryError::INVALID_INDEX);
    }

    // Then check permissions (for V2 vaults)
    if (!can_view(index)) {
        return std::unexpected(RepositoryError::PERMISSION_DENIED);
    }

    // Use get_account (returns pointer)
    const auto* account = m_vault_manager->get_account(index);
    if (!account) {
        return std::unexpected(RepositoryError::INVALID_INDEX);
    }

    return *account;
}

std::expected<keeptower::AccountRecord, RepositoryError>
AccountRepository::get_by_id(std::string_view account_id) const {
    if (!m_vault_manager->is_vault_open()) {
        return std::unexpected(RepositoryError::VAULT_CLOSED);
    }

    // Find index by ID
    auto index = find_index_by_id(account_id);
    if (!index) {
        return std::unexpected(RepositoryError::ACCOUNT_NOT_FOUND);
    }

    // Use get() for permission checking
    return get(*index);
}

std::expected<std::vector<keeptower::AccountRecord>, RepositoryError>
AccountRepository::get_all() const {
    if (!m_vault_manager->is_vault_open()) {
        return std::unexpected(RepositoryError::VAULT_CLOSED);
    }

    // Get all accounts from VaultManager
    auto all_accounts = m_vault_manager->get_all_accounts();

    // For V2 vaults, filter by permissions
    if (m_vault_manager->is_v2_vault()) {
        std::vector<keeptower::AccountRecord> viewable;
        viewable.reserve(all_accounts.size());

        for (size_t i = 0; i < all_accounts.size(); ++i) {
            if (can_view(i)) {
                viewable.push_back(all_accounts[i]);
            }
        }

        return viewable;
    }

    // V1 vaults: all accounts are viewable
    return all_accounts;
}

std::expected<void, RepositoryError>
AccountRepository::update(size_t index, const keeptower::AccountRecord& account) {
    if (!m_vault_manager->is_vault_open()) {
        return std::unexpected(RepositoryError::VAULT_CLOSED);
    }

    // Check bounds first
    if (index >= m_vault_manager->get_account_count()) {
        return std::unexpected(RepositoryError::INVALID_INDEX);
    }

    // Then check permissions (for V2 vaults)
    if (!can_modify(index)) {
        return std::unexpected(RepositoryError::PERMISSION_DENIED);
    }

    // Delegate to VaultManager
    if (m_vault_manager->update_account(index, account)) {
        return {};
    }

    return std::unexpected(RepositoryError::SAVE_FAILED);
}

std::expected<void, RepositoryError>
AccountRepository::remove(size_t index) {
    if (!m_vault_manager->is_vault_open()) {
        return std::unexpected(RepositoryError::VAULT_CLOSED);
    }

    // Check bounds first
    if (index >= m_vault_manager->get_account_count()) {
        return std::unexpected(RepositoryError::INVALID_INDEX);
    }

    // Then check permissions (for V2 vaults)
    if (!can_modify(index)) {
        return std::unexpected(RepositoryError::PERMISSION_DENIED);
    }

    // Delegate to VaultManager
    if (m_vault_manager->delete_account(index)) {
        return {};
    }

    return std::unexpected(RepositoryError::SAVE_FAILED);
}

std::expected<size_t, RepositoryError>
AccountRepository::count() const {
    if (!m_vault_manager->is_vault_open()) {
        return std::unexpected(RepositoryError::VAULT_CLOSED);
    }

    return m_vault_manager->get_account_count();
}

bool AccountRepository::can_view(size_t index) const noexcept {
    if (!m_vault_manager->is_vault_open()) {
        return false;
    }

    return m_vault_manager->can_view_account(index);
}

bool AccountRepository::can_modify(size_t index) const noexcept {
    if (!m_vault_manager->is_vault_open()) {
        return false;
    }

    // For V2 vaults, check modify permission (future enhancement: separate can_edit)
    // For V1 vaults, same as can_view (all visible accounts are modifiable)
    // Currently using can_view_account as proxy for both read and write permissions
    return m_vault_manager->can_view_account(index);
}

bool AccountRepository::is_vault_open() const noexcept {
    return m_vault_manager && m_vault_manager->is_vault_open();
}

std::optional<size_t>
AccountRepository::find_index_by_id(std::string_view account_id) const noexcept {
    if (!m_vault_manager->is_vault_open()) {
        return std::nullopt;
    }

    const auto& all_accounts = m_vault_manager->get_all_accounts();
    for (size_t i = 0; i < all_accounts.size(); ++i) {
        if (all_accounts[i].id() == account_id) {
            return i;
        }
    }

    return std::nullopt;
}

}  // namespace KeepTower
