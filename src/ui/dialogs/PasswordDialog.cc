// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "PasswordDialog.h"
#include <cstdio>  // For debug printf

PasswordDialog::PasswordDialog(Gtk::Window& parent)
    : Gtk::Dialog("Enter Password", parent, true),
      m_content_box(Gtk::Orientation::VERTICAL, 12),
      m_label("Please enter your password to unlock the application:"),
      m_show_password_check("Show password") {

    // Set dialog properties
    set_default_size(400, 200);
    set_modal(true);

    // Add buttons
    m_cancel_button = add_button("_Cancel", Gtk::ResponseType::CANCEL);
    m_ok_button = add_button("_OK", Gtk::ResponseType::OK);
    m_ok_button->set_sensitive(false); // Disabled until password is entered

    // Set up the content box
    m_content_box.set_margin(20);
    get_content_area()->append(m_content_box);

    // Configure password entry
    m_password_entry.set_visibility(false);
    m_password_entry.set_input_purpose(Gtk::InputPurpose::PASSWORD);
    m_password_entry.set_placeholder_text("Enter password");
    m_password_entry.set_activates_default(true);

    // Add widgets to content box
    m_content_box.append(m_label);
    m_content_box.append(m_password_entry);
    m_content_box.append(m_show_password_check);

    // Set margins
    m_label.set_margin_bottom(12);
    m_password_entry.set_margin_bottom(12);

    // Connect signals
    m_show_password_check.signal_toggled().connect(
        sigc::mem_fun(*this, &PasswordDialog::on_show_password_toggled)
    );

    m_password_entry.signal_changed().connect(
        sigc::mem_fun(*this, &PasswordDialog::on_password_changed)
    );

    // Set OK button as default
    set_default_widget(*m_ok_button);

    // Focus the password entry
    m_password_entry.grab_focus();
}

PasswordDialog::~PasswordDialog() {
}

Glib::ustring PasswordDialog::get_password() const {
    auto password = m_password_entry.get_text();
    std::printf("[DEBUG] PasswordDialog::get_password() - length: %zu chars, %zu bytes\n",
                password.length(), password.bytes());
    std::fflush(stdout);
    return password;
}

void PasswordDialog::on_show_password_toggled() {
    m_password_entry.set_visibility(m_show_password_check.get_active());
}

void PasswordDialog::on_password_changed() {
    // Enable OK button only if password is not empty
    bool has_text = !m_password_entry.get_text().empty();
    m_ok_button->set_sensitive(has_text);
}
