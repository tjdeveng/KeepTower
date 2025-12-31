// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file AccountViewController.h
 * @brief Controller for account list management and filtering
 *
 * Part of Phase 1 & 2 refactoring:
 * - Phase 1: Extracted account list logic from MainWindow
 * - Phase 2: Integrated Repository Pattern for data access
 *
 * This controller provides a clean separation of concerns and improved testability.
 */

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <sigc++/sigc++.h>
#include "../../core/VaultManager.h"
#include "../../core/repositories/IAccountRepository.h"
#include "../../core/repositories/IGroupRepository.h"
#include "record.pb.h"

/**
 * @brief Controller for managing account list display and interactions
 *
 * AccountViewController handles:
 * - Account list updates based on vault data (via repositories)
 * - Permission filtering (V2 multi-user vaults)
 * - Account selection state
 * - Account favorite toggling (using AccountRepository)
 * - Account list refresh coordination
 *
 * This class separates account list management from MainWindow,
 * making the logic testable and reducing MainWindow complexity.
 *
 * Architecture (Phase 2):
 * - Uses AccountRepository for account operations
 * - Uses GroupRepository for group operations
 * - Delegates to repositories instead of direct VaultManager access
 *
 * @section usage Usage Example
 * @code
 * AccountViewController controller(vault_manager.get());
 *
 * // Connect to signals
 * controller.signal_list_updated().connect([&](const auto& accounts, const auto& groups, size_t total) {
 *     // Update UI with new account list
 * });
 *
 * // Refresh account list
 * controller.refresh_account_list();
 *
 * // Toggle favorite status
 * controller.toggle_favorite(account_index);
 * @endcode
 */
class AccountViewController {
public:
    /**
     * @brief Construct a new AccountViewController
     * @param vault_manager Pointer to VaultManager (must outlive controller)
     *
     * Creates internal AccountRepository and GroupRepository instances
     * that wrap the VaultManager for data access.
     */
    explicit AccountViewController(VaultManager* vault_manager);
    ~AccountViewController() = default;

    // Disable copy, allow move
    AccountViewController(const AccountViewController&) = delete;
    AccountViewController& operator=(const AccountViewController&) = delete;

    /** @brief Move constructor - transfers ownership of repositories */
    AccountViewController(AccountViewController&&) = default;

    /** @brief Move assignment - transfers ownership of repositories */
    AccountViewController& operator=(AccountViewController&&) = default;

    /**
     * @brief Refresh the account list from vault
     *
     * Retrieves all accounts from the vault, applies permission filtering
     * for V2 multi-user vaults, and emits signal_list_updated.
     */
    void refresh_account_list();

    /**
     * @brief Get the current viewable accounts
     * @return Vector of accounts the current user can view
     */
    [[nodiscard]] const std::vector<keeptower::AccountRecord>& get_viewable_accounts() const;

    /**
     * @brief Get the current groups
     * @return Vector of all groups in the vault
     */
    [[nodiscard]] const std::vector<keeptower::AccountGroup>& get_groups() const;

    /**
     * @brief Get the number of viewable accounts
     * @return Count of accounts user can view
     */
    [[nodiscard]] size_t get_viewable_account_count() const;

    /**
     * @brief Check if an account is viewable by current user
     * @param account_index Index of account in vault
     * @return true if user can view this account
     */
    [[nodiscard]] bool can_view_account(size_t account_index) const;

    /**
     * @brief Find account index by account ID
     * @param account_id Unique account identifier
     * @return Index of account, or -1 if not found
     */
    [[nodiscard]] int find_account_index_by_id(const std::string& account_id) const;

    /**
     * @brief Toggle favorite status for an account
     * @param account_index Index of account to toggle
     * @return true if successful, false if failed
     */
    bool toggle_favorite(size_t account_index);

    /**
     * @brief Check if vault is currently open
     * @return true if vault is open
     */
    [[nodiscard]] bool is_vault_open() const;

    // Signals

    /**
     * @brief Signal emitted when account list is updated
     *
     * Parameters: (viewable_accounts, groups, total_accounts)
     */
    sigc::signal<void(const std::vector<keeptower::AccountRecord>&,
                      const std::vector<keeptower::AccountGroup>&,
                      size_t)>& signal_list_updated();

    /**
     * @brief Signal emitted when favorite status is toggled
     *
     * Parameters: (account_index, is_favorite)
     */
    sigc::signal<void(size_t, bool)>& signal_favorite_toggled();

    /**
     * @brief Signal emitted when an error occurs
     *
     * Parameters: (error_message)
     */
    sigc::signal<void(const std::string&)>& signal_error();

private:
    // Dependencies
    VaultManager* m_vault_manager;  ///< Non-owning pointer to vault manager (for backward compatibility)
    std::unique_ptr<KeepTower::IAccountRepository> m_account_repo;  ///< Repository for account operations
    std::unique_ptr<KeepTower::IGroupRepository> m_group_repo;  ///< Repository for group operations

    // Cached state
    std::vector<keeptower::AccountRecord> m_viewable_accounts;  ///< Accounts user can view
    std::vector<keeptower::AccountGroup> m_groups;  ///< All groups in vault

    // Signals
    sigc::signal<void(const std::vector<keeptower::AccountRecord>&,
                      const std::vector<keeptower::AccountGroup>&,
                      size_t)> m_signal_list_updated;
    sigc::signal<void(size_t, bool)> m_signal_favorite_toggled;
    sigc::signal<void(const std::string&)> m_signal_error;

    /**
     * @brief Apply permission filtering to accounts
     * @param all_accounts All accounts from vault
     * @return Accounts the current user can view
     */
    std::vector<keeptower::AccountRecord> filter_by_permissions(
        const std::vector<keeptower::AccountRecord>& all_accounts);
};
