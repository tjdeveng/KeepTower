// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file IAccountService.h
 * @brief Interface for account business logic operations
 *
 * Part of Phase 3 refactoring: Service Layer
 * Separates business logic from data access and UI concerns
 */

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <expected>
#include "../record.pb.h"
#include "../repositories/IAccountRepository.h"

namespace KeepTower {

/**
 * @brief Error types for service operations
 *
 * Extends repository errors with business logic specific errors
 */
enum class ServiceError {
    // Repository errors (passthrough)
    VAULT_CLOSED,
    ACCOUNT_NOT_FOUND,
    INVALID_INDEX,
    PERMISSION_DENIED,
    DUPLICATE_ID,
    SAVE_FAILED,

    // Business logic errors
    VALIDATION_FAILED,      ///< Input validation failed
    INVALID_EMAIL,          ///< Email format invalid
    FIELD_TOO_LONG,         ///< Field exceeds maximum length
    PASSWORD_TOO_WEAK,      ///< Password doesn't meet strength requirements
    DUPLICATE_NAME,         ///< Account name already exists
    BUSINESS_RULE_VIOLATION ///< Other business rule violation
};

/**
 * @brief Convert service error to human-readable string
 */
[[nodiscard]] constexpr std::string_view to_string(ServiceError error) noexcept {
    switch (error) {
        case ServiceError::VAULT_CLOSED:       return "Vault is not open";
        case ServiceError::ACCOUNT_NOT_FOUND:  return "Account not found";
        case ServiceError::INVALID_INDEX:      return "Invalid index";
        case ServiceError::PERMISSION_DENIED:  return "Permission denied";
        case ServiceError::DUPLICATE_ID:       return "Duplicate account ID";
        case ServiceError::SAVE_FAILED:        return "Failed to save";
        case ServiceError::VALIDATION_FAILED:  return "Validation failed";
        case ServiceError::INVALID_EMAIL:      return "Invalid email format";
        case ServiceError::FIELD_TOO_LONG:     return "Field exceeds maximum length";
        case ServiceError::PASSWORD_TOO_WEAK:  return "Password is too weak";
        case ServiceError::DUPLICATE_NAME:     return "Account name already exists";
        case ServiceError::BUSINESS_RULE_VIOLATION: return "Business rule violation";
    }
    return "Unknown error";
}

/**
 * @brief Convert repository error to service error
 */
[[nodiscard]] constexpr ServiceError to_service_error(RepositoryError repo_error) noexcept {
    switch (repo_error) {
        case RepositoryError::VAULT_CLOSED:      return ServiceError::VAULT_CLOSED;
        case RepositoryError::ACCOUNT_NOT_FOUND: return ServiceError::ACCOUNT_NOT_FOUND;
        case RepositoryError::INVALID_INDEX:     return ServiceError::INVALID_INDEX;
        case RepositoryError::PERMISSION_DENIED: return ServiceError::PERMISSION_DENIED;
        case RepositoryError::DUPLICATE_ID:      return ServiceError::DUPLICATE_ID;
        case RepositoryError::SAVE_FAILED:       return ServiceError::SAVE_FAILED;
        case RepositoryError::UNKNOWN_ERROR:     return ServiceError::BUSINESS_RULE_VIOLATION;
    }
    return ServiceError::BUSINESS_RULE_VIOLATION;
}

/**
 * @brief Interface for account business logic operations
 *
 * Provides high-level account operations with business rules:
 * - Input validation (field lengths, email format, etc.)
 * - Business rule enforcement (duplicate names, password strength)
 * - Command pattern integration for undo/redo
 * - Audit logging and notifications
 *
 * Design Principles:
 * - Delegates data access to IAccountRepository
 * - Enforces business rules before repository calls
 * - Returns service-level errors with context
 * - Supports transaction-like operations
 * - Testable through interface
 *
 * @note Services should not depend on UI components
 * @note Services may coordinate multiple repositories
 */
class IAccountService {
public:
    virtual ~IAccountService() = default;

    /**
     * @brief Create a new account with validation
     * @param account Account record to create
     * @return Account ID or error
     *
     * Validates:
     * - Field lengths (name, username, password, email, website, notes)
     * - Email format (if provided)
     * - No duplicate account names
     * - Password strength (if policy enabled)
     *
     * Errors:
     * - VAULT_CLOSED: Vault not open
     * - VALIDATION_FAILED: General validation error
     * - INVALID_EMAIL: Email format invalid
     * - FIELD_TOO_LONG: One or more fields exceed limits
     * - DUPLICATE_NAME: Account name already exists
     * - PERMISSION_DENIED: User cannot create accounts
     * - SAVE_FAILED: Could not persist to vault
     */
    [[nodiscard]] virtual std::expected<std::string, ServiceError>
        create_account(const keeptower::AccountRecord& account) = 0;

    /**
     * @brief Get account by index with permission check
     * @param index Zero-based account index
     * @return Account record or error
     */
    [[nodiscard]] virtual std::expected<keeptower::AccountRecord, ServiceError>
        get_account(size_t index) const = 0;

    /**
     * @brief Get account by ID with permission check
     * @param account_id Unique account identifier
     * @return Account record or error
     */
    [[nodiscard]] virtual std::expected<keeptower::AccountRecord, ServiceError>
        get_account_by_id(std::string_view account_id) const = 0;

    /**
     * @brief Get all viewable accounts (respecting permissions)
     * @return Vector of accounts or error
     */
    [[nodiscard]] virtual std::expected<std::vector<keeptower::AccountRecord>, ServiceError>
        get_all_accounts() const = 0;

    /**
     * @brief Update account with validation
     * @param index Zero-based account index
     * @param account Updated account record
     * @return Success or error
     *
     * Validates same rules as create_account
     */
    [[nodiscard]] virtual std::expected<void, ServiceError>
        update_account(size_t index, const keeptower::AccountRecord& account) = 0;

    /**
     * @brief Delete account with permission check
     * @param index Zero-based account index
     * @return Success or error
     */
    [[nodiscard]] virtual std::expected<void, ServiceError>
        delete_account(size_t index) = 0;

    /**
     * @brief Toggle favorite status
     * @param index Zero-based account index
     * @return New favorite status or error
     */
    [[nodiscard]] virtual std::expected<bool, ServiceError>
        toggle_favorite(size_t index) = 0;

    /**
     * @brief Search accounts by text
     * @param search_text Text to search for
     * @param field_filter Which fields to search (empty = all)
     * @return Matching account indices or error
     */
    [[nodiscard]] virtual std::expected<std::vector<size_t>, ServiceError>
        search_accounts(std::string_view search_text,
                       std::string_view field_filter = "") const = 0;

    /**
     * @brief Filter accounts by tag
     * @param tag Tag to filter by
     * @return Matching account indices or error
     */
    [[nodiscard]] virtual std::expected<std::vector<size_t>, ServiceError>
        filter_by_tag(std::string_view tag) const = 0;

    /**
     * @brief Get account count
     * @return Number of accounts or error
     */
    [[nodiscard]] virtual std::expected<size_t, ServiceError>
        count() const = 0;

    /**
     * @brief Validate account data without saving
     * @param account Account to validate
     * @return Success or validation error with details
     */
    [[nodiscard]] virtual std::expected<void, ServiceError>
        validate_account(const keeptower::AccountRecord& account) const = 0;

    /**
     * @brief Check if account name is unique
     * @param name Account name to check
     * @param exclude_id Account ID to exclude from check (for updates)
     * @return true if name is unique
     */
    [[nodiscard]] virtual bool
        is_name_unique(std::string_view name,
                      std::string_view exclude_id = "") const = 0;
};

}  // namespace KeepTower
