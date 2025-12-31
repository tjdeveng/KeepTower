// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file IAccountRepository.h
 * @brief Interface for account data access operations
 *
 * Part of Phase 2 refactoring: Repository Pattern
 * Separates account data access from business logic
 */

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <expected>
#include "../record.pb.h"

namespace KeepTower {

/**
 * @brief Error types for repository operations
 */
enum class RepositoryError {
    VAULT_CLOSED,           ///< Vault is not open
    ACCOUNT_NOT_FOUND,      ///< Account does not exist
    INVALID_INDEX,          ///< Index out of bounds
    PERMISSION_DENIED,      ///< User lacks permission for operation
    DUPLICATE_ID,           ///< Account ID already exists
    SAVE_FAILED,            ///< Failed to persist changes
    UNKNOWN_ERROR           ///< Unspecified error
};

/**
 * @brief Convert error to human-readable string
 */
[[nodiscard]] constexpr std::string_view to_string(RepositoryError error) noexcept {
    switch (error) {
        case RepositoryError::VAULT_CLOSED:       return "Vault is not open";
        case RepositoryError::ACCOUNT_NOT_FOUND:  return "Account not found";
        case RepositoryError::INVALID_INDEX:      return "Invalid index";
        case RepositoryError::PERMISSION_DENIED:  return "Permission denied";
        case RepositoryError::DUPLICATE_ID:       return "Duplicate account ID";
        case RepositoryError::SAVE_FAILED:        return "Failed to save";
        case RepositoryError::UNKNOWN_ERROR:      return "Unknown error";
    }
    return "Unknown error";
}

/**
 * @brief Interface for account repository operations
 *
 * Provides CRUD operations for accounts with proper error handling.
 * Implementations handle V1/V2 vault differences and permission checks.
 *
 * Design Principles:
 * - Index-based access for compatibility with existing code
 * - ID-based lookup for flexibility
 * - std::expected for explicit error handling
 * - Permission-aware for V2 multi-user vaults
 * - Testable through interface
 *
 * @note All operations assume vault is already open
 * @note Implementations must handle thread-safety if needed
 */
class IAccountRepository {
public:
    virtual ~IAccountRepository() = default;

    /**
     * @brief Add a new account to the vault
     * @param account Account record to add
     * @return Success or error
     *
     * Errors:
     * - VAULT_CLOSED: Vault not open
     * - DUPLICATE_ID: Account ID already exists
     * - PERMISSION_DENIED: User cannot add accounts (V2)
     * - SAVE_FAILED: Could not persist to vault
     */
    [[nodiscard]] virtual std::expected<void, RepositoryError>
        add(const keeptower::AccountRecord& account) = 0;

    /**
     * @brief Get account by index
     * @param index Zero-based account index
     * @return Account record or error
     *
     * Errors:
     * - VAULT_CLOSED: Vault not open
     * - INVALID_INDEX: Index out of bounds
     * - PERMISSION_DENIED: User cannot view this account (V2)
     */
    [[nodiscard]] virtual std::expected<keeptower::AccountRecord, RepositoryError>
        get(size_t index) const = 0;

    /**
     * @brief Get account by ID
     * @param account_id Unique account identifier
     * @return Account record or error
     *
     * Errors:
     * - VAULT_CLOSED: Vault not open
     * - ACCOUNT_NOT_FOUND: No account with this ID
     * - PERMISSION_DENIED: User cannot view this account (V2)
     */
    [[nodiscard]] virtual std::expected<keeptower::AccountRecord, RepositoryError>
        get_by_id(std::string_view account_id) const = 0;

    /**
     * @brief Get all accounts (respecting permissions)
     * @return Vector of all viewable accounts or error
     *
     * For V2 vaults, only returns accounts the current user can view.
     *
     * Errors:
     * - VAULT_CLOSED: Vault not open
     */
    [[nodiscard]] virtual std::expected<std::vector<keeptower::AccountRecord>, RepositoryError>
        get_all() const = 0;

    /**
     * @brief Update an existing account
     * @param index Zero-based account index
     * @param account Updated account record
     * @return Success or error
     *
     * Errors:
     * - VAULT_CLOSED: Vault not open
     * - INVALID_INDEX: Index out of bounds
     * - PERMISSION_DENIED: User cannot modify this account (V2)
     * - SAVE_FAILED: Could not persist changes
     */
    [[nodiscard]] virtual std::expected<void, RepositoryError>
        update(size_t index, const keeptower::AccountRecord& account) = 0;

    /**
     * @brief Delete an account
     * @param index Zero-based account index
     * @return Success or error
     *
     * Errors:
     * - VAULT_CLOSED: Vault not open
     * - INVALID_INDEX: Index out of bounds
     * - PERMISSION_DENIED: User cannot delete this account (V2)
     * - SAVE_FAILED: Could not persist changes
     */
    [[nodiscard]] virtual std::expected<void, RepositoryError>
        remove(size_t index) = 0;

    /**
     * @brief Get total count of accounts
     * @return Number of accounts or error
     *
     * For V2 vaults, returns total count (not just viewable).
     *
     * Errors:
     * - VAULT_CLOSED: Vault not open
     */
    [[nodiscard]] virtual std::expected<size_t, RepositoryError>
        count() const = 0;

    /**
     * @brief Check if user can view account
     * @param index Zero-based account index
     * @return true if viewable, false otherwise
     *
     * For V1 vaults, returns true for valid indices.
     * For V2 vaults, checks user permissions.
     */
    [[nodiscard]] virtual bool can_view(size_t index) const noexcept = 0;

    /**
     * @brief Check if user can modify account
     * @param index Zero-based account index
     * @return true if modifiable, false otherwise
     *
     * For V1 vaults, returns true for valid indices.
     * For V2 vaults, checks user permissions.
     */
    [[nodiscard]] virtual bool can_modify(size_t index) const noexcept = 0;

    /**
     * @brief Check if vault is currently open
     * @return true if vault is open
     */
    [[nodiscard]] virtual bool is_vault_open() const noexcept = 0;

    /**
     * @brief Find account index by ID
     * @param account_id Unique account identifier
     * @return Account index or nullopt if not found
     */
    [[nodiscard]] virtual std::optional<size_t>
        find_index_by_id(std::string_view account_id) const noexcept = 0;
};

}  // namespace KeepTower
