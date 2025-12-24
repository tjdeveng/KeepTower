// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "VaultMigrationDialog.h"
#include <filesystem>

VaultMigrationDialog::VaultMigrationDialog(Gtk::Window& parent, const std::string& vault_path)
    : Gtk::Dialog("Migrate Vault to Multi-User Format", parent, true)
    , m_vault_path(vault_path)
{
    set_default_size(600, -1);
    set_resizable(false);

    // Content box setup
    m_content_box.set_margin(24);
    get_content_area()->append(m_content_box);

    // Warning section
    m_warning_icon.set_from_icon_name("dialog-warning-symbolic");
    m_warning_icon.set_icon_size(Gtk::IconSize::LARGE);
    m_warning_icon.set_valign(Gtk::Align::START);

    m_warning_label.set_markup(
        "<b>Important: This migration is irreversible</b>\n\n"
        "This will convert your vault to the new multi-user format (V2).\n"
        "After migration, this vault <b>cannot be opened</b> by older versions of KeepTower.\n"
        "A backup will be created automatically before migration."
    );
    m_warning_label.set_wrap(true);
    m_warning_label.set_xalign(0.0);
    m_warning_label.set_max_width_chars(70);

    m_warning_box.append(m_warning_icon);
    m_warning_box.append(m_warning_label);
    m_warning_box.add_css_class("warning-box");
    m_content_box.append(m_warning_box);

    // Information section
    m_info_label.set_markup(
        "<b>What will happen:</b>\n"
        "• Your vault will be upgraded to support multiple users\n"
        "• You will become the first administrator\n"
        "• All existing accounts will be preserved\n"
        "• Privacy controls will become available\n"
        "• You can add additional users after migration"
    );
    m_info_label.set_wrap(true);
    m_info_label.set_xalign(0.0);
    m_info_label.set_margin_top(12);
    m_info_label.set_margin_bottom(12);
    m_content_box.append(m_info_label);

    // Vault path
    std::string vault_filename = std::filesystem::path(vault_path).filename();
    m_vault_path_label.set_markup("<b>Vault:</b> " + Glib::Markup::escape_text(vault_filename));
    m_vault_path_label.set_xalign(0.0);
    m_vault_path_label.set_margin_bottom(12);
    m_content_box.append(m_vault_path_label);

    // Admin account section
    m_admin_title.set_markup("<b>Create Administrator Account</b>");
    m_admin_title.set_xalign(0.0);
    m_admin_title.set_margin_bottom(6);
    m_admin_box.append(m_admin_title);

    // Username
    m_username_label.set_text("Username:");
    m_username_label.set_xalign(0.0);
    m_username_label.set_size_request(120, -1);
    m_username_entry.set_placeholder_text("Enter admin username");
    m_username_entry.set_hexpand(true);
    m_username_entry.set_max_length(32);
    m_username_box.append(m_username_label);
    m_username_box.append(m_username_entry);
    m_admin_box.append(m_username_box);

    // Password
    m_password_label.set_text("Password:");
    m_password_label.set_xalign(0.0);
    m_password_label.set_size_request(120, -1);
    m_password_entry.set_placeholder_text("Enter admin password");
    m_password_entry.set_hexpand(true);
    m_password_entry.set_visibility(false);
    m_password_entry.set_max_length(128);
    m_password_box.append(m_password_label);
    m_password_box.append(m_password_entry);
    m_admin_box.append(m_password_box);

    // Confirm password
    m_confirm_label.set_text("Confirm:");
    m_confirm_label.set_xalign(0.0);
    m_confirm_label.set_size_request(120, -1);
    m_confirm_entry.set_placeholder_text("Confirm admin password");
    m_confirm_entry.set_hexpand(true);
    m_confirm_entry.set_visibility(false);
    m_confirm_entry.set_max_length(128);
    m_confirm_box.append(m_confirm_label);
    m_confirm_box.append(m_confirm_entry);
    m_admin_box.append(m_confirm_box);

    // Strength indicator
    m_strength_label.set_text("");
    m_strength_label.set_xalign(0.0);
    m_strength_label.set_margin_start(126);  // Align with entry fields
    m_strength_label.set_margin_top(3);
    m_admin_box.append(m_strength_label);

    m_admin_frame.set_child(m_admin_box);
    m_admin_frame.set_margin_bottom(12);
    m_content_box.append(m_admin_frame);

    // Security policy (advanced options)
    m_policy_expander.set_label("Advanced Security Policy");
    m_policy_expander.set_expanded(false);
    m_policy_box.set_margin(12);

    // Min password length
    m_min_length_label.set_text("Minimum Password Length:");
    m_min_length_label.set_xalign(0.0);
    m_min_length_label.set_hexpand(true);

    auto min_length_adj = Gtk::Adjustment::create(12.0, 8.0, 128.0, 1.0, 4.0);
    m_min_length_spin.set_adjustment(min_length_adj);
    m_min_length_spin.set_value(12);
    m_min_length_spin.set_numeric(true);
    m_min_length_spin.set_width_chars(6);

    m_min_length_box.append(m_min_length_label);
    m_min_length_box.append(m_min_length_spin);
    m_policy_box.append(m_min_length_box);

    // PBKDF2 iterations
    m_iterations_label.set_text("PBKDF2 Iterations:");
    m_iterations_label.set_xalign(0.0);
    m_iterations_label.set_hexpand(true);
    m_iterations_label.set_tooltip_text(
        "Higher iterations = stronger security but slower vault opening.\n"
        "Recommended: 600,000 (OWASP 2023)"
    );

    auto iterations_adj = Gtk::Adjustment::create(600000.0, 100000.0, 5000000.0, 100000.0, 500000.0);
    m_iterations_spin.set_adjustment(iterations_adj);
    m_iterations_spin.set_value(600000);
    m_iterations_spin.set_numeric(true);
    m_iterations_spin.set_width_chars(10);

    m_iterations_box.append(m_iterations_label);
    m_iterations_box.append(m_iterations_spin);
    m_policy_box.append(m_iterations_box);

    m_policy_expander.set_child(m_policy_box);
    m_content_box.append(m_policy_expander);

    // Action buttons
    m_cancel_button = add_button("_Cancel", Gtk::ResponseType::CANCEL);
    m_migrate_button = add_button("_Migrate Vault", Gtk::ResponseType::OK);
    m_migrate_button->add_css_class("suggested-action");
    m_migrate_button->set_sensitive(false);  // Initially disabled

    // Set default button
    set_default_widget(*m_migrate_button);

    // Connect signals
    m_username_entry.signal_changed().connect(
        sigc::mem_fun(*this, &VaultMigrationDialog::on_username_changed)
    );
    m_password_entry.signal_changed().connect(
        sigc::mem_fun(*this, &VaultMigrationDialog::on_password_changed)
    );
    m_confirm_entry.signal_changed().connect(
        sigc::mem_fun(*this, &VaultMigrationDialog::on_confirm_changed)
    );

    // Enter key activates default button
    m_username_entry.signal_activate().connect([this]() {
        m_password_entry.grab_focus();
    });
    m_password_entry.signal_activate().connect([this]() {
        m_confirm_entry.grab_focus();
    });
    m_confirm_entry.signal_activate().connect([this]() {
        if (m_migrate_button->get_sensitive()) {
            response(Gtk::ResponseType::OK);
        }
    });

    // Focus username entry initially
    m_username_entry.grab_focus();
}

Glib::ustring VaultMigrationDialog::get_admin_username() const {
    return m_username_entry.get_text();
}

Glib::ustring VaultMigrationDialog::get_admin_password() const {
    return m_password_entry.get_text();
}

uint32_t VaultMigrationDialog::get_min_password_length() const {
    return static_cast<uint32_t>(m_min_length_spin.get_value());
}

uint32_t VaultMigrationDialog::get_pbkdf2_iterations() const {
    return static_cast<uint32_t>(m_iterations_spin.get_value());
}

void VaultMigrationDialog::on_username_changed() {
    validate_inputs();
}

void VaultMigrationDialog::on_password_changed() {
    update_password_strength();
    validate_inputs();
}

void VaultMigrationDialog::on_confirm_changed() {
    validate_inputs();
}

void VaultMigrationDialog::validate_inputs() {
    const auto username = m_username_entry.get_text();
    const auto password = m_password_entry.get_text();
    const auto confirm = m_confirm_entry.get_text();
    const uint32_t min_length = get_min_password_length();

    bool valid = true;
    std::string error_message;

    // Username validation
    if (username.empty()) {
        valid = false;
        error_message = "Username is required";
    } else if (username.length() < 3) {
        valid = false;
        error_message = "Username must be at least 3 characters";
    } else if (username.length() > 32) {
        valid = false;
        error_message = "Username must be at most 32 characters";
    }

    // Password validation
    if (valid && password.empty()) {
        valid = false;
        error_message = "Password is required";
    } else if (valid && password.length() < min_length) {
        valid = false;
        error_message = Glib::ustring::sprintf(
            "Password must be at least %u characters", min_length
        );
    }

    // Confirm validation
    if (valid && confirm.empty()) {
        valid = false;
        error_message = "Please confirm password";
    } else if (valid && password != confirm) {
        valid = false;
        error_message = "Passwords do not match";
    }

    // Update UI
    m_migrate_button->set_sensitive(valid);

    // Show error in strength label if invalid and user has typed something
    if (!valid && (!password.empty() || !confirm.empty())) {
        m_strength_label.set_markup("<span foreground='#c01c28'>" +
            Glib::Markup::escape_text(error_message) + "</span>");
    }
}

void VaultMigrationDialog::update_password_strength() {
    const auto password = m_password_entry.get_text();

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
