// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 TJDev

/**
 * @file AccountManager.h
 * @brief Account CRUD operations for vault management
 *
 * This class handles all account-related operations including:
 * - Adding, updating, and deleting accounts
 * - Account retrieval and validation
 * - Account reordering for UI drag-and-drop
 * - Permission checking for account operations
 */

#ifndef ACCOUNTMANAGER_H
#define ACCOUNTMANAGER_H

#include <vector>
#include <cstddef>
#include "record.pb.h"

namespace KeepTower {

/**
 * @class AccountManager
 * @brief Manages account CRUD operations within a vault
 *
 * AccountManager provides a clean interface for managing accounts
 * within an encrypted vault. It handles:
 * - Account creation and deletion
 * - Account updates and modifications
 * - Account retrieval (read-only and mutable)
 * - Account reordering for UI consistency
 * - Permission validation
 *
 * ## Thread Safety
 * This class is not thread-safe. The caller must ensure
 * proper synchronization when accessing from multiple threads.
 *
 * ## Example Usage
 * @code
 * AccountManager account_mgr(vault_data, modified_flag);
 *
 * // Add new account
 * keeptower::AccountRecord account;
 * account.set_title("Gmail");
 * account.set_username("user@example.com");
 * if (account_mgr.add_account(account)) {
 *     // Account added successfully
 * }
 *
 * // Get account count
 * size_t count = account_mgr.get_account_count();
 *
 * // Update account
 * auto* mutable_account = account_mgr.get_account_mutable(0);
 * if (mutable_account) {
 *     mutable_account->set_notes("Updated notes");
 * }
 * @endcode
 */
class AccountManager {
public:
    /**
     * @brief Construct AccountManager with vault data references
     * @param vault_data Reference to the protobuf vault data
     * @param modified_flag Reference to the modified flag
     *
     * @note AccountManager does not own the vault data, it only
     *       provides an interface to manage accounts within it
     */
    AccountManager(keeptower::VaultData& vault_data, bool& modified_flag);

    // Disable copy and move (references can't be safely copied)
    AccountManager(const AccountManager&) = delete;
    AccountManager& operator=(const AccountManager&) = delete;
    AccountManager(AccountManager&&) = delete;
    AccountManager& operator=(AccountManager&&) = delete;

    /**
     * @brief Add new account to vault
     * @param account Account record to add
     * @return true if added successfully, false on error
     *
     * @note Sets modified flag on success
     */
    [[nodiscard]] bool add_account(const keeptower::AccountRecord& account);

    /**
     * @brief Get all accounts from vault
     * @return Vector of all account records (copies)
     */
    [[nodiscard]] std::vector<keeptower::AccountRecord> get_all_accounts() const;

    /**
     * @brief Update existing account
     * @param index Zero-based index of account to update
     * @param account New account data
     * @return true if updated successfully, false if index invalid
     *
     * @note Sets modified flag on success
     */
    [[nodiscard]] bool update_account(size_t index, const keeptower::AccountRecord& account);

    /**
     * @brief Delete account from vault
     * @param index Zero-based index of account to delete
     * @return true if deleted successfully, false if index invalid
     *
     * @note Sets modified flag on success
     * @note Shifts remaining accounts down in the list
     */
    [[nodiscard]] bool delete_account(size_t index);

    /**
     * @brief Get read-only pointer to account
     * @param index Zero-based index of account
     * @return Pointer to account or nullptr if invalid index
     */
    [[nodiscard]] const keeptower::AccountRecord* get_account(size_t index) const;

    /**
     * @brief Get mutable pointer to account
     * @param index Zero-based index of account
     * @return Pointer to account or nullptr if invalid index
     *
     * @warning Caller must set modified flag after making changes
     */
    [[nodiscard]] keeptower::AccountRecord* get_account_mutable(size_t index);

    /**
     * @brief Get number of accounts in vault
     * @return Account count
     */
    [[nodiscard]] size_t get_account_count() const;

    /**
     * @brief Reorder account by moving it from one position to another
     * @param old_index Current position of the account (0-based)
     * @param new_index Target position for the account (0-based)
     * @return true if reordered successfully, false on error
     *
     * This method handles drag-and-drop reordering by updating the
     * global_display_order field for all affected accounts. The ordering
     * is normalized to sequential values (0, 1, 2, ...) after the move.
     *
     * @note Sets modified flag on success
     */
    [[nodiscard]] bool reorder_account(size_t old_index, size_t new_index);

    /**
     * @brief Check if account can be deleted
     * @param account_index Zero-based index of account
     * @return true if account exists and can be deleted
     *
     * This is a permission check method that validates:
     * - Account index is valid
     * - Account exists in the vault
     *
     * @note This currently always returns true for valid indices,
     *       but provides a hook for future permission systems
     */
    [[nodiscard]] bool can_delete_account(size_t account_index) const noexcept;

private:
    keeptower::VaultData& m_vault_data;  ///< Reference to protobuf vault data
    bool& m_modified_flag;               ///< Reference to vault modified flag
};

}  // namespace KeepTower

#endif  // ACCOUNTMANAGER_H
