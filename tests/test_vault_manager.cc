// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include <gtest/gtest.h>

#include "VaultManager.h"

#include <filesystem>
#include <fstream>

#include "lib/storage/VaultIO.h"

namespace fs = std::filesystem;

namespace {
void disable_backups_for_test(VaultManager& manager) {
    VaultManager::BackupSettings settings = manager.get_backup_settings();
    settings.enabled = false;
    ASSERT_TRUE(manager.apply_backup_settings(settings));
}
KeepTower::AccountDetail make_account_detail(
    const std::string& id,
    const std::string& account_name,
    const std::string& user_name,
    int32_t global_display_order = -1) {
    KeepTower::AccountDetail detail;
    detail.id = id;
    detail.account_name = account_name;
    detail.user_name = user_name;
    detail.password = "SecretPassword123!";
    detail.email = id + "@example.com";
    detail.website = "https://" + id + ".example.com";
    detail.notes = "notes-for-" + id;
    detail.tags = {"alpha", "beta"};
    detail.password_history = {"old-password-1", "old-password-2"};
    detail.is_favorite = true;
    detail.is_archived = false;
    detail.is_admin_only_viewable = true;
    detail.is_admin_only_deletable = false;
    detail.global_display_order = global_display_order;
    detail.created_at = 111;
    detail.modified_at = 222;
    detail.password_changed_at = 333;
    return detail;
}

std::vector<uint8_t> read_file_bytes(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return {};
    }

    return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
}
}  // namespace

static KeepTower::VaultSecurityPolicy make_test_policy() {
    KeepTower::VaultSecurityPolicy policy;
    policy.require_yubikey = false;
    policy.min_password_length = 12;
    policy.pbkdf2_iterations = 100000;
    policy.password_history_depth = 0;
    return policy;
}

class VaultManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir = fs::temp_directory_path() / "keeptower_tests";
        fs::create_directories(test_dir);
        test_vault_path = (test_dir / "test_vault.v2").string();

        vault_manager = std::make_unique<VaultManager>();
        disable_backups_for_test(*vault_manager);
        vault_manager->set_reed_solomon_enabled(false);
    }

    void TearDown() override {
        vault_manager.reset();
        try {
            fs::remove_all(test_dir);
        } catch (...) {
        }
    }

    fs::path test_dir;
    std::string test_vault_path;
    std::unique_ptr<VaultManager> vault_manager;
    const Glib::ustring test_username = "admin";
    const Glib::ustring test_password = "TestPassword123!";
};

TEST_F(VaultManagerTest, CreateVaultV2_Success) {
    const auto policy = make_test_policy();
    auto result = vault_manager->create_vault_v2(test_vault_path, test_username, test_password, policy);
    ASSERT_TRUE(result);

    EXPECT_TRUE(vault_manager->is_vault_open());
    EXPECT_EQ(vault_manager->get_current_vault_path(), test_vault_path);
    EXPECT_TRUE(fs::exists(test_vault_path));
}

TEST_F(VaultManagerTest, CreateVaultV2_FileHasRestrictivePermissions) {
    const auto policy = make_test_policy();
    ASSERT_TRUE(vault_manager->create_vault_v2(test_vault_path, test_username, test_password, policy));

    auto perms = fs::status(test_vault_path).permissions();
    EXPECT_TRUE((perms & fs::perms::owner_read) != fs::perms::none);
    EXPECT_TRUE((perms & fs::perms::owner_write) != fs::perms::none);
    EXPECT_TRUE((perms & fs::perms::group_read) == fs::perms::none);
    EXPECT_TRUE((perms & fs::perms::others_read) == fs::perms::none);
}

TEST_F(VaultManagerTest, OpenVaultV2_WithCorrectPassword_Success) {
    const auto policy = make_test_policy();
    ASSERT_TRUE(vault_manager->create_vault_v2(test_vault_path, test_username, test_password, policy));
    ASSERT_TRUE(vault_manager->close_vault());

    auto session = vault_manager->open_vault_v2(test_vault_path, test_username, test_password);
    ASSERT_TRUE(session);
    EXPECT_TRUE(vault_manager->is_vault_open());
}

TEST_F(VaultManagerTest, OpenVaultV2_WithWrongPassword_Fails) {
    const auto policy = make_test_policy();
    ASSERT_TRUE(vault_manager->create_vault_v2(test_vault_path, test_username, test_password, policy));
    ASSERT_TRUE(vault_manager->close_vault());

    auto session = vault_manager->open_vault_v2(test_vault_path, test_username, "WrongPassword");
    EXPECT_FALSE(session);
    EXPECT_FALSE(vault_manager->is_vault_open());
}

TEST_F(VaultManagerTest, OpenVaultV2_NonExistentFile_Fails) {
    auto session = vault_manager->open_vault_v2("/nonexistent/vault.v2", test_username, test_password);
    EXPECT_FALSE(session);
    EXPECT_FALSE(vault_manager->is_vault_open());
}

TEST_F(VaultManagerTest, OpenVaultV2_CorruptedFile_Fails) {
    {
        std::ofstream file(test_vault_path, std::ios::binary);
        ASSERT_TRUE(file.is_open());
        file << "This is not a valid vault file";
    }

    auto session = vault_manager->open_vault_v2(test_vault_path, test_username, test_password);
    EXPECT_FALSE(session);
    EXPECT_FALSE(vault_manager->is_vault_open());
}

TEST_F(VaultManagerTest, CorruptionRecoveryV2_TruncatedFile_Fails) {
    const auto policy = make_test_policy();
    ASSERT_TRUE(vault_manager->create_vault_v2(test_vault_path, test_username, test_password, policy));
    ASSERT_TRUE(vault_manager->save_vault());
    ASSERT_TRUE(vault_manager->close_vault());

    {
        std::ofstream file(test_vault_path, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(file.is_open());
        const std::string partial(50, '\xFF');
        file.write(partial.data(), static_cast<std::streamsize>(partial.size()));
    }

    auto session = vault_manager->open_vault_v2(test_vault_path, test_username, test_password);
    EXPECT_FALSE(session);
    EXPECT_FALSE(vault_manager->is_vault_open());
}

TEST_F(VaultManagerTest, ApplyBackupSettingsRejectsInvalidCountWithoutMutatingState) {
    VaultManager::BackupSettings baseline = vault_manager->get_backup_settings();

    VaultManager::BackupSettings valid{true, 7, test_dir.string()};
    ASSERT_TRUE(vault_manager->apply_backup_settings(valid));

    VaultManager::BackupSettings invalid = valid;
    invalid.enabled = false;
    invalid.count = 0;  // Invalid range
    invalid.path = (test_dir / "other").string();

    EXPECT_FALSE(vault_manager->apply_backup_settings(invalid));

    const VaultManager::BackupSettings after = vault_manager->get_backup_settings();
    EXPECT_EQ(after.enabled, valid.enabled);
    EXPECT_EQ(after.count, valid.count);
    EXPECT_EQ(after.path, valid.path);

    // Keep baseline referenced so test intent is explicit and future assertions can extend safely.
    (void)baseline;
}

TEST_F(VaultManagerTest, RestoreFromMostRecentBackupFailsWhenVaultOpen) {
    const auto policy = make_test_policy();
    ASSERT_TRUE(vault_manager->create_vault_v2(test_vault_path, test_username, test_password, policy));
    ASSERT_TRUE(vault_manager->is_vault_open());

    auto restore_result = vault_manager->restore_from_most_recent_backup(test_vault_path);
    ASSERT_FALSE(restore_result);
    EXPECT_EQ(restore_result.error(), KeepTower::VaultError::VaultAlreadyOpen);
}

TEST_F(VaultManagerTest, ExplicitSaveBackupCapturesPreSaveState) {
    const auto policy = make_test_policy();
    ASSERT_TRUE(vault_manager->create_vault_v2(test_vault_path, test_username, test_password, policy));

    const fs::path backup_dir = test_dir / "explicit_backups";
    VaultManager::BackupSettings settings = vault_manager->get_backup_settings();
    settings.enabled = true;
    settings.count = 10;
    settings.path = backup_dir.string();
    ASSERT_TRUE(vault_manager->apply_backup_settings(settings));

    auto account_a = make_account_detail("acct-a", "Account A", "alice");
    ASSERT_TRUE(vault_manager->add_account(account_a));
    ASSERT_TRUE(vault_manager->save_vault(true));

    auto account_b = make_account_detail("acct-b", "Account B", "bob");
    ASSERT_TRUE(vault_manager->add_account(account_b));
    ASSERT_TRUE(vault_manager->save_vault(true));

    auto backups = KeepTower::VaultIO::list_backups(test_vault_path, backup_dir.string());
    ASSERT_FALSE(backups.empty());

    VaultManager backup_view_manager;
    auto session = backup_view_manager.open_vault_v2(backups[0], test_username, test_password);
    ASSERT_TRUE(session);
    EXPECT_EQ(backup_view_manager.get_account_count(), 1u);

    const auto backup_accounts = backup_view_manager.get_all_accounts_view();
    ASSERT_EQ(backup_accounts.size(), 1u);
    EXPECT_EQ(backup_accounts[0].account_name, "Account A");
}

TEST_F(VaultManagerTest, ExplicitSaveFailsWhenPreSaveBackupCreationFails) {
    const auto policy = make_test_policy();
    ASSERT_TRUE(vault_manager->create_vault_v2(test_vault_path, test_username, test_password, policy));

    auto baseline_account = make_account_detail("acct-base", "Baseline", "alice");
    ASSERT_TRUE(vault_manager->add_account(baseline_account));
    ASSERT_TRUE(vault_manager->save_vault(true));

    const std::vector<uint8_t> baseline_file_bytes = read_file_bytes(test_vault_path);
    ASSERT_FALSE(baseline_file_bytes.empty());

    const fs::path invalid_backup_target = test_dir / "backup_target_file";
    {
        std::ofstream marker(invalid_backup_target);
        ASSERT_TRUE(marker.is_open());
        marker << "not-a-directory";
    }

    VaultManager::BackupSettings settings = vault_manager->get_backup_settings();
    settings.enabled = true;
    settings.count = 5;
    settings.path = invalid_backup_target.string();
    ASSERT_TRUE(vault_manager->apply_backup_settings(settings));

    auto new_account = make_account_detail("acct-new", "Unsaved", "bob");
    ASSERT_TRUE(vault_manager->add_account(new_account));
    EXPECT_FALSE(vault_manager->save_vault(true));

    const std::vector<uint8_t> after_failed_save_bytes = read_file_bytes(test_vault_path);
    EXPECT_EQ(after_failed_save_bytes, baseline_file_bytes);
}

TEST_F(VaultManagerTest, AccountBoundaryViewsRoundTripThroughVaultManager) {
    const auto policy = make_test_policy();
    ASSERT_TRUE(vault_manager->create_vault_v2(test_vault_path, test_username, test_password, policy));

    const std::string group_id = vault_manager->create_group("Work");
    ASSERT_FALSE(group_id.empty());

    auto detail = make_account_detail("email-id", "Email", "alice", 7);
    detail.groups.push_back({group_id, 3});

    ASSERT_TRUE(vault_manager->add_account(detail));
    EXPECT_EQ(vault_manager->get_account_count(), 1u);

    auto all_accounts = vault_manager->get_all_accounts_view();
    ASSERT_EQ(all_accounts.size(), 1u);
    EXPECT_EQ(all_accounts[0].id, "email-id");
    EXPECT_EQ(all_accounts[0].account_name, "Email");
    EXPECT_EQ(all_accounts[0].user_name, "alice");
    EXPECT_EQ(all_accounts[0].email, "email-id@example.com");
    EXPECT_EQ(all_accounts[0].website, "https://email-id.example.com");
    EXPECT_EQ(all_accounts[0].notes, "notes-for-email-id");
    EXPECT_EQ(all_accounts[0].tags.size(), 2u);
    ASSERT_EQ(all_accounts[0].groups.size(), 1u);
    EXPECT_EQ(all_accounts[0].groups[0].group_id, group_id);
    EXPECT_EQ(all_accounts[0].groups[0].display_order, 3);
    EXPECT_TRUE(all_accounts[0].is_favorite);
    EXPECT_FALSE(all_accounts[0].is_archived);
    EXPECT_EQ(all_accounts[0].global_display_order, 7);

    auto stored_detail = vault_manager->get_account_view(0);
    ASSERT_TRUE(stored_detail.has_value());
    EXPECT_EQ(stored_detail->id, detail.id);
    EXPECT_EQ(stored_detail->account_name, detail.account_name);
    EXPECT_EQ(stored_detail->user_name, detail.user_name);
    EXPECT_EQ(stored_detail->password, detail.password);
    EXPECT_EQ(stored_detail->tags, detail.tags);
    EXPECT_EQ(stored_detail->password_history, detail.password_history);
    ASSERT_EQ(stored_detail->groups.size(), 1u);
    EXPECT_EQ(stored_detail->groups[0].group_id, group_id);
    EXPECT_EQ(stored_detail->groups[0].display_order, 3);
    EXPECT_TRUE(stored_detail->is_admin_only_viewable);
    EXPECT_FALSE(stored_detail->is_admin_only_deletable);
    EXPECT_EQ(stored_detail->created_at, 111);
    EXPECT_EQ(stored_detail->modified_at, 222);
    EXPECT_EQ(stored_detail->password_changed_at, 333);
}

TEST_F(VaultManagerTest, CreateGroupSaveFailureRollsBackInMemoryState) {
    const auto policy = make_test_policy();
    ASSERT_TRUE(vault_manager->create_vault_v2(test_vault_path, test_username, test_password, policy));

    const auto baseline_groups = vault_manager->get_all_groups_view();

    const fs::path blocking_temp_path = fs::path(test_vault_path).concat(".tmp");
    ASSERT_TRUE(fs::create_directory(blocking_temp_path));

    EXPECT_TRUE(vault_manager->is_vault_open());
    EXPECT_EQ(vault_manager->create_group("Work"), "");

    const auto after_groups = vault_manager->get_all_groups_view();
    EXPECT_EQ(after_groups.size(), baseline_groups.size());
    EXPECT_TRUE(std::none_of(
        after_groups.begin(),
        after_groups.end(),
        [](const KeepTower::GroupView& group) {
            return group.group_name == "Work";
        }));
}

TEST_F(VaultManagerTest, DeleteGroupSaveFailureRollsBackInMemoryState) {
    const auto policy = make_test_policy();
    ASSERT_TRUE(vault_manager->create_vault_v2(test_vault_path, test_username, test_password, policy));

    const std::string group_id = vault_manager->create_group("Work");
    ASSERT_FALSE(group_id.empty());
    const auto baseline_groups = vault_manager->get_all_groups_view();
    ASSERT_TRUE(std::any_of(
        baseline_groups.begin(),
        baseline_groups.end(),
        [](const KeepTower::GroupView& group) {
            return group.group_name == "Work";
        }));

    const fs::path blocking_temp_path = fs::path(test_vault_path).concat(".tmp");
    ASSERT_TRUE(fs::create_directory(blocking_temp_path));

    EXPECT_FALSE(vault_manager->delete_group(group_id));

    const auto after_groups = vault_manager->get_all_groups_view();
    EXPECT_TRUE(std::any_of(
        after_groups.begin(),
        after_groups.end(),
        [](const KeepTower::GroupView& group) {
            return group.group_name == "Work";
        }));
}

TEST_F(VaultManagerTest, RenameGroupSaveFailureRollsBackInMemoryState) {
    const auto policy = make_test_policy();
    ASSERT_TRUE(vault_manager->create_vault_v2(test_vault_path, test_username, test_password, policy));

    const std::string group_id = vault_manager->create_group("Work");
    ASSERT_FALSE(group_id.empty());

    const fs::path blocking_temp_path = fs::path(test_vault_path).concat(".tmp");
    ASSERT_TRUE(fs::create_directory(blocking_temp_path));

    EXPECT_FALSE(vault_manager->rename_group(group_id, "Renamed Work"));

    const auto after_groups = vault_manager->get_all_groups_view();
    EXPECT_TRUE(std::any_of(
        after_groups.begin(),
        after_groups.end(),
        [](const KeepTower::GroupView& group) {
            return group.group_name == "Work";
        }));
    EXPECT_TRUE(std::none_of(
        after_groups.begin(),
        after_groups.end(),
        [](const KeepTower::GroupView& group) {
            return group.group_name == "Renamed Work";
        }));
}

TEST_F(VaultManagerTest, UpdateAndDeleteAccountUseBoundaryModelGuards) {
    auto detail = make_account_detail("closed-id", "Closed", "user");
    EXPECT_FALSE(vault_manager->add_account(detail));
    EXPECT_FALSE(vault_manager->update_account(0, detail));
    EXPECT_FALSE(vault_manager->delete_account(0));
    EXPECT_EQ(vault_manager->get_account_count(), 0u);
    EXPECT_FALSE(vault_manager->get_account_view(0).has_value());

    const auto policy = make_test_policy();
    ASSERT_TRUE(vault_manager->create_vault_v2(test_vault_path, test_username, test_password, policy));
    ASSERT_TRUE(vault_manager->add_account(detail));

    auto updated = detail;
    updated.account_name = "Updated Account";
    updated.user_name = "updated-user";
    updated.notes = "updated-notes";
    updated.global_display_order = 4;
    updated.is_archived = true;
    updated.is_admin_only_deletable = true;
    updated.tags = {"updated"};
    updated.password_history = {"historic"};

    EXPECT_FALSE(vault_manager->update_account(99, updated));
    ASSERT_TRUE(vault_manager->update_account(0, updated));

    auto stored = vault_manager->get_account_view(0);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->account_name, "Updated Account");
    EXPECT_EQ(stored->user_name, "updated-user");
    EXPECT_EQ(stored->notes, "updated-notes");
    EXPECT_EQ(stored->global_display_order, 4);
    EXPECT_TRUE(stored->is_archived);
    EXPECT_TRUE(stored->is_admin_only_deletable);
    EXPECT_EQ(stored->tags, (std::vector<std::string>{"updated"}));
    EXPECT_EQ(stored->password_history, (std::vector<std::string>{"historic"}));

    EXPECT_FALSE(vault_manager->delete_account(5));
    ASSERT_TRUE(vault_manager->delete_account(0));
    EXPECT_EQ(vault_manager->get_account_count(), 0u);
    EXPECT_FALSE(vault_manager->get_account_view(0).has_value());
}

TEST_F(VaultManagerTest, GlobalOrderingHelpersResetAndPersistOrderingState) {
    const auto policy = make_test_policy();
    ASSERT_TRUE(vault_manager->create_vault_v2(test_vault_path, test_username, test_password, policy));

    auto first = make_account_detail("one", "One", "user-one", -1);
    auto second = make_account_detail("two", "Two", "user-two", -1);
    auto third = make_account_detail("three", "Three", "user-three", -1);

    ASSERT_TRUE(vault_manager->add_account(first));
    ASSERT_TRUE(vault_manager->add_account(second));
    ASSERT_TRUE(vault_manager->add_account(third));
    EXPECT_FALSE(vault_manager->has_custom_global_ordering());

    ASSERT_TRUE(vault_manager->reorder_account(0, 2));
    EXPECT_TRUE(vault_manager->has_custom_global_ordering());

    auto reordered = vault_manager->get_all_accounts_view();
    ASSERT_EQ(reordered.size(), 3u);
    int custom_order_count = 0;
    for (const auto& account : reordered) {
        if (account.global_display_order >= 0) {
            custom_order_count++;
        }
    }
    EXPECT_EQ(custom_order_count, 3);

    ASSERT_TRUE(vault_manager->reset_global_display_order());
    EXPECT_FALSE(vault_manager->has_custom_global_ordering());

    auto reset_accounts = vault_manager->get_all_accounts_view();
    ASSERT_EQ(reset_accounts.size(), 3u);
    for (const auto& account : reset_accounts) {
        EXPECT_EQ(account.global_display_order, -1);
    }

    ASSERT_TRUE(vault_manager->close_vault());
    auto reopened = vault_manager->open_vault_v2(test_vault_path, test_username, test_password);
    ASSERT_TRUE(reopened);
    EXPECT_FALSE(vault_manager->has_custom_global_ordering());
}
