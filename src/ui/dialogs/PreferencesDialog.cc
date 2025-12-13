// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "PreferencesDialog.h"
#include "../../core/VaultManager.h"
#include "../../utils/SettingsValidator.h"
#include <stdexcept>

PreferencesDialog::PreferencesDialog(Gtk::Window& parent, VaultManager* vault_manager)
    : Gtk::Dialog("Preferences", parent, true),
      m_vault_manager(vault_manager),
      m_main_box(Gtk::Orientation::HORIZONTAL, 0),
      m_appearance_box(Gtk::Orientation::VERTICAL, 18),
      m_color_scheme_box(Gtk::Orientation::HORIZONTAL, 12),
      m_color_scheme_label("Color scheme:"),
      m_security_box(Gtk::Orientation::VERTICAL, 18),
      m_clipboard_timeout_box(Gtk::Orientation::HORIZONTAL, 12),
      m_clipboard_timeout_label("Clear clipboard after:"),
      m_clipboard_timeout_suffix(" seconds"),
      m_auto_lock_enabled_check("Enable auto-lock after inactivity"),
      m_auto_lock_timeout_box(Gtk::Orientation::HORIZONTAL, 12),
      m_auto_lock_timeout_label("Lock timeout:"),
      m_auto_lock_timeout_suffix(" seconds"),
      m_password_history_enabled_check("Track password history (prevents reuse)"),
      m_storage_box(Gtk::Orientation::VERTICAL, 18),
      m_rs_section_title("<b>Error Correction</b>"),
      m_rs_description("Protect vault files from corruption on unreliable storage"),
      m_rs_enabled_check("Enable Reed-Solomon error correction for new vaults"),
      m_redundancy_box(Gtk::Orientation::HORIZONTAL, 12),
      m_redundancy_label("Redundancy:"),
      m_redundancy_suffix("%"),
      m_redundancy_help("Higher values provide more protection but increase file size"),
      m_apply_to_current_check("Apply to current vault"),
      m_backup_section_title("<b>Automatic Backups</b>"),
      m_backup_description("Create timestamped backups when saving vaults"),
      m_backup_enabled_check("Enable automatic backups"),
      m_backup_count_box(Gtk::Orientation::HORIZONTAL, 12),
      m_backup_count_label("Keep up to:"),
      m_backup_count_suffix(" backups"),
      m_backup_help("Older backups are automatically deleted") {

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

    m_auto_lock_enabled_check.signal_toggled().connect(
        sigc::mem_fun(*this, &PreferencesDialog::on_auto_lock_enabled_toggled));

    // Connect apply-to-current checkbox to reload settings when toggled
    if (m_vault_manager && m_vault_manager->is_vault_open()) {
        m_apply_to_current_check.signal_toggled().connect(
            sigc::mem_fun(*this, &PreferencesDialog::on_apply_to_current_toggled));
    }

    signal_response().connect(
        sigc::mem_fun(*this, &PreferencesDialog::on_response));
}

void PreferencesDialog::setup_ui() {
    // Add standard dialog buttons (Apply/Cancel pattern for settings that need confirmation)
    add_button("_Cancel", Gtk::ResponseType::CANCEL);
    add_button("_Apply", Gtk::ResponseType::APPLY);

    // Setup sidebar + stack layout (GNOME HIG pattern)
    m_sidebar.set_stack(m_stack);
    m_sidebar.set_vexpand(true);

    m_main_box.append(m_sidebar);

    // Add separator between sidebar and content
    auto* separator = Gtk::make_managed<Gtk::Separator>(Gtk::Orientation::VERTICAL);
    m_main_box.append(*separator);

    m_stack.set_hexpand(true);
    m_stack.set_vexpand(true);
    m_main_box.append(m_stack);

    // Setup individual pages
    setup_appearance_page();
    setup_security_page();
    setup_storage_page();

    // Add main box to dialog with proper margins (GNOME HIG)
    m_main_box.set_margin_bottom(12);
    get_content_area()->append(m_main_box);

    // Connect color scheme signal after UI is set up
    m_color_scheme_dropdown.property_selected().signal_changed().connect(
        sigc::mem_fun(*this, &PreferencesDialog::on_color_scheme_changed));
}

void PreferencesDialog::setup_appearance_page() {
    m_appearance_box.set_margin_start(18);
    m_appearance_box.set_margin_end(18);
    m_appearance_box.set_margin_top(18);
    m_appearance_box.set_margin_bottom(18);

    // Color scheme controls
    auto* scheme_row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);

    auto* scheme_label = Gtk::make_managed<Gtk::Label>("Color Scheme");
    scheme_label->set_halign(Gtk::Align::START);
    scheme_label->add_css_class("heading");
    scheme_row->append(*scheme_label);

    m_color_scheme_label.set_halign(Gtk::Align::START);
    m_color_scheme_box.append(m_color_scheme_label);

    // Create string list for dropdown
    auto color_schemes = Gtk::StringList::create({"System Default", "Light", "Dark"});
    m_color_scheme_dropdown.set_model(color_schemes);
    m_color_scheme_dropdown.set_selected(0);
    m_color_scheme_box.append(m_color_scheme_dropdown);

    m_color_scheme_box.set_halign(Gtk::Align::START);
    scheme_row->append(m_color_scheme_box);

    m_appearance_box.append(*scheme_row);

    // Add page to stack
    m_stack.add(m_appearance_box, "appearance", "Appearance");
}

void PreferencesDialog::setup_security_page() {
    m_security_box.set_margin_start(18);
    m_security_box.set_margin_end(18);
    m_security_box.set_margin_top(18);
    m_security_box.set_margin_bottom(18);

    // Clipboard timeout section
    auto* clipboard_section = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);

    auto* clipboard_title = Gtk::make_managed<Gtk::Label>("Clipboard Protection");
    clipboard_title->set_halign(Gtk::Align::START);
    clipboard_title->add_css_class("heading");
    clipboard_section->append(*clipboard_title);

    auto* clipboard_desc = Gtk::make_managed<Gtk::Label>("Automatically clear copied passwords from clipboard");
    clipboard_desc->set_halign(Gtk::Align::START);
    clipboard_desc->add_css_class("dim-label");
    clipboard_desc->set_wrap(true);
    clipboard_section->append(*clipboard_desc);

    m_clipboard_timeout_label.set_halign(Gtk::Align::START);
    m_clipboard_timeout_box.append(m_clipboard_timeout_label);

    auto clipboard_adjustment = Gtk::Adjustment::create(
        static_cast<double>(DEFAULT_CLIPBOARD_TIMEOUT),
        static_cast<double>(MIN_CLIPBOARD_TIMEOUT),
        static_cast<double>(MAX_CLIPBOARD_TIMEOUT),
        1.0, 10.0, 0.0
    );
    m_clipboard_timeout_spin.set_adjustment(clipboard_adjustment);
    m_clipboard_timeout_spin.set_digits(0);
    m_clipboard_timeout_spin.set_value(DEFAULT_CLIPBOARD_TIMEOUT);
    m_clipboard_timeout_box.append(m_clipboard_timeout_spin);

    m_clipboard_timeout_suffix.set_halign(Gtk::Align::START);
    m_clipboard_timeout_box.append(m_clipboard_timeout_suffix);

    m_clipboard_timeout_box.set_halign(Gtk::Align::START);
    clipboard_section->append(m_clipboard_timeout_box);

    m_security_box.append(*clipboard_section);

    // Auto-lock section
    auto* auto_lock_section = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    auto_lock_section->set_margin_top(24);

    auto* auto_lock_title = Gtk::make_managed<Gtk::Label>("Auto-Lock");
    auto_lock_title->set_halign(Gtk::Align::START);
    auto_lock_title->add_css_class("heading");
    auto_lock_section->append(*auto_lock_title);

    auto* auto_lock_desc = Gtk::make_managed<Gtk::Label>("Lock vault after period of inactivity");
    auto_lock_desc->set_halign(Gtk::Align::START);
    auto_lock_desc->add_css_class("dim-label");
    auto_lock_desc->set_wrap(true);
    auto_lock_section->append(*auto_lock_desc);

    auto_lock_section->append(m_auto_lock_enabled_check);

    m_auto_lock_timeout_label.set_halign(Gtk::Align::START);
    m_auto_lock_timeout_box.append(m_auto_lock_timeout_label);

    auto auto_lock_adjustment = Gtk::Adjustment::create(
        static_cast<double>(DEFAULT_AUTO_LOCK_TIMEOUT),
        static_cast<double>(MIN_AUTO_LOCK_TIMEOUT),
        static_cast<double>(MAX_AUTO_LOCK_TIMEOUT),
        10.0, 60.0, 0.0
    );
    m_auto_lock_timeout_spin.set_adjustment(auto_lock_adjustment);
    m_auto_lock_timeout_spin.set_digits(0);
    m_auto_lock_timeout_spin.set_value(DEFAULT_AUTO_LOCK_TIMEOUT);
    m_auto_lock_timeout_box.append(m_auto_lock_timeout_spin);

    m_auto_lock_timeout_suffix.set_halign(Gtk::Align::START);
    m_auto_lock_timeout_box.append(m_auto_lock_timeout_suffix);

    m_auto_lock_timeout_box.set_halign(Gtk::Align::START);
    auto_lock_section->append(m_auto_lock_timeout_box);

    m_security_box.append(*auto_lock_section);

    // Password history section
    auto* password_history_section = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    password_history_section->set_margin_top(24);

    auto* password_history_title = Gtk::make_managed<Gtk::Label>("Password History");
    password_history_title->set_halign(Gtk::Align::START);
    password_history_title->add_css_class("heading");
    password_history_section->append(*password_history_title);

    auto* password_history_desc = Gtk::make_managed<Gtk::Label>("Track previous passwords to prevent reuse");
    password_history_desc->set_halign(Gtk::Align::START);
    password_history_desc->add_css_class("dim-label");
    password_history_desc->set_wrap(true);
    password_history_section->append(*password_history_desc);

    password_history_section->append(m_password_history_enabled_check);

    m_security_box.append(*password_history_section);

    // Add page to stack
    m_stack.add(m_security_box, "security", "Security");
}

void PreferencesDialog::setup_storage_page() {
    m_storage_box.set_margin_start(18);
    m_storage_box.set_margin_end(18);
    m_storage_box.set_margin_top(18);
    m_storage_box.set_margin_bottom(18);

    // Reed-Solomon section
    auto* rs_section = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);

    m_rs_section_title.set_use_markup(true);
    m_rs_section_title.set_halign(Gtk::Align::START);
    m_rs_section_title.add_css_class("heading");
    rs_section->append(m_rs_section_title);

    m_rs_description.set_wrap(true);
    m_rs_description.set_max_width_chars(60);
    m_rs_description.set_halign(Gtk::Align::START);
    m_rs_description.add_css_class("dim-label");
    rs_section->append(m_rs_description);

    rs_section->append(m_rs_enabled_check);

    // Redundancy level controls
    m_redundancy_label.set_halign(Gtk::Align::START);
    m_redundancy_box.append(m_redundancy_label);

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
    rs_section->append(m_redundancy_box);

    m_redundancy_help.set_wrap(true);
    m_redundancy_help.set_max_width_chars(60);
    m_redundancy_help.set_halign(Gtk::Align::START);
    m_redundancy_help.add_css_class("dim-label");
    rs_section->append(m_redundancy_help);

    // Apply to current vault checkbox (only show if vault is open)
    if (m_vault_manager && m_vault_manager->is_vault_open()) {
        m_apply_to_current_check.set_margin_top(6);
        rs_section->append(m_apply_to_current_check);
    }

    m_storage_box.append(*rs_section);

    // Backup section
    auto* backup_section = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    backup_section->set_margin_top(24);

    m_backup_section_title.set_use_markup(true);
    m_backup_section_title.set_halign(Gtk::Align::START);
    m_backup_section_title.add_css_class("heading");
    backup_section->append(m_backup_section_title);

    m_backup_description.set_wrap(true);
    m_backup_description.set_max_width_chars(60);
    m_backup_description.set_halign(Gtk::Align::START);
    m_backup_description.add_css_class("dim-label");
    backup_section->append(m_backup_description);

    backup_section->append(m_backup_enabled_check);

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
    backup_section->append(m_backup_count_box);

    m_backup_help.set_wrap(true);
    m_backup_help.set_max_width_chars(60);
    m_backup_help.set_halign(Gtk::Align::START);
    m_backup_help.add_css_class("dim-label");
    backup_section->append(m_backup_help);

    m_storage_box.append(*backup_section);

    // Add page to stack
    m_stack.add(m_storage_box, "storage", "Storage");
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

    // Load security settings with validation
    int clipboard_timeout = SettingsValidator::get_clipboard_timeout(m_settings);
    m_clipboard_timeout_spin.set_value(clipboard_timeout);

    bool auto_lock_enabled = SettingsValidator::is_auto_lock_enabled(m_settings);
    m_auto_lock_enabled_check.set_active(auto_lock_enabled);

    int auto_lock_timeout = SettingsValidator::get_auto_lock_timeout(m_settings);
    m_auto_lock_timeout_spin.set_value(auto_lock_timeout);

    bool password_history_enabled = SettingsValidator::is_password_history_enabled(m_settings);
    m_password_history_enabled_check.set_active(password_history_enabled);

    // Update auto-lock controls sensitivity
    m_auto_lock_timeout_label.set_sensitive(auto_lock_enabled);
    m_auto_lock_timeout_spin.set_sensitive(auto_lock_enabled);
    m_auto_lock_timeout_suffix.set_sensitive(auto_lock_enabled);
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

    // Checkbox controls whether to apply to current vault or save as defaults
    if (m_vault_manager && m_vault_manager->is_vault_open() && m_apply_to_current_check.get_active()) {
        // Apply only to current vault, don't change defaults
        m_vault_manager->set_reed_solomon_enabled(rs_enabled);
        m_vault_manager->set_rs_redundancy_percent(rs_redundancy);
    } else {
        // Save to preferences (defaults for new vaults)
        m_settings->set_boolean("use-reed-solomon", rs_enabled);
        m_settings->set_int("rs-redundancy-percent", rs_redundancy);
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

    // Save security settings
    const int clipboard_timeout = std::clamp(
        static_cast<int>(m_clipboard_timeout_spin.get_value()),
        MIN_CLIPBOARD_TIMEOUT,
        MAX_CLIPBOARD_TIMEOUT
    );
    m_settings->set_int("clipboard-clear-timeout", clipboard_timeout);

    const bool auto_lock_enabled = m_auto_lock_enabled_check.get_active();
    m_settings->set_boolean("auto-lock-enabled", auto_lock_enabled);

    const int auto_lock_timeout = std::clamp(
        static_cast<int>(m_auto_lock_timeout_spin.get_value()),
        MIN_AUTO_LOCK_TIMEOUT,
        MAX_AUTO_LOCK_TIMEOUT
    );
    m_settings->set_int("auto-lock-timeout", auto_lock_timeout);

    const bool password_history_enabled = m_password_history_enabled_check.get_active();
    m_settings->set_boolean("password-history-enabled", password_history_enabled);
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

void PreferencesDialog::on_auto_lock_enabled_toggled() noexcept {
    const bool enabled = m_auto_lock_enabled_check.get_active();
    m_auto_lock_timeout_label.set_sensitive(enabled);
    m_auto_lock_timeout_spin.set_sensitive(enabled);
    m_auto_lock_timeout_suffix.set_sensitive(enabled);
}

void PreferencesDialog::on_response([[maybe_unused]] const int response_id) noexcept {
    if (response_id == Gtk::ResponseType::APPLY) {
        save_settings();
        hide();  // Close dialog after applying settings
    } else {
        // Cancel - just close without saving
        hide();
    }
}
