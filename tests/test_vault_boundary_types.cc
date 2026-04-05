// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#include "core/VaultManager.h"

#include <gtest/gtest.h>

// ============================================================================
// View accessor integration: the public VaultManager facade should expose
// boundary/view types without requiring protobuf headers in the test itself.
// ============================================================================

TEST(VaultBoundaryTypesTest, ClosedVaultViewAccessorsReturnEmpty) {
    VaultManager vm;
    // Vault not opened — all view accessors must return empty without crashing.
    EXPECT_TRUE(vm.get_all_accounts_view().empty());
    EXPECT_TRUE(vm.get_all_groups_view().empty());
    EXPECT_TRUE(vm.get_yubikey_list_view().empty());
}

TEST(VaultBoundaryTypesTest, AccountDetailViewReturnsNulloptForClosedVault) {
    VaultManager vm;
    auto detail = vm.get_account_view(0);
    EXPECT_FALSE(detail.has_value());
}