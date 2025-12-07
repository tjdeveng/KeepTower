// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#ifndef PASSWORDDIALOG_H
#define PASSWORDDIALOG_H

#include <gtkmm.h>

class PasswordDialog : public Gtk::Dialog {
public:
    PasswordDialog(Gtk::Window& parent);
    virtual ~PasswordDialog();

    // Get the entered password
    Glib::ustring get_password() const;

protected:
    // Signal handlers
    void on_show_password_toggled();
    void on_password_changed();

    // Member widgets
    Gtk::Box m_content_box;
    Gtk::Label m_label;
    Gtk::Entry m_password_entry;
    Gtk::CheckButton m_show_password_check;

    Gtk::Button* m_ok_button;
    Gtk::Button* m_cancel_button;
};

#endif // PASSWORDDIALOG_H
