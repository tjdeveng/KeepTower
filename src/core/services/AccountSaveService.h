// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#pragma once

/**
 * @file AccountSaveService.h
 * @brief Business logic for account-save operations extracted from MainWindow.
 *
 * Responsibilities:
 * - Map AccountService validation errors to user-facing dialog messages
 * - Enforce edit-permission policy for admin-protected accounts
 * - Enforce password reuse policy
 * - Manage password history (add entry, trim to limit, stamp changed-at)
 * - Stamp the modification timestamp
 *
 * NOT responsible for:
 * - Reading widget state (MainWindow)
 * - Displaying dialogs (MainWindow)
 * - Persisting the vault (VaultManager)
 */

#include "IAccountService.h"
#include "../VaultBoundaryTypes.h"
#include <expected>
#include <string>

namespace KeepTower {

/**
 * @brief Error returned by AccountSaveService when a save must be aborted.
 */
struct AccountSaveFailure {
    std::string message;          ///< User-facing error message.
    bool reload_account = false;  ///< True when the caller should reload the account widget.
};

/**
 * @brief Context carrying all non-UI state required for an account-save operation.
 *
 * The caller (MainWindow) populates this struct from widget state and settings
 * before handing it to AccountSaveService::prepare_save().
 */
struct AccountSaveContext {
    AccountDetail&     detail;           ///< Account being saved (mutated in-place on success).
    const std::string& old_password;     ///< Password value before editing began.
    bool               is_admin;         ///< True when the current session user is an administrator.
    bool               history_enabled;  ///< True when password history is enabled in settings.
    int                history_limit;    ///< Maximum number of historical passwords to retain.
};

/**
 * @class AccountSaveService
 * @brief Stateless helper that encapsulates account-save business logic.
 *
 * All methods are static; the constructor is deleted to enforce stateless usage.
 * The caller (MainWindow) owns all mutable state and supplies it via
 * AccountSaveContext on each call.
 */
class AccountSaveService {
public:
    AccountSaveService() = delete;

    /**
     * @brief Convert a ServiceError into a user-friendly dialog message.
     *
     * Covers the validation errors produced by AccountService::validate_account():
     * VALIDATION_FAILED, FIELD_TOO_LONG, INVALID_EMAIL, and a fallback for others.
     *
     * @param error The error produced by AccountService::validate_account().
     * @return Non-empty message string suitable for display in an error dialog.
     */
    [[nodiscard]] static std::string validation_error_message(ServiceError error);

    /**
     * @brief Run permission checks, password-history policy, and metadata stamps.
     *
     * On success, @p ctx.detail has been updated with the new password history,
     * @c password_changed_at, and @c modified_at timestamps ready for persistence.
     *
     * On failure, the returned AccountSaveFailure contains a user-facing message
     * and a flag indicating whether the caller should reload the account widget
     * (required after a permission-denied rejection to discard in-widget edits).
     *
     * @param ctx Live context supplied by the save call-site.
     * @return Empty expected on success; AccountSaveFailure on policy violation.
     */
    [[nodiscard]] static std::expected<void, AccountSaveFailure>
        prepare_save(AccountSaveContext& ctx);
};

}  // namespace KeepTower
