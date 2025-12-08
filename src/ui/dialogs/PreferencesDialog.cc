// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "PreferencesDialog.h"
#include "../../core/VaultManager.h"
#include <stdexcept>

PreferencesDialog::PreferencesDialog(Gtk::Window& parent, VaultManager* vault_manager)
    : Gtk::Dialog("Preferences", parent, true),
      m_vault_manager(vault_manager),
      m_content_box(Gtk::Orientation::VERTICAL, 12),
      m_appearance_box(Gtk::Orientation::VERTICAL, 6),
      m_appearance_title("<b>Appearance</b>"),
      m_appearance_description("Choose how KeepTower looks"),
      m_color_scheme_box(Gtk::Orientation::HORIZONTAL, 6),
      m_color_scheme_label("Color scheme:"),
      m_rs_box(Gtk::Orientation::VERTICAL, 6),
      m_rs_title("<b>Reed-Solomon Error Correction</b>"),
      m_rs_description("Protect vault files from corruption on unreliable storage media (USB drives, SD cards, etc.)"),
      m_rs_enabled_check("Enable error correction for new vaults"),
      m_redundancy_box(Gtk::Orientation::HORIZONTAL, 6),
      m_redundancy_label("Redundancy level:"),
      m_redundancy_suffix("%"),
      m_redundancy_help("Higher values provide more protection but increase file size.\nCan recover up to half the redundancy percentage in corruption."),
      m_apply_to_current_check("Apply to current vault (overrides file's original settings)"),
      m_backup_box(Gtk::Orientation::VERTICAL, 6),
      m_backup_title("<b>Automatic Backups</b>"),
      m_backup_description("Create timestamped backups when saving vaults to protect against accidental data loss"),
      m_backup_enabled_check("Enable automatic backups"),
      m_backup_count_box(Gtk::Orientation::HORIZONTAL, 6),
      m_backup_count_label("Keep up to:"),
      m_backup_count_suffix(" backups"),
      m_backup_help("Older backups are automatically deleted when this limit is exceeded.\nBackups are named with timestamps for easy identification.") {

    set_default_size(DEFAULT_WIDTH, DEFAULT_HEIGHT);

    // Load settings with error handling
    try {
        m_settings = Gio::Settings::create("com.tjdeveng.keeptower");
    } catch (const Glib::Error& e) {
        // Fatal: cannot continue without settings
        throw std::runtime_error("Failed to load settings: " + std::string(e.what()));
    }

    setup_ui();
    load_settings();

    // Connect signals
    m_rs_enabled_check.signal_toggled().connect(
        sigc::mem_fun(*this, &PreferencesDialog::on_rs_enabled_toggled));

    m_backup_enabled_check.signal_toggled().connect(
        sigc::mem_fun(*this, &PreferencesDialog::on_backup_enabled_toggled));

    // Connect apply-to-current checkbox to reload settings when toggled
    if (m_vault_manager && m_vault_manager->is_vault_open()) {
        m_apply_to_current_check.signal_toggled().connect(
            sigc::mem_fun(*this, &PreferencesDialog::on_apply_to_current_toggled));
    }

    signal_response().connect(
        sigc::mem_fun(*this, &PreferencesDialog::on_response));
}

void PreferencesDialog::setup_ui() {
    // Add standard dialog buttons
    add_button("_Cancel", Gtk::ResponseType::CANCEL);
    add_button("_Apply", Gtk::ResponseType::OK);

    // Configure content box
    m_content_box.set_margin(18);

    // Appearance section
    m_appearance_title.set_use_markup(true);
    m_appearance_title.set_halign(Gtk::Align::START);
    m_appearance_box.append(m_appearance_title);

    m_appearance_description.set_wrap(true);
    m_appearance_description.set_max_width_chars(60);
    m_appearance_description.set_halign(Gtk::Align::START);
    m_appearance_description.add_css_class("dim-label");
    m_appearance_box.append(m_appearance_description);

    // Color scheme dropdown
    m_color_scheme_label.set_halign(Gtk::Align::START);
    m_color_scheme_box.append(m_color_scheme_label);

    // Create string list for dropdown
    auto color_schemes = Gtk::StringList::create({"System Default", "Light", "Dark"});
    m_color_scheme_dropdown.set_model(color_schemes);
    m_color_scheme_dropdown.set_selected(0);
    m_color_scheme_box.append(m_color_scheme_dropdown);

    m_color_scheme_box.set_halign(Gtk::Align::START);
    m_appearance_box.append(m_color_scheme_box);

    m_content_box.append(m_appearance_box);

    // Add separator between sections
    auto* separator = Gtk::make_managed<Gtk::Separator>(Gtk::Orientation::HORIZONTAL);
    separator->set_margin_top(12);
    separator->set_margin_bottom(12);
    m_content_box.append(*separator);

    // Reed-Solomon section
    m_rs_title.set_use_markup(true);
    m_rs_title.set_halign(Gtk::Align::START);
    m_rs_box.append(m_rs_title);

    m_rs_description.set_wrap(true);
    m_rs_description.set_max_width_chars(60);
    m_rs_description.set_halign(Gtk::Align::START);
    m_rs_description.add_css_class("dim-label");
    m_rs_box.append(m_rs_description);

    m_rs_box.append(m_rs_enabled_check);

    // Redundancy level controls
    m_redundancy_label.set_halign(Gtk::Align::START);
    m_redundancy_box.append(m_redundancy_label);

    // Use constexpr constants for bounds checking
    auto adjustment = Gtk::Adjustment::create(
        static_cast<double>(DEFAULT_REDUNDANCY),
        static_cast<double>(MIN_REDUNDANCY),
        static_cast<double>(MAX_REDUNDANCY),
        1.0, 5.0, 0.0
    );
    m_redundancy_spin.set_adjustment(adjustment);
    m_redundancy_spin.set_digits(0);
    m_redundancy_spin.set_value(DEFAULT_REDUNDANCY);
    m_redundancy_box.append(m_redundancy_spin);

    m_redundancy_suffix.set_halign(Gtk::Align::START);
    m_redundancy_box.append(m_redundancy_suffix);

    m_redundancy_box.set_halign(Gtk::Align::START);
    m_rs_box.append(m_redundancy_box);

    m_redundancy_help.set_wrap(true);
    m_redundancy_help.set_max_width_chars(60);
    m_redundancy_help.set_halign(Gtk::Align::START);
    m_redundancy_help.add_css_class("dim-label");
    m_rs_box.append(m_redundancy_help);

    // Apply to current vault checkbox (only show if vault is open)
    if (m_vault_manager && m_vault_manager->is_vault_open()) {
        m_apply_to_current_check.set_margin_top(6);
        m_apply_to_current_check.add_css_class("warning");
        m_rs_box.append(m_apply_to_current_check);
    }

    m_content_box.append(m_rs_box);

    // Add separator before backup section
    auto* separator2 = Gtk::make_managed<Gtk::Separator>(Gtk::Orientation::HORIZONTAL);
    separator2->set_margin_top(12);
    separator2->set_margin_bottom(12);
    m_content_box.append(*separator2);

    // Backup section
    m_backup_title.set_use_markup(true);
    m_backup_title.set_halign(Gtk::Align::START);
    m_backup_box.append(m_backup_title);

    m_backup_description.set_wrap(true);
    m_backup_description.set_max_width_chars(60);
    m_backup_description.set_halign(Gtk::Align::START);
    m_backup_description.add_css_class("dim-label");
    m_backup_box.append(m_backup_description);

    m_backup_box.append(m_backup_enabled_check);

    // Backup count controls
    m_backup_count_label.set_halign(Gtk::Align::START);
    m_backup_count_box.append(m_backup_count_label);

    auto backup_adjustment = Gtk::Adjustment::create(
        static_cast<double>(DEFAULT_BACKUP_COUNT),
        static_cast<double>(MIN_BACKUP_COUNT),
        static_cast<double>(MAX_BACKUP_COUNT),
        1.0, 5.0, 0.0
    );
    m_backup_count_spin.set_adjustment(backup_adjustment);
    m_backup_count_spin.set_digits(0);
    m_backup_count_spin.set_value(DEFAULT_BACKUP_COUNT);
    m_backup_count_box.append(m_backup_count_spin);

    m_backup_count_suffix.set_halign(Gtk::Align::START);
    m_backup_count_box.append(m_backup_count_suffix);

    m_backup_count_box.set_halign(Gtk::Align::START);
    m_backup_box.append(m_backup_count_box);

    m_backup_help.set_wrap(true);
    m_backup_help.set_max_width_chars(60);
    m_backup_help.set_halign(Gtk::Align::START);
    m_backup_help.add_css_class("dim-label");
    m_backup_box.append(m_backup_help);

    m_content_box.append(m_backup_box);

    // Add content to dialog
    get_content_area()->append(m_content_box);

    // Connect color scheme signal after UI is set up
    m_color_scheme_dropdown.property_selected().signal_changed().connect(
        sigc::mem_fun(*this, &PreferencesDialog::on_color_scheme_changed));
}

void PreferencesDialog::load_settings() {
    if (!m_settings) [[unlikely]] {
        return; // Defensive: should never happen
    }

    // Load color scheme
    Glib::ustring color_scheme = m_settings->get_string("color-scheme");
    if (color_scheme == "light") {
        m_color_scheme_dropdown.set_selected(1);
    } else if (color_scheme == "dark") {
        m_color_scheme_dropdown.set_selected(2);
    } else {
        m_color_scheme_dropdown.set_selected(0);  // default
    }

    // If vault is open, show current vault settings and check the "apply to current" box
    // Otherwise, show default settings from preferences
    bool rs_enabled;
    int rs_redundancy;

    if (m_vault_manager && m_vault_manager->is_vault_open()) {
        // Show current vault's FEC settings
        rs_enabled = m_vault_manager->is_reed_solomon_enabled();
        rs_redundancy = m_vault_manager->get_rs_redundancy_percent();
        // Check the "apply to current vault" checkbox by default
        m_apply_to_current_check.set_active(true);
    } else {
        // Show default settings from preferences
        rs_enabled = m_settings->get_boolean("use-reed-solomon");
        rs_redundancy = m_settings->get_int("rs-redundancy-percent");
    }

    // Validate and clamp redundancy value for security
    rs_redundancy = std::clamp(rs_redundancy, MIN_REDUNDANCY, MAX_REDUNDANCY);

    m_rs_enabled_check.set_active(rs_enabled);
    m_redundancy_spin.set_value(rs_redundancy);

    // Update sensitivity
    m_redundancy_label.set_sensitive(rs_enabled);
    m_redundancy_spin.set_sensitive(rs_enabled);
    m_redundancy_suffix.set_sensitive(rs_enabled);
    m_redundancy_help.set_sensitive(rs_enabled);

    // Load backup settings
    bool backup_enabled = m_settings->get_boolean("backup-enabled");
    int backup_count = m_settings->get_int("backup-count");

    // Validate and clamp backup count
    backup_count = std::clamp(backup_count, MIN_BACKUP_COUNT, MAX_BACKUP_COUNT);

    m_backup_enabled_check.set_active(backup_enabled);
    m_backup_count_spin.set_value(backup_count);

    // Update sensitivity
    m_backup_count_label.set_sensitive(backup_enabled);
    m_backup_count_spin.set_sensitive(backup_enabled);
    m_backup_count_suffix.set_sensitive(backup_enabled);
    m_backup_help.set_sensitive(backup_enabled);
}

void PreferencesDialog::save_settings() {
    if (!m_settings) [[unlikely]] {
        return; // Defensive: should never happen
    }

    // Save color scheme
    guint selected = m_color_scheme_dropdown.get_selected();
    Glib::ustring scheme;
    if (selected == 1) {
        scheme = "light";
    } else if (selected == 2) {
        scheme = "dark";
    } else {
        scheme = "default";
    }
    m_settings->set_string("color-scheme", scheme);
    apply_color_scheme(scheme);

    const bool rs_enabled = m_rs_enabled_check.get_active();
    const int rs_redundancy = std::clamp(
        static_cast<int>(m_redundancy_spin.get_value()),
        MIN_REDUNDANCY,
        MAX_REDUNDANCY
    );

    // Always save to preferences (default settings for new vaults)
    m_settings->set_boolean("use-reed-solomon", rs_enabled);
    m_settings->set_int("rs-redundancy-percent", rs_redundancy);

    // If checkbox is checked, also apply to current vault
    if (m_vault_manager && m_vault_manager->is_vault_open() && m_apply_to_current_check.get_active()) {
        m_vault_manager->set_reed_solomon_enabled(rs_enabled);
        m_vault_manager->set_rs_redundancy_percent(rs_redundancy);
    }

    // Save backup settings
    const bool backup_enabled = m_backup_enabled_check.get_active();
    const int backup_count = std::clamp(
        static_cast<int>(m_backup_count_spin.get_value()),
        MIN_BACKUP_COUNT,
        MAX_BACKUP_COUNT
    );

    m_settings->set_boolean("backup-enabled", backup_enabled);
    m_settings->set_int("backup-count", backup_count);
}

void PreferencesDialog::apply_color_scheme(const Glib::ustring& scheme) {
    auto settings = Gtk::Settings::get_default();
    if (!settings) [[unlikely]] {
        return;
    }

    if (scheme == "light") {
        settings->property_gtk_application_prefer_dark_theme() = false;
    } else if (scheme == "dark") {
        settings->property_gtk_application_prefer_dark_theme() = true;
    } else {
        // Default: follow system preference (reset to system default)
        settings->reset_property("gtk-application-prefer-dark-theme");
    }
}

void PreferencesDialog::on_color_scheme_changed() noexcept {
    // Preview the color scheme change immediately
    guint selected = m_color_scheme_dropdown.get_selected();
    if (selected == 1) {
        apply_color_scheme("light");
    } else if (selected == 2) {
        apply_color_scheme("dark");
    } else {
        apply_color_scheme("default");
    }
}

void PreferencesDialog::on_rs_enabled_toggled() noexcept {
    const bool enabled = m_rs_enabled_check.get_active();

    m_redundancy_label.set_sensitive(enabled);
    m_redundancy_spin.set_sensitive(enabled);
    m_redundancy_suffix.set_sensitive(enabled);
    m_redundancy_help.set_sensitive(enabled);
}

void PreferencesDialog::on_backup_enabled_toggled() noexcept {
    const bool enabled = m_backup_enabled_check.get_active();

    m_backup_count_label.set_sensitive(enabled);
    m_backup_count_spin.set_sensitive(enabled);
    m_backup_count_suffix.set_sensitive(enabled);
    m_backup_help.set_sensitive(enabled);
}

void PreferencesDialog::on_apply_to_current_toggled() noexcept {
    if (!m_vault_manager || !m_vault_manager->is_vault_open()) {
        return;
    }

    // When toggled, reload FEC settings to show either vault or default settings
    bool rs_enabled;
    int rs_redundancy;

    if (m_apply_to_current_check.get_active()) {
        // Show current vault's FEC settings
        rs_enabled = m_vault_manager->is_reed_solomon_enabled();
        rs_redundancy = m_vault_manager->get_rs_redundancy_percent();
    } else {
        // Show default settings from preferences
        if (m_settings) {
            rs_enabled = m_settings->get_boolean("use-reed-solomon");
            rs_redundancy = m_settings->get_int("rs-redundancy-percent");
            rs_redundancy = std::clamp(rs_redundancy, MIN_REDUNDANCY, MAX_REDUNDANCY);
        } else {
            return;
        }
    }

    // Update UI to reflect the appropriate settings
    m_rs_enabled_check.set_active(rs_enabled);
    m_redundancy_spin.set_value(rs_redundancy);
}

void PreferencesDialog::on_response(int response_id) noexcept {
    if (response_id == Gtk::ResponseType::OK) {
        save_settings();
    }
    hide();
}
