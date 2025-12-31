// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file AccountService.h
 * @brief Concrete implementation of IAccountService
 *
 * Part of Phase 3 refactoring: Service Layer
 * Implements business logic for account operations
 */

#pragma once

#include "IAccountService.h"
#include "../repositories/IAccountRepository.h"
#include <memory>
#include <regex>

namespace KeepTower {

// Field length limits (from UI namespace constants)
inline constexpr int MAX_NOTES_LENGTH = 1000;           ///< Maximum notes field length
inline constexpr int MAX_ACCOUNT_NAME_LENGTH = 256;     ///< Maximum account name length
inline constexpr int MAX_USERNAME_LENGTH = 256;         ///< Maximum username field length
inline constexpr int MAX_PASSWORD_LENGTH = 512;         ///< Maximum password length
inline constexpr int MAX_EMAIL_LENGTH = 256;            ///< Maximum email address length
inline constexpr int MAX_WEBSITE_LENGTH = 512;          ///< Maximum website URL length

/**
 * @brief Concrete account service implementation
 *
 * Implements business logic for account operations:
 * - Field validation (lengths, formats)
 * - Business rules (unique names, email format)
 * - Search and filtering logic
 * - Delegates data access to repository
 *
 * Thread Safety:
 * - Same as underlying repository
 * - Not thread-safe by default
 * - All calls must be from same thread
 */
class AccountService : public IAccountService {
public:
    /**
     * @brief Construct service with repository
     * @param account_repo Non-owning pointer to account repository
     * @throws std::invalid_argument if account_repo is null
     */
    explicit AccountService(IAccountRepository* account_repo);

    ~AccountService() override = default;

    // Non-copyable, non-movable (holds non-owning pointer)
    AccountService(const AccountService&) = delete;
    AccountService& operator=(const AccountService&) = delete;
    AccountService(AccountService&&) = delete;
    AccountService& operator=(AccountService&&) = delete;

    // IAccountService interface implementation
    [[nodiscard]] std::expected<std::string, ServiceError>
        create_account(const keeptower::AccountRecord& account) override;

    [[nodiscard]] std::expected<keeptower::AccountRecord, ServiceError>
        get_account(size_t index) const override;

    [[nodiscard]] std::expected<keeptower::AccountRecord, ServiceError>
        get_account_by_id(std::string_view account_id) const override;

    [[nodiscard]] std::expected<std::vector<keeptower::AccountRecord>, ServiceError>
        get_all_accounts() const override;

    [[nodiscard]] std::expected<void, ServiceError>
        update_account(size_t index, const keeptower::AccountRecord& account) override;

    [[nodiscard]] std::expected<void, ServiceError>
        delete_account(size_t index) override;

    [[nodiscard]] std::expected<bool, ServiceError>
        toggle_favorite(size_t index) override;

    [[nodiscard]] std::expected<std::vector<size_t>, ServiceError>
        search_accounts(std::string_view search_text,
                       std::string_view field_filter = "") const override;

    [[nodiscard]] std::expected<std::vector<size_t>, ServiceError>
        filter_by_tag(std::string_view tag) const override;

    [[nodiscard]] std::expected<size_t, ServiceError>
        count() const override;

    [[nodiscard]] std::expected<void, ServiceError>
        validate_account(const keeptower::AccountRecord& account) const override;

    [[nodiscard]] bool
        is_name_unique(std::string_view name,
                      std::string_view exclude_id = "") const override;

private:
    IAccountRepository* m_account_repo;  ///< Non-owning pointer to repository

    /**
     * @brief Validate field length
     * @param field_value Field content
     * @param max_length Maximum allowed length
     * @return true if within limit
     */
    [[nodiscard]] bool validate_field_length(std::string_view field_value,
                                             size_t max_length) const;

    /**
     * @brief Validate email format
     * @param email Email address to validate
     * @return true if format is valid (or empty)
     */
    [[nodiscard]] bool validate_email_format(std::string_view email) const;
};

}  // namespace KeepTower
