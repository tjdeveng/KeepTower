// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 TJDev

#include "GroupManager.h"
#include "../../utils/Cpp23Compat.h"
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>
#include <cctype>

#if KEEPTOWER_HAS_RANGES
#include <ranges>
namespace ranges = std::ranges;
namespace views = std::views;
#endif

namespace KeepTower {

GroupManager::GroupManager(keeptower::VaultData& vault_data, bool& modified_flag)
    : m_vault_data(vault_data), m_modified_flag(modified_flag) {}

std::string GroupManager::create_group(std::string_view name) {
    // Validate group name
    if (!is_valid_group_name(name)) {
        return "";
    }

    // Convert to std::string for protobuf API
    std::string name_str{name};

    // Check for duplicate names (usability)
#if KEEPTOWER_HAS_RANGES
    // Modern C++23: Use ranges::any_of for cleaner logic
    auto has_duplicate = ranges::any_of(m_vault_data.groups(),
        [&name_str](const auto& group) { return group.group_name() == name_str; });
    if (has_duplicate) {
        return "";  // Group with this name already exists
    }
#else
    // GCC 13 fallback: Traditional loop
    for (int i = 0; i < m_vault_data.groups_size(); ++i) {
        if (m_vault_data.groups(i).group_name() == name_str) {
            return "";  // Group with this name already exists
        }
    }
#endif

    // Generate unique group ID
    std::string group_id = generate_uuid();

    // Create new group
    auto* new_group = m_vault_data.add_groups();
    new_group->set_group_id(group_id);
    new_group->set_group_name(name_str);
    new_group->set_is_system_group(false);
    new_group->set_display_order(m_vault_data.groups_size() - 1);
    new_group->set_is_expanded(true);  // New groups start expanded

    m_modified_flag = true;
    return group_id;
}

bool GroupManager::delete_group(std::string_view group_id) {
    // Validate group ID format (basic check)
    if (group_id.empty()) {
        return false;
    }

    // Find the group
    int group_index = -1;
#if KEEPTOWER_HAS_RANGES
    // Modern C++23: Use ranges::find_if for cleaner search
    auto it = ranges::find_if(m_vault_data.groups(),
        [group_id](const auto& group) { return group.group_id() == group_id; });

    if (it != m_vault_data.groups().end()) {
        // Prevent deletion of system groups
        if (it->is_system_group()) {
            return false;
        }
        group_index = static_cast<int>(std::distance(m_vault_data.groups().begin(), it));
    }
#else
    // GCC 13 fallback: Traditional loop
    for (int i = 0; i < m_vault_data.groups_size(); ++i) {
        if (m_vault_data.groups(i).group_id() == group_id) {
            // Prevent deletion of system groups
            if (m_vault_data.groups(i).is_system_group()) {
                return false;
            }
            group_index = i;
            break;
        }
    }
#endif

    if (group_index == -1) {
        return false;  // Group not found
    }

    // Remove all references to this group from accounts
    for (int i = 0; i < m_vault_data.accounts_size(); ++i) {
        auto* account = m_vault_data.mutable_accounts(i);
        auto* groups = account->mutable_groups();

        // Remove matching group memberships
        for (int j = groups->size() - 1; j >= 0; --j) {
            if (groups->Get(j).group_id() == group_id) {
                groups->erase(groups->begin() + j);
            }
        }
    }

    // Remove the group itself
    m_vault_data.mutable_groups()->erase(
        m_vault_data.mutable_groups()->begin() + group_index
    );

    m_modified_flag = true;
    return true;
}

bool GroupManager::rename_group(std::string_view group_id, std::string_view new_name) {
    // Validate new name
    if (!is_valid_group_name(new_name)) {
        return false;
    }

    // Find the group
    keeptower::AccountGroup* group = find_group_by_id(group_id);
    if (!group) {
        return false;
    }

    // Cannot rename system groups
    if (group->is_system_group()) {
        return false;
    }

    // Check for duplicate name (case-sensitive)
    for (const auto& existing_group : m_vault_data.groups()) {
        if (existing_group.group_id() != group_id &&
            existing_group.group_name() == new_name) {
            return false;  // Name already exists
        }
    }

    // Update the group name
    group->set_group_name(std::string{new_name});
    m_modified_flag = true;
    return true;
}

bool GroupManager::reorder_group(std::string_view group_id, int new_order) {
    // Validate new order is reasonable
    if (new_order < 0) {
        return false;
    }

    // Find the group
    keeptower::AccountGroup* group = find_group_by_id(group_id);
    if (!group) {
        return false;
    }

    // System groups always have display_order = 0, cannot be reordered
    if (group->is_system_group()) {
        return false;
    }

    // Update the display order
    group->set_display_order(new_order);
    m_modified_flag = true;
    return true;
}

bool GroupManager::add_account_to_group(size_t account_index, std::string_view group_id) {
    // Validate indices
    if (account_index >= static_cast<size_t>(m_vault_data.accounts_size())) {
        return false;
    }

    // Validate group exists
    const auto* group = find_group_by_id(group_id);
    if (!group) {
        return false;
    }

    auto* account = m_vault_data.mutable_accounts(static_cast<int>(account_index));

    // Check if already in group (prevent duplicates)
    for (const auto& membership : account->groups()) {
        if (membership.group_id() == group_id) {
            return true;  // Already in group, success (idempotent)
        }
    }

    // Add group membership
    auto* membership = account->add_groups();
    membership->set_group_id(std::string{group_id});
    membership->set_display_order(-1);  // Use automatic ordering initially

    m_modified_flag = true;
    return true;
}

bool GroupManager::remove_account_from_group(size_t account_index, std::string_view group_id) {
    // Validate indices
    if (account_index >= static_cast<size_t>(m_vault_data.accounts_size())) {
        return false;
    }

    auto* account = m_vault_data.mutable_accounts(static_cast<int>(account_index));
    auto* groups = account->mutable_groups();

    // Find and remove the group membership
    bool found = false;
    for (int i = groups->size() - 1; i >= 0; --i) {
        if (groups->Get(i).group_id() == group_id) {
            groups->erase(groups->begin() + i);
            found = true;
            break;  // Only remove one membership (should be unique anyway)
        }
    }

    if (!found) {
        return true;  // Not in group, success (idempotent)
    }

    m_modified_flag = true;
    return true;
}

bool GroupManager::reorder_account_in_group(size_t account_index,
                                           std::string_view group_id,
                                           int new_order) {
    // Validate account index
    if (account_index >= static_cast<size_t>(m_vault_data.accounts_size())) {
        return false;
    }

    // Validate group exists
    const keeptower::AccountGroup* group = find_group_by_id(group_id);
    if (!group) {
        return false;
    }

    // Validate new order is reasonable
    if (new_order < 0) {
        return false;
    }

    auto* account = m_vault_data.mutable_accounts(static_cast<int>(account_index));

    // Find the membership for this group
    for (int i = 0; i < account->groups_size(); ++i) {
        auto* membership = account->mutable_groups(i);
        if (membership->group_id() == group_id) {
            membership->set_display_order(new_order);
            m_modified_flag = true;
            return true;
        }
    }

    // Account is not in this group
    return false;
}

std::string GroupManager::get_favorites_group_id() {
    // Look for existing Favorites group
    for (int i = 0; i < m_vault_data.groups_size(); ++i) {
        const auto& group = m_vault_data.groups(i);
        if (group.is_system_group() && group.group_name() == "Favorites") {
            return group.group_id();
        }
    }

    // Create Favorites group if it doesn't exist
    std::string group_id = generate_uuid();

    auto* favorites_group = m_vault_data.add_groups();
    favorites_group->set_group_id(group_id);
    favorites_group->set_group_name("Favorites");
    favorites_group->set_is_system_group(true);
    favorites_group->set_display_order(0);  // Always first
    favorites_group->set_is_expanded(true);  // Always expanded
    favorites_group->set_icon("favorite");  // Special icon

    m_modified_flag = true;
    return group_id;
}

bool GroupManager::is_account_in_group(size_t account_index, std::string_view group_id) const {
    // Validate indices
    if (account_index >= static_cast<size_t>(m_vault_data.accounts_size())) {
        return false;
    }

    const auto& account = m_vault_data.accounts(static_cast<int>(account_index));

    // Check if account has this group membership
    for (const auto& membership : account.groups()) {
        if (membership.group_id() == group_id) {
            return true;
        }
    }

    return false;
}

std::vector<keeptower::AccountGroup> GroupManager::get_all_groups() const {
    std::vector<keeptower::AccountGroup> groups;
    groups.reserve(m_vault_data.groups_size());

    // Copy all groups from vault data
    for (const auto& group : m_vault_data.groups()) {
        groups.push_back(group);
    }

    return groups;
}

size_t GroupManager::get_group_count() const {
    return static_cast<size_t>(m_vault_data.groups_size());
}

bool GroupManager::is_valid_group_name(std::string_view name) const {
    if (name.empty() || name.length() > 100) {
        return false;
    }

    // Check for control characters and path traversal
    for (char c : name) {
        if (std::iscntrl(static_cast<unsigned char>(c))) {
            return false;
        }
    }

    // Reject names that could cause issues
    if (name == "." || name == ".." || name.find('/') != std::string::npos ||
        name.find('\\') != std::string::npos) {
        return false;
    }

    return true;
}

std::string GroupManager::generate_uuid() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;

    // UUID v4 format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
    uint64_t part1 = dis(gen);
    uint64_t part2 = dis(gen);

    // Set version bits (4xxx)
    part1 = (part1 & 0xFFFFFFFF0000FFFFULL) | 0x0000000040000000ULL;
    // Set variant bits (yxxx where y = 8, 9, A, or B)
    part2 = (part2 & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(8) << ((part1 >> 32) & 0xFFFFFFFF) << '-'
        << std::setw(4) << ((part1 >> 16) & 0xFFFF) << '-'
        << std::setw(4) << (part1 & 0xFFFF) << '-'
        << std::setw(4) << ((part2 >> 48) & 0xFFFF) << '-'
        << std::setw(12) << (part2 & 0xFFFFFFFFFFFFULL);

    return oss.str();
}

keeptower::AccountGroup* GroupManager::find_group_by_id(std::string_view group_id) {
    for (int i = 0; i < m_vault_data.groups_size(); ++i) {
        if (m_vault_data.groups(i).group_id() == group_id) {
            return m_vault_data.mutable_groups(i);
        }
    }
    return nullptr;
}

const keeptower::AccountGroup* GroupManager::find_group_by_id(std::string_view group_id) const {
    for (int i = 0; i < m_vault_data.groups_size(); ++i) {
        if (m_vault_data.groups(i).group_id() == group_id) {
            return &m_vault_data.groups(i);
        }
    }
    return nullptr;
}

}  // namespace KeepTower
