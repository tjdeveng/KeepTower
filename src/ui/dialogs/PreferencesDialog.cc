// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "PreferencesDialog.h"
#include <stdexcept>

PreferencesDialog::PreferencesDialog(Gtk::Window& parent)
    : Gtk::Dialog("Preferences", parent, true),
      m_content_box(Gtk::Orientation::VERTICAL, 12),
      m_rs_box(Gtk::Orientation::VERTICAL, 6),
      m_rs_title("<b>Reed-Solomon Error Correction</b>"),
      m_rs_description("Protect vault files from corruption on unreliable storage media (USB drives, SD cards, etc.)"),
      m_rs_enabled_check("Enable error correction for new vaults"),
      m_redundancy_box(Gtk::Orientation::HORIZONTAL, 6),
      m_redundancy_label("Redundancy level:"),
      m_redundancy_suffix("%"),
      m_redundancy_help("Higher values provide more protection but increase file size.\nCan recover up to half the redundancy percentage in corruption.") {

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

    signal_response().connect(
        sigc::mem_fun(*this, &PreferencesDialog::on_response));
}

void PreferencesDialog::setup_ui() {
    // Add standard dialog buttons
    add_button("_Cancel", Gtk::ResponseType::CANCEL);
    add_button("_Apply", Gtk::ResponseType::OK);

    // Configure content box
    m_content_box.set_margin(18);

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

    m_content_box.append(m_rs_box);

    // Add content to dialog
    get_content_area()->append(m_content_box);
}

void PreferencesDialog::load_settings() {
    if (!m_settings) [[unlikely]] {
        return; // Defensive: should never happen
    }

    bool rs_enabled = m_settings->get_boolean("use-reed-solomon");
    int rs_redundancy = m_settings->get_int("rs-redundancy-percent");

    // Validate and clamp redundancy value for security
    rs_redundancy = std::clamp(rs_redundancy, MIN_REDUNDANCY, MAX_REDUNDANCY);

    m_rs_enabled_check.set_active(rs_enabled);
    m_redundancy_spin.set_value(rs_redundancy);

    // Update sensitivity
    m_redundancy_label.set_sensitive(rs_enabled);
    m_redundancy_spin.set_sensitive(rs_enabled);
    m_redundancy_suffix.set_sensitive(rs_enabled);
    m_redundancy_help.set_sensitive(rs_enabled);
}

void PreferencesDialog::save_settings() {
    if (!m_settings) [[unlikely]] {
        return; // Defensive: should never happen
    }

    const bool rs_enabled = m_rs_enabled_check.get_active();
    const int rs_redundancy = std::clamp(
        static_cast<int>(m_redundancy_spin.get_value()),
        MIN_REDUNDANCY,
        MAX_REDUNDANCY
    );

    m_settings->set_boolean("use-reed-solomon", rs_enabled);
    m_settings->set_int("rs-redundancy-percent", rs_redundancy);
}

void PreferencesDialog::on_rs_enabled_toggled() noexcept {
    const bool enabled = m_rs_enabled_check.get_active();

    m_redundancy_label.set_sensitive(enabled);
    m_redundancy_spin.set_sensitive(enabled);
    m_redundancy_suffix.set_sensitive(enabled);
    m_redundancy_help.set_sensitive(enabled);
}

void PreferencesDialog::on_response(int response_id) noexcept {
    if (response_id == Gtk::ResponseType::OK) {
        save_settings();
    }
    hide();
}
