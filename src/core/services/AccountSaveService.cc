// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#include "AccountSaveService.h"
#include "AccountService.h"  // KeepTower::MAX_* field-length constants
#include <ctime>

namespace KeepTower {

std::string AccountSaveService::validation_error_message(ServiceError error) {
    switch (error) {
        case ServiceError::VALIDATION_FAILED:
            return "Account name cannot be empty.";
        case ServiceError::FIELD_TOO_LONG:
            return "One or more fields exceed maximum length.\n\n"
                   "Maximum lengths:\n"
                   "\u2022 Account Name: " + std::to_string(MAX_ACCOUNT_NAME_LENGTH) + "\n"
                   "\u2022 Username: " + std::to_string(MAX_USERNAME_LENGTH) + "\n"
                   "\u2022 Password: " + std::to_string(MAX_PASSWORD_LENGTH) + "\n"
                   "\u2022 Email: " + std::to_string(MAX_EMAIL_LENGTH) + "\n"
                   "\u2022 Website: " + std::to_string(MAX_WEBSITE_LENGTH) + "\n"
                   "\u2022 Notes: " + std::to_string(MAX_NOTES_LENGTH);
        case ServiceError::INVALID_EMAIL:
            return "Invalid email format.\n\n"
                   "Email must be in the format: user@domain.ext\n\n"
                   "Examples:\n"
                   "  \u2022 john@example.com\n"
                   "  \u2022 jane.doe@company.co.uk\n"
                   "  \u2022 user+tag@mail.example.org";
        default:
            return "Validation error: " + std::string(to_string(error));
    }
}

std::expected<void, AccountSaveFailure>
AccountSaveService::prepare_save(AccountSaveContext& ctx) {
    // Permission check: standard users cannot edit admin-protected accounts.
    if (!ctx.is_admin && ctx.detail.is_admin_only_deletable) {
        return std::unexpected(AccountSaveFailure{
            "You do not have permission to edit this account.\n\n"
            "This account is marked as admin-only-deletable.\n"
            "Only administrators can modify protected accounts.",
            /*reload_account=*/true
        });
    }

    // Password reuse and history management.
    if (ctx.detail.password != ctx.old_password && ctx.history_enabled) {
        // Prevent reuse of a previously stored password.
        for (const auto& hist_entry : ctx.detail.password_history) {
            if (hist_entry == ctx.detail.password) {
                return std::unexpected(AccountSaveFailure{
                    "Password reuse detected!\n\n"
                    "This password was used previously. Please choose a different password.\n\n"
                    "Using unique passwords for each change improves security.",
                    /*reload_account=*/false
                });
            }
        }

        // Archive the old password (skip empty placeholders).
        if (!ctx.old_password.empty()) {
            ctx.detail.password_history.push_back(ctx.old_password);
            // Trim oldest entries when the history limit is exceeded.
            while (static_cast<int>(ctx.detail.password_history.size()) > ctx.history_limit) {
                ctx.detail.password_history.erase(ctx.detail.password_history.begin());
            }
        }

        ctx.detail.password_changed_at = std::time(nullptr);
    }

    // Stamp the modification time.
    ctx.detail.modified_at = std::time(nullptr);

    return {};
}

}  // namespace KeepTower
