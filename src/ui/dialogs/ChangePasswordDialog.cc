// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "ChangePasswordDialog.h"
#include "../../utils/SecureMemory.h"
#include <cstdio>   // For printf debugging

// Secure clear implementation for password change request
void PasswordChangeRequest::clear() noexcept {
    // Clear passwords using OPENSSL_cleanse to prevent compiler optimization
    KeepTower::secure_clear_ustring(current_password);
    KeepTower::secure_clear_ustring(new_password);
    if (!yubikey_pin.empty()) {
        OPENSSL_cleanse(yubikey_pin.data(), yubikey_pin.size());
        yubikey_pin.clear();
    }
}

ChangePasswordDialog::ChangePasswordDialog(
    Gtk::Window& parent,
    uint32_t min_password_length,
    bool is_forced_change
)
    : Gtk::Dialog("Change Password", parent, true),
      m_min_password_length(min_password_length),
      m_is_forced_change(is_forced_change) {

    // Configure dialog properties
    set_default_size(500, is_forced_change ? 450 : 400);
    set_modal(true);
    set_resizable(false);

    // Add dialog buttons
    m_cancel_button = add_button("_Cancel", Gtk::ResponseType::CANCEL);
    m_ok_button = add_button("_Change Password", Gtk::ResponseType::OK);
    m_ok_button->set_sensitive(false);  // Disabled until validation passes
    m_ok_button->add_css_class("suggested-action");

    // Configure main content box
    m_content_box.set_margin_start(24);
    m_content_box.set_margin_end(24);
    m_content_box.set_margin_top(24);
    m_content_box.set_margin_bottom(24);
    get_content_area()->append(m_content_box);

    // Title label
    if (m_is_forced_change) {
        m_title_label.set_markup(
            "<b>First Login: Change Your Password</b>\n"
            "<span size='small'>You are using a temporary password. "
            "Please create your own secure password to continue.</span>"
        );
    } else {
        m_title_label.set_markup("<b>Change Your Password</b>");
    }
    m_title_label.set_halign(Gtk::Align::START);
    m_title_label.set_margin_bottom(16);
    m_content_box.append(m_title_label);

    // Warning message for forced password change
    if (m_is_forced_change) {
        m_warning_icon.set_from_icon_name("dialog-warning-symbolic");
        m_warning_icon.set_icon_size(Gtk::IconSize::NORMAL);

        m_warning_label.set_markup(
            "<b>Security Notice:</b> "
            "For your security, you must change the temporary password "
            "before accessing the vault."
        );
        m_warning_label.add_css_class("warning-text");
        m_warning_label.set_wrap(true);
        m_warning_label.set_halign(Gtk::Align::START);

        m_warning_box.append(m_warning_icon);
        m_warning_box.append(m_warning_label);
        m_warning_box.set_margin_bottom(16);
        m_content_box.append(m_warning_box);
    }

    // Current password field with eye button
    m_current_password_label.set_text(m_is_forced_change ? "Temporary Password:" : "Current Password:");
    m_current_password_label.set_halign(Gtk::Align::START);
    m_current_password_label.add_css_class("caption");
    m_current_password_box.append(m_current_password_label);

    // Current password entry with show/hide toggle button
    m_current_password_entry_box.set_spacing(6);
    m_current_password_entry.set_visibility(false);
    m_current_password_entry.set_input_purpose(Gtk::InputPurpose::PASSWORD);
    m_current_password_entry.set_placeholder_text(m_is_forced_change ? "Enter your temporary password" : "Enter current password");
    m_current_password_entry.set_max_length(512);
    m_current_password_entry.set_activates_default(false);
    m_current_password_entry.set_hexpand(true);
    m_current_password_entry_box.append(m_current_password_entry);

    // Eye icon toggle button
    m_current_password_show_button.set_tooltip_text("Show/hide passwords");
    m_current_password_show_button.add_css_class("flat");
    m_current_password_entry_box.append(m_current_password_show_button);

    m_current_password_box.append(m_current_password_entry_box);
    m_current_password_box.set_margin_bottom(12);
    m_content_box.append(m_current_password_box);

    // New password field with spacer to match current password field width
    m_new_password_label.set_halign(Gtk::Align::START);
    m_new_password_label.add_css_class("caption");
    m_new_password_box.append(m_new_password_label);

    // New password entry with spacer to match eye button width
    m_new_password_entry_box.set_spacing(6);
    m_new_password_entry.set_visibility(false);
    m_new_password_entry.set_input_purpose(Gtk::InputPurpose::PASSWORD);
    m_new_password_entry.set_placeholder_text(
        "Enter new password (min " + std::to_string(m_min_password_length) + " characters)"
    );
    m_new_password_entry.set_max_length(512);
    m_new_password_entry.set_activates_default(false);
    m_new_password_entry.set_hexpand(true);
    m_new_password_entry_box.append(m_new_password_entry);

    // Add spacer widget to match the width of the eye button
    auto* new_spacer = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 0);
    new_spacer->set_size_request(34, -1);  // Match eye button width
    m_new_password_entry_box.append(*new_spacer);

    m_new_password_box.append(m_new_password_entry_box);
    m_new_password_box.set_margin_bottom(4);
    m_content_box.append(m_new_password_box);

    // Password strength indicator
    m_strength_label.set_halign(Gtk::Align::START);
    m_strength_label.set_margin_bottom(8);
    m_content_box.append(m_strength_label);

    // Confirm password field with spacer to match new password field width
    m_confirm_password_label.set_halign(Gtk::Align::START);
    m_confirm_password_label.add_css_class("caption");
    m_confirm_password_box.append(m_confirm_password_label);

    m_confirm_password_entry_box.set_spacing(6);
    m_confirm_password_entry.set_visibility(false);
    m_confirm_password_entry.set_input_purpose(Gtk::InputPurpose::PASSWORD);
    m_confirm_password_entry.set_placeholder_text("Re-enter new password");
    m_confirm_password_entry.set_max_length(512);
    m_confirm_password_entry.set_activates_default(true);  // Submit on Enter
    m_confirm_password_entry.set_hexpand(true);
    m_confirm_password_entry_box.append(m_confirm_password_entry);

    // Add spacer widget to match the width of the eye button
    auto* confirm_spacer = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 0);
    confirm_spacer->set_size_request(34, -1);  // Match eye button width
    m_confirm_password_entry_box.append(*confirm_spacer);

    m_confirm_password_box.append(m_confirm_password_entry_box);
    m_confirm_password_box.set_margin_bottom(12);
    m_content_box.append(m_confirm_password_box);

    // Validation feedback label
    m_validation_label.set_halign(Gtk::Align::START);
    m_validation_label.set_wrap(true);
    m_validation_box.append(m_validation_label);
    m_validation_box.set_margin_bottom(12);
    m_content_box.append(m_validation_box);

#ifdef HAVE_YUBIKEY_SUPPORT
    // YubiKey PIN section (hidden by default, shown via set_yubikey_required())
    m_yubikey_separator.set_margin_top(8);
    m_yubikey_separator.set_margin_bottom(12);
    m_yubikey_separator.set_visible(false);
    m_content_box.append(m_yubikey_separator);

    m_yubikey_pin_box.set_spacing(6);
    m_yubikey_pin_box.set_margin_start(0);
    m_yubikey_pin_box.set_margin_bottom(12);
    m_yubikey_pin_box.set_visible(false);

    m_yubikey_pin_label.set_xalign(0.0);
    m_yubikey_pin_label.add_css_class("caption");
    m_yubikey_pin_box.append(m_yubikey_pin_label);

    // PIN entry with show/hide toggle button
    m_yubikey_pin_entry_box.set_spacing(6);
    m_yubikey_pin_entry.set_visibility(false);
    m_yubikey_pin_entry.set_input_purpose(Gtk::InputPurpose::PIN);
    m_yubikey_pin_entry.set_max_length(48);
    m_yubikey_pin_entry.set_placeholder_text("Enter your YubiKey PIN");
    m_yubikey_pin_entry.set_hexpand(true);
    m_yubikey_pin_entry_box.append(m_yubikey_pin_entry);

    // Eye icon toggle button
    m_yubikey_pin_show_button.set_tooltip_text("Show/hide PIN");
    m_yubikey_pin_show_button.add_css_class("flat");
    m_yubikey_pin_entry_box.append(m_yubikey_pin_show_button);

    m_yubikey_pin_box.append(m_yubikey_pin_entry_box);
    m_content_box.append(m_yubikey_pin_box);

    // Connect PIN show/hide toggle
    m_yubikey_pin_show_button.signal_toggled().connect([this]() {
        bool show = m_yubikey_pin_show_button.get_active();
        m_yubikey_pin_entry.set_visibility(show);
    });

    // Connect PIN change signal to validation
    m_yubikey_pin_entry.signal_changed().connect(
        sigc::mem_fun(*this, &ChangePasswordDialog::on_input_changed)
    );
#endif

    // Connect eye button toggle signal to show/hide all password fields
    m_current_password_show_button.signal_toggled().connect([this]() {
        bool show = m_current_password_show_button.get_active();
        m_current_password_entry.set_visibility(show);
        m_new_password_entry.set_visibility(show);
        m_confirm_password_entry.set_visibility(show);
    });

    // Connect signals
    if (!m_is_forced_change) {
        m_current_password_entry.signal_changed().connect(
            sigc::mem_fun(*this, &ChangePasswordDialog::on_input_changed)
        );
    }

    m_new_password_entry.signal_changed().connect(
        sigc::mem_fun(*this, &ChangePasswordDialog::on_input_changed)
    );

    m_confirm_password_entry.signal_changed().connect(
        sigc::mem_fun(*this, &ChangePasswordDialog::on_input_changed)
    );

    // Set default widget and initial focus
    set_default_widget(*m_ok_button);
    // Always focus current password field (temporary password in forced mode)
    m_current_password_entry.grab_focus();
}

ChangePasswordDialog::~ChangePasswordDialog() {
    // Securely clear all password entries
    secure_clear_entry(m_current_password_entry);
    secure_clear_entry(m_new_password_entry);
    secure_clear_entry(m_confirm_password_entry);
#ifdef HAVE_YUBIKEY_SUPPORT
    secure_clear_entry(m_yubikey_pin_entry);
#endif
}

PasswordChangeRequest ChangePasswordDialog::get_request() const {
    PasswordChangeRequest req;

    // Get passwords as Glib::ustring to preserve UTF-8 encoding
    req.current_password = m_current_password_entry.get_text();
    req.new_password = m_new_password_entry.get_text();

#ifdef HAVE_YUBIKEY_SUPPORT
    // Get YubiKey PIN if field is visible
    if (m_yubikey_pin_box.get_visible()) {
        req.yubikey_pin = m_yubikey_pin_entry.get_text().raw();
    }
#endif

    std::printf("[DEBUG] ChangePasswordDialog: Retrieved passwords - current: %zu chars, %zu bytes; new: %zu chars, %zu bytes\n",
                req.current_password.length(), req.current_password.bytes(),
                req.new_password.length(), req.new_password.bytes());
    std::fflush(stdout);

    return req;
}

void ChangePasswordDialog::set_current_password(std::string_view temp_password) {
    m_current_password_entry.set_text(Glib::ustring(std::string(temp_password)));
    on_input_changed();  // Update validation state
}

void ChangePasswordDialog::set_yubikey_required(bool required) {
#ifdef HAVE_YUBIKEY_SUPPORT
    m_yubikey_separator.set_visible(required);
    m_yubikey_pin_box.set_visible(required);
    if (required) {
        set_default_size(500, m_is_forced_change ? 550 : 500);
    }
#endif
}

void ChangePasswordDialog::on_input_changed() {
    // Update password strength indicator
    update_password_strength();

    // Get current field values
    Glib::ustring current_pwd = m_current_password_entry.get_text();
    Glib::ustring new_pwd = m_new_password_entry.get_text();
    Glib::ustring confirm_pwd = m_confirm_password_entry.get_text();

    bool is_valid = false;
    std::string validation_message;

    // Validation logic
    if (current_pwd.empty()) {
        validation_message = m_is_forced_change ? "⚠ Enter your temporary password" : "⚠ Enter your current password";
    } else if (new_pwd.empty()) {
        validation_message = "⚠ Enter a new password";
    } else if (new_pwd.length() < m_min_password_length) {
        validation_message = "⚠ Password must be at least " +
                           std::to_string(m_min_password_length) + " characters";
    } else if (confirm_pwd.empty()) {
        validation_message = "⚠ Confirm your new password";
    } else if (new_pwd != confirm_pwd) {
        validation_message = "⚠ Passwords do not match";
    } else if (new_pwd == current_pwd) {
        validation_message = "⚠ New password must differ from current password";
#ifdef HAVE_YUBIKEY_SUPPORT
    } else if (m_yubikey_pin_box.get_visible()) {
        // Validate PIN if YubiKey is required
        std::string pin = m_yubikey_pin_entry.get_text().raw();
        if (pin.empty()) {
            validation_message = "⚠ Enter your YubiKey PIN";
        } else if (pin.length() < 4 || pin.length() > 63) {
            validation_message = "⚠ PIN must be 4-63 characters";
        } else {
            is_valid = true;
            validation_message = "✓ Password and PIN requirements met";
        }
#endif
    } else {
        is_valid = true;
        validation_message = "✓ Password requirements met";
    }

    // Update validation label with color coding
    m_validation_label.set_text(validation_message);

    // Remove previous CSS classes
    m_validation_label.remove_css_class("success-text");
    m_validation_label.remove_css_class("error-text");

    // Add appropriate CSS class
    if (is_valid) {
        m_validation_label.add_css_class("success-text");
    } else {
        m_validation_label.add_css_class("error-text");
    }

    // Enable/disable OK button
    m_ok_button->set_sensitive(is_valid);
}

void ChangePasswordDialog::on_response(int response_id) {
    // DO NOT clear password fields here - signal_response() handlers need to read them first
    // Caller is responsible for clearing via PasswordChangeRequest::clear()

    // Call base class handler
    Gtk::Dialog::on_response(response_id);
}

void ChangePasswordDialog::secure_clear_entry(Gtk::Entry& entry) {
    Glib::ustring text = entry.get_text();
    // Clear password using OPENSSL_cleanse to prevent compiler optimization
    KeepTower::secure_clear_ustring(text);
    entry.set_text("");  // Clear widget
}

void ChangePasswordDialog::update_password_strength() {
    const auto password = m_new_password_entry.get_text();

    if (password.empty()) {
        m_strength_label.set_text("");
        return;
    }

    // Simple password strength calculation
    size_t length = password.length();
    bool has_upper = false, has_lower = false, has_digit = false, has_special = false;

    for (const auto& ch : password.raw()) {
        if (std::isupper(ch)) has_upper = true;
        else if (std::islower(ch)) has_lower = true;
        else if (std::isdigit(ch)) has_digit = true;
        else has_special = true;
    }

    int variety = has_upper + has_lower + has_digit + has_special;
    int score = (length >= 12 ? 2 : (length >= 8 ? 1 : 0)) + variety;

    std::string color;
    std::string strength_text;

    if (score <= 2) {
        color = "#c01c28";  // Red
        strength_text = "Weak";
    } else if (score <= 4) {
        color = "#e66100";  // Orange
        strength_text = "Moderate";
    } else if (score <= 5) {
        color = "#26a269";  // Green
        strength_text = "Strong";
    } else {
        color = "#1c71d8";  // Blue
        strength_text = "Very Strong";
    }

    m_strength_label.set_markup(
        "<span foreground='" + color + "'>Password strength: " + strength_text + "</span>"
    );
}
