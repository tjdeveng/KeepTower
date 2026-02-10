// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#ifndef KEEPTOWER_UI_DIALOGS_PREFERENCES_STORAGEPREFERENCESPAGE_H
#define KEEPTOWER_UI_DIALOGS_PREFERENCES_STORAGEPREFERENCESPAGE_H

#include "PreferencesModel.h"

#include <gtkmm.h>
#include <giomm/settings.h>

class VaultManager;

namespace KeepTower::Ui {

/**
 * @brief Preferences page for storage-related settings.
 *
 * Includes Reed-Solomon error correction settings and automatic backup options.
 * When a vault is open, some controls can apply to the current vault.
 */
class StoragePreferencesPage final : public Gtk::Box {
public:
    /**
     * @brief Construct the page widget.
     * @param vault_manager Optional vault manager (non-owning) for vault-scoped operations.
     * @param settings Application settings used when no vault is open.
     */
    StoragePreferencesPage(VaultManager* vault_manager, Glib::RefPtr<Gio::Settings> settings);

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

private:
    /** @brief Toggle whether storage settings apply to current vault vs defaults. */
    void on_apply_to_current_toggled() noexcept;

    /** @brief Update sensitivity of Reed-Solomon controls based on enable toggle. */
    void on_rs_enabled_toggled() noexcept;

    /** @brief Update sensitivity of backup controls based on enable toggle. */
    void on_backup_enabled_toggled() noexcept;

    /** @brief Open a file dialog to choose the backup directory. */
    void on_backup_path_browse();

    /** @brief Restore a vault from the most recent backup for a selected vault file. */
    void on_restore_backup();

    Gtk::Label* m_info_label = nullptr;  ///< Informational note (vault-scoped vs defaults)

    VaultManager* m_vault_manager = nullptr;  ///< Non-owning pointer to vault manager (optional)
    Glib::RefPtr<Gio::Settings> m_settings;  ///< Settings backend for defaults when no vault is open

    // Reed-Solomon
    Gtk::Label m_rs_section_title;  ///< Section title label
    Gtk::Label m_rs_description;  ///< Section description label
    Gtk::CheckButton m_rs_enabled_check;  ///< Enable/disable Reed-Solomon for new vaults
    Gtk::Box m_redundancy_box;  ///< Redundancy row container
    Gtk::Label m_redundancy_label;  ///< Redundancy label
    Gtk::SpinButton m_redundancy_spin;  ///< Redundancy percent value
    Gtk::Label m_redundancy_suffix;  ///< Units suffix label
    Gtk::Label m_redundancy_help;  ///< Help text
    Gtk::CheckButton m_apply_to_current_check;  ///< Apply-to-current-vault toggle (only when vault open)

    // Backups
    Gtk::Label m_backup_section_title;  ///< Section title label
    Gtk::Label m_backup_description;  ///< Section description label
    Gtk::CheckButton m_backup_enabled_check;  ///< Enable/disable backups
    Gtk::Box m_backup_count_box;  ///< Backup count row container
    Gtk::Label m_backup_count_label;  ///< Backup count label
    Gtk::SpinButton m_backup_count_spin;  ///< Number of backups to keep
    Gtk::Label m_backup_count_suffix;  ///< Units suffix label
    Gtk::Label m_backup_help;  ///< Help text
    Gtk::Box m_backup_path_box;  ///< Backup directory row container
    Gtk::Label m_backup_path_label;  ///< Backup directory label
    Gtk::Entry m_backup_path_entry;  ///< Backup directory path entry
    Gtk::Button m_backup_path_browse_button;  ///< Browse button for backup directory
    Gtk::Button m_restore_backup_button;  ///< Restore from backup action
};

}  // namespace KeepTower::Ui

#endif
