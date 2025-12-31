// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file GroupService.h
 * @brief Concrete implementation of IGroupService
 *
 * Part of Phase 3 refactoring: Service Layer
 * Implements business logic for group operations
 */

#pragma once

#include "IGroupService.h"
#include "../repositories/IGroupRepository.h"
#include <memory>

namespace KeepTower {

// Group name length limits (matches VaultManager constraints)
inline constexpr int MAX_GROUP_NAME_LENGTH = 100;  ///< Maximum group name length

/**
 * @brief Concrete group service implementation
 *
 * Implements business logic for group operations:
 * - Group name validation
 * - Duplicate name detection
 * - Account-group relationship management
 * - Cascade deletion
 *
 * Thread Safety:
 * - Same as underlying repository
 * - Not thread-safe by default
 * - All calls must be from same thread
 */
class GroupService : public IGroupService {
public:
    /**
     * @brief Construct service with repository
     * @param group_repo Non-owning pointer to group repository
     * @throws std::invalid_argument if group_repo is null
     */
    explicit GroupService(IGroupRepository* group_repo);

    ~GroupService() override = default;

    // Non-copyable, non-movable (holds non-owning pointer)
    GroupService(const GroupService&) = delete;
    GroupService& operator=(const GroupService&) = delete;
    GroupService(GroupService&&) = delete;
    GroupService& operator=(GroupService&&) = delete;

    // IGroupService interface implementation
    [[nodiscard]] std::expected<std::string, ServiceError>
        create_group(std::string_view name) override;

    [[nodiscard]] std::expected<keeptower::AccountGroup, ServiceError>
        get_group(std::string_view group_id) const override;

    [[nodiscard]] std::expected<std::vector<keeptower::AccountGroup>, ServiceError>
        get_all_groups() const override;

    [[nodiscard]] std::expected<void, ServiceError>
        rename_group(std::string_view group_id, std::string_view new_name) override;

    [[nodiscard]] std::expected<void, ServiceError>
        delete_group(std::string_view group_id) override;

    [[nodiscard]] std::expected<void, ServiceError>
        add_account_to_group(size_t account_index, std::string_view group_id) override;

    [[nodiscard]] std::expected<void, ServiceError>
        remove_account_from_group(size_t account_index, std::string_view group_id) override;

    [[nodiscard]] std::expected<std::vector<size_t>, ServiceError>
        get_accounts_in_group(std::string_view group_id) const override;

    [[nodiscard]] std::expected<size_t, ServiceError>
        count() const override;

    [[nodiscard]] bool
        is_name_unique(std::string_view name,
                      std::string_view exclude_id = "") const override;

private:
    IGroupRepository* m_group_repo;  ///< Non-owning pointer to repository

    /**
     * @brief Validate group name
     * @param name Group name to validate
     * @return Success or validation error
     */
    [[nodiscard]] std::expected<void, ServiceError>
        validate_group_name(std::string_view name) const;
};

}  // namespace KeepTower
