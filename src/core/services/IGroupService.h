// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file IGroupService.h
 * @brief Interface for group business logic operations
 *
 * Part of Phase 3 refactoring: Service Layer
 * Separates group management business logic from data access
 */

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <expected>
#include "../record.pb.h"
#include "IAccountService.h"  // For ServiceError

namespace KeepTower {

/**
 * @brief Interface for group business logic operations
 *
 * Provides high-level group operations with business rules:
 * - Group name validation
 * - Duplicate name detection
 * - Account-group relationship management
 * - Cascade deletion handling
 *
 * Design Principles:
 * - Delegates data access to IGroupRepository
 * - Enforces business rules before repository calls
 * - Coordinates with account repository when needed
 * - Returns service-level errors
 *
 * @note Services should not depend on UI components
 */
class IGroupService {
public:
    virtual ~IGroupService() = default;

    /**
     * @brief Create a new group with validation
     * @param name Group name
     * @return Group ID or error
     *
     * Validates:
     * - Name is not empty
     * - Name is unique
     * - Name length within limits
     *
     * Errors:
     * - VAULT_CLOSED: Vault not open
     * - VALIDATION_FAILED: Name empty or invalid
     * - DUPLICATE_NAME: Group name already exists
     * - FIELD_TOO_LONG: Name exceeds maximum length
     * - SAVE_FAILED: Could not persist to vault
     */
    [[nodiscard]] virtual std::expected<std::string, ServiceError>
        create_group(std::string_view name) = 0;

    /**
     * @brief Get group by ID
     * @param group_id Group identifier
     * @return Group record or error
     */
    [[nodiscard]] virtual std::expected<keeptower::AccountGroup, ServiceError>
        get_group(std::string_view group_id) const = 0;

    /**
     * @brief Get all groups
     * @return Vector of all groups or error
     */
    [[nodiscard]] virtual std::expected<std::vector<keeptower::AccountGroup>, ServiceError>
        get_all_groups() const = 0;

    /**
     * @brief Rename group with validation
     * @param group_id Group identifier
     * @param new_name New group name
     * @return Success or error
     *
     * Validates same rules as create_group
     */
    [[nodiscard]] virtual std::expected<void, ServiceError>
        rename_group(std::string_view group_id, std::string_view new_name) = 0;

    /**
     * @brief Delete group and remove from all accounts
     * @param group_id Group identifier
     * @return Success or error
     *
     * This is a cascade operation that:
     * 1. Removes group from all accounts
     * 2. Deletes the group record
     */
    [[nodiscard]] virtual std::expected<void, ServiceError>
        delete_group(std::string_view group_id) = 0;

    /**
     * @brief Add account to group
     * @param account_index Account index
     * @param group_id Group identifier
     * @return Success or error
     */
    [[nodiscard]] virtual std::expected<void, ServiceError>
        add_account_to_group(size_t account_index, std::string_view group_id) = 0;

    /**
     * @brief Remove account from group
     * @param account_index Account index
     * @param group_id Group identifier
     * @return Success or error
     */
    [[nodiscard]] virtual std::expected<void, ServiceError>
        remove_account_from_group(size_t account_index, std::string_view group_id) = 0;

    /**
     * @brief Get accounts in a group
     * @param group_id Group identifier
     * @return Vector of account indices or error
     */
    [[nodiscard]] virtual std::expected<std::vector<size_t>, ServiceError>
        get_accounts_in_group(std::string_view group_id) const = 0;

    /**
     * @brief Get group count
     * @return Number of groups or error
     */
    [[nodiscard]] virtual std::expected<size_t, ServiceError>
        count() const = 0;

    /**
     * @brief Check if group name is unique
     * @param name Group name to check
     * @param exclude_id Group ID to exclude from check (for renames)
     * @return true if name is unique
     */
    [[nodiscard]] virtual bool
        is_name_unique(std::string_view name,
                      std::string_view exclude_id = "") const = 0;
};

}  // namespace KeepTower
