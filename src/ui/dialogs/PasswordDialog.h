// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file PasswordDialog.h
 * @brief Simple password entry dialog for vault authentication
 *
 * Provides a minimal password entry interface for opening existing vaults.
 * Supports optional password visibility toggle for user convenience.
 *
 * @section usage Usage Example
 * @code
 * PasswordDialog dialog(main_window);
 * int response = dialog.run();
 * if (response == Gtk::ResponseType::OK) {
 *     Glib::ustring password = dialog.get_password();
 *     // ... attempt to open vault ...
 * }
 * @endcode
 */

#ifndef PASSWORDDIALOG_H
#define PASSWORDDIALOG_H

#include <gtkmm.h>

/**
 * @class PasswordDialog
 * @brief Dialog for entering vault passwords
 *
 * Simple, focused password entry dialog used when opening existing vaults.
 * Does not perform validation beyond checking for empty input.
 */
class PasswordDialog : public Gtk::Dialog {
public:
    /**
     * @brief Construct password entry dialog
     * @param parent Parent window for modal display
     */
    PasswordDialog(Gtk::Window& parent);

    /**
     * @brief Destructor - securely clears password from memory
     */
    virtual ~PasswordDialog();

    /**
     * @brief Get the entered password
     * @return User-entered password
     * @note Only call after dialog returns Gtk::ResponseType::OK
     */
    Glib::ustring get_password() const;

protected:
    /** @brief Handle show/hide password toggle */
    void on_show_password_toggled();

    /** @brief Handle password entry changes - enables OK button when non-empty */
    void on_password_changed();

    Gtk::Box m_content_box;               ///< Main content container
    Gtk::Label m_label;                   ///< Prompt label
    Gtk::Entry m_password_entry;          ///< Password text entry (masked by default)
    Gtk::CheckButton m_show_password_check; ///< Show/hide password toggle

    Gtk::Button* m_ok_button;             ///< OK button
    Gtk::Button* m_cancel_button;         ///< Cancel button
};

#endif // PASSWORDDIALOG_H
