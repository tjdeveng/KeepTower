// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include <gtest/gtest.h>

#include "VaultManager.h"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

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
        vault_manager->set_backup_enabled(false);
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
