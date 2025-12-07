// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#ifndef CREATEPASSWORDDIALOG_H
#define CREATEPASSWORDDIALOG_H

#include <gtkmm.h>

class CreatePasswordDialog : public Gtk::Dialog {
public:
    CreatePasswordDialog(Gtk::Window& parent);
    virtual ~CreatePasswordDialog();

    // Get the created password
    Glib::ustring get_password() const;

protected:
    // Signal handlers
    void on_show_password_toggled();
    void on_password_changed();
    void on_confirm_changed();
    void validate_passwords();

    // Password validation helpers
    bool validate_nist_requirements(const Glib::ustring& password);
    void update_strength_indicator();

    // Member widgets
    Gtk::Box m_content_box;
    Gtk::Label m_title_label;
    Gtk::Label m_requirements_label;

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

    Gtk::Button* m_ok_button;
    Gtk::Button* m_cancel_button;
};

#endif // CREATEPASSWORDDIALOG_H
