// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#ifndef KEEPTOWER_UI_DIALOGS_PREFERENCES_VAULTSECURITYPREFERENCESPAGE_H
#define KEEPTOWER_UI_DIALOGS_PREFERENCES_VAULTSECURITYPREFERENCESPAGE_H

#include "PreferencesModel.h"

#include <gtkmm.h>

class VaultManager;

namespace KeepTower::Ui {

/**
 * @brief Preferences page for vault security settings.
 *
 * This page covers:
 * - Auto-lock behavior
 * - FIPS mode preference (persistent; typically takes effect after restart)
 * - Default policy for new vaults (password history depth)
 * - Key derivation algorithm selection for new vaults
 * - Vault-scoped information/actions when a vault is open
 */
class VaultSecurityPreferencesPage final : public Gtk::Box {
public:
    /**
     * @brief Construct the page widget.
     * @param vault_manager Optional vault manager (non-owning) for vault-scoped state and actions.
     */
    explicit VaultSecurityPreferencesPage(VaultManager* vault_manager);

    /**
     * @brief Populate widgets from the provided model.
     * @param model Preferences state to load.
     */
    void load_from_model(const PreferencesModel& model);

    /**
     * @brief Store current widget values into the provided model.
     * @param model Preferences state to update.
     */
    void store_to_model(PreferencesModel& model) const;

    /**
     * @brief Handler invoked when the preferences dialog is shown.
     *
     * Used to lazily initialize and refresh UI sections that depend on live vault state.
     */
    void on_dialog_shown();

    /** @brief Exposes the FIPS mode checkbox (primarily for tests). */
    [[nodiscard]] Gtk::CheckButton& fips_mode_check() noexcept { return m_fips_mode_check; }

    /** @brief Exposes the username hash algorithm combo box (primarily for tests). */
    [[nodiscard]] Gtk::ComboBoxText& username_hash_combo() noexcept { return m_username_hash_combo; }

    /** @brief Exposes PBKDF2 iterations spin button (may be null if not constructed). */
    [[nodiscard]] Gtk::SpinButton* pbkdf2_iterations_spin() noexcept { return m_pbkdf2_iterations_spin; }

    /** @brief Exposes Argon2 memory spin button (may be null if not constructed). */
    [[nodiscard]] Gtk::SpinButton* argon2_memory_spin() noexcept { return m_argon2_memory_spin; }

    /** @brief Exposes Argon2 time cost spin button (may be null if not constructed). */
    [[nodiscard]] Gtk::SpinButton* argon2_time_spin() noexcept { return m_argon2_time_spin; }

private:
    /** @brief Update auto-lock timeout sensitivity when toggled. */
    void on_auto_lock_enabled_toggled() noexcept;

    /** @brief Enforce constraints and refresh UI when username hash algorithm changes. */
    void on_username_hash_changed() noexcept;

    /** @brief Clear the current user's password history from the vault. */
    void on_clear_password_history_clicked() noexcept;

    /** @brief Update descriptive information for the selected algorithm. */
    void update_username_hash_info() noexcept;

    /** @brief Show/hide advanced parameter controls based on algorithm. */
    void update_username_hash_advanced_params() noexcept;

    /** @brief Update warning text for Argon2 parameter combinations. */
    void update_argon2_performance_warning() noexcept;

    /** @brief Adjust layout based on whether a vault is open. */
    void update_security_layout() noexcept;

    /** @brief Refresh current vault KEK/algorithm information labels. */
    void update_current_vault_kek_info() noexcept;

    /** @brief Refresh password history status/actions when a vault is open. */
    void update_vault_password_history_ui() noexcept;

    VaultManager* m_vault_manager;  ///< Non-owning pointer to vault manager (optional)
    bool m_history_ui_loaded = false;  ///< Lazy init guard for password history UI

    Gtk::CheckButton m_auto_lock_enabled_check;  ///< Enable/disable auto-lock
    Gtk::Box m_auto_lock_timeout_box;  ///< Auto-lock timeout row container
    Gtk::Label m_auto_lock_timeout_label;  ///< Auto-lock timeout label
    Gtk::SpinButton m_auto_lock_timeout_spin;  ///< Auto-lock timeout value
    Gtk::Label m_auto_lock_timeout_suffix;  ///< Units suffix label

    Gtk::CheckButton m_fips_mode_check;  ///< FIPS mode preference toggle
    Gtk::Label m_fips_status_label;  ///< FIPS availability/status label
    Gtk::Label m_fips_restart_warning;  ///< Restart-required warning

    // Default for new vaults
    Gtk::Box m_vault_password_history_default_box;  ///< Default history depth row container
    Gtk::Label m_vault_password_history_default_label;  ///< Default history depth label
    Gtk::SpinButton m_vault_password_history_default_spin;  ///< Default history depth value
    Gtk::Label m_vault_password_history_default_suffix;  ///< Units suffix label
    Gtk::Label m_vault_password_history_default_help;  ///< Help text

    // Username hashing
    Gtk::ComboBoxText m_username_hash_combo;  ///< Algorithm selector
    Gtk::Label m_username_hash_info;  ///< Algorithm description/warning text
    Gtk::Box* m_username_hash_advanced_box = nullptr;  ///< Container for advanced parameter controls
    Gtk::Box* m_pbkdf2_iterations_box = nullptr;  ///< Container for PBKDF2 iteration control
    Gtk::SpinButton* m_pbkdf2_iterations_spin = nullptr;  ///< PBKDF2 iterations value
    Gtk::Box* m_argon2_params_box = nullptr;  ///< Container for Argon2 parameter controls
    Gtk::SpinButton* m_argon2_memory_spin = nullptr;  ///< Argon2 memory (MB)
    Gtk::SpinButton* m_argon2_time_spin = nullptr;  ///< Argon2 iterations/time cost
    Gtk::Label* m_argon2_perf_warning = nullptr;  ///< Performance warning label

    // Two-column layout
    Gtk::Grid* m_security_grid = nullptr;  ///< Root grid layout container
    Gtk::Box* m_security_right_column = nullptr;  ///< Right column shown only when vault is open

    Gtk::Box* m_current_vault_kek_box = nullptr;  ///< Container for current vault KEK info
    Gtk::Label m_current_username_hash_label;  ///< Current username algorithm label
    Gtk::Label m_current_kek_label;  ///< Current password KEK algorithm label
    Gtk::Label m_current_kek_params_label;  ///< Current KEK parameters label

    Gtk::Box m_vault_password_history_box;  ///< Container for vault password history UI
    Gtk::Label m_vault_policy_label;  ///< Vault policy summary label
    Gtk::Label m_current_user_label;  ///< Current user label
    Gtk::Label m_history_count_label;  ///< History entry count label
    Gtk::Button m_clear_history_button;  ///< Clear history action
    Gtk::Label m_clear_history_warning;  ///< Destructive action warning

    bool m_fips_mode_enabled_pref = false;  ///< Cached preference value for validation behavior
    bool m_fips_available = false;  ///< Cached availability from model/vault

    /**
     * @brief Resize dialog to fit content when on-screen.
     *
     * Guarded to avoid resizing while the page is hidden inside a Gtk::Stack.
     */
    void resize_to_content() noexcept;
};

}  // namespace KeepTower::Ui

#endif
