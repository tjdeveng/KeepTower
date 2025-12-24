// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#ifndef CREATEPASSWORDDIALOG_H
#define CREATEPASSWORDDIALOG_H

#include <gtkmm.h>

#ifdef HAVE_YUBIKEY_SUPPORT
#include "../../core/YubiKeyManager.h"
#endif

class CreatePasswordDialog : public Gtk::Dialog {
public:
    CreatePasswordDialog(Gtk::Window& parent);
    virtual ~CreatePasswordDialog();

    // Get the created password
    Glib::ustring get_password() const;

    // Get the username
    Glib::ustring get_username() const;

    // Check if YubiKey protection is requested
    bool get_yubikey_enabled() const;

protected:
    // Signal handlers
    void on_show_password_toggled();
    void on_password_changed();
    void on_confirm_changed();
    void on_username_changed();
    void validate_passwords();
    void validate_all_fields();
    void on_yubikey_toggled();

    // Password validation helpers
    bool validate_nist_requirements(const Glib::ustring& password);
    void update_strength_indicator();

    // Member widgets
    Gtk::Box m_content_box;
    Gtk::Label m_title_label;
    Gtk::Label m_requirements_label;

    Gtk::Box m_username_box;
    Gtk::Label m_username_label;
    Gtk::Entry m_username_entry;
    Gtk::Label m_username_error_label;

    Gtk::Box m_password_box;
    Gtk::Label m_password_label;
    Gtk::Entry m_password_entry;

    Gtk::Box m_confirm_box;
    Gtk::Label m_confirm_label;
    Gtk::Entry m_confirm_entry;

    Gtk::CheckButton m_show_password_check;
    Gtk::Label m_strength_label;
    Gtk::ProgressBar m_strength_bar;
    Gtk::Label m_validation_message;

    // YubiKey widgets
    Gtk::Separator m_yubikey_separator;
    Gtk::CheckButton m_yubikey_check;
    Gtk::Label m_yubikey_info_label;

    Gtk::Button* m_ok_button;
    Gtk::Button* m_cancel_button;
};

#endif // CREATEPASSWORDDIALOG_H
