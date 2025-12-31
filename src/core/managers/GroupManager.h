// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 TJDev

#ifndef KEEPTOWER_GROUPMANAGER_H
#define KEEPTOWER_GROUPMANAGER_H

#include <string>
#include <string_view>
#include <vector>
#include "record.pb.h"

namespace KeepTower {

/**
 * @brief Manages account group operations for the vault
 *
 * GroupManager provides a focused interface for group-related operations,
 * including creation, deletion, renaming, and membership management.
 *
 * @section Design
 * - Does not own vault data - holds references
 * - Delegates to VaultManager for save operations
 * - Non-copyable, non-movable (due to reference members)
 *
 * @section Thread Safety
 * - Not thread-safe by itself
 * - Caller must ensure thread safety (e.g., via VaultManager's mutex)
 * - All methods require vault to be open
 *
 * @section Usage Example
 * @code
 * keeptower::VaultData vault_data;
 * bool modified = false;
 * GroupManager gm(vault_data, modified);
 *
 * // Create a group
 * std::string group_id = gm.create_group("Work");
 *
 * // Add account to group
 * gm.add_account_to_group(0, group_id);
 *
 * // Query groups
 * auto groups = gm.get_all_groups();
 * @endcode
 */
class GroupManager {
public:
    /**
     * @brief Construct a GroupManager
     * @param vault_data Reference to vault data (must outlive GroupManager)
     * @param modified_flag Reference to modified flag (must outlive GroupManager)
     */
    GroupManager(keeptower::VaultData& vault_data, bool& modified_flag);

    // Delete copy and move operations (holds references)
    GroupManager(const GroupManager&) = delete;
    GroupManager& operator=(const GroupManager&) = delete;
    GroupManager(GroupManager&&) = delete;
    GroupManager& operator=(GroupManager&&) = delete;

    /**
     * @brief Create a new account group
     * @param name Display name for the group
     * @return Group ID (UUID) if created successfully, empty string on error
     *
     * @note Validates name for security (length, special characters)
     * @note Prevents duplicate group names
     * @note Sets modified flag on success
     */
    [[nodiscard]] std::string create_group(std::string_view name);

    /**
     * @brief Delete an account group
     * @param group_id UUID of the group to delete
     * @return true if deleted successfully, false on error
     *
     * @note System groups (e.g., "Favorites") cannot be deleted
     * @note Removes all account memberships to this group
     * @note Sets modified flag on success
     */
    [[nodiscard]] bool delete_group(std::string_view group_id);

    /**
     * @brief Rename an existing account group
     * @param group_id UUID of the group to rename
     * @param new_name New display name for the group
     * @return true if renamed successfully, false on error
     *
     * @note System groups (e.g., "Favorites") cannot be renamed
     * @note Validates new name (length, special characters)
     * @note Prevents duplicate names
     * @note Sets modified flag on success
     */
    [[nodiscard]] bool rename_group(std::string_view group_id, std::string_view new_name);

    /**
     * @brief Reorder groups in the UI display
     * @param group_id UUID of the group to move
     * @param new_order New display order (0 = first, higher = later)
     * @return true if reordered successfully, false on error
     *
     * @note System groups maintain display_order = 0 (always first)
     * @note Sets modified flag on success
     */
    [[nodiscard]] bool reorder_group(std::string_view group_id, int new_order);

    /**
     * @brief Add an account to a group
     * @param account_index Index of the account
     * @param group_id UUID of the group
     * @return true if added successfully, false on error
     *
     * @note Accounts can belong to multiple groups
     * @note Idempotent - returns true if already in group
     * @note Validates account index and group existence
     * @note Sets modified flag on success
     */
    [[nodiscard]] bool add_account_to_group(size_t account_index, std::string_view group_id);

    /**
     * @brief Remove an account from a group
     * @param account_index Index of the account
     * @param group_id UUID of the group
     * @return true if removed successfully, false on error
     *
     * @note Idempotent - returns true if not in group
     * @note Validates account index
     * @note Sets modified flag on success
     */
    [[nodiscard]] bool remove_account_from_group(size_t account_index, std::string_view group_id);

    /**
     * @brief Reorder an account within a specific group
     * @param account_index Index of the account
     * @param group_id UUID of the group
     * @param new_order New display order within the group (0 = first, higher = later)
     * @return true if reordered successfully, false on error
     *
     * @note Account must already be a member of the group
     * @note Sets modified flag on success
     */
    [[nodiscard]] bool reorder_account_in_group(size_t account_index,
                                                 std::string_view group_id,
                                                 int new_order);

    /**
     * @brief Get or create the "Favorites" system group
     * @return Group ID of the Favorites group, empty string on error
     *
     * @note Auto-creates the group if it doesn't exist
     * @note Favorites is always display_order = 0 (first)
     * @note Sets modified flag if created
     */
    [[nodiscard]] std::string get_favorites_group_id();

    /**
     * @brief Check if an account belongs to a specific group
     * @param account_index Index of the account
     * @param group_id UUID of the group
     * @return true if the account is in the group, false otherwise
     */
    [[nodiscard]] bool is_account_in_group(size_t account_index, std::string_view group_id) const;

    /**
     * @brief Get all account groups
     * @return Vector of all groups in the vault
     */
    [[nodiscard]] std::vector<keeptower::AccountGroup> get_all_groups() const;

    /**
     * @brief Get number of groups
     * @return Count of groups
     */
    [[nodiscard]] size_t get_group_count() const;

private:
    keeptower::VaultData& m_vault_data;  ///< Reference to vault data
    bool& m_modified_flag;               ///< Reference to modified flag

    /**
     * @brief Validate group name for security and usability
     * @param name Proposed group name
     * @return true if valid, false if invalid
     */
    [[nodiscard]] bool is_valid_group_name(std::string_view name) const;

    /**
     * @brief Generate a UUID v4 for group IDs
     * @return UUID string
     */
    [[nodiscard]] static std::string generate_uuid();

    /**
     * @brief Find a group by ID
     * @param group_id Group ID to find
     * @return Pointer to group if found, nullptr otherwise
     */
    [[nodiscard]] keeptower::AccountGroup* find_group_by_id(std::string_view group_id);

    /**
     * @brief Find a group by ID (const version)
     * @param group_id Group ID to find
     * @return Pointer to group if found, nullptr otherwise
     */
    [[nodiscard]] const keeptower::AccountGroup* find_group_by_id(std::string_view group_id) const;
};

}  // namespace KeepTower

#endif  // KEEPTOWER_GROUPMANAGER_H
