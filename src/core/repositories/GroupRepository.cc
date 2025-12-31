// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file GroupRepository.cc
 * @brief Implementation of GroupRepository
 *
 * Part of Phase 2 refactoring: Repository Pattern
 *
 * This is a transitional implementation that wraps VaultManager.
 * Future refactoring will move group-specific logic from VaultManager
 * into this repository.
 *
 * Note: Some operations like get_accounts_in_group() iterate through
 * all accounts because VaultManager doesn't provide a direct method.
 * This will be optimized in future refactoring phases.
 */

#include "GroupRepository.h"
#include <stdexcept>
#include <algorithm>

namespace KeepTower {

GroupRepository::GroupRepository(VaultManager* vault_manager)
    : m_vault_manager(vault_manager) {
    if (!m_vault_manager) {
        throw std::invalid_argument("GroupRepository: vault_manager cannot be null");
    }
}

std::expected<std::string, RepositoryError>
GroupRepository::create(std::string_view name) {
    if (!m_vault_manager->is_vault_open()) {
        return std::unexpected(RepositoryError::VAULT_CLOSED);
    }

    if (name.empty()) {
        return std::unexpected(RepositoryError::INVALID_INDEX);  // Reusing for invalid input
    }

    // Delegate to VaultManager
    std::string group_id = m_vault_manager->create_group(name);

    if (group_id.empty()) {
        return std::unexpected(RepositoryError::SAVE_FAILED);
    }

    return group_id;
}

std::expected<keeptower::AccountGroup, RepositoryError>
GroupRepository::get(std::string_view group_id) const {
    if (!m_vault_manager->is_vault_open()) {
        return std::unexpected(RepositoryError::VAULT_CLOSED);
    }

    // VaultManager doesn't have get_group_by_id, so search in all groups
    auto all_groups = m_vault_manager->get_all_groups();

    for (const auto& group : all_groups) {
        if (group.group_id() == group_id) {
            return group;
        }
    }

    return std::unexpected(RepositoryError::ACCOUNT_NOT_FOUND);  // Reusing for group not found
}

std::expected<std::vector<keeptower::AccountGroup>, RepositoryError>
GroupRepository::get_all() const {
    if (!m_vault_manager->is_vault_open()) {
        return std::unexpected(RepositoryError::VAULT_CLOSED);
    }

    return m_vault_manager->get_all_groups();
}

std::expected<void, RepositoryError>
GroupRepository::update(const keeptower::AccountGroup& group) {
    if (!m_vault_manager->is_vault_open()) {
        return std::unexpected(RepositoryError::VAULT_CLOSED);
    }

    // Verify group exists first
    if (!exists(group.group_id())) {
        return std::unexpected(RepositoryError::ACCOUNT_NOT_FOUND);
    }

    // Use VaultManager::rename_group for name updates
    // Note: This only supports renaming currently
    // Other group property updates would need additional VaultManager methods
    if (!m_vault_manager->rename_group(group.group_id(), group.group_name())) {
        return std::unexpected(RepositoryError::SAVE_FAILED);
    }

    return {};
}

std::expected<void, RepositoryError>
GroupRepository::remove(std::string_view group_id) {
    if (!m_vault_manager->is_vault_open()) {
        return std::unexpected(RepositoryError::VAULT_CLOSED);
    }

    // Verify group exists
    if (!exists(group_id)) {
        return std::unexpected(RepositoryError::ACCOUNT_NOT_FOUND);
    }

    // Delegate to VaultManager
    if (m_vault_manager->delete_group(group_id)) {
        return {};
    }

    return std::unexpected(RepositoryError::SAVE_FAILED);
}

std::expected<size_t, RepositoryError>
GroupRepository::count() const {
    if (!m_vault_manager->is_vault_open()) {
        return std::unexpected(RepositoryError::VAULT_CLOSED);
    }

    auto groups = m_vault_manager->get_all_groups();
    return groups.size();
}

std::expected<void, RepositoryError>
GroupRepository::add_account_to_group(size_t account_index, std::string_view group_id) {
    if (!m_vault_manager->is_vault_open()) {
        return std::unexpected(RepositoryError::VAULT_CLOSED);
    }

    // Verify account index
    if (account_index >= m_vault_manager->get_account_count()) {
        return std::unexpected(RepositoryError::INVALID_INDEX);
    }

    // Verify group exists
    if (!exists(group_id)) {
        return std::unexpected(RepositoryError::ACCOUNT_NOT_FOUND);
    }

    // Delegate to VaultManager
    if (m_vault_manager->add_account_to_group(account_index, group_id)) {
        return {};
    }

    return std::unexpected(RepositoryError::SAVE_FAILED);
}

std::expected<void, RepositoryError>
GroupRepository::remove_account_from_group(size_t account_index, std::string_view group_id) {
    if (!m_vault_manager->is_vault_open()) {
        return std::unexpected(RepositoryError::VAULT_CLOSED);
    }

    // Verify account index
    if (account_index >= m_vault_manager->get_account_count()) {
        return std::unexpected(RepositoryError::INVALID_INDEX);
    }

    // Verify group exists
    if (!exists(group_id)) {
        return std::unexpected(RepositoryError::ACCOUNT_NOT_FOUND);
    }

    // Delegate to VaultManager
    if (m_vault_manager->remove_account_from_group(account_index, group_id)) {
        return {};
    }

    return std::unexpected(RepositoryError::SAVE_FAILED);
}

std::expected<std::vector<size_t>, RepositoryError>
GroupRepository::get_accounts_in_group(std::string_view group_id) const {
    if (!m_vault_manager->is_vault_open()) {
        return std::unexpected(RepositoryError::VAULT_CLOSED);
    }

    // Verify group exists
    if (!exists(group_id)) {
        return std::unexpected(RepositoryError::ACCOUNT_NOT_FOUND);
    }

    // VaultManager doesn't have get_accounts_in_group
    // Need to search through all accounts
    std::vector<size_t> account_indices;
    const auto& all_accounts = m_vault_manager->get_all_accounts();

    for (size_t i = 0; i < all_accounts.size(); ++i) {
        const auto& account = all_accounts[i];

        // Check if this account has the group_id (groups field is GroupMembership)
        for (int j = 0; j < account.groups_size(); ++j) {
            if (account.groups(j).group_id() == group_id) {
                account_indices.push_back(i);
                break;
            }
        }
    }

    return account_indices;
}

bool GroupRepository::is_vault_open() const noexcept {
    return m_vault_manager && m_vault_manager->is_vault_open();
}

bool GroupRepository::exists(std::string_view group_id) const noexcept {
    if (!m_vault_manager->is_vault_open()) {
        return false;
    }

    auto all_groups = m_vault_manager->get_all_groups();

    for (const auto& group : all_groups) {
        if (group.group_id() == group_id) {
            return true;
        }
    }

    return false;
}

}  // namespace KeepTower
