// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#pragma once

#include <gtkmm.h>
#include <string>

/**
 * @brief Dialog for migrating V1 vaults to V2 multi-user format
 *
 * This dialog guides the user through converting a legacy single-user vault (V1)
 * to the modern multi-user vault format (V2) with role-based access control.
 *
 * Migration process:
 * 1. User confirms vault migration (warns about V1 compatibility)
 * 2. User creates admin username/password
 * 3. Optionally adjusts security policy (min password length, iterations)
 * 4. Vault is converted in-place (with automatic backup)
 *
 * Phase 8: V1 → V2 Migration UI
 *
 * @note Migration is destructive - V1 clients cannot open migrated vaults
 * @note Automatic backup created before migration
 * @note All existing accounts preserved during migration
 */
class VaultMigrationDialog : public Gtk::Dialog {
public:
    /**
     * @brief Construct migration dialog
     * @param parent Parent window for modal behavior
     * @param vault_path Path to V1 vault being migrated
     */
    explicit VaultMigrationDialog(Gtk::Window& parent, const std::string& vault_path);
    ~VaultMigrationDialog() override = default;

    // Prevent copying
    VaultMigrationDialog(const VaultMigrationDialog&) = delete;
    VaultMigrationDialog& operator=(const VaultMigrationDialog&) = delete;

    /**
     * @brief Get admin username entered by user
     * @return Admin username as UTF-8 string
     */
    [[nodiscard]] Glib::ustring get_admin_username() const;

    /**
     * @brief Get admin password entered by user
     * @return Admin password as UTF-8 string
     * @note Password should be securely cleared after use
     */
    [[nodiscard]] Glib::ustring get_admin_password() const;

    /**
     * @brief Get minimum password length policy
     * @return Minimum password length (characters)
     */
    [[nodiscard]] uint32_t get_min_password_length() const;

    /**
     * @brief Get PBKDF2 iteration count policy
     * @return Number of PBKDF2 iterations for key derivation
     */
    [[nodiscard]] uint32_t get_pbkdf2_iterations() const;

private:
    // UI Layout
    Gtk::Box m_content_box{Gtk::Orientation::VERTICAL, 12};

    // Warning section
    Gtk::Box m_warning_box{Gtk::Orientation::HORIZONTAL, 12};
    Gtk::Image m_warning_icon;
    Gtk::Label m_warning_label;

    // Information section
    Gtk::Label m_info_label;
    Gtk::Label m_vault_path_label;

    // Admin account section
    Gtk::Frame m_admin_frame;
    Gtk::Box m_admin_box{Gtk::Orientation::VERTICAL, 6};
    Gtk::Label m_admin_title;

    Gtk::Box m_username_box{Gtk::Orientation::HORIZONTAL, 6};
    Gtk::Label m_username_label;
    Gtk::Entry m_username_entry;

    Gtk::Box m_password_box{Gtk::Orientation::HORIZONTAL, 6};
    Gtk::Label m_password_label;
    Gtk::Entry m_password_entry;

    Gtk::Box m_confirm_box{Gtk::Orientation::HORIZONTAL, 6};
    Gtk::Label m_confirm_label;
    Gtk::Entry m_confirm_entry;

    // Password strength indicator
    Gtk::Label m_strength_label;

    // Security policy section (advanced)
    Gtk::Expander m_policy_expander;
    Gtk::Box m_policy_box{Gtk::Orientation::VERTICAL, 6};

    Gtk::Box m_min_length_box{Gtk::Orientation::HORIZONTAL, 6};
    Gtk::Label m_min_length_label;
    Gtk::SpinButton m_min_length_spin;

    Gtk::Box m_iterations_box{Gtk::Orientation::HORIZONTAL, 6};
    Gtk::Label m_iterations_label;
    Gtk::SpinButton m_iterations_spin;

    // Action buttons (in dialog action area)
    Gtk::Button* m_migrate_button;
    Gtk::Button* m_cancel_button;

    // State
    std::string m_vault_path;

    // Callbacks
    void on_password_changed();
    void on_confirm_changed();
    void on_username_changed();
    void validate_inputs();
    void update_password_strength();
};
