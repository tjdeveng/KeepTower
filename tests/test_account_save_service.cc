// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

/**
 * @file test_account_save_service.cc
 * @brief Unit tests for AccountSaveService business logic.
 *
 * These tests exercise the stateless helpers in isolation — no VaultManager,
 * no GTK, no protobuf.  Every test only constructs AccountDetail and
 * AccountSaveContext on the stack.
 *
 * Coverage:
 *   validation_error_message()
 *     - VALIDATION_FAILED maps to a non-empty string
 *     - FIELD_TOO_LONG maps to a non-empty string mentioning field limits
 *     - INVALID_EMAIL maps to a non-empty string mentioning format
 *     - Unknown error produces a non-empty fallback string
 *
 *   prepare_save()
 *     - Admin user, no history → succeeds; modified_at stamped
 *     - Standard user, non-protected account → succeeds
 *     - Standard user, admin-only-deletable account → fails, reload_account=true
 *     - Password unchanged, history enabled → succeeds (no history mutation)
 *     - Password changed, history disabled → succeeds (no history entry added)
 *     - Password changed, history enabled → old password pushed, changed_at stamped
 *     - History trimming to limit
 *     - Password reuse detection → fails, reload_account=false
 *     - History not grown when old_password is empty
 */

#include <gtest/gtest.h>
#include "../src/core/services/AccountSaveService.h"
#include <ctime>
#include <thread>
#include <chrono>

using namespace KeepTower;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static AccountDetail make_detail(const std::string& password = "hunter2") {
    AccountDetail d;
    d.account_name         = "Test Account";
    d.user_name            = "alice";
    d.password             = password;
    d.email                = "alice@example.com";
    d.is_admin_only_deletable = false;
    d.modified_at          = 0;
    d.password_changed_at  = 0;
    return d;
}

// ---------------------------------------------------------------------------
// validation_error_message tests
// ---------------------------------------------------------------------------

TEST(AccountSaveServiceValidationMessageTest, ValidationFailedIsNonEmpty) {
    auto msg = AccountSaveService::validation_error_message(ServiceError::VALIDATION_FAILED);
    EXPECT_FALSE(msg.empty());
    EXPECT_NE(msg.find("empty"), std::string::npos);
}

TEST(AccountSaveServiceValidationMessageTest, FieldTooLongMentionsLimits) {
    auto msg = AccountSaveService::validation_error_message(ServiceError::FIELD_TOO_LONG);
    EXPECT_FALSE(msg.empty());
    // Must mention at least one field name so the user knows what to shorten
    EXPECT_NE(msg.find("Account Name"), std::string::npos);
}

TEST(AccountSaveServiceValidationMessageTest, InvalidEmailMentionsFormat) {
    auto msg = AccountSaveService::validation_error_message(ServiceError::INVALID_EMAIL);
    EXPECT_FALSE(msg.empty());
    EXPECT_NE(msg.find("user@domain"), std::string::npos);
}

TEST(AccountSaveServiceValidationMessageTest, UnknownErrorProducesFallback) {
    auto msg = AccountSaveService::validation_error_message(ServiceError::SAVE_FAILED);
    EXPECT_FALSE(msg.empty());
}

// ---------------------------------------------------------------------------
// prepare_save — success paths
// ---------------------------------------------------------------------------

TEST(AccountSaveServicePrepareSaveTest, AdminNonProtectedSucceeds) {
    auto detail = make_detail();
    const std::string old_pw = detail.password;
    AccountSaveContext ctx { detail, old_pw, /*is_admin=*/true,
                             /*history_enabled=*/false, /*history_limit=*/5 };
    auto result = AccountSaveService::prepare_save(ctx);
    EXPECT_TRUE(result.has_value());
}

TEST(AccountSaveServicePrepareSaveTest, AdminSuccessStampsModifiedAt) {
    auto detail = make_detail();
    ASSERT_EQ(detail.modified_at, 0);
    const std::string old_pw = detail.password;
    AccountSaveContext ctx { detail, old_pw, true, false, 5 };
    auto result = AccountSaveService::prepare_save(ctx);
    ASSERT_TRUE(result.has_value());
    EXPECT_GT(detail.modified_at, 0);
}

TEST(AccountSaveServicePrepareSaveTest, StandardUserNonProtectedSucceeds) {
    auto detail = make_detail();
    const std::string old_pw = detail.password;
    AccountSaveContext ctx { detail, old_pw, /*is_admin=*/false,
                             false, 5 };
    auto result = AccountSaveService::prepare_save(ctx);
    EXPECT_TRUE(result.has_value());
}

TEST(AccountSaveServicePrepareSaveTest, PasswordUnchangedHistoryEnabledSucceeds) {
    auto detail = make_detail("secret");
    const std::string old_pw = "secret";  // Same as current
    AccountSaveContext ctx { detail, old_pw, true, /*history_enabled=*/true, 5 };
    auto result = AccountSaveService::prepare_save(ctx);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(detail.password_history.empty());
    EXPECT_EQ(detail.password_changed_at, 0);  // Not stamped when password unchanged
}

TEST(AccountSaveServicePrepareSaveTest, PasswordChangedHistoryDisabledSucceeds) {
    auto detail = make_detail("newpass");
    const std::string old_pw = "oldpass";
    AccountSaveContext ctx { detail, old_pw, true, /*history_enabled=*/false, 5 };
    auto result = AccountSaveService::prepare_save(ctx);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(detail.password_history.empty());
}

TEST(AccountSaveServicePrepareSaveTest, PasswordChangedHistoryEnabledAddsEntry) {
    auto detail = make_detail("newpass");
    const std::string old_pw = "oldpass";
    AccountSaveContext ctx { detail, old_pw, true, /*history_enabled=*/true, 5 };
    auto result = AccountSaveService::prepare_save(ctx);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(detail.password_history.size(), 1u);
    EXPECT_EQ(detail.password_history.front(), "oldpass");
}

TEST(AccountSaveServicePrepareSaveTest, PasswordChangedStampsPasswordChangedAt) {
    auto detail = make_detail("newpass");
    ASSERT_EQ(detail.password_changed_at, 0);
    const std::string old_pw = "oldpass";
    AccountSaveContext ctx { detail, old_pw, true, true, 5 };
    auto result = AccountSaveService::prepare_save(ctx);
    ASSERT_TRUE(result.has_value());
    EXPECT_GT(detail.password_changed_at, 0);
}

TEST(AccountSaveServicePrepareSaveTest, HistoryTrimmingEnforcesLimit) {
    auto detail = make_detail("newpass");
    // Pre-populate history up to the limit
    detail.password_history = {"pw1", "pw2", "pw3"};
    const std::string old_pw = "oldpass";
    AccountSaveContext ctx { detail, old_pw, true, true, /*history_limit=*/3 };
    auto result = AccountSaveService::prepare_save(ctx);
    ASSERT_TRUE(result.has_value());
    // 3 existing + 1 new = 4, trimmed back to limit 3
    EXPECT_EQ(static_cast<int>(detail.password_history.size()), 3);
    // Oldest entry ("pw1") should have been evicted
    EXPECT_EQ(detail.password_history.front(), "pw2");
    EXPECT_EQ(detail.password_history.back(), "oldpass");
}

TEST(AccountSaveServicePrepareSaveTest, EmptyOldPasswordNotAddedToHistory) {
    auto detail = make_detail("newpass");
    const std::string old_pw = "";  // No previous password
    AccountSaveContext ctx { detail, old_pw, true, true, 5 };
    auto result = AccountSaveService::prepare_save(ctx);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(detail.password_history.empty());
}

// ---------------------------------------------------------------------------
// prepare_save — failure paths
// ---------------------------------------------------------------------------

TEST(AccountSaveServicePrepareSaveTest, StandardUserAdminProtectedAccountFails) {
    auto detail = make_detail();
    detail.is_admin_only_deletable = true;
    const std::string old_pw = detail.password;
    AccountSaveContext ctx { detail, old_pw, /*is_admin=*/false, false, 5 };
    auto result = AccountSaveService::prepare_save(ctx);
    ASSERT_FALSE(result.has_value());
    EXPECT_FALSE(result.error().message.empty());
    EXPECT_TRUE(result.error().reload_account);
}

TEST(AccountSaveServicePrepareSaveTest, AdminUserAdminProtectedAccountSucceeds) {
    auto detail = make_detail();
    detail.is_admin_only_deletable = true;
    const std::string old_pw = detail.password;
    AccountSaveContext ctx { detail, old_pw, /*is_admin=*/true, false, 5 };
    auto result = AccountSaveService::prepare_save(ctx);
    EXPECT_TRUE(result.has_value());
}

TEST(AccountSaveServicePrepareSaveTest, PasswordReuseFails) {
    auto detail = make_detail("reused");
    detail.password_history = {"first", "reused", "second"};
    const std::string old_pw = "old";
    AccountSaveContext ctx { detail, old_pw, true, /*history_enabled=*/true, 10 };
    auto result = AccountSaveService::prepare_save(ctx);
    ASSERT_FALSE(result.has_value());
    EXPECT_FALSE(result.error().message.empty());
    EXPECT_NE(result.error().message.find("reuse"), std::string::npos);
    EXPECT_FALSE(result.error().reload_account);
}

TEST(AccountSaveServicePrepareSaveTest, PasswordReuseDoesNotMutateHistory) {
    auto detail = make_detail("reused");
    detail.password_history = {"reused"};
    const std::string old_pw = "old";
    AccountSaveContext ctx { detail, old_pw, true, true, 10 };
    (void)AccountSaveService::prepare_save(ctx);
    // History must not have grown — the reuse check should short-circuit
    EXPECT_EQ(detail.password_history.size(), 1u);
}
