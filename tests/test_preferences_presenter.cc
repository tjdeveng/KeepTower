// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#include <gtest/gtest.h>

#include "../src/core/VaultManager.h"
#include "../src/ui/dialogs/preferences/PreferencesPresenter.h"

#include <giomm.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace {

class PreferencesPresenterTest : public ::testing::Test {
protected:
    void SetUp() override {
        Glib::init();
        Gio::init();

        const char* schema_dir = std::getenv("GSETTINGS_SCHEMA_DIR");
        if (!schema_dir) {
            GTEST_SKIP() << "GSETTINGS_SCHEMA_DIR not set";
        }

        namespace fs = std::filesystem;
        const fs::path schema_path{schema_dir};
        const fs::path schema_xml = schema_path / "com.tjdeveng.keeptower.gschema.xml";
        if (fs::exists(schema_xml)) {
            const std::string cmd = "glib-compile-schemas " + schema_path.string();
            const int rc = std::system(cmd.c_str());
            if (rc != 0) {
                GTEST_SKIP() << "Failed to compile GSettings schemas with: " << cmd;
            }
        }

        try {
            m_settings = Gio::Settings::create("com.tjdeveng.keeptower");
        } catch (const Glib::Error& e) {
            GTEST_SKIP() << "Could not create settings: " << e.what();
        }

        // Keep test isolated from user-specific persisted settings.
        m_settings->reset("backup-enabled");
        m_settings->reset("backup-count");
        m_settings->reset("backup-path");

        m_vault_manager = std::make_unique<VaultManager>();

        const auto nonce = static_cast<long long>(
            std::chrono::steady_clock::now().time_since_epoch().count());
        const std::filesystem::path temp_dir = std::filesystem::temp_directory_path();
        m_vault_path = (temp_dir / ("test_preferences_presenter_" + std::to_string(nonce) + ".vault")).string();

        const Glib::ustring username = "admin";
        const Glib::ustring password = "TestPassword123!";

        KeepTower::VaultSecurityPolicy policy;
        policy.require_yubikey = false;
        policy.min_password_length = 12;
        policy.pbkdf2_iterations = 100000;
        policy.password_history_depth = 0;

        std::filesystem::remove(m_vault_path);

        auto create_result = m_vault_manager->create_vault_v2(m_vault_path, username, password, policy);
        ASSERT_TRUE(create_result) << "Failed to create test vault";
        ASSERT_TRUE(m_vault_manager->close_vault()) << "Failed to close test vault after create";

        auto open_result = m_vault_manager->open_vault_v2(m_vault_path, username, password);
        ASSERT_TRUE(open_result) << "Failed to open test vault";

        m_presenter = std::make_unique<KeepTower::Ui::PreferencesPresenter>(m_vault_manager.get());
        ASSERT_TRUE(m_presenter->has_settings());
    }

    void TearDown() override {
        if (m_settings) {
            m_settings->reset("backup-enabled");
            m_settings->reset("backup-count");
            m_settings->reset("backup-path");
        }

        if (m_vault_manager) {
            (void)m_vault_manager->close_vault();
        }

        if (!m_vault_path.empty()) {
            std::filesystem::remove(m_vault_path);
            std::filesystem::remove(m_vault_path + ".backup");
        }
    }

    Glib::RefPtr<Gio::Settings> m_settings;
    std::unique_ptr<VaultManager> m_vault_manager;
    std::unique_ptr<KeepTower::Ui::PreferencesPresenter> m_presenter;
    std::string m_vault_path;
};

TEST_F(PreferencesPresenterTest, SaveWhileVaultOpenPreservesAppDefaultsForBackupEnabledAndCount) {
    m_settings->set_boolean("backup-enabled", true);
    m_settings->set_int("backup-count", 9);
    m_settings->set_string("backup-path", "/tmp/app-default-backups");

    const VaultManager::BackupSettings initial_vault_settings{true, 8, "/tmp/initial-vault-backups"};
    ASSERT_TRUE(m_vault_manager->apply_backup_settings(initial_vault_settings));

    KeepTower::Ui::PreferencesModel model = m_presenter->load();
    model.backup_enabled = false;
    model.backup_count = 3;
    model.backup_path = "/tmp/vault-specific-backups";

    m_presenter->save(model);

    // Enabled/count remain app-scoped defaults while a vault is open.
    EXPECT_TRUE(m_settings->get_boolean("backup-enabled"));
    EXPECT_EQ(m_settings->get_int("backup-count"), 9);

    // Path is mirrored to runtime defaults for file chooser convenience.
    EXPECT_EQ(m_settings->get_string("backup-path"), "/tmp/vault-specific-backups");

    const VaultManager::BackupSettings updated_vault_settings = m_vault_manager->get_backup_settings();
    EXPECT_FALSE(updated_vault_settings.enabled);
    EXPECT_EQ(updated_vault_settings.count, 3);
    EXPECT_EQ(updated_vault_settings.path, "/tmp/vault-specific-backups");
}

TEST_F(PreferencesPresenterTest, SaveWithoutOpenVaultPersistsBackupDefaultsAndOnlyUpdatesRuntimePath) {
    m_settings->set_boolean("backup-enabled", false);
    m_settings->set_int("backup-count", 4);
    m_settings->set_string("backup-path", "/tmp/old-app-default-backups");

    const VaultManager::BackupSettings initial_runtime_settings{true, 8, "/tmp/runtime-old-path"};
    ASSERT_TRUE(m_vault_manager->apply_backup_settings(initial_runtime_settings));
    ASSERT_TRUE(m_vault_manager->close_vault());

    KeepTower::Ui::PreferencesModel model = m_presenter->load();
    model.backup_enabled = true;
    model.backup_count = 11;
    model.backup_path = "/tmp/new-app-default-backups";

    m_presenter->save(model);

    // Without an open vault, defaults are persisted to app settings.
    EXPECT_TRUE(m_settings->get_boolean("backup-enabled"));
    EXPECT_EQ(m_settings->get_int("backup-count"), 11);
    EXPECT_EQ(m_settings->get_string("backup-path"), "/tmp/new-app-default-backups");

    // Runtime manager state should only have backup path synchronized.
    const VaultManager::BackupSettings updated_runtime_settings = m_vault_manager->get_backup_settings();
    EXPECT_TRUE(updated_runtime_settings.enabled);
    EXPECT_EQ(updated_runtime_settings.count, 8);
    EXPECT_EQ(updated_runtime_settings.path, "/tmp/new-app-default-backups");
}

}  // namespace
