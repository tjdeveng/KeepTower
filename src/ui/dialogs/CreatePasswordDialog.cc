// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "CreatePasswordDialog.h"
#include "../../core/CommonPasswords.h"
#include <algorithm>
#include <cctype>

CreatePasswordDialog::CreatePasswordDialog(Gtk::Window& parent)
    : Gtk::Dialog("Create New Password", parent, true),
      m_content_box(Gtk::Orientation::VERTICAL, 12),
      m_title_label("Create a strong password for your new vault"),
      m_requirements_label(),
      m_password_box(Gtk::Orientation::VERTICAL, 6),
      m_password_label("Password:"),
      m_confirm_box(Gtk::Orientation::VERTICAL, 6),
      m_confirm_label("Confirm Password:"),
      m_show_password_check("Show passwords"),
      m_strength_label("Password Strength:"),
      m_yubikey_separator(Gtk::Orientation::HORIZONTAL),
      m_yubikey_check("Require YubiKey for vault access"),
      m_yubikey_info_label() {

    // Set dialog properties
    set_default_size(500, 400);
    set_modal(true);

    // Add buttons
    m_cancel_button = add_button("_Cancel", Gtk::ResponseType::CANCEL);
    m_ok_button = add_button("_Create", Gtk::ResponseType::OK);
    m_ok_button->set_sensitive(false);

    // Set up the content box
    m_content_box.set_margin(20);
    get_content_area()->append(m_content_box);

    // Configure title label
    m_title_label.set_wrap(true);
    m_title_label.set_xalign(0.0);
    auto title_attrs = Pango::AttrList();
    auto title_attr = Pango::Attribute::create_attr_weight(Pango::Weight::BOLD);
    title_attrs.insert(title_attr);
    m_title_label.set_attributes(title_attrs);

    // NIST SP 800-63B requirements
    Glib::ustring requirements_text =
        "Password Requirements (NIST SP 800-63B):\n"
        "• Minimum 8 characters (12+ recommended)\n"
        "• No composition rules (mix of character types not required)\n"
        "• Check against common/compromised passwords\n"
        "• No periodic password changes required\n"
        "• Unicode characters are allowed";

    m_requirements_label.set_text(requirements_text);
    m_requirements_label.set_wrap(true);
    m_requirements_label.set_xalign(0.0);
    m_requirements_label.set_margin_top(6);
    m_requirements_label.set_margin_bottom(12);

    // Configure password entries
    m_password_label.set_xalign(0.0);
    m_password_entry.set_visibility(false);
    m_password_entry.set_input_purpose(Gtk::InputPurpose::PASSWORD);
    m_password_entry.set_placeholder_text("Enter password");

    m_confirm_label.set_xalign(0.0);
    m_confirm_entry.set_visibility(false);
    m_confirm_entry.set_input_purpose(Gtk::InputPurpose::PASSWORD);
    m_confirm_entry.set_placeholder_text("Confirm password");
    m_confirm_entry.set_activates_default(true);

    // Add widgets to boxes
    m_password_box.append(m_password_label);
    m_password_box.append(m_password_entry);

    m_confirm_box.append(m_confirm_label);
    m_confirm_box.append(m_confirm_entry);

    // Configure strength indicator
    m_strength_label.set_xalign(0.0);
    m_strength_bar.set_show_text(false);
    m_strength_bar.set_fraction(0.0);

    // Configure validation message
    m_validation_message.set_wrap(true);
    m_validation_message.set_xalign(0.0);
    m_validation_message.set_margin_top(6);

    // Add all widgets to main content box
    m_content_box.append(m_title_label);
    m_content_box.append(m_requirements_label);
    m_content_box.append(m_password_box);
    m_content_box.append(m_confirm_box);
    m_content_box.append(m_show_password_check);
    m_content_box.append(m_strength_label);
    m_content_box.append(m_strength_bar);
    m_content_box.append(m_validation_message);

#ifdef HAVE_YUBIKEY_SUPPORT
    // Add YubiKey option section
    m_yubikey_separator.set_margin_top(12);
    m_yubikey_separator.set_margin_bottom(12);
    m_content_box.append(m_yubikey_separator);
    m_content_box.append(m_yubikey_check);

    // Configure YubiKey info label
    m_yubikey_info_label.set_text(
        "Two-factor protection: Vault will require both password AND YubiKey to open.\\n"
        "Make sure your YubiKey is connected and configured for HMAC-SHA1 challenge-response.");
    m_yubikey_info_label.set_wrap(true);
    m_yubikey_info_label.set_xalign(0.0);
    m_yubikey_info_label.set_margin_start(24);
    m_yubikey_info_label.set_margin_top(6);
    m_yubikey_info_label.set_margin_bottom(6);
    m_yubikey_info_label.set_visible(false);

    // Add subtle styling to info label
    auto css = Gtk::CssProvider::create();
    css->load_from_data("label { font-size: 0.9em; color: alpha(@theme_fg_color, 0.7); }");
    m_yubikey_info_label.get_style_context()->add_provider(
        css, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );

    m_content_box.append(m_yubikey_info_label);

    // Check if YubiKey is available
    YubiKeyManager yk_manager;
    if (!yk_manager.is_available()) {
        m_yubikey_check.set_sensitive(false);
        m_yubikey_check.set_tooltip_text("No YubiKey detected. Please connect your YubiKey.");
    }
#endif

    // Set margins
    m_password_box.set_margin_bottom(12);
    m_confirm_box.set_margin_bottom(12);
    m_show_password_check.set_margin_bottom(12);

    // Connect signals
    m_show_password_check.signal_toggled().connect(
        sigc::mem_fun(*this, &CreatePasswordDialog::on_show_password_toggled)
    );

    m_password_entry.signal_changed().connect(
        sigc::mem_fun(*this, &CreatePasswordDialog::on_password_changed)
    );

    m_confirm_entry.signal_changed().connect(
        sigc::mem_fun(*this, &CreatePasswordDialog::on_confirm_changed)
    );

#ifdef HAVE_YUBIKEY_SUPPORT
    m_yubikey_check.signal_toggled().connect(
        sigc::mem_fun(*this, &CreatePasswordDialog::on_yubikey_toggled)
    );
#endif

    // Set default widget
    set_default_widget(*m_ok_button);

    // Focus the password entry
    m_password_entry.grab_focus();
}

CreatePasswordDialog::~CreatePasswordDialog() {
}

Glib::ustring CreatePasswordDialog::get_password() const {
    return m_password_entry.get_text();
}

void CreatePasswordDialog::on_show_password_toggled() {
    bool show = m_show_password_check.get_active();
    m_password_entry.set_visibility(show);
    m_confirm_entry.set_visibility(show);
}

void CreatePasswordDialog::on_password_changed() {
    update_strength_indicator();
    validate_passwords();
}

void CreatePasswordDialog::on_confirm_changed() {
    validate_passwords();
}

void CreatePasswordDialog::validate_passwords() {
    Glib::ustring password = m_password_entry.get_text();
    Glib::ustring confirm = m_confirm_entry.get_text();

    bool is_valid = false;
    Glib::ustring message;

    if (password.empty()) {
        message = "Please enter a password";
    } else if (password.length() < 8) {
        message = "Password must be at least 8 characters";
    } else if (!validate_nist_requirements(password)) {
        message = "Password appears to be commonly used or weak";
    } else if (confirm.empty()) {
        message = "Please confirm your password";
    } else if (password != confirm) {
        message = "Passwords do not match";
    } else {
        message = "✓ Password meets requirements";
        is_valid = true;

        // Set message color to green
        auto css = Gtk::CssProvider::create();
        css->load_from_data("label { color: #26a269; }");
        m_validation_message.get_style_context()->add_provider(
            css, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
        );
    }

    if (!is_valid && !message.empty()) {
        // Set message color to red
        auto css = Gtk::CssProvider::create();
        css->load_from_data("label { color: #c01c28; }");
        m_validation_message.get_style_context()->add_provider(
            css, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
        );
    }

    m_validation_message.set_text(message);
    m_ok_button->set_sensitive(is_valid);
}

bool CreatePasswordDialog::validate_nist_requirements(const Glib::ustring& password) {
    // NIST SP 800-63B requirements:
    // 1. Minimum length check (already done in validate_passwords)
    // 2. Check against common passwords list using comprehensive database

    // Use the comprehensive common password list
    std::string password_str(password.raw());
    return !KeepTower::is_common_password(password_str);
}


void CreatePasswordDialog::update_strength_indicator() {
    Glib::ustring password = m_password_entry.get_text();

    if (password.empty()) {
        m_strength_bar.set_fraction(0.0);
        return;
    }

    // Simple strength calculation based on length and character diversity
    double strength = 0.0;
    size_t length = password.length();

    // Length component (40% of score)
    if (length >= 16) {
        strength += 0.4;
    } else if (length >= 12) {
        strength += 0.3;
    } else if (length >= 8) {
        strength += 0.2;
    } else {
        strength += 0.1;
    }

    // Character diversity component (60% of score)
    bool has_lower = false, has_upper = false, has_digit = false, has_special = false;

    for (auto c : password) {
        if (g_unichar_islower(c)) has_lower = true;
        else if (g_unichar_isupper(c)) has_upper = true;
        else if (g_unichar_isdigit(c)) has_digit = true;
        else has_special = true;
    }

    int diversity = (has_lower ? 1 : 0) + (has_upper ? 1 : 0) +
                   (has_digit ? 1 : 0) + (has_special ? 1 : 0);
    strength += diversity * 0.15;

    m_strength_bar.set_fraction(std::min(strength, 1.0));

    // Set colour based on strength
    auto css = Gtk::CssProvider::create();
    if (strength < 0.4) {
        css->load_from_data("progressbar progress { background-color: #c01c28; }");
    } else if (strength < 0.7) {
        css->load_from_data("progressbar progress { background-color: #f6d32d; }");
    } else {
        css->load_from_data("progressbar progress { background-color: #26a269; }");
    }
    m_strength_bar.get_style_context()->add_provider(
        css, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
}

bool CreatePasswordDialog::get_yubikey_enabled() const {
#ifdef HAVE_YUBIKEY_SUPPORT
    return m_yubikey_check.get_active();
#else
    return false;
#endif
}

void CreatePasswordDialog::on_yubikey_toggled() {
#ifdef HAVE_YUBIKEY_SUPPORT
    m_yubikey_info_label.set_visible(m_yubikey_check.get_active());
#endif
}
