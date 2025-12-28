// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "PreferencesDialog.h"
#include "../../core/VaultManager.h"
#include "../../utils/SettingsValidator.h"
#include <stdexcept>
#include <cstdlib>  // for std::getenv

PreferencesDialog::PreferencesDialog(Gtk::Window& parent, VaultManager* vault_manager)
    : Gtk::Dialog("Preferences", parent, false),  // false = non-modal for faster response
      m_vault_manager(vault_manager),
      m_main_box(Gtk::Orientation::HORIZONTAL, 0),
      m_appearance_box(Gtk::Orientation::VERTICAL, 18),
      m_color_scheme_box(Gtk::Orientation::HORIZONTAL, 12),
      m_color_scheme_label("Color scheme:"),
      m_account_security_box(Gtk::Orientation::VERTICAL, 18),
      m_clipboard_timeout_box(Gtk::Orientation::HORIZONTAL, 12),
      m_clipboard_timeout_label("Clear clipboard after:"),
      m_clipboard_timeout_suffix(" seconds"),
      m_account_password_history_check("Prevent account password reuse"),
      m_account_password_history_limit_box(Gtk::Orientation::HORIZONTAL, 12),
      m_account_password_history_limit_label("Remember up to"),
      m_account_password_history_limit_suffix(" previous passwords per account"),
      m_undo_redo_enabled_check("Enable undo/redo (Ctrl+Z/Ctrl+Shift+Z)"),
      m_undo_history_limit_box(Gtk::Orientation::HORIZONTAL, 12),
      m_undo_history_limit_label("Keep up to"),
      m_undo_history_limit_suffix(" operations"),
      m_vault_security_box(Gtk::Orientation::VERTICAL, 18),
      m_auto_lock_enabled_check("Enable auto-lock after inactivity"),
      m_auto_lock_timeout_box(Gtk::Orientation::HORIZONTAL, 12),
      m_auto_lock_timeout_label("Lock timeout:"),
      m_auto_lock_timeout_suffix(" seconds"),
      m_vault_password_history_box(Gtk::Orientation::VERTICAL, 6),
      m_vault_policy_label("Current vault policy: N/A"),
      m_current_user_label("No user logged in"),
      m_history_count_label("Password history: N/A"),
      m_clear_history_button("Clear My Password History"),
      m_clear_history_warning("⚠ This will permanently delete all saved password history for your account"),
      m_vault_password_history_default_box(Gtk::Orientation::HORIZONTAL, 12),
      m_vault_password_history_default_label("Remember up to"),
      m_vault_password_history_default_suffix(" previous passwords per user"),
      m_vault_password_history_default_help("Default setting for newly created vaults"),
      m_fips_mode_check("Enable FIPS-140-3 mode (requires restart)"),
      m_storage_box(Gtk::Orientation::VERTICAL, 18),
      m_rs_section_title("<b>Error Correction</b>"),
      m_rs_description("Protect vault files from corruption on unreliable storage"),
      m_rs_enabled_check("Enable Reed-Solomon error correction for new vaults"),
      m_redundancy_box(Gtk::Orientation::HORIZONTAL, 12),
      m_redundancy_label("Redundancy:"),
      m_redundancy_suffix("%"),
      m_redundancy_help("Higher values provide more protection but increase file size"),
      m_apply_to_current_check("Apply to current vault (not defaults)"),
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

    m_account_password_history_check.signal_toggled().connect(
        sigc::mem_fun(*this, &PreferencesDialog::on_account_password_history_toggled));

    m_undo_redo_enabled_check.signal_toggled().connect(
        sigc::mem_fun(*this, &PreferencesDialog::on_undo_redo_enabled_toggled));

    m_clear_history_button.signal_clicked().connect(
        sigc::mem_fun(*this, &PreferencesDialog::on_clear_password_history_clicked));

    // Defer vault password history UI update until dialog is shown (lazy loading)
    // This prevents slow dialog opening when vault has many users
    signal_show().connect(sigc::mem_fun(*this, &PreferencesDialog::on_dialog_shown));

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
    setup_account_security_page();
    setup_vault_security_page();
    setup_storage_page();

    // Add main box to dialog content area
    get_content_area()->append(m_main_box);

    // Apply CSS to ensure proper spacing between content and buttons (GNOME HIG: 12px)
    auto css_provider = Gtk::CssProvider::create();
    css_provider->load_from_data("dialog > box > box { margin-bottom: 12px; }");
    get_style_context()->add_provider(
        css_provider,
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );

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

void PreferencesDialog::setup_account_security_page() {
    m_account_security_box.set_margin_start(18);
    m_account_security_box.set_margin_end(18);
    m_account_security_box.set_margin_top(18);
    m_account_security_box.set_margin_bottom(18);

    // Add informational note at top of page
    auto* info_label = Gtk::make_managed<Gtk::Label>();
    if (m_vault_manager && m_vault_manager->is_vault_open()) {
        info_label->set_markup("<span size='small'>ℹ️  Settings for the current vault only (defaults not affected)</span>");
    } else {
        info_label->set_markup("<span size='small'>ℹ️  These settings will be used as defaults for new vaults</span>");
    }
    info_label->set_halign(Gtk::Align::START);
    info_label->set_wrap(true);
    info_label->add_css_class("dim-label");
    info_label->set_margin_bottom(12);
    m_account_security_box.append(*info_label);

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

    m_account_security_box.append(*clipboard_section);

    // Account Password History section
    auto* account_pwd_history_section = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    account_pwd_history_section->set_margin_top(24);

    auto* account_pwd_history_title = Gtk::make_managed<Gtk::Label>("Account Password History");
    account_pwd_history_title->set_halign(Gtk::Align::START);
    account_pwd_history_title->add_css_class("heading");
    account_pwd_history_section->append(*account_pwd_history_title);

    auto* account_pwd_history_desc = Gtk::make_managed<Gtk::Label>(
        "Prevent reusing passwords when updating account entries (Gmail, GitHub, etc.)");
    account_pwd_history_desc->set_halign(Gtk::Align::START);
    account_pwd_history_desc->add_css_class("dim-label");
    account_pwd_history_desc->set_wrap(true);
    account_pwd_history_desc->set_max_width_chars(60);
    account_pwd_history_section->append(*account_pwd_history_desc);

    account_pwd_history_section->append(m_account_password_history_check);

    // Account password history limit controls
    m_account_password_history_limit_box.set_orientation(Gtk::Orientation::HORIZONTAL);
    m_account_password_history_limit_box.set_spacing(12);
    m_account_password_history_limit_box.set_margin_top(12);
    m_account_password_history_limit_box.set_margin_start(24);

    m_account_password_history_limit_label.set_text("Remember up to");
    m_account_password_history_limit_label.set_halign(Gtk::Align::START);
    m_account_password_history_limit_box.append(m_account_password_history_limit_label);

    auto account_pwd_history_adjustment = Gtk::Adjustment::create(5.0, 0.0, 24.0, 1.0, 5.0, 0.0);
    m_account_password_history_limit_spin.set_adjustment(account_pwd_history_adjustment);
    m_account_password_history_limit_spin.set_digits(0);
    m_account_password_history_limit_spin.set_value(5.0);
    m_account_password_history_limit_box.append(m_account_password_history_limit_spin);

    m_account_password_history_limit_suffix.set_text("previous passwords per account");
    m_account_password_history_limit_suffix.set_halign(Gtk::Align::START);
    m_account_password_history_limit_box.append(m_account_password_history_limit_suffix);

    m_account_password_history_limit_box.set_halign(Gtk::Align::START);
    account_pwd_history_section->append(m_account_password_history_limit_box);

    m_account_security_box.append(*account_pwd_history_section);

    // Undo/Redo section
    auto* undo_redo_section = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    undo_redo_section->set_margin_top(24);

    auto* undo_redo_title = Gtk::make_managed<Gtk::Label>("Undo/Redo");
    undo_redo_title->set_halign(Gtk::Align::START);
    undo_redo_title->add_css_class("heading");
    undo_redo_section->append(*undo_redo_title);

    auto* undo_redo_desc = Gtk::make_managed<Gtk::Label>("Allow undoing vault operations");
    undo_redo_desc->set_halign(Gtk::Align::START);
    undo_redo_desc->add_css_class("dim-label");
    undo_redo_desc->set_wrap(true);
    undo_redo_section->append(*undo_redo_desc);

    undo_redo_section->append(m_undo_redo_enabled_check);

    m_undo_redo_warning.set_markup("<span size='small'>⚠️  When disabled, operations cannot be undone but passwords are not kept in memory for undo history</span>");
    m_undo_redo_warning.set_halign(Gtk::Align::START);
    m_undo_redo_warning.set_wrap(true);
    m_undo_redo_warning.set_max_width_chars(60);
    m_undo_redo_warning.add_css_class("dim-label");
    m_undo_redo_warning.set_margin_start(24);
    undo_redo_section->append(m_undo_redo_warning);

    // Undo history limit controls
    m_undo_history_limit_box.set_orientation(Gtk::Orientation::HORIZONTAL);
    m_undo_history_limit_box.set_spacing(12);
    m_undo_history_limit_box.set_margin_top(12);
    m_undo_history_limit_box.set_margin_start(24);

    m_undo_history_limit_label.set_text("Keep up to");
    m_undo_history_limit_label.set_halign(Gtk::Align::START);
    m_undo_history_limit_box.append(m_undo_history_limit_label);

    auto undo_history_adjustment = Gtk::Adjustment::create(50.0, 1.0, 100.0, 1.0, 10.0, 0.0);
    m_undo_history_limit_spin.set_adjustment(undo_history_adjustment);
    m_undo_history_limit_spin.set_digits(0);
    m_undo_history_limit_spin.set_value(50.0);
    m_undo_history_limit_box.append(m_undo_history_limit_spin);

    m_undo_history_limit_suffix.set_text("operations");
    m_undo_history_limit_suffix.set_halign(Gtk::Align::START);
    m_undo_history_limit_box.append(m_undo_history_limit_suffix);

    m_undo_history_limit_box.set_halign(Gtk::Align::START);
    undo_redo_section->append(m_undo_history_limit_box);

    m_account_security_box.append(*undo_redo_section);

    // Add page to stack
    m_stack.add(m_account_security_box, "account-security", "Account Security");

    // Disable account security page for non-admin users (V2 multi-user vaults)
    // Account security settings are vault-level policies that only admins can modify
    if (m_vault_manager && m_vault_manager->is_vault_open()) {
        auto session = m_vault_manager->get_current_user_session();
        if (session && session->role != KeepTower::UserRole::ADMINISTRATOR) {
            auto stack_page = m_stack.get_page(m_account_security_box);
            if (stack_page) {
                stack_page->set_visible(false);
            }
        }
    }
}

void PreferencesDialog::setup_vault_security_page() {
    m_vault_security_box.set_margin_start(18);
    m_vault_security_box.set_margin_end(18);
    m_vault_security_box.set_margin_top(18);
    m_vault_security_box.set_margin_bottom(18);

    // Add informational note at top of page
    auto* info_label = Gtk::make_managed<Gtk::Label>();
    if (m_vault_manager && m_vault_manager->is_vault_open()) {
        info_label->set_markup("<span size='small'>ℹ️  Settings for the current vault only (defaults not affected)</span>");
    } else {
        info_label->set_markup("<span size='small'>ℹ️  These settings will be used as defaults for new vaults</span>");
    }
    info_label->set_halign(Gtk::Align::START);
    info_label->set_wrap(true);
    info_label->add_css_class("dim-label");
    info_label->set_margin_bottom(12);
    m_vault_security_box.append(*info_label);

    // Auto-lock section
    auto* auto_lock_section = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);

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

    m_vault_security_box.append(*auto_lock_section);

    // ========================================================================
    // FIPS-140-3 Compliance Section
    // ========================================================================
    /**
     * @brief FIPS-140-3 mode configuration UI
     *
     * Provides user interface for enabling/disabling FIPS-140-3 validated
     * cryptographic operations. FIPS mode requires OpenSSL 3.5+ with the
     * FIPS module installed and properly configured.
     *
     * **Section Components:**
     * 1. Title: "FIPS-140-3 Compliance"
     * 2. Description: "Use FIPS-140-3 validated cryptographic operations"
     * 3. Checkbox: Enable/disable FIPS mode (requires restart)
     * 4. Status Label: Shows FIPS provider availability (✓ or ⚠️)
     * 5. Warning Label: Restart requirement notice
     *
     * **Dynamic Behavior:**
     * - Queries VaultManager::is_fips_available() at dialog creation
     * - If available: Checkbox enabled, shows "✓ FIPS module available"
     * - If unavailable: Checkbox disabled, shows "⚠️ FIPS module not available"
     *
     * **User Workflow:**
     * 1. User opens Preferences → Security
     * 2. User sees FIPS availability status
     * 3. If available, user can check/uncheck FIPS checkbox
     * 4. User clicks Apply to save preference
     * 5. User restarts application for FIPS mode to take effect
     *
     * **Implementation Details:**
     * - Status determined once at dialog creation (not dynamic)
     * - FIPS availability is process-wide and doesn't change at runtime
     * - Checkbox state loaded from GSettings in load_settings()
     * - Checkbox state saved to GSettings in save_settings()
     * - Application.cc reads GSettings and initializes FIPS at startup
     *
     * @note FIPS provider availability depends on OpenSSL configuration
     * @note Changes require application restart for full effect
     * @see VaultManager::init_fips_mode() for FIPS initialization
     * @see VaultManager::is_fips_available() for availability check
     * @see load_settings() for reading FIPS preference
     * @see save_settings() for persisting FIPS preference
     */
    auto* fips_section = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    fips_section->set_margin_top(24);

    // Section title with "heading" CSS class for emphasis
    auto* fips_title = Gtk::make_managed<Gtk::Label>("FIPS-140-3 Compliance");
    fips_title->set_halign(Gtk::Align::START);
    fips_title->add_css_class("heading");
    fips_section->append(*fips_title);

    // Brief description of FIPS mode purpose
    auto* fips_desc = Gtk::make_managed<Gtk::Label>("Use FIPS-140-3 validated cryptographic operations");
    fips_desc->set_halign(Gtk::Align::START);
    fips_desc->add_css_class("dim-label");
    fips_desc->set_wrap(true);
    fips_section->append(*fips_desc);

    // Main FIPS mode toggle checkbox
    // Label includes "(requires restart)" to set user expectations
    fips_section->append(m_fips_mode_check);

    // FIPS provider availability status label
    // Only shown when FIPS is NOT available (warning message)
    m_fips_status_label.set_halign(Gtk::Align::START);
    m_fips_status_label.set_wrap(true);
    m_fips_status_label.set_max_width_chars(60);
    m_fips_status_label.set_margin_start(24);  // Indent under checkbox
    m_fips_status_label.set_margin_top(6);

    // Restart warning label
    // Only shown when FIPS is available (can be toggled)
    m_fips_restart_warning.set_halign(Gtk::Align::START);
    m_fips_restart_warning.set_wrap(true);
    m_fips_restart_warning.set_max_width_chars(60);
    m_fips_restart_warning.set_margin_start(24);  // Indent under checkbox
    m_fips_restart_warning.set_margin_top(6);

    // Dynamic availability detection using VaultManager API
    // This query happens once at dialog creation time
    if (VaultManager::is_fips_available()) {
        // FIPS provider loaded successfully - show restart warning
        m_fips_restart_warning.set_markup("<span size='small'>⚠️  Changes require application restart to take effect</span>");
        m_fips_restart_warning.add_css_class("warning");
        fips_section->append(m_fips_restart_warning);
        // Don't show status label when FIPS is available
    } else {
        // FIPS provider not available - show warning (NOT dimmed) and disable checkbox
        m_fips_status_label.set_markup("<span size='small' foreground='#e01b24'>⚠️  FIPS module not available (requires OpenSSL FIPS configuration)</span>");
        fips_section->append(m_fips_status_label);
        m_fips_mode_check.set_sensitive(false);  // Prevent enabling unsupported mode
        // Don't show restart warning when FIPS is not available
    }

    m_vault_security_box.append(*fips_section);

    // ========================================================================
    // User Password History Section (only visible when vault is open)
    // ========================================================================
    m_vault_password_history_box.set_orientation(Gtk::Orientation::VERTICAL);
    m_vault_password_history_box.set_spacing(6);
    m_vault_password_history_box.set_margin_top(24);

    auto* history_title = Gtk::make_managed<Gtk::Label>("User Password History");
    history_title->set_halign(Gtk::Align::START);
    history_title->add_css_class("heading");
    m_vault_password_history_box.append(*history_title);

    auto* history_desc = Gtk::make_managed<Gtk::Label>("Track previous user passwords to prevent reuse");
    history_desc->set_halign(Gtk::Align::START);
    history_desc->add_css_class("dim-label");
    history_desc->set_wrap(true);
    m_vault_password_history_box.append(*history_desc);

    m_vault_policy_label.set_halign(Gtk::Align::START);
    m_vault_policy_label.set_margin_top(12);
    m_vault_password_history_box.append(m_vault_policy_label);

    m_current_user_label.set_halign(Gtk::Align::START);
    m_current_user_label.set_margin_top(6);
    m_vault_password_history_box.append(m_current_user_label);

    m_history_count_label.set_halign(Gtk::Align::START);
    m_history_count_label.set_margin_top(6);
    m_vault_password_history_box.append(m_history_count_label);

    m_clear_history_button.set_label("Clear My Password History");
    m_clear_history_button.set_halign(Gtk::Align::START);
    m_clear_history_button.set_margin_top(12);
    m_vault_password_history_box.append(m_clear_history_button);

    m_clear_history_warning.set_markup("<span size='small'>⚠️  This will delete all your password history. You will be able to reuse old passwords.</span>");
    m_clear_history_warning.set_halign(Gtk::Align::START);
    m_clear_history_warning.set_wrap(true);
    m_clear_history_warning.set_max_width_chars(60);
    m_clear_history_warning.add_css_class("dim-label");
    m_clear_history_warning.set_margin_top(6);
    m_vault_password_history_box.append(m_clear_history_warning);

    m_vault_security_box.append(m_vault_password_history_box);

    // ========================================================================
    // Vault User Password History Default (only visible when NO vault is open)
    // ========================================================================
    auto* default_history_section = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    default_history_section->set_margin_top(24);

    auto* default_history_title = Gtk::make_managed<Gtk::Label>("User Password History (Default for New Vaults)");
    default_history_title->set_halign(Gtk::Align::START);
    default_history_title->add_css_class("heading");
    default_history_section->append(*default_history_title);

    auto* default_history_desc = Gtk::make_managed<Gtk::Label>(
        "Set default policy for preventing vault user authentication password reuse");
    default_history_desc->set_halign(Gtk::Align::START);
    default_history_desc->add_css_class("dim-label");
    default_history_desc->set_wrap(true);
    default_history_desc->set_max_width_chars(60);
    default_history_section->append(*default_history_desc);

    m_vault_password_history_default_box.set_orientation(Gtk::Orientation::HORIZONTAL);
    m_vault_password_history_default_box.set_spacing(12);
    m_vault_password_history_default_box.set_margin_top(12);

    m_vault_password_history_default_label.set_halign(Gtk::Align::START);
    m_vault_password_history_default_box.append(m_vault_password_history_default_label);

    auto vault_pwd_history_adjustment = Gtk::Adjustment::create(5.0, 0.0, 24.0, 1.0, 5.0, 0.0);
    m_vault_password_history_default_spin.set_adjustment(vault_pwd_history_adjustment);
    m_vault_password_history_default_spin.set_digits(0);
    m_vault_password_history_default_spin.set_value(5.0);
    m_vault_password_history_default_box.append(m_vault_password_history_default_spin);

    m_vault_password_history_default_suffix.set_halign(Gtk::Align::START);
    m_vault_password_history_default_box.append(m_vault_password_history_default_suffix);

    m_vault_password_history_default_box.set_halign(Gtk::Align::START);
    default_history_section->append(m_vault_password_history_default_box);

    m_vault_password_history_default_help.set_text("0 = disabled (password reuse allowed)");
    m_vault_password_history_default_help.set_halign(Gtk::Align::START);
    m_vault_password_history_default_help.add_css_class("dim-label");
    m_vault_password_history_default_help.set_margin_top(6);
    default_history_section->append(m_vault_password_history_default_help);

    m_vault_security_box.append(*default_history_section);

    // Add page to stack
    m_stack.add(m_vault_security_box, "vault-security", "Vault Security");

    // Disable vault security page for non-admin users (V2 multi-user vaults)
    if (m_vault_manager && m_vault_manager->is_vault_open()) {
        auto session = m_vault_manager->get_current_user_session();
        if (session && session->role != KeepTower::UserRole::ADMINISTRATOR) {
            auto stack_page = m_stack.get_page(m_vault_security_box);
            if (stack_page) {
                stack_page->set_visible(false);
            }
        }
    }
}

void PreferencesDialog::setup_storage_page() {
    m_storage_box.set_margin_start(18);
    m_storage_box.set_margin_end(18);
    m_storage_box.set_margin_top(18);
    m_storage_box.set_margin_bottom(18);

    // Add informational note at top of page
    auto* info_label = Gtk::make_managed<Gtk::Label>();
    if (m_vault_manager && m_vault_manager->is_vault_open()) {
        info_label->set_markup("<span size='small'>ℹ️  Showing settings for the current vault (use checkbox to change defaults for new vaults)</span>");
    } else {
        info_label->set_markup("<span size='small'>ℹ️  These settings will be used as defaults for new vaults</span>");
    }
    info_label->set_halign(Gtk::Align::START);
    info_label->set_wrap(true);
    info_label->set_max_width_chars(60);
    info_label->add_css_class("dim-label");
    info_label->set_margin_bottom(12);
    m_storage_box.append(*info_label);

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

    // Disable storage page for non-admin users (V2 multi-user vaults)
    if (m_vault_manager && m_vault_manager->is_vault_open()) {
        auto session = m_vault_manager->get_current_user_session();
        if (session && session->role != KeepTower::UserRole::ADMINISTRATOR) {
            // Hide storage page from non-admin users
            auto stack_page = m_stack.get_page(m_storage_box);
            if (stack_page) {
                stack_page->set_visible(false);
            }
        }
    }
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
    // If vault is open, load vault-specific settings; otherwise use GSettings defaults
    int clipboard_timeout;
    int auto_lock_timeout;
    bool auto_lock_enabled;

    if (m_vault_manager && m_vault_manager->is_vault_open()) {
        // Load from current vault
        clipboard_timeout = m_vault_manager->get_clipboard_timeout();
        auto_lock_enabled = m_vault_manager->get_auto_lock_enabled();
        auto_lock_timeout = m_vault_manager->get_auto_lock_timeout();

        // If vault has no clipboard setting (0), fall back to GSettings defaults
        if (clipboard_timeout == 0) {
            clipboard_timeout = SettingsValidator::get_clipboard_timeout(m_settings);
        }
        // If vault has no auto-lock timeout (0), fall back to GSettings defaults
        if (auto_lock_timeout == 0) {
            auto_lock_timeout = SettingsValidator::get_auto_lock_timeout(m_settings);
        }
    } else {
        // Load from GSettings (defaults for new vaults)
        clipboard_timeout = SettingsValidator::get_clipboard_timeout(m_settings);
        auto_lock_enabled = SettingsValidator::is_auto_lock_enabled(m_settings);
        auto_lock_timeout = SettingsValidator::get_auto_lock_timeout(m_settings);
    }

    m_clipboard_timeout_spin.set_value(clipboard_timeout);
    m_auto_lock_enabled_check.set_active(auto_lock_enabled);
    m_auto_lock_timeout_spin.set_value(auto_lock_timeout);

    // Update auto-lock controls sensitivity
    m_auto_lock_timeout_label.set_sensitive(auto_lock_enabled);
    m_auto_lock_timeout_spin.set_sensitive(auto_lock_enabled);
    m_auto_lock_timeout_suffix.set_sensitive(auto_lock_enabled);

    // Load account password history settings
    // If vault is open, load vault-specific settings; otherwise use GSettings defaults
    bool account_pwd_history_enabled;
    int account_pwd_history_limit;

    if (m_vault_manager && m_vault_manager->is_vault_open()) {
        // Load from current vault
        account_pwd_history_enabled = m_vault_manager->get_account_password_history_enabled();
        account_pwd_history_limit = m_vault_manager->get_account_password_history_limit();

        // If vault has no setting (0), fall back to GSettings defaults
        if (account_pwd_history_limit == 0) {
            account_pwd_history_enabled = SettingsValidator::is_password_history_enabled(m_settings);
            account_pwd_history_limit = SettingsValidator::get_password_history_limit(m_settings);
        }
    } else {
        // Load from GSettings (defaults for new vaults)
        account_pwd_history_enabled = SettingsValidator::is_password_history_enabled(m_settings);
        account_pwd_history_limit = SettingsValidator::get_password_history_limit(m_settings);
    }

    m_account_password_history_check.set_active(account_pwd_history_enabled);
    m_account_password_history_limit_spin.set_value(account_pwd_history_limit);

    // Update account password history controls sensitivity
    m_account_password_history_limit_label.set_sensitive(account_pwd_history_enabled);
    m_account_password_history_limit_spin.set_sensitive(account_pwd_history_enabled);
    m_account_password_history_limit_suffix.set_sensitive(account_pwd_history_enabled);

    // Load undo/redo settings
    // If vault is open, load vault-specific settings; otherwise use GSettings defaults
    bool undo_redo_enabled;
    int undo_history_limit;

    if (m_vault_manager && m_vault_manager->is_vault_open()) {
        // Load from current vault
        undo_redo_enabled = m_vault_manager->get_undo_redo_enabled();
        undo_history_limit = m_vault_manager->get_undo_history_limit();

        // If vault has no setting (0), fall back to GSettings defaults
        if (undo_history_limit == 0) {
            undo_redo_enabled = m_settings->get_boolean("undo-redo-enabled");
            undo_history_limit = m_settings->get_int("undo-history-limit");
        }
    } else {
        // Load from GSettings (defaults for new vaults)
        undo_redo_enabled = m_settings->get_boolean("undo-redo-enabled");
        undo_history_limit = m_settings->get_int("undo-history-limit");
    }

    undo_history_limit = std::clamp(undo_history_limit, 1, 100);
    m_undo_redo_enabled_check.set_active(undo_redo_enabled);
    m_undo_history_limit_spin.set_value(undo_history_limit);

    // Update undo/redo controls sensitivity
    m_undo_history_limit_label.set_sensitive(undo_redo_enabled);
    m_undo_history_limit_spin.set_sensitive(undo_redo_enabled);
    m_undo_history_limit_suffix.set_sensitive(undo_redo_enabled);

    // Load vault user password history default (only relevant when no vault open)
    int vault_pwd_history_depth = m_settings->get_int("vault-user-password-history-depth");
    vault_pwd_history_depth = std::clamp(vault_pwd_history_depth, 0, 24);
    m_vault_password_history_default_spin.set_value(vault_pwd_history_depth);

    /**
     * @brief Load FIPS-140-3 mode preference from GSettings
     *
     * Reads the user's FIPS mode preference and updates the checkbox state.
     * This preference is read by Application.cc at startup to initialize
     * the OpenSSL FIPS provider.
     *
     * **GSettings Key:** "fips-mode-enabled" (boolean, default: false)
     *
     * **Data Flow:**
     * 1. User preference stored in GSettings (persistent)
     * 2. PreferencesDialog reads preference → updates checkbox
     * 3. Application.cc reads same preference at startup
     * 4. Application calls VaultManager::init_fips_mode(preference)
     *
     * **Default Value:** false (FIPS mode disabled by default)
     * User must explicitly opt-in to FIPS-140-3 compliance mode.
     *
     * @note Checkbox may be disabled if FIPS provider unavailable
     * @see save_settings() for persisting FIPS preference changes
     * @see Application::on_startup() for FIPS initialization from settings
     */
    bool fips_enabled = m_settings->get_boolean("fips-mode-enabled");

    // Only set checkbox state if FIPS is available
    // If FIPS unavailable, checkbox is disabled in setup_ui()
    if (VaultManager::is_fips_available()) {
        m_fips_mode_check.set_active(fips_enabled);
    } else {
        // FIPS not available - ensure checkbox is disabled and unchecked
        m_fips_mode_check.set_sensitive(false);
        m_fips_mode_check.set_active(false);
    }
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
        // Apply to current vault ONLY (do NOT save to defaults)
        m_vault_manager->set_reed_solomon_enabled(rs_enabled);
        m_vault_manager->set_rs_redundancy_percent(rs_redundancy);
    } else {
        // Save to preferences (defaults for new vaults only)
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
    // CRITICAL: If vault is open, save ONLY to vault (not to GSettings)
    // If vault is closed, save to GSettings as defaults for new vaults
    const int clipboard_timeout = std::clamp(
        static_cast<int>(m_clipboard_timeout_spin.get_value()),
        MIN_CLIPBOARD_TIMEOUT,
        MAX_CLIPBOARD_TIMEOUT
    );

    if (m_vault_manager && m_vault_manager->is_vault_open()) {
        // Save ONLY to current vault
        m_vault_manager->set_clipboard_timeout(clipboard_timeout);
    } else {
        // Save to GSettings as default for new vaults
        m_settings->set_int("clipboard-clear-timeout", clipboard_timeout);
    }

    // Save account password history settings
    // CRITICAL: If vault is open, save ONLY to vault (not to GSettings)
    // If vault is closed, save to GSettings as defaults for new vaults
    const bool account_pwd_history_enabled = m_account_password_history_check.get_active();
    const int account_pwd_history_limit = std::clamp(
        static_cast<int>(m_account_password_history_limit_spin.get_value()),
        MIN_PASSWORD_HISTORY_LIMIT,
        MAX_PASSWORD_HISTORY_LIMIT
    );

    if (m_vault_manager && m_vault_manager->is_vault_open()) {
        // Save ONLY to current vault
        m_vault_manager->set_account_password_history_enabled(account_pwd_history_enabled);
        m_vault_manager->set_account_password_history_limit(account_pwd_history_limit);
    } else {
        // Save to GSettings as defaults for new vaults
        m_settings->set_boolean("password-history-enabled", account_pwd_history_enabled);
        m_settings->set_int("password-history-limit", account_pwd_history_limit);
    }

    // Save auto-lock settings
    // CRITICAL: If vault is open, save ONLY to vault (not to GSettings)
    // If vault is closed, save to GSettings as defaults for new vaults
    const bool auto_lock_enabled = m_auto_lock_enabled_check.get_active();
    const int auto_lock_timeout = std::clamp(
        static_cast<int>(m_auto_lock_timeout_spin.get_value()),
        MIN_AUTO_LOCK_TIMEOUT,
        MAX_AUTO_LOCK_TIMEOUT
    );

    if (m_vault_manager && m_vault_manager->is_vault_open()) {
        // Save ONLY to current vault
        m_vault_manager->set_auto_lock_enabled(auto_lock_enabled);
        m_vault_manager->set_auto_lock_timeout(auto_lock_timeout);
    } else {
        // Save to GSettings as defaults for new vaults
        m_settings->set_boolean("auto-lock-enabled", auto_lock_enabled);
        m_settings->set_int("auto-lock-timeout", auto_lock_timeout);
    }

    // Save undo/redo settings
    // CRITICAL: If vault is open, save ONLY to vault (not to GSettings)
    // If vault is closed, save to GSettings as defaults for new vaults
    const bool undo_redo_enabled = m_undo_redo_enabled_check.get_active();
    const int undo_history_limit = std::clamp(
        static_cast<int>(m_undo_history_limit_spin.get_value()),
        1,
        100
    );

    if (m_vault_manager && m_vault_manager->is_vault_open()) {
        // Save ONLY to current vault
        m_vault_manager->set_undo_redo_enabled(undo_redo_enabled);
        m_vault_manager->set_undo_history_limit(undo_history_limit);
    } else {
        // Save to GSettings as defaults for new vaults
        m_settings->set_boolean("undo-redo-enabled", undo_redo_enabled);
        m_settings->set_int("undo-history-limit", undo_history_limit);
    }

    // Save vault user password history default (only relevant when no vault open)
    if (!m_vault_manager || !m_vault_manager->is_vault_open()) {
        const int vault_pwd_history_depth = std::clamp(
            static_cast<int>(m_vault_password_history_default_spin.get_value()),
            0,
            24
        );
        m_settings->set_int("vault-user-password-history-depth", vault_pwd_history_depth);
    }

    /**
     * @brief Save FIPS-140-3 mode preference to GSettings
     *
     * Persists the user's FIPS mode preference for use at next application
     * startup. The preference is read by Application.cc to determine whether
     * to initialize OpenSSL in FIPS mode.
     *
     * **GSettings Key:** "fips-mode-enabled" (boolean)
     *
     * **Lifecycle:**
     * 1. User toggles FIPS checkbox in preferences
     * 2. User clicks "Apply" button
     * 3. This method saves checkbox state to GSettings
     * 4. User sees restart warning
     * 5. User restarts application
     * 6. Application.cc reads preference and initializes FIPS accordingly
     *
     * **Restart Requirement:**
     * FIPS mode changes don't take full effect until application restart
     * because OpenSSL provider initialization must occur before any crypto
     * operations. While VaultManager::set_fips_mode() supports runtime
     * switching, restart ensures consistent provider state across all
     * cryptographic contexts.
     *
     * **Security Implications:**
     * - Enabling FIPS: All crypto uses FIPS-validated algorithms (compliant)
     * - Disabling FIPS: All crypto uses standard OpenSSL algorithms (default)
     *
     * @note Changes are persisted immediately but don't affect current session
     * @see load_settings() for reading FIPS preference
     * @see Application::on_startup() for FIPS initialization at startup
     * @see VaultManager::init_fips_mode() for provider initialization
     */
    const bool fips_enabled = m_fips_mode_check.get_active();
    m_settings->set_boolean("fips-mode-enabled", fips_enabled);
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
        // Default: follow system preference
        // Try to read system color scheme from GNOME desktop settings
        bool applied = false;
        try {
            auto gio_settings = Gio::Settings::create("org.gnome.desktop.interface");
            auto color_scheme = gio_settings->get_string("color-scheme");
            // color-scheme can be: "default", "prefer-dark", "prefer-light"
            settings->property_gtk_application_prefer_dark_theme() = (color_scheme == "prefer-dark");
            applied = true;
        } catch (const Glib::Error& e) {
            // Schema not available or error reading it
        } catch (...) {
            // Other errors
        }

        if (!applied) {
            // Fallback: Check GTK_THEME environment variable or just unset the override
            // This allows GTK to follow its own detection
            const char* gtk_theme = std::getenv("GTK_THEME");
            if (gtk_theme && std::string(gtk_theme).find("dark") != std::string::npos) {
                settings->property_gtk_application_prefer_dark_theme() = true;
            } else {
                // Last resort: assume light theme
                settings->property_gtk_application_prefer_dark_theme() = false;
            }
        }
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

void PreferencesDialog::on_account_password_history_toggled() noexcept {
    const bool enabled = m_account_password_history_check.get_active();
    m_account_password_history_limit_label.set_sensitive(enabled);
    m_account_password_history_limit_spin.set_sensitive(enabled);
    m_account_password_history_limit_suffix.set_sensitive(enabled);
}

void PreferencesDialog::on_undo_redo_enabled_toggled() noexcept {
    const bool enabled = m_undo_redo_enabled_check.get_active();
    m_undo_history_limit_label.set_sensitive(enabled);
    m_undo_history_limit_spin.set_sensitive(enabled);
    m_undo_history_limit_suffix.set_sensitive(enabled);
}

void PreferencesDialog::on_dialog_shown() noexcept {
    // Lazy load vault password history UI only once (expensive operation if many users)
    if (!m_history_ui_loaded) {
        m_history_ui_loaded = true;
        update_vault_password_history_ui();
    }
}

void PreferencesDialog::update_vault_password_history_ui() noexcept {
    // Check if vault is open
    if (!m_vault_manager || !m_vault_manager->is_vault_open()) {
        // No vault open - show default settings, hide current vault info
        m_vault_password_history_box.set_visible(false);
        m_vault_password_history_default_box.get_parent()->set_visible(true);
        return;
    }

    // Vault open - show current vault info, hide default settings
    m_vault_password_history_box.set_visible(true);
    m_vault_password_history_default_box.get_parent()->set_visible(false);

    // Get vault policy
    const auto policy_opt = m_vault_manager->get_vault_security_policy();
    if (!policy_opt) {
        m_vault_policy_label.set_text("Current vault policy: N/A");
        m_current_user_label.set_text("No policy available");
        m_history_count_label.set_text("Password history: N/A");
        m_clear_history_button.set_sensitive(false);
        return;
    }

    const auto& policy = *policy_opt;
    m_vault_policy_label.set_text(
        "Current vault policy: " + std::to_string(policy.password_history_depth) + " passwords");

    // Get current user session
    const auto session = m_vault_manager->get_current_user_session();
    if (!session) {
        m_current_user_label.set_text("No user logged in");
        m_history_count_label.set_text("Password history: N/A");
        m_clear_history_button.set_sensitive(false);
        return;
    }

    m_current_user_label.set_text("Logged in as: " + Glib::ustring(session->username));

    // Get user's password history count
    const auto users = m_vault_manager->list_users();
    size_t history_count = 0;
    for (const auto& user : users) {
        if (user.username == session->username) {
            history_count = user.password_history.size();
            break;
        }
    }

    m_history_count_label.set_text("Password history: " + std::to_string(history_count) + " entries");

    // Enable clear button only if there's history to clear
    m_clear_history_button.set_sensitive(history_count > 0);
}

void PreferencesDialog::on_clear_password_history_clicked() noexcept {
    // Get current session
    if (!m_vault_manager || !m_vault_manager->is_vault_open()) {
        return;
    }

    const auto session = m_vault_manager->get_current_user_session();
    if (!session) {
        return;
    }

    const std::string username = session->username;

    // Create confirmation dialog on heap (will be deleted in response handler)
    auto* dialog = new Gtk::MessageDialog(
        *this,
        "Clear Password History?",
        false,
        Gtk::MessageType::WARNING,
        Gtk::ButtonsType::OK_CANCEL,
        true
    );

    dialog->set_secondary_text(
        "This will permanently delete all saved password history for user '" +
        username + "'.\n\n" +
        "This action cannot be undone."
    );

    dialog->set_modal(true);

    // Connect response handler (captures this and username)
    dialog->signal_response().connect([this, dialog, username](int response) {
        dialog->hide();

        if (response == Gtk::ResponseType::OK) {
            try {
                // Clear the password history
                auto result = m_vault_manager->clear_user_password_history(username);
                if (!result) {
                    throw std::runtime_error(std::string(KeepTower::to_string(result.error())));
                }

                // Save vault
                if (!m_vault_manager->save_vault()) {
                    throw std::runtime_error("Failed to save vault");
                }

                // Refresh UI
                update_vault_password_history_ui();

                // Show success message
                auto* success_dialog = new Gtk::MessageDialog(
                    *this,
                    "Password history cleared",
                    false,
                    Gtk::MessageType::INFO,
                    Gtk::ButtonsType::OK,
                    true
                );
                success_dialog->set_secondary_text("Password history for '" + username + "' has been cleared.");
                success_dialog->set_modal(true);
                success_dialog->signal_response().connect([success_dialog](int) {
                    delete success_dialog;
                });
                success_dialog->show();

            } catch (const std::exception& e) {
                // Show error message
                auto* error_dialog = new Gtk::MessageDialog(
                    *this,
                    "Failed to clear password history",
                    false,
                    Gtk::MessageType::ERROR,
                    Gtk::ButtonsType::OK,
                    true
                );
                error_dialog->set_secondary_text(std::string(e.what()));
                error_dialog->set_modal(true);
                error_dialog->signal_response().connect([error_dialog](int) {
                    delete error_dialog;
                });
                error_dialog->show();
            }
        }

        // Delete the confirmation dialog
        delete dialog;
    });

    dialog->show();
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
