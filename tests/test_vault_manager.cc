// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include <gtest/gtest.h>

#include "VaultManager.h"

#include <filesystem>
#include <fstream>

#include "lib/crypto/KeyWrapping.h"
#include "lib/storage/VaultIO.h"
#include "../src/core/services/IVaultYubiKeyService.h"

namespace fs = std::filesystem;

namespace {
class FakeVaultYubiKeyService final : public KeepTower::IVaultYubiKeyService {
public:
    KeepTower::VaultResult<ChallengeResult> perform_authenticated_challenge(
        const std::vector<uint8_t>& challenge,
        const std::vector<uint8_t>& credential_id,
        const std::string& pin,
        const std::string& expected_serial,
        ::YubiKeyAlgorithm algorithm,
        bool require_touch,
        int timeout_ms,
        bool enforce_fips = false,
        KeepTower::IVaultYubiKeyService::SerialMismatchPolicy serial_mismatch_policy =
            KeepTower::IVaultYubiKeyService::SerialMismatchPolicy::StrictError) override {
        authenticated_called = true;
        last_challenge = challenge;
        last_credential_id = credential_id;
        last_pin = pin;
        last_expected_serial = expected_serial;
        last_algorithm = algorithm;
        last_require_touch = require_touch;
        last_timeout_ms = timeout_ms;
        last_enforce_fips = enforce_fips;
        last_serial_mismatch_policy = serial_mismatch_policy;
        if (next_authenticated_result.has_value()) {
            return next_authenticated_result.value();
        }
        return std::unexpected(next_authenticated_error);
    }

    KeepTower::VaultResult<EnrollmentResult> enroll_yubikey(
        const std::string& user_id,
        const std::array<uint8_t, 32>& policy_challenge,
        const std::array<uint8_t, 32>& user_challenge,
        const std::string& pin,
        uint8_t slot = 1,
        bool enforce_fips = false,
        std::function<void(const std::string&)> progress_callback = nullptr) override {
        (void)slot;
        (void)enforce_fips;

        enrolled_called = true;
        last_enroll_user_id = user_id;
        last_enroll_policy_challenge = policy_challenge;
        last_enroll_user_challenge = user_challenge;
        last_enroll_pin = pin;
        if (progress_callback) {
            progress_callback("fake enroll callback");
        }

        if (next_enrollment_result.has_value()) {
            return next_enrollment_result.value();
        }
        return std::unexpected(next_enrollment_error);
    }

    KeepTower::VaultResult<std::vector<DeviceInfo>> detect_devices() override {
        return std::vector<DeviceInfo>{};  // Empty device list by default
    }

    bool authenticated_called = false;
    std::vector<uint8_t> last_challenge;
    std::vector<uint8_t> last_credential_id;
    std::string last_pin;
    std::string last_expected_serial;
    ::YubiKeyAlgorithm last_algorithm = ::YubiKeyAlgorithm::HMAC_SHA256;
    bool last_require_touch = false;
    int last_timeout_ms = 0;
    bool last_enforce_fips = false;
    KeepTower::IVaultYubiKeyService::SerialMismatchPolicy last_serial_mismatch_policy =
        KeepTower::IVaultYubiKeyService::SerialMismatchPolicy::StrictError;
    std::optional<ChallengeResult> next_authenticated_result;
    KeepTower::VaultError next_authenticated_error = KeepTower::VaultError::YubiKeyError;

    bool enrolled_called = false;
    std::string last_enroll_user_id;
    std::array<uint8_t, 32> last_enroll_policy_challenge{};
    std::array<uint8_t, 32> last_enroll_user_challenge{};
    std::string last_enroll_pin;
    std::optional<EnrollmentResult> next_enrollment_result;
    KeepTower::VaultError next_enrollment_error = KeepTower::VaultError::YubiKeyError;
};

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
#ifndef _WIN32
    // Windows uses ACLs, not POSIX mode bits; group/other read bits are always reported as set
    EXPECT_TRUE((perms & fs::perms::group_read) == fs::perms::none);
    EXPECT_TRUE((perms & fs::perms::others_read) == fs::perms::none);
#endif
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

TEST_F(VaultManagerTest, ReorderAccountFailsWhenVaultClosed) {
    EXPECT_FALSE(vault_manager->reorder_account(0, 1));
}

TEST_F(VaultManagerTest, ReorderGroup_SuccessAndSaveFailureRollback) {
    const auto policy = make_test_policy();
    ASSERT_TRUE(vault_manager->create_vault_v2(test_vault_path, test_username, test_password, policy));

    const std::string first_group_id = vault_manager->create_group("First");
    const std::string second_group_id = vault_manager->create_group("Second");
    ASSERT_FALSE(first_group_id.empty());
    ASSERT_FALSE(second_group_id.empty());

    ASSERT_TRUE(vault_manager->reorder_group(second_group_id, 0));

    auto groups = vault_manager->get_all_groups_view();
    auto second_it = std::find_if(groups.begin(), groups.end(),
        [&](const KeepTower::GroupView& group) { return group.group_id == second_group_id; });
    ASSERT_NE(second_it, groups.end());
    EXPECT_EQ(second_it->display_order, 0);

    const fs::path blocking_tmp = fs::path(test_vault_path).concat(".tmp");
    ASSERT_TRUE(fs::create_directory(blocking_tmp));
    EXPECT_FALSE(vault_manager->reorder_group(first_group_id, 0));
}

TEST_F(VaultManagerTest, SetRsRedundancyPercentRejectsOutOfRangeAndAcceptsValid) {
    EXPECT_FALSE(vault_manager->set_rs_redundancy_percent(4));
    EXPECT_FALSE(vault_manager->set_rs_redundancy_percent(51));

    EXPECT_TRUE(vault_manager->set_rs_redundancy_percent(25));
    EXPECT_EQ(vault_manager->get_rs_redundancy_percent(), 25);
}

TEST_F(VaultManagerTest, ApplyBackupSettingsPersistsAcrossReopenWhenVaultOpen) {
    const auto policy = make_test_policy();
    ASSERT_TRUE(vault_manager->create_vault_v2(test_vault_path, test_username, test_password, policy));

    VaultManager::BackupSettings settings;
    settings.enabled = true;
    settings.count = 9;
    settings.path = (test_dir / "persisted_backups").string();

    ASSERT_TRUE(vault_manager->apply_backup_settings(settings));
    ASSERT_TRUE(vault_manager->save_vault());
    ASSERT_TRUE(vault_manager->close_vault());

    ASSERT_TRUE(vault_manager->open_vault_v2(test_vault_path, test_username, test_password));
    const auto loaded = vault_manager->get_backup_settings();
    EXPECT_TRUE(loaded.enabled);
    EXPECT_EQ(loaded.count, 9);
    EXPECT_EQ(loaded.path, settings.path);
}

TEST_F(VaultManagerTest, RenameGroup_Success) {
    const auto policy = make_test_policy();
    ASSERT_TRUE(vault_manager->create_vault_v2(test_vault_path, test_username, test_password, policy));

    const std::string group_id = vault_manager->create_group("Work");
    ASSERT_FALSE(group_id.empty());

    ASSERT_TRUE(vault_manager->rename_group(group_id, "Personal"));

    const auto groups = vault_manager->get_all_groups_view();
    EXPECT_TRUE(std::any_of(groups.begin(), groups.end(), [](const KeepTower::GroupView& g) {
        return g.group_name == "Personal";
    }));
    EXPECT_TRUE(std::none_of(groups.begin(), groups.end(), [](const KeepTower::GroupView& g) {
        return g.group_name == "Work";
    }));
}

TEST_F(VaultManagerTest, DeleteGroup_Success) {
    const auto policy = make_test_policy();
    ASSERT_TRUE(vault_manager->create_vault_v2(test_vault_path, test_username, test_password, policy));

    const std::string group_id = vault_manager->create_group("Temporary");
    ASSERT_FALSE(group_id.empty());

    ASSERT_TRUE(vault_manager->delete_group(group_id));

    const auto groups = vault_manager->get_all_groups_view();
    EXPECT_TRUE(std::none_of(groups.begin(), groups.end(), [](const KeepTower::GroupView& g) {
        return g.group_name == "Temporary";
    }));
}

TEST_F(VaultManagerTest, AddAccountToGroup_Success) {
    const auto policy = make_test_policy();
    ASSERT_TRUE(vault_manager->create_vault_v2(test_vault_path, test_username, test_password, policy));

    const std::string group_id = vault_manager->create_group("Finance");
    ASSERT_FALSE(group_id.empty());

    auto detail = make_account_detail("bank-1", "BankAccount", "alice");
    ASSERT_TRUE(vault_manager->add_account(detail));

    ASSERT_TRUE(vault_manager->add_account_to_group(0, group_id));
    EXPECT_TRUE(vault_manager->is_account_in_group(0, group_id));
}

TEST_F(VaultManagerTest, AddAccountToGroup_SaveFailRollsBack) {
    const auto policy = make_test_policy();
    ASSERT_TRUE(vault_manager->create_vault_v2(test_vault_path, test_username, test_password, policy));

    const std::string group_id = vault_manager->create_group("Finance");
    ASSERT_FALSE(group_id.empty());

    auto detail = make_account_detail("bank-1", "BankAccount", "alice");
    ASSERT_TRUE(vault_manager->add_account(detail));

    const fs::path blocking_tmp = fs::path(test_vault_path).concat(".tmp");
    ASSERT_TRUE(fs::create_directory(blocking_tmp));

    EXPECT_FALSE(vault_manager->add_account_to_group(0, group_id));
    EXPECT_FALSE(vault_manager->is_account_in_group(0, group_id));
}

TEST_F(VaultManagerTest, RemoveAccountFromGroup_Success) {
    const auto policy = make_test_policy();
    ASSERT_TRUE(vault_manager->create_vault_v2(test_vault_path, test_username, test_password, policy));

    const std::string group_id = vault_manager->create_group("Finance");
    ASSERT_FALSE(group_id.empty());

    auto detail = make_account_detail("bank-1", "BankAccount", "alice");
    ASSERT_TRUE(vault_manager->add_account(detail));
    ASSERT_TRUE(vault_manager->add_account_to_group(0, group_id));
    ASSERT_TRUE(vault_manager->is_account_in_group(0, group_id));

    ASSERT_TRUE(vault_manager->remove_account_from_group(0, group_id));
    EXPECT_FALSE(vault_manager->is_account_in_group(0, group_id));
}

TEST_F(VaultManagerTest, RemoveAccountFromGroup_SaveFailRollsBack) {
    const auto policy = make_test_policy();
    ASSERT_TRUE(vault_manager->create_vault_v2(test_vault_path, test_username, test_password, policy));

    const std::string group_id = vault_manager->create_group("Finance");
    ASSERT_FALSE(group_id.empty());

    auto detail = make_account_detail("bank-1", "BankAccount", "alice");
    ASSERT_TRUE(vault_manager->add_account(detail));
    ASSERT_TRUE(vault_manager->add_account_to_group(0, group_id));

    const fs::path blocking_tmp = fs::path(test_vault_path).concat(".tmp");
    ASSERT_TRUE(fs::create_directory(blocking_tmp));

    EXPECT_FALSE(vault_manager->remove_account_from_group(0, group_id));
    EXPECT_TRUE(vault_manager->is_account_in_group(0, group_id));
}

TEST_F(VaultManagerTest, ReorderAccountInGroup_Success) {
    const auto policy = make_test_policy();
    ASSERT_TRUE(vault_manager->create_vault_v2(test_vault_path, test_username, test_password, policy));

    const std::string group_id = vault_manager->create_group("Work");
    ASSERT_FALSE(group_id.empty());

    auto detail = make_account_detail("acct-1", "Acct1", "alice");
    ASSERT_TRUE(vault_manager->add_account(detail));
    ASSERT_TRUE(vault_manager->add_account_to_group(0, group_id));

    EXPECT_TRUE(vault_manager->reorder_account_in_group(0, group_id, 5));

    const auto accounts = vault_manager->get_all_accounts_view();
    ASSERT_EQ(accounts.size(), 1u);
    ASSERT_EQ(accounts[0].groups.size(), 1u);
    EXPECT_EQ(accounts[0].groups[0].display_order, 5);
}

TEST_F(VaultManagerTest, ReorderAccountInGroup_SaveFailRollsBack) {
    const auto policy = make_test_policy();
    ASSERT_TRUE(vault_manager->create_vault_v2(test_vault_path, test_username, test_password, policy));

    const std::string group_id = vault_manager->create_group("Work");
    ASSERT_FALSE(group_id.empty());

    auto detail = make_account_detail("acct-1", "Acct1", "alice");
    ASSERT_TRUE(vault_manager->add_account(detail));
    ASSERT_TRUE(vault_manager->add_account_to_group(0, group_id));

    const fs::path blocking_tmp = fs::path(test_vault_path).concat(".tmp");
    ASSERT_TRUE(fs::create_directory(blocking_tmp));

    EXPECT_FALSE(vault_manager->reorder_account_in_group(0, group_id, 5));
}

TEST_F(VaultManagerTest, GetFavoritesGroupId_CreatesGroupAutomatically) {
    const auto policy = make_test_policy();
    ASSERT_TRUE(vault_manager->create_vault_v2(test_vault_path, test_username, test_password, policy));

    const std::string favorites_id = vault_manager->get_favorites_group_id();
    EXPECT_FALSE(favorites_id.empty());

    const auto groups = vault_manager->get_all_groups_view();
    const bool favorites_present = std::any_of(groups.begin(), groups.end(),
        [&](const KeepTower::GroupView& g) { return g.group_id == favorites_id; });
    EXPECT_TRUE(favorites_present);
}

TEST_F(VaultManagerTest, RuntimePreferencesPersistToVaultMetadataWhenOpen) {
    const auto policy = make_test_policy();
    ASSERT_TRUE(vault_manager->create_vault_v2(test_vault_path, test_username, test_password, policy));

    // Verify setters route through vault metadata when vault is open (covers the m_vault_open
    // branch in each setter that writes through to mutable_metadata()).
    vault_manager->set_clipboard_timeout(45);
    EXPECT_EQ(vault_manager->get_clipboard_timeout(), 45);

    vault_manager->set_auto_lock_enabled(true);
    EXPECT_TRUE(vault_manager->get_auto_lock_enabled());

    vault_manager->set_auto_lock_timeout(120);
    EXPECT_EQ(vault_manager->get_auto_lock_timeout(), 120);

    vault_manager->set_undo_redo_enabled(false);
    EXPECT_FALSE(vault_manager->get_undo_redo_enabled());

    vault_manager->set_undo_history_limit(25);
    EXPECT_EQ(vault_manager->get_undo_history_limit(), 25);

    vault_manager->set_account_password_history_enabled(true);
    EXPECT_TRUE(vault_manager->get_account_password_history_enabled());

    vault_manager->set_account_password_history_limit(8);
    EXPECT_EQ(vault_manager->get_account_password_history_limit(), 8);

    // Confirm m_modified was set — the vault data was mutated.
    EXPECT_TRUE(vault_manager->save_vault());
}

TEST_F(VaultManagerTest, CurrentUsernameReflectsOpenAndClosedState) {
    EXPECT_TRUE(vault_manager->get_current_username().empty());

    const auto policy = make_test_policy();
    ASSERT_TRUE(vault_manager->create_vault_v2(test_vault_path, test_username, test_password, policy));
    EXPECT_EQ(vault_manager->get_current_username(), "admin");

    ASSERT_TRUE(vault_manager->close_vault());
    EXPECT_TRUE(vault_manager->get_current_username().empty());
}

TEST_F(VaultManagerTest, YubiKeyAccessorsReturnSafeDefaultsWithoutEnrollment) {
    EXPECT_FALSE(vault_manager->current_user_requires_yubikey());
    EXPECT_TRUE(vault_manager->get_yubikey_list_view().empty());

    const auto policy = make_test_policy();
    ASSERT_TRUE(vault_manager->create_vault_v2(test_vault_path, test_username, test_password, policy));

    EXPECT_FALSE(vault_manager->current_user_requires_yubikey());
    EXPECT_TRUE(vault_manager->get_yubikey_list_view().empty());
}

TEST_F(VaultManagerTest, CheckVaultRequiresYubiKeyReturnsFalseForMissingFile) {
    std::string serial;
    EXPECT_FALSE(vault_manager->check_vault_requires_yubikey(
        (test_dir / "missing.vault").string(), serial));
}

TEST_F(VaultManagerTest, VerifyCredentialsFailsWhenVaultClosed) {
    EXPECT_FALSE(vault_manager->verify_credentials(test_password));
}

TEST_F(VaultManagerTest, VerifyCredentialsV2PasswordPathAcceptsCorrectAndRejectsWrongPassword) {
    const auto policy = make_test_policy();
    ASSERT_TRUE(vault_manager->create_vault_v2(test_vault_path, test_username, test_password, policy));

    EXPECT_TRUE(vault_manager->verify_credentials(test_password));
    EXPECT_FALSE(vault_manager->verify_credentials("WrongPassword123!"));
}

TEST_F(VaultManagerTest, VerifyCredentialsV2YubiKeyPathUsesInjectedService) {
    auto fake_service = std::make_shared<FakeVaultYubiKeyService>();
    VaultManager injected_manager(fake_service);

    KeepTower::VaultHeaderV2 header;
    header.security_policy.pbkdf2_iterations = 100000;

    KeepTower::KeySlot slot;
    slot.active = true;
    slot.username = test_username;
    slot.yubikey_serial = "YK-123456";
    slot.yubikey_credential_id = {0x10, 0x20, 0x30, 0x40};
    slot.yubikey_challenge.fill(0x5A);
    slot.salt.fill(0x11);

    const auto password_kek = KeepTower::KeyWrapping::derive_kek_from_password(
        test_password,
        slot.salt,
        header.security_policy.pbkdf2_iterations);
    ASSERT_TRUE(password_kek.has_value());

    KeepTower::IVaultYubiKeyService::ChallengeResult fake_result;
    fake_result.response.assign(32, 0x22);
    fake_result.device_info.serial = slot.yubikey_serial;
    fake_service->next_authenticated_result = fake_result;

    const auto final_kek = KeepTower::KeyWrapping::combine_with_yubikey_v2(password_kek.value(), fake_result.response);

    std::array<uint8_t, 32> dek{};
    dek.fill(0x33);
    const auto wrapped_dek = KeepTower::KeyWrapping::wrap_key(final_kek, dek);
    ASSERT_TRUE(wrapped_dek.has_value());
    slot.wrapped_dek = wrapped_dek->wrapped_key;

    header.key_slots.push_back(slot);

    injected_manager.m_vault_open = true;
    injected_manager.m_is_v2_vault = true;
    injected_manager.m_v2_header = header;
    injected_manager.m_current_session = KeepTower::UserSession{std::string(test_username), KeepTower::UserRole::ADMINISTRATOR, false, false, 0};

    EXPECT_TRUE(injected_manager.verify_credentials(test_password, slot.yubikey_serial));
    EXPECT_TRUE(fake_service->authenticated_called);
    EXPECT_EQ(fake_service->last_expected_serial, slot.yubikey_serial);
    EXPECT_EQ(fake_service->last_credential_id, slot.yubikey_credential_id);
    EXPECT_EQ(fake_service->last_challenge.size(), slot.yubikey_challenge.size());
    EXPECT_EQ(fake_service->last_timeout_ms, VaultManager::YUBIKEY_TIMEOUT_MS);
}

