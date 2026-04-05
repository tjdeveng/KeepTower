// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#include "core/VaultBoundaryTypes.h"

#include <gtest/gtest.h>

TEST(VaultBoundaryModelsTest, AccountListItemStoresListFieldsWithoutProtobuf) {
    KeepTower::AccountListItem account;
    account.id = "acct-1";
    account.account_name = "Example";
    account.user_name = "user@example.com";
    account.email = "user@example.com";
    account.website = "https://example.com";
    account.notes = "notes";
    account.tags = {"work", "shared"};
    account.groups.push_back({"group-1", 3});
    account.is_favorite = true;
    account.global_display_order = 8;

    ASSERT_EQ(account.id, "acct-1");
    ASSERT_EQ(account.account_name, "Example");
    ASSERT_EQ(account.user_name, "user@example.com");
    ASSERT_EQ(account.tags.size(), 2U);
    ASSERT_EQ(account.groups.size(), 1U);
    EXPECT_EQ(account.groups.front().group_id, "group-1");
    EXPECT_EQ(account.groups.front().display_order, 3);
    EXPECT_TRUE(account.is_favorite);
    EXPECT_EQ(account.global_display_order, 8);
}

TEST(VaultBoundaryModelsTest, GroupAndYubiKeyViewsExposeStableBoundaryFields) {
    KeepTower::GroupView group;
    group.group_id = "favorites";
    group.group_name = "Favorites";
    group.icon = "starred-symbolic";
    group.is_system_group = true;

    KeepTower::YubiKeyView key;
    key.serial = "12345678";
    key.name = "Primary";
    key.added_at = 1700000000;

    EXPECT_EQ(group.group_id, "favorites");
    EXPECT_EQ(group.group_name, "Favorites");
    EXPECT_EQ(group.icon, "starred-symbolic");
    EXPECT_TRUE(group.is_system_group);

    EXPECT_EQ(key.serial, "12345678");
    EXPECT_EQ(key.name, "Primary");
    EXPECT_EQ(key.added_at, 1700000000);
}