// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file AccountService.cc
 * @brief Implementation of AccountService
 *
 * Part of Phase 3 refactoring: Service Layer
 * Implements business logic and validation for account operations
 */

#include "AccountService.h"
#include <algorithm>
#include <stdexcept>
#include <cctype>

namespace KeepTower {

AccountService::AccountService(IAccountRepository* account_repo)
    : m_account_repo(account_repo) {
    if (!m_account_repo) {
        throw std::invalid_argument("AccountService: account_repo cannot be null");
    }
}

std::expected<std::string, ServiceError>
AccountService::create_account(const keeptower::AccountRecord& account) {
    // Validate account data
    auto validation_result = validate_account(account);
    if (!validation_result) {
        return std::unexpected(validation_result.error());
    }

    // Check for duplicate name
    if (!account.account_name().empty() &&
        !is_name_unique(account.account_name(), account.id())) {
        return std::unexpected(ServiceError::DUPLICATE_NAME);
    }

    // Delegate to repository
    auto result = m_account_repo->add(account);
    if (!result) {
        return std::unexpected(to_service_error(result.error()));
    }

    return account.id();
}

std::expected<keeptower::AccountRecord, ServiceError>
AccountService::get_account(size_t index) const {
    auto result = m_account_repo->get(index);
    if (!result) {
        return std::unexpected(to_service_error(result.error()));
    }
    return result.value();
}

std::expected<keeptower::AccountRecord, ServiceError>
AccountService::get_account_by_id(std::string_view account_id) const {
    auto result = m_account_repo->get_by_id(account_id);
    if (!result) {
        return std::unexpected(to_service_error(result.error()));
    }
    return result.value();
}

std::expected<std::vector<keeptower::AccountRecord>, ServiceError>
AccountService::get_all_accounts() const {
    auto result = m_account_repo->get_all();
    if (!result) {
        return std::unexpected(to_service_error(result.error()));
    }
    return result.value();
}

std::expected<void, ServiceError>
AccountService::update_account(size_t index, const keeptower::AccountRecord& account) {
    // Validate account data
    auto validation_result = validate_account(account);
    if (!validation_result) {
        return std::unexpected(validation_result.error());
    }

    // Check for duplicate name (excluding current account)
    if (!account.account_name().empty() &&
        !is_name_unique(account.account_name(), account.id())) {
        return std::unexpected(ServiceError::DUPLICATE_NAME);
    }

    // Delegate to repository
    auto result = m_account_repo->update(index, account);
    if (!result) {
        return std::unexpected(to_service_error(result.error()));
    }

    return {};
}

std::expected<void, ServiceError>
AccountService::delete_account(size_t index) {
    auto result = m_account_repo->remove(index);
    if (!result) {
        return std::unexpected(to_service_error(result.error()));
    }
    return {};
}

std::expected<bool, ServiceError>
AccountService::toggle_favorite(size_t index) {
    // Get current account
    auto account_result = m_account_repo->get(index);
    if (!account_result) {
        return std::unexpected(to_service_error(account_result.error()));
    }

    auto account = account_result.value();
    bool new_favorite = !account.is_favorite();
    account.set_is_favorite(new_favorite);

    // Update account
    auto update_result = m_account_repo->update(index, account);
    if (!update_result) {
        return std::unexpected(to_service_error(update_result.error()));
    }

    return new_favorite;
}

std::expected<std::vector<size_t>, ServiceError>
AccountService::search_accounts(std::string_view search_text,
                               std::string_view field_filter) const {
    // Get all accounts
    auto accounts_result = m_account_repo->get_all();
    if (!accounts_result) {
        return std::unexpected(to_service_error(accounts_result.error()));
    }

    const auto& accounts = accounts_result.value();
    std::vector<size_t> matches;

    if (search_text.empty()) {
        // Empty search returns all indices
        for (size_t i = 0; i < accounts.size(); ++i) {
            matches.push_back(i);
        }
        return matches;
    }

    // Convert search text to lowercase for case-insensitive search
    std::string search_lower;
    search_lower.reserve(search_text.size());
    for (char c : search_text) {
        search_lower.push_back(std::tolower(static_cast<unsigned char>(c)));
    }

    // Search through accounts
    for (size_t i = 0; i < accounts.size(); ++i) {
        const auto& account = accounts[i];
        bool match = false;

        // Helper lambda for case-insensitive substring search
        auto contains = [&search_lower](const std::string& field) -> bool {
            std::string field_lower;
            field_lower.reserve(field.size());
            for (char c : field) {
                field_lower.push_back(std::tolower(static_cast<unsigned char>(c)));
            }
            return field_lower.find(search_lower) != std::string::npos;
        };

        // Search based on field filter
        if (field_filter.empty() || field_filter == "all") {
            // Search all fields
            match = contains(account.account_name()) ||
                   contains(account.user_name()) ||
                   contains(account.email()) ||
                   contains(account.website()) ||
                   contains(account.notes());
        } else if (field_filter == "name") {
            match = contains(account.account_name());
        } else if (field_filter == "username") {
            match = contains(account.user_name());
        } else if (field_filter == "email") {
            match = contains(account.email());
        } else if (field_filter == "website") {
            match = contains(account.website());
        } else if (field_filter == "notes") {
            match = contains(account.notes());
        }

        if (match) {
            matches.push_back(i);
        }
    }

    return matches;
}

std::expected<std::vector<size_t>, ServiceError>
AccountService::filter_by_tag(std::string_view tag) const {
    // Get all accounts
    auto accounts_result = m_account_repo->get_all();
    if (!accounts_result) {
        return std::unexpected(to_service_error(accounts_result.error()));
    }

    const auto& accounts = accounts_result.value();
    std::vector<size_t> matches;

    if (tag.empty()) {
        // Empty tag returns all indices
        for (size_t i = 0; i < accounts.size(); ++i) {
            matches.push_back(i);
        }
        return matches;
    }

    // Search through accounts for tag
    for (size_t i = 0; i < accounts.size(); ++i) {
        const auto& account = accounts[i];

        // Check if account has the tag
        for (int j = 0; j < account.tags_size(); ++j) {
            if (account.tags(j) == tag) {
                matches.push_back(i);
                break;
            }
        }
    }

    return matches;
}

std::expected<size_t, ServiceError>
AccountService::count() const {
    auto result = m_account_repo->count();
    if (!result) {
        return std::unexpected(to_service_error(result.error()));
    }
    return result.value();
}

std::expected<void, ServiceError>
AccountService::validate_account(const keeptower::AccountRecord& account) const {
    // Validate account name (required field)
    if (account.account_name().empty()) {
        return std::unexpected(ServiceError::VALIDATION_FAILED);
    }
    if (!validate_field_length(account.account_name(), MAX_ACCOUNT_NAME_LENGTH)) {
        return std::unexpected(ServiceError::FIELD_TOO_LONG);
    }

    // Validate username
    if (!validate_field_length(account.user_name(), MAX_USERNAME_LENGTH)) {
        return std::unexpected(ServiceError::FIELD_TOO_LONG);
    }

    // Validate password
    if (!validate_field_length(account.password(), MAX_PASSWORD_LENGTH)) {
        return std::unexpected(ServiceError::FIELD_TOO_LONG);
    }

    // Validate email (check both length and format)
    if (!validate_field_length(account.email(), MAX_EMAIL_LENGTH)) {
        return std::unexpected(ServiceError::FIELD_TOO_LONG);
    }
    if (!account.email().empty() && !validate_email_format(account.email())) {
        return std::unexpected(ServiceError::INVALID_EMAIL);
    }

    // Validate website
    if (!validate_field_length(account.website(), MAX_WEBSITE_LENGTH)) {
        return std::unexpected(ServiceError::FIELD_TOO_LONG);
    }

    // Validate notes
    if (!validate_field_length(account.notes(), MAX_NOTES_LENGTH)) {
        return std::unexpected(ServiceError::FIELD_TOO_LONG);
    }

    return {};
}

bool AccountService::is_name_unique(std::string_view name,
                                   std::string_view exclude_id) const {
    // Get all accounts
    auto accounts_result = m_account_repo->get_all();
    if (!accounts_result) {
        return true;  // Can't check, assume unique
    }

    const auto& accounts = accounts_result.value();

    // Check for duplicate name
    for (const auto& account : accounts) {
        if (account.id() != exclude_id && account.account_name() == name) {
            return false;  // Duplicate found
        }
    }

    return true;  // Name is unique
}

bool AccountService::validate_field_length(std::string_view field_value,
                                          size_t max_length) const {
    return field_value.size() <= max_length;
}

bool AccountService::validate_email_format(std::string_view email) const {
    if (email.empty()) {
        return true;  // Empty email is valid (optional field)
    }

    // Simple email validation regex
    // Pattern: local-part@domain
    // - local-part: alphanumeric, dots, hyphens, underscores
    // - domain: alphanumeric, dots, hyphens, at least one dot
    static const std::regex email_pattern(
        R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)"
    );

    std::string email_str(email);
    return std::regex_match(email_str, email_pattern);
}

}  // namespace KeepTower
