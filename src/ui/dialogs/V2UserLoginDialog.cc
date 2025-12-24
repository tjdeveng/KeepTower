// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "V2UserLoginDialog.h"
#include "../../utils/SecureMemory.h"

// Secure clear implementation for credentials
void V2LoginCredentials::clear() noexcept {
    // Clear password using OPENSSL_cleanse to prevent compiler optimization
    KeepTower::secure_clear_ustring(password);
    // Username is not sensitive, but clear for consistency
    username.clear();
}

V2UserLoginDialog::V2UserLoginDialog(Gtk::Window& parent, bool vault_requires_yubikey)
    : Gtk::Dialog("Vault Login", parent, true),
      m_vault_requires_yubikey(vault_requires_yubikey) {

    // Configure dialog properties
    set_default_size(450, 300);
    set_modal(true);
    set_resizable(false);

    // Add dialog buttons
    m_cancel_button = add_button("_Cancel", Gtk::ResponseType::CANCEL);
    m_ok_button = add_button("_Log In", Gtk::ResponseType::OK);
    m_ok_button->set_sensitive(false);  // Disabled until both fields filled
    m_ok_button->add_css_class("suggested-action");  // GTK4 primary button style

    // Configure main content box
    m_content_box.set_margin_start(24);
    m_content_box.set_margin_end(24);
    m_content_box.set_margin_top(24);
    m_content_box.set_margin_bottom(24);
    get_content_area()->append(m_content_box);

    // Title label with emphasis
    m_title_label.set_markup("<b>Enter your credentials to unlock the vault</b>");
    m_title_label.set_halign(Gtk::Align::START);
    m_title_label.set_margin_bottom(16);
    m_content_box.append(m_title_label);

    // YubiKey requirement info (only if vault requires it)
    if (m_vault_requires_yubikey) {
        m_yubikey_icon.set_from_icon_name("security-high-symbolic");
        m_yubikey_icon.set_icon_size(Gtk::IconSize::NORMAL);

        m_yubikey_info_label.set_markup(
            "<b>YubiKey Required:</b> "
            "Please insert your YubiKey after clicking Log In."
        );
        m_yubikey_info_label.add_css_class("info-text");
        m_yubikey_info_label.set_wrap(true);
        m_yubikey_info_label.set_halign(Gtk::Align::START);

        m_yubikey_box.append(m_yubikey_icon);
        m_yubikey_box.append(m_yubikey_info_label);
        m_yubikey_box.set_margin_bottom(16);
        m_content_box.append(m_yubikey_box);
    }

    // Username field
    m_username_label.set_halign(Gtk::Align::START);
    m_username_label.add_css_class("caption");
    m_username_box.append(m_username_label);

    m_username_entry.set_placeholder_text("Enter your username");
    m_username_entry.set_max_length(256);  // Match backend MAX_USERNAME_LENGTH
    m_username_entry.set_activates_default(false);  // Don't submit on Enter in username
    m_username_box.append(m_username_entry);
    m_username_box.set_margin_bottom(12);
    m_content_box.append(m_username_box);

    // Password field
    m_password_label.set_halign(Gtk::Align::START);
    m_password_label.add_css_class("caption");
    m_password_box.append(m_password_label);

    m_password_entry.set_visibility(false);  // Masked by default
    m_password_entry.set_input_purpose(Gtk::InputPurpose::PASSWORD);
    m_password_entry.set_placeholder_text("Enter your password");
    m_password_entry.set_max_length(512);  // Match backend MAX_PASSWORD_LENGTH
    m_password_entry.set_activates_default(true);  // Submit on Enter in password
    m_password_box.append(m_password_entry);
    m_password_box.set_margin_bottom(12);
    m_content_box.append(m_password_box);

    // Show password checkbox
    m_show_password_check.set_margin_bottom(8);
    m_content_box.append(m_show_password_check);

    // Connect signals
    m_show_password_check.signal_toggled().connect(
        sigc::mem_fun(*this, &V2UserLoginDialog::on_show_password_toggled)
    );

    m_username_entry.signal_changed().connect(
        sigc::mem_fun(*this, &V2UserLoginDialog::on_input_changed)
    );

    m_password_entry.signal_changed().connect(
        sigc::mem_fun(*this, &V2UserLoginDialog::on_input_changed)
    );

    // Set default widget and initial focus
    set_default_widget(*m_ok_button);
    m_username_entry.grab_focus();
}

V2UserLoginDialog::~V2UserLoginDialog() {
    // Securely clear password entry before destruction
    Glib::ustring password_text = m_password_entry.get_text();
    KeepTower::secure_clear_ustring(password_text);
    m_password_entry.set_text("");  // Clear entry widget
}

V2LoginCredentials V2UserLoginDialog::get_credentials() const {
    V2LoginCredentials creds;

    // Return Glib::ustring directly (preserves UTF-8 encoding properly)
    creds.username = m_username_entry.get_text();
    creds.password = m_password_entry.get_text();

    return creds;
}

void V2UserLoginDialog::set_username(std::string_view username) {
    m_username_entry.set_text(Glib::ustring(std::string(username)));
    m_password_entry.grab_focus();  // Move to password field
    on_input_changed();  // Update OK button state
}

void V2UserLoginDialog::on_show_password_toggled() {
    m_password_entry.set_visibility(m_show_password_check.get_active());
}

void V2UserLoginDialog::on_input_changed() {
    // Enable OK button only if both username and password are non-empty
    bool username_valid = !m_username_entry.get_text().empty();
    bool password_valid = !m_password_entry.get_text().empty();

    m_ok_button->set_sensitive(username_valid && password_valid);
}

void V2UserLoginDialog::on_response(int response_id) {
    // Don't clear password here - let the caller retrieve it first via get_credentials()
    // Password will be cleared by the caller using V2LoginCredentials::clear()

    // Call base class handler
    Gtk::Dialog::on_response(response_id);
}
