// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#include <gtest/gtest.h>

#include "../src/core/VaultRuntimePreferences.h"

namespace {

using KeepTower::VaultRuntimePreferences;

class VaultRuntimePreferencesTest : public ::testing::Test {
protected:
    static void expect_default_values(const VaultRuntimePreferences& preferences) {
        EXPECT_EQ(preferences.get_clipboard_timeout(), 30);
        EXPECT_TRUE(preferences.get_auto_lock_enabled());
        EXPECT_EQ(preferences.get_auto_lock_timeout(), 300);
        EXPECT_TRUE(preferences.get_undo_redo_enabled());
        EXPECT_EQ(preferences.get_undo_history_limit(), 50);
        EXPECT_FALSE(preferences.get_account_password_history_enabled());
        EXPECT_EQ(preferences.get_account_password_history_limit(), 5);
    }

    VaultRuntimePreferences preferences;
};

TEST_F(VaultRuntimePreferencesTest, DefaultsMatchSchemaValues) {
    expect_default_values(preferences);
}

TEST_F(VaultRuntimePreferencesTest, IndividualSettersUpdateValues) {
    preferences.set_clipboard_timeout(90);
    preferences.set_auto_lock_enabled(false);
    preferences.set_auto_lock_timeout(900);
    preferences.set_undo_redo_enabled(false);
    preferences.set_undo_history_limit(12);
    preferences.set_account_password_history_enabled(true);
    preferences.set_account_password_history_limit(9);

    EXPECT_EQ(preferences.get_clipboard_timeout(), 90);
    EXPECT_FALSE(preferences.get_auto_lock_enabled());
    EXPECT_EQ(preferences.get_auto_lock_timeout(), 900);
    EXPECT_FALSE(preferences.get_undo_redo_enabled());
    EXPECT_EQ(preferences.get_undo_history_limit(), 12);
    EXPECT_TRUE(preferences.get_account_password_history_enabled());
    EXPECT_EQ(preferences.get_account_password_history_limit(), 9);
}

TEST_F(VaultRuntimePreferencesTest, SyncFromVaultMetadataUpdatesAllFields) {
    preferences.sync_from_vault_metadata(
        45,
        false,
        120,
        false,
        25,
        true,
        8);

    EXPECT_EQ(preferences.get_clipboard_timeout(), 45);
    EXPECT_FALSE(preferences.get_auto_lock_enabled());
    EXPECT_EQ(preferences.get_auto_lock_timeout(), 120);
    EXPECT_FALSE(preferences.get_undo_redo_enabled());
    EXPECT_EQ(preferences.get_undo_history_limit(), 25);
    EXPECT_TRUE(preferences.get_account_password_history_enabled());
    EXPECT_EQ(preferences.get_account_password_history_limit(), 8);
}

TEST_F(VaultRuntimePreferencesTest, ResetToDefaultsRestoresSchemaValues) {
    preferences.set_clipboard_timeout(10);
    preferences.set_auto_lock_enabled(false);
    preferences.set_auto_lock_timeout(60);
    preferences.set_undo_redo_enabled(false);
    preferences.set_undo_history_limit(3);
    preferences.set_account_password_history_enabled(true);
    preferences.set_account_password_history_limit(2);

    preferences.reset_to_defaults();

    expect_default_values(preferences);
}

TEST_F(VaultRuntimePreferencesTest, SyncAfterMutationOverwritesPreviousRuntimeState) {
    preferences.set_clipboard_timeout(5);
    preferences.set_auto_lock_enabled(false);
    preferences.set_auto_lock_timeout(15);
    preferences.set_undo_redo_enabled(false);
    preferences.set_undo_history_limit(1);
    preferences.set_account_password_history_enabled(true);
    preferences.set_account_password_history_limit(1);

    preferences.sync_from_vault_metadata(
        75,
        true,
        600,
        true,
        40,
        false,
        6);

    EXPECT_EQ(preferences.get_clipboard_timeout(), 75);
    EXPECT_TRUE(preferences.get_auto_lock_enabled());
    EXPECT_EQ(preferences.get_auto_lock_timeout(), 600);
    EXPECT_TRUE(preferences.get_undo_redo_enabled());
    EXPECT_EQ(preferences.get_undo_history_limit(), 40);
    EXPECT_FALSE(preferences.get_account_password_history_enabled());
    EXPECT_EQ(preferences.get_account_password_history_limit(), 6);
}

TEST_F(VaultRuntimePreferencesTest, ResetAfterMetadataSyncClearsLoadedValues) {
    preferences.sync_from_vault_metadata(
        120,
        false,
        30,
        false,
        4,
        true,
        11);

    preferences.reset_to_defaults();

    expect_default_values(preferences);
}

}  // namespace