// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#include "PreferencesPresenter.h"

#include "../../../core/VaultManager.h"
#include "../../../utils/SettingsValidator.h"
#include "../../../utils/Log.h"

#include <giomm/settingsschemasource.h>

#include <algorithm>

namespace KeepTower::Ui {

namespace {
constexpr int MIN_REDUNDANCY = 5;
constexpr int MAX_REDUNDANCY = 50;

constexpr int MIN_BACKUP_COUNT = 1;
constexpr int MAX_BACKUP_COUNT = 50;

constexpr int MIN_CLIPBOARD_TIMEOUT = 5;
constexpr int MAX_CLIPBOARD_TIMEOUT = 300;

constexpr int MIN_AUTO_LOCK_TIMEOUT = 60;
constexpr int MAX_AUTO_LOCK_TIMEOUT = 3600;

constexpr int MIN_PASSWORD_HISTORY_LIMIT = 0;
constexpr int MAX_PASSWORD_HISTORY_LIMIT = 24;

} // namespace

PreferencesPresenter::PreferencesPresenter(VaultManager* vault_manager)
    : m_vault_manager(vault_manager),
      m_settings(try_create_settings()) {}

Glib::RefPtr<Gio::Settings> PreferencesPresenter::try_create_settings() {
    try {
        auto schema_source = Gio::SettingsSchemaSource::get_default();
        if (!schema_source) {
            KeepTower::Log::warning("GSettings schema source unavailable - settings persistence disabled");
            return {};
        }

        auto schema = schema_source->lookup("com.tjdeveng.keeptower", false);
        if (!schema) {
            KeepTower::Log::warning("GSettings schema not found - settings persistence disabled");
            return {};
        }

        return Gio::Settings::create("com.tjdeveng.keeptower");
    } catch (const Glib::Error& e) {
        KeepTower::Log::warning("Failed to load settings: {} - settings persistence disabled", e.what());
        return {};
    }
}

PreferencesModel PreferencesPresenter::load() const {
    PreferencesModel model;

    if (!m_settings) {
        return model;
    }

    model.vault_open = m_vault_manager && m_vault_manager->is_vault_open();
    model.fips_available = VaultManager::is_fips_available();

    if (model.vault_open) {
        const auto session = m_vault_manager->get_current_user_session();
        model.vault_admin = session && session->role == KeepTower::UserRole::ADMINISTRATOR;
    }

    // Appearance
    model.color_scheme = m_settings->get_string("color-scheme");
    if (model.color_scheme != "light" && model.color_scheme != "dark") {
        model.color_scheme = "default";
    }

    // Storage (FEC)
    if (model.vault_open) {
        model.rs_enabled = m_vault_manager->is_reed_solomon_enabled();
        model.rs_redundancy_percent = m_vault_manager->get_rs_redundancy_percent();
        model.apply_to_current_vault_fec = true;
    } else {
        model.rs_enabled = m_settings->get_boolean("use-reed-solomon");
        model.rs_redundancy_percent = m_settings->get_int("rs-redundancy-percent");
        model.apply_to_current_vault_fec = false;
    }

    model.rs_redundancy_percent = std::clamp(model.rs_redundancy_percent, MIN_REDUNDANCY, MAX_REDUNDANCY);

    // Backups
    if (model.vault_open) {
        const VaultManager::BackupSettings backup_settings = m_vault_manager->get_backup_settings();
        model.backup_enabled = backup_settings.enabled;
        model.backup_count = std::clamp(backup_settings.count, MIN_BACKUP_COUNT, MAX_BACKUP_COUNT);
        model.backup_path = backup_settings.path;
    } else {
        model.backup_enabled = m_settings->get_boolean("backup-enabled");
        model.backup_count = std::clamp(m_settings->get_int("backup-count"), MIN_BACKUP_COUNT, MAX_BACKUP_COUNT);
        model.backup_path = m_settings->get_string("backup-path");
    }

    // Security
    if (model.vault_open) {
        model.clipboard_timeout_seconds = m_vault_manager->get_clipboard_timeout();
        model.auto_lock_enabled = m_vault_manager->get_auto_lock_enabled();
        model.auto_lock_timeout_seconds = m_vault_manager->get_auto_lock_timeout();

        if (model.clipboard_timeout_seconds == 0) {
            model.clipboard_timeout_seconds = SettingsValidator::get_clipboard_timeout(m_settings);
        }
        if (model.auto_lock_timeout_seconds == 0) {
            model.auto_lock_timeout_seconds = SettingsValidator::get_auto_lock_timeout(m_settings);
        }

        model.account_password_history_enabled = m_vault_manager->get_account_password_history_enabled();
        model.account_password_history_limit = m_vault_manager->get_account_password_history_limit();
        if (model.account_password_history_limit == 0) {
            model.account_password_history_enabled = SettingsValidator::is_password_history_enabled(m_settings);
            model.account_password_history_limit = SettingsValidator::get_password_history_limit(m_settings);
        }

        model.undo_redo_enabled = m_vault_manager->get_undo_redo_enabled();
        model.undo_history_limit = m_vault_manager->get_undo_history_limit();
        if (model.undo_history_limit == 0) {
            model.undo_redo_enabled = m_settings->get_boolean("undo-redo-enabled");
            model.undo_history_limit = m_settings->get_int("undo-history-limit");
        }
    } else {
        model.clipboard_timeout_seconds = SettingsValidator::get_clipboard_timeout(m_settings);
        model.auto_lock_enabled = SettingsValidator::is_auto_lock_enabled(m_settings);
        model.auto_lock_timeout_seconds = SettingsValidator::get_auto_lock_timeout(m_settings);

        model.account_password_history_enabled = SettingsValidator::is_password_history_enabled(m_settings);
        model.account_password_history_limit = SettingsValidator::get_password_history_limit(m_settings);

        model.undo_redo_enabled = m_settings->get_boolean("undo-redo-enabled");
        model.undo_history_limit = m_settings->get_int("undo-history-limit");
    }

    model.clipboard_timeout_seconds = std::clamp(model.clipboard_timeout_seconds, MIN_CLIPBOARD_TIMEOUT, MAX_CLIPBOARD_TIMEOUT);
    model.auto_lock_timeout_seconds = std::clamp(model.auto_lock_timeout_seconds, MIN_AUTO_LOCK_TIMEOUT, MAX_AUTO_LOCK_TIMEOUT);
    model.account_password_history_limit = std::clamp(model.account_password_history_limit, MIN_PASSWORD_HISTORY_LIMIT, MAX_PASSWORD_HISTORY_LIMIT);
    model.undo_history_limit = std::clamp(model.undo_history_limit, 1, 100);

    // Default vault user history depth (only relevant when no vault open)
    model.vault_user_password_history_depth_default = std::clamp(
        m_settings->get_int("vault-user-password-history-depth"), 0, 24);

    // FIPS preference
    try {
        model.fips_mode_enabled = m_settings->get_boolean("fips-mode-enabled");
    } catch (const Glib::Error& e) {
        KeepTower::Log::warning("Failed to read fips-mode-enabled: {}", e.what());
        model.fips_mode_enabled = false;
    }

    // Username hash algorithm + params
    model.username_hash_algorithm = m_settings->get_string("username-hash-algorithm");
    if (model.username_hash_algorithm.empty()) {
        model.username_hash_algorithm = "sha3-256";
    }

    if (model.fips_mode_enabled && (model.username_hash_algorithm == "plaintext" || model.username_hash_algorithm == "argon2id")) {
        KeepTower::Log::info("FIPS mode enabled: Overriding saved algorithm {} with SHA3-256", model.username_hash_algorithm);
        model.username_hash_algorithm = "sha3-256";
    }

    model.username_pbkdf2_iterations = m_settings->get_uint("username-pbkdf2-iterations");
    model.username_argon2_memory_mb = m_settings->get_uint("username-argon2-memory-kb") / 1024;
    model.username_argon2_iterations = m_settings->get_uint("username-argon2-iterations");

    return model;
}

void PreferencesPresenter::save(const PreferencesModel& model) const {
    if (!m_settings) {
        return;
    }

    // Appearance
    m_settings->set_string("color-scheme", model.color_scheme);

    // Storage (FEC)
    const bool vault_open = m_vault_manager && m_vault_manager->is_vault_open();

    const int redundancy = std::clamp(model.rs_redundancy_percent, MIN_REDUNDANCY, MAX_REDUNDANCY);
    if (vault_open && model.apply_to_current_vault_fec) {
        m_vault_manager->set_reed_solomon_enabled(model.rs_enabled);
        m_vault_manager->set_rs_redundancy_percent(redundancy);
    } else {
        m_settings->set_boolean("use-reed-solomon", model.rs_enabled);
        m_settings->set_int("rs-redundancy-percent", redundancy);
    }

    // Backups
    const bool backup_enabled = model.backup_enabled;
    const int backup_count = std::clamp(model.backup_count, MIN_BACKUP_COUNT, MAX_BACKUP_COUNT);

    if (vault_open) {
        const VaultManager::BackupSettings backup_settings{backup_enabled, backup_count, model.backup_path};
        if (!m_vault_manager->apply_backup_settings(backup_settings)) {
            KeepTower::Log::warning("PreferencesPresenter: Ignoring invalid backup count in preferences dialog");
        }
    } else {
        m_settings->set_boolean("backup-enabled", backup_enabled);
        m_settings->set_int("backup-count", backup_count);
    }

    m_settings->set_string("backup-path", model.backup_path);
    if (m_vault_manager && !vault_open) {
        m_vault_manager->set_backup_path(model.backup_path);
    }

    // Clipboard timeout
    const int clipboard_timeout = std::clamp(model.clipboard_timeout_seconds, MIN_CLIPBOARD_TIMEOUT, MAX_CLIPBOARD_TIMEOUT);
    if (vault_open) {
        m_vault_manager->set_clipboard_timeout(clipboard_timeout);
    } else {
        m_settings->set_int("clipboard-clear-timeout", clipboard_timeout);
    }

    // Account password history
    const bool pwd_hist_enabled = model.account_password_history_enabled;
    const int pwd_hist_limit = std::clamp(model.account_password_history_limit, MIN_PASSWORD_HISTORY_LIMIT, MAX_PASSWORD_HISTORY_LIMIT);

    if (vault_open) {
        m_vault_manager->set_account_password_history_enabled(pwd_hist_enabled);
        m_vault_manager->set_account_password_history_limit(pwd_hist_limit);
    } else {
        m_settings->set_boolean("password-history-enabled", pwd_hist_enabled);
        m_settings->set_int("password-history-limit", pwd_hist_limit);
    }

    // Auto-lock
    const bool auto_lock_enabled = model.auto_lock_enabled;
    const int auto_lock_timeout = std::clamp(model.auto_lock_timeout_seconds, MIN_AUTO_LOCK_TIMEOUT, MAX_AUTO_LOCK_TIMEOUT);

    if (vault_open) {
        m_vault_manager->set_auto_lock_enabled(auto_lock_enabled);
        m_vault_manager->set_auto_lock_timeout(auto_lock_timeout);
    } else {
        m_settings->set_boolean("auto-lock-enabled", auto_lock_enabled);
        m_settings->set_int("auto-lock-timeout", auto_lock_timeout);
    }

    // Undo/redo
    const bool undo_redo_enabled = model.undo_redo_enabled;
    const int undo_history_limit = std::clamp(model.undo_history_limit, 1, 100);

    if (vault_open) {
        m_vault_manager->set_undo_redo_enabled(undo_redo_enabled);
        m_vault_manager->set_undo_history_limit(undo_history_limit);
    } else {
        m_settings->set_boolean("undo-redo-enabled", undo_redo_enabled);
        m_settings->set_int("undo-history-limit", undo_history_limit);
    }

    // Vault user password history default (new vaults only)
    if (!vault_open) {
        m_settings->set_int(
            "vault-user-password-history-depth",
            std::clamp(model.vault_user_password_history_depth_default, 0, 24));
    }

    // FIPS preference
    try {
        m_settings->set_boolean("fips-mode-enabled", model.fips_mode_enabled);
    } catch (const Glib::Error& e) {
        KeepTower::Log::warning("Failed to save fips-mode-enabled: {}", e.what());
    }

    // Username hashing algorithm + params
    if (!model.username_hash_algorithm.empty()) {
        m_settings->set_string("username-hash-algorithm", model.username_hash_algorithm);
    }

    m_settings->set_uint("username-pbkdf2-iterations", model.username_pbkdf2_iterations);
    m_settings->set_uint("username-argon2-memory-kb", model.username_argon2_memory_mb * 1024);
    m_settings->set_uint("username-argon2-iterations", model.username_argon2_iterations);

    if (vault_open) {
        [[maybe_unused]] const bool saved = m_vault_manager->save_vault(false);
    }
}

}  // namespace KeepTower::Ui
