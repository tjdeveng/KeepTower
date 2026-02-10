// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#ifndef KEEPTOWER_UI_DIALOGS_PREFERENCES_ACCOUNTSECURITYPREFERENCESPAGE_H
#define KEEPTOWER_UI_DIALOGS_PREFERENCES_ACCOUNTSECURITYPREFERENCESPAGE_H

#include "PreferencesModel.h"

#include <gtkmm.h>

namespace KeepTower::Ui {

/**
 * @brief Preferences page for account and security-related settings.
 *
 * This page includes clipboard protection options, account password history
 * controls, and undo/redo preferences.
 */
class AccountSecurityPreferencesPage final : public Gtk::Box {
public:
    /** @brief Construct the page widget. */
    AccountSecurityPreferencesPage();

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
    /** @brief Update sensitivity of password history controls when toggled. */
    void on_account_password_history_toggled() noexcept;

    /** @brief Update sensitivity of undo history controls when toggled. */
    void on_undo_redo_enabled_toggled() noexcept;

    Gtk::Label* m_info_label = nullptr;  ///< Informational note (vault-scoped vs defaults)

    Gtk::Box m_clipboard_timeout_box;  ///< Clipboard timeout row container
    Gtk::Label m_clipboard_timeout_label;  ///< Clipboard timeout label
    Gtk::SpinButton m_clipboard_timeout_spin;  ///< Clipboard timeout value
    Gtk::Label m_clipboard_timeout_suffix;  ///< Units suffix label

    Gtk::CheckButton m_account_password_history_check;  ///< Enable/disable password reuse prevention
    Gtk::Box m_account_password_history_limit_box;  ///< Password history limit row container
    Gtk::Label m_account_password_history_limit_label;  ///< Password history limit label
    Gtk::SpinButton m_account_password_history_limit_spin;  ///< Password history limit value
    Gtk::Label m_account_password_history_limit_suffix;  ///< Units suffix label

    Gtk::CheckButton m_undo_redo_enabled_check;  ///< Enable/disable undo/redo
    Gtk::Box m_undo_history_limit_box;  ///< Undo history limit row container
    Gtk::Label m_undo_history_limit_label;  ///< Undo history limit label
    Gtk::SpinButton m_undo_history_limit_spin;  ///< Undo history limit value
    Gtk::Label m_undo_history_limit_suffix;  ///< Units suffix label
    Gtk::Label m_undo_redo_warning;  ///< Warning about security implications
};

}  // namespace KeepTower::Ui

#endif
