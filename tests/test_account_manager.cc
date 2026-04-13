// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#include <gtest/gtest.h>

#include "../src/core/managers/AccountManager.h"

#include <algorithm>
#include <array>
#include <string>

using namespace KeepTower;

namespace {

keeptower::AccountRecord make_account(const std::string& name, int32_t global_display_order = -1) {
    keeptower::AccountRecord account;
    account.set_account_name(name);
    account.set_user_name(name + "-user");
    account.set_global_display_order(global_display_order);
    return account;
}

std::array<int32_t, 3> current_orders(const keeptower::VaultData& vault_data) {
    return {
        vault_data.accounts(0).global_display_order(),
        vault_data.accounts(1).global_display_order(),
        vault_data.accounts(2).global_display_order(),
    };
}

} // namespace

class AccountManagerUnitTests : public ::testing::Test {
protected:
    keeptower::VaultData vault_data;
    bool modified{false};
    AccountManager manager{vault_data, modified};
};

TEST_F(AccountManagerUnitTests, AddAccountCopiesRecordAndMarksModified) {
    auto account = make_account("Email");

    EXPECT_TRUE(manager.add_account(account));
    EXPECT_TRUE(modified);
    ASSERT_EQ(manager.get_account_count(), 1u);
    EXPECT_EQ(vault_data.accounts(0).account_name(), "Email");
    EXPECT_EQ(vault_data.accounts(0).user_name(), "Email-user");
}

TEST_F(AccountManagerUnitTests, GetAllAccountsReturnsCopiesInStoredOrder) {
    ASSERT_TRUE(manager.add_account(make_account("First")));
    modified = false;
    ASSERT_TRUE(manager.add_account(make_account("Second")));
    modified = false;

    auto accounts = manager.get_all_accounts();

    ASSERT_EQ(accounts.size(), 2u);
    EXPECT_EQ(accounts[0].account_name(), "First");
    EXPECT_EQ(accounts[1].account_name(), "Second");

    accounts[0].set_account_name("Changed Copy");
    EXPECT_EQ(vault_data.accounts(0).account_name(), "First");
    EXPECT_FALSE(modified);
}

TEST_F(AccountManagerUnitTests, GetAllAccountsReturnsEmptyWhenVaultHasNoAccounts) {
    auto accounts = manager.get_all_accounts();

    EXPECT_TRUE(accounts.empty());
    EXPECT_FALSE(modified);
}

TEST_F(AccountManagerUnitTests, UpdateAccountRejectsInvalidIndex) {
    ASSERT_TRUE(manager.add_account(make_account("Original")));
    modified = false;

    auto replacement = make_account("Replacement");

    EXPECT_FALSE(manager.update_account(3, replacement));
    EXPECT_EQ(vault_data.accounts(0).account_name(), "Original");
    EXPECT_FALSE(modified);
}

TEST_F(AccountManagerUnitTests, UpdateAccountReplacesStoredRecordAndMarksModified) {
    ASSERT_TRUE(manager.add_account(make_account("Original")));
    modified = false;

    auto replacement = make_account("Replacement", 7);
    replacement.set_notes("Updated");

    EXPECT_TRUE(manager.update_account(0, replacement));
    EXPECT_TRUE(modified);
    EXPECT_EQ(vault_data.accounts(0).account_name(), "Replacement");
    EXPECT_EQ(vault_data.accounts(0).notes(), "Updated");
    EXPECT_EQ(vault_data.accounts(0).global_display_order(), 7);
}

TEST_F(AccountManagerUnitTests, DeleteAccountRejectsInvalidIndex) {
    ASSERT_TRUE(manager.add_account(make_account("Only")));
    modified = false;

    EXPECT_FALSE(manager.delete_account(2));
    EXPECT_EQ(manager.get_account_count(), 1u);
    EXPECT_FALSE(modified);
}

TEST_F(AccountManagerUnitTests, DeleteAccountRemovesRecordAndShiftsRemainingEntries) {
    ASSERT_TRUE(manager.add_account(make_account("First")));
    ASSERT_TRUE(manager.add_account(make_account("Second")));
    ASSERT_TRUE(manager.add_account(make_account("Third")));
    modified = false;

    EXPECT_TRUE(manager.delete_account(1));

    EXPECT_TRUE(modified);
    ASSERT_EQ(manager.get_account_count(), 2u);
    EXPECT_EQ(vault_data.accounts(0).account_name(), "First");
    EXPECT_EQ(vault_data.accounts(1).account_name(), "Third");
}

TEST_F(AccountManagerUnitTests, AccountAccessorsReturnPointersForValidIndicesOnly) {
    ASSERT_TRUE(manager.add_account(make_account("Mutable")));

    EXPECT_EQ(manager.get_account(1), nullptr);
    EXPECT_EQ(manager.get_account_mutable(1), nullptr);

    const auto* account = manager.get_account(0);
    ASSERT_NE(account, nullptr);
    EXPECT_EQ(account->account_name(), "Mutable");

    auto* mutable_account = manager.get_account_mutable(0);
    ASSERT_NE(mutable_account, nullptr);
    mutable_account->set_notes("edited");
    EXPECT_EQ(vault_data.accounts(0).notes(), "edited");
}

TEST_F(AccountManagerUnitTests, ReorderAccountRejectsOutOfRangeIndices) {
    ASSERT_TRUE(manager.add_account(make_account("One")));
    ASSERT_TRUE(manager.add_account(make_account("Two")));
    modified = false;

    EXPECT_FALSE(manager.reorder_account(2, 0));
    EXPECT_FALSE(manager.reorder_account(0, 2));
    EXPECT_FALSE(modified);
}

TEST_F(AccountManagerUnitTests, ReorderAccountSameIndexIsNoOp) {
    ASSERT_TRUE(manager.add_account(make_account("One", 0)));
    ASSERT_TRUE(manager.add_account(make_account("Two", 1)));
    modified = false;

    EXPECT_TRUE(manager.reorder_account(1, 1));
    EXPECT_FALSE(modified);
    EXPECT_EQ(vault_data.accounts(0).global_display_order(), 0);
    EXPECT_EQ(vault_data.accounts(1).global_display_order(), 1);
}

TEST_F(AccountManagerUnitTests, ReorderAccountInitializesOrderingAndNormalizesWhenMovingDown) {
    ASSERT_TRUE(manager.add_account(make_account("One")));
    ASSERT_TRUE(manager.add_account(make_account("Two")));
    ASSERT_TRUE(manager.add_account(make_account("Three")));
    modified = false;

    EXPECT_TRUE(manager.reorder_account(0, 2));

    EXPECT_TRUE(modified);
    auto orders = current_orders(vault_data);
    std::sort(orders.begin(), orders.end());
    EXPECT_EQ(orders, (std::array<int32_t, 3>{0, 1, 2}));
}

TEST_F(AccountManagerUnitTests, ReorderAccountNormalizesExistingCustomOrderingWhenMovingUp) {
    ASSERT_TRUE(manager.add_account(make_account("One", 10)));
    ASSERT_TRUE(manager.add_account(make_account("Two", 20)));
    ASSERT_TRUE(manager.add_account(make_account("Three", 30)));
    modified = false;

    EXPECT_TRUE(manager.reorder_account(2, 0));

    EXPECT_TRUE(modified);
    auto orders = current_orders(vault_data);
    std::sort(orders.begin(), orders.end());
    EXPECT_EQ(orders, (std::array<int32_t, 3>{0, 1, 2}));
}

TEST_F(AccountManagerUnitTests, CanDeleteAccountMatchesIndexValidity) {
    ASSERT_TRUE(manager.add_account(make_account("Only")));

    EXPECT_TRUE(manager.can_delete_account(0));
    EXPECT_FALSE(manager.can_delete_account(1));
}