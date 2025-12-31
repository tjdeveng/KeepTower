// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file GroupService.cc
 * @brief Implementation of GroupService
 *
 * Part of Phase 3 refactoring: Service Layer
 * Implements business logic and validation for group operations
 */

#include "GroupService.h"
#include <stdexcept>

namespace KeepTower {

GroupService::GroupService(IGroupRepository* group_repo)
    : m_group_repo(group_repo) {
    if (!m_group_repo) {
        throw std::invalid_argument("GroupService: group_repo cannot be null");
    }
}

std::expected<std::string, ServiceError>
GroupService::create_group(std::string_view name) {
    // Validate group name
    auto validation_result = validate_group_name(name);
    if (!validation_result) {
        return std::unexpected(validation_result.error());
    }

    // Check for duplicate name
    if (!is_name_unique(name)) {
        return std::unexpected(ServiceError::DUPLICATE_NAME);
    }

    // Delegate to repository
    auto result = m_group_repo->create(name);
    if (!result) {
        return std::unexpected(to_service_error(result.error()));
    }

    return result.value();
}

std::expected<keeptower::AccountGroup, ServiceError>
GroupService::get_group(std::string_view group_id) const {
    auto result = m_group_repo->get(group_id);
    if (!result) {
        return std::unexpected(to_service_error(result.error()));
    }
    return result.value();
}

std::expected<std::vector<keeptower::AccountGroup>, ServiceError>
GroupService::get_all_groups() const {
    auto result = m_group_repo->get_all();
    if (!result) {
        return std::unexpected(to_service_error(result.error()));
    }
    return result.value();
}

std::expected<void, ServiceError>
GroupService::rename_group(std::string_view group_id, std::string_view new_name) {
    // Validate new name
    auto validation_result = validate_group_name(new_name);
    if (!validation_result) {
        return std::unexpected(validation_result.error());
    }

    // Check for duplicate name (excluding current group)
    if (!is_name_unique(new_name, group_id)) {
        return std::unexpected(ServiceError::DUPLICATE_NAME);
    }

    // Get current group
    auto group_result = m_group_repo->get(group_id);
    if (!group_result) {
        return std::unexpected(to_service_error(group_result.error()));
    }

    // Update group name
    auto group = group_result.value();
    group.set_group_name(std::string(new_name));

    // Save changes
    auto update_result = m_group_repo->update(group);
    if (!update_result) {
        return std::unexpected(to_service_error(update_result.error()));
    }

    return {};
}

std::expected<void, ServiceError>
GroupService::delete_group(std::string_view group_id) {
    // Delegate to repository (handles cascade deletion)
    auto result = m_group_repo->remove(group_id);
    if (!result) {
        return std::unexpected(to_service_error(result.error()));
    }
    return {};
}

std::expected<void, ServiceError>
GroupService::add_account_to_group(size_t account_index, std::string_view group_id) {
    auto result = m_group_repo->add_account_to_group(account_index, group_id);
    if (!result) {
        return std::unexpected(to_service_error(result.error()));
    }
    return {};
}

std::expected<void, ServiceError>
GroupService::remove_account_from_group(size_t account_index, std::string_view group_id) {
    auto result = m_group_repo->remove_account_from_group(account_index, group_id);
    if (!result) {
        return std::unexpected(to_service_error(result.error()));
    }
    return {};
}

std::expected<std::vector<size_t>, ServiceError>
GroupService::get_accounts_in_group(std::string_view group_id) const {
    auto result = m_group_repo->get_accounts_in_group(group_id);
    if (!result) {
        return std::unexpected(to_service_error(result.error()));
    }
    return result.value();
}

std::expected<size_t, ServiceError>
GroupService::count() const {
    auto result = m_group_repo->count();
    if (!result) {
        return std::unexpected(to_service_error(result.error()));
    }
    return result.value();
}

bool GroupService::is_name_unique(std::string_view name,
                                 std::string_view exclude_id) const {
    // Empty names are never unique (invalid)
    if (name.empty()) {
        return false;
    }

    // Get all groups
    auto groups_result = m_group_repo->get_all();
    if (!groups_result) {
        return true;  // Can't check, assume unique
    }

    const auto& groups = groups_result.value();

    // Check for duplicate name
    for (const auto& group : groups) {
        if (group.group_id() != exclude_id && group.group_name() == name) {
            return false;  // Duplicate found
        }
    }

    return true;  // Name is unique
}

std::expected<void, ServiceError>
GroupService::validate_group_name(std::string_view name) const {
    // Check if name is empty
    if (name.empty()) {
        return std::unexpected(ServiceError::VALIDATION_FAILED);
    }

    // Check length
    if (name.size() > MAX_GROUP_NAME_LENGTH) {
        return std::unexpected(ServiceError::FIELD_TOO_LONG);
    }

    return {};
}

}  // namespace KeepTower
