// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file IGroupRepository.h
 * @brief Interface for group data access operations
 *
 * Part of Phase 2 refactoring: Repository Pattern
 * Separates group data access from business logic
 */

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <expected>
#include "../record.pb.h"
#include "IAccountRepository.h"  // For RepositoryError

namespace KeepTower {

/**
 * @brief Interface for group repository operations
 *
 * Provides CRUD operations for account groups with proper error handling.
 * Groups organize accounts hierarchically in the vault.
 *
 * Design Principles:
 * - ID-based access (groups use string IDs, typically UUIDs)
 * - std::expected for explicit error handling
 * - Handles account-group associations
 * - Testable through interface
 *
 * @note All operations assume vault is already open
 * @note Groups are a V1/V2 feature, available in both vault types
 */
class IGroupRepository {
public:
    virtual ~IGroupRepository() = default;

    /**
     * @brief Create a new group
     * @param name Group name (must not be empty)
     * @return Group ID (UUID) or error
     *
     * Errors:
     * - VAULT_CLOSED: Vault not open
     * - INVALID_INDEX: Name is empty or invalid
     * - SAVE_FAILED: Could not persist to vault
     */
    [[nodiscard]] virtual std::expected<std::string, RepositoryError>
        create(std::string_view name) = 0;

    /**
     * @brief Get group by ID
     * @param group_id Group identifier (UUID)
     * @return Group record or error
     *
     * Errors:
     * - VAULT_CLOSED: Vault not open
     * - ACCOUNT_NOT_FOUND: No group with this ID (reusing error enum)
     */
    [[nodiscard]] virtual std::expected<keeptower::AccountGroup, RepositoryError>
        get(std::string_view group_id) const = 0;

    /**
     * @brief Get all groups
     * @return Vector of all groups or error
     *
     * Errors:
     * - VAULT_CLOSED: Vault not open
     */
    [[nodiscard]] virtual std::expected<std::vector<keeptower::AccountGroup>, RepositoryError>
        get_all() const = 0;

    /**
     * @brief Update an existing group
     * @param group Updated group record
     * @return Success or error
     *
     * Errors:
     * - VAULT_CLOSED: Vault not open
     * - ACCOUNT_NOT_FOUND: Group ID not found
     * - SAVE_FAILED: Could not persist changes
     */
    [[nodiscard]] virtual std::expected<void, RepositoryError>
        update(const keeptower::AccountGroup& group) = 0;

    /**
     * @brief Delete a group
     * @param group_id Group identifier (UUID)
     * @return Success or error
     *
     * Note: Deleting a group removes it from all accounts
     *
     * Errors:
     * - VAULT_CLOSED: Vault not open
     * - ACCOUNT_NOT_FOUND: Group ID not found
     * - SAVE_FAILED: Could not persist changes
     */
    [[nodiscard]] virtual std::expected<void, RepositoryError>
        remove(std::string_view group_id) = 0;

    /**
     * @brief Get total count of groups
     * @return Number of groups or error
     *
     * Errors:
     * - VAULT_CLOSED: Vault not open
     */
    [[nodiscard]] virtual std::expected<size_t, RepositoryError>
        count() const = 0;

    /**
     * @brief Add an account to a group
     * @param account_index Zero-based account index
     * @param group_id Group identifier (UUID)
     * @return Success or error
     *
     * Errors:
     * - VAULT_CLOSED: Vault not open
     * - INVALID_INDEX: Account index out of bounds
     * - ACCOUNT_NOT_FOUND: Group ID not found
     * - SAVE_FAILED: Could not persist changes
     */
    [[nodiscard]] virtual std::expected<void, RepositoryError>
        add_account_to_group(size_t account_index, std::string_view group_id) = 0;

    /**
     * @brief Remove an account from a group
     * @param account_index Zero-based account index
     * @param group_id Group identifier (UUID)
     * @return Success or error
     *
     * Errors:
     * - VAULT_CLOSED: Vault not open
     * - INVALID_INDEX: Account index out of bounds
     * - ACCOUNT_NOT_FOUND: Group ID not found
     * - SAVE_FAILED: Could not persist changes
     */
    [[nodiscard]] virtual std::expected<void, RepositoryError>
        remove_account_from_group(size_t account_index, std::string_view group_id) = 0;

    /**
     * @brief Get accounts in a specific group
     * @param group_id Group identifier (UUID)
     * @return Vector of account indices or error
     *
     * Returns indices of accounts belonging to this group.
     *
     * Errors:
     * - VAULT_CLOSED: Vault not open
     * - ACCOUNT_NOT_FOUND: Group ID not found
     */
    [[nodiscard]] virtual std::expected<std::vector<size_t>, RepositoryError>
        get_accounts_in_group(std::string_view group_id) const = 0;

    /**
     * @brief Check if vault is currently open
     * @return true if vault is open
     */
    [[nodiscard]] virtual bool is_vault_open() const noexcept = 0;

    /**
     * @brief Check if a group exists
     * @param group_id Group identifier (UUID)
     * @return true if group exists
     */
    [[nodiscard]] virtual bool exists(std::string_view group_id) const noexcept = 0;
};

}  // namespace KeepTower
