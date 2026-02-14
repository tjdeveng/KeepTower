// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "VaultSecurityPreferencesPage.h"

#include "../../../core/VaultManager.h"
#include "../../../utils/Log.h"
#include "../../../utils/StringHelpers.h"

#include <algorithm>
#include <stdexcept>

namespace KeepTower::Ui {

namespace {
constexpr int DEFAULT_AUTO_LOCK_TIMEOUT = 300;
constexpr int MIN_AUTO_LOCK_TIMEOUT = 60;
constexpr int MAX_AUTO_LOCK_TIMEOUT = 3600;

}  // namespace

VaultSecurityPreferencesPage::VaultSecurityPreferencesPage(VaultManager* vault_manager)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 18),
      m_vault_manager(vault_manager),
      m_auto_lock_enabled_check("Enable auto-lock after inactivity"),
      m_auto_lock_timeout_box(Gtk::Orientation::HORIZONTAL, 12),
      m_auto_lock_timeout_label("Lock timeout:"),
      m_auto_lock_timeout_suffix(" seconds"),
      m_fips_mode_check("Enable FIPS-140-3 mode (requires restart)"),
      m_vault_password_history_default_box(Gtk::Orientation::HORIZONTAL, 12),
      m_vault_password_history_default_label("Remember up to"),
      m_vault_password_history_default_suffix(" previous passwords per user"),
      m_vault_password_history_default_help("Default setting for newly created vaults"),
      m_vault_password_history_box(Gtk::Orientation::VERTICAL, 6),
      m_vault_policy_label("Current vault policy: N/A"),
      m_current_user_label("No user logged in"),
      m_history_count_label("Password history: N/A"),
      m_clear_history_button("Clear My Password History"),
      m_clear_history_warning("⚠ This will permanently delete all saved password history for your account") {
    set_margin_start(18);
    set_margin_end(18);
    set_margin_top(18);
    set_margin_bottom(18);

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
    append(*info_label);

    // Two-column layout
    m_security_grid = Gtk::make_managed<Gtk::Grid>();
    m_security_grid->set_column_spacing(24);
    m_security_grid->set_row_spacing(0);
    m_security_grid->set_column_homogeneous(false);

    auto* left_column = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 0);
    left_column->set_hexpand(true);
    left_column->set_vexpand(true);
    left_column->set_valign(Gtk::Align::START);

    m_security_right_column = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 0);
    m_security_right_column->set_hexpand(true);
    m_security_right_column->set_vexpand(true);
    m_security_right_column->set_valign(Gtk::Align::START);

    m_security_grid->attach(*left_column, 0, 0, 1, 1);
    m_security_grid->attach(*m_security_right_column, 1, 0, 1, 1);

    append(*m_security_grid);

    // Auto-lock
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
        10.0, 60.0, 0.0);

    m_auto_lock_timeout_spin.set_adjustment(auto_lock_adjustment);
    m_auto_lock_timeout_spin.set_digits(0);
    m_auto_lock_timeout_spin.set_value(DEFAULT_AUTO_LOCK_TIMEOUT);
    m_auto_lock_timeout_box.append(m_auto_lock_timeout_spin);

    m_auto_lock_timeout_suffix.set_halign(Gtk::Align::START);
    m_auto_lock_timeout_box.append(m_auto_lock_timeout_suffix);

    m_auto_lock_timeout_box.set_halign(Gtk::Align::START);
    auto_lock_section->append(m_auto_lock_timeout_box);

    left_column->append(*auto_lock_section);

    // FIPS
    auto* fips_section = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    fips_section->set_margin_top(24);

    auto* fips_title = Gtk::make_managed<Gtk::Label>("FIPS-140-3 Compliance");
    fips_title->set_halign(Gtk::Align::START);
    fips_title->add_css_class("heading");
    fips_section->append(*fips_title);

    auto* fips_desc = Gtk::make_managed<Gtk::Label>("Use FIPS-140-3 validated cryptographic operations");
    fips_desc->set_halign(Gtk::Align::START);
    fips_desc->add_css_class("dim-label");
    fips_desc->set_wrap(true);
    fips_section->append(*fips_desc);

    fips_section->append(m_fips_mode_check);

    m_fips_status_label.set_halign(Gtk::Align::START);
    m_fips_status_label.set_wrap(true);
    m_fips_status_label.set_max_width_chars(60);
    m_fips_status_label.set_margin_start(24);
    m_fips_status_label.set_margin_top(6);

    m_fips_restart_warning.set_halign(Gtk::Align::START);
    m_fips_restart_warning.set_wrap(true);
    m_fips_restart_warning.set_max_width_chars(60);
    m_fips_restart_warning.set_margin_start(24);
    m_fips_restart_warning.set_margin_top(6);

    if (VaultManager::is_fips_available()) {
        m_fips_restart_warning.set_markup("<span size='small'>⚠️  Changes require application restart to take effect</span>");
        m_fips_restart_warning.add_css_class("warning");
        fips_section->append(m_fips_restart_warning);
    } else {
        m_fips_status_label.set_markup("<span size='small' foreground='#e01b24'>⚠️  FIPS module not available (requires OpenSSL FIPS configuration)</span>");
        fips_section->append(m_fips_status_label);
        m_fips_mode_check.set_sensitive(false);
    }

    left_column->append(*fips_section);

    // Default history section (outer box)
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

    append(*default_history_section);

    // Username hash section
    auto* username_hash_section = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    username_hash_section->set_margin_top(24);

    auto* username_hash_title = Gtk::make_managed<Gtk::Label>("Key Derivation Algorithm (New Vaults Only)");
    username_hash_title->set_halign(Gtk::Align::START);
    username_hash_title->add_css_class("heading");
    username_hash_section->append(*username_hash_title);

    auto* username_hash_desc = Gtk::make_managed<Gtk::Label>(
        "Choose the cryptographic algorithm for securing usernames and master passwords in newly created vaults");
    username_hash_desc->set_halign(Gtk::Align::START);
    username_hash_desc->add_css_class("dim-label");
    username_hash_desc->set_wrap(true);
    username_hash_desc->set_max_width_chars(60);
    username_hash_section->append(*username_hash_desc);

    auto* username_hash_row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 12);
    username_hash_row->set_margin_top(12);

    auto* username_hash_label = Gtk::make_managed<Gtk::Label>("Algorithm:");
    username_hash_label->set_halign(Gtk::Align::START);
    username_hash_row->append(*username_hash_label);

    m_username_hash_combo.append("plaintext", "Plaintext (DEPRECATED)");
    m_username_hash_combo.append("sha3-256", "SHA3-256 (FIPS)");
    m_username_hash_combo.append("sha3-384", "SHA3-384 (FIPS)");
    m_username_hash_combo.append("sha3-512", "SHA3-512 (FIPS)");
    m_username_hash_combo.append("pbkdf2-sha256", "PBKDF2-SHA256 (FIPS)");
#ifdef ENABLE_ARGON2
    m_username_hash_combo.append("argon2id", "Argon2id (non-FIPS)");
#endif

    m_username_hash_combo.set_active_id("sha3-256");
    username_hash_row->append(m_username_hash_combo);

    username_hash_row->set_halign(Gtk::Align::START);
    username_hash_section->append(*username_hash_row);

    m_username_hash_info.set_halign(Gtk::Align::START);
    m_username_hash_info.set_wrap(true);
    m_username_hash_info.set_max_width_chars(60);
    m_username_hash_info.set_margin_top(6);
    m_username_hash_info.set_margin_start(12);
    username_hash_section->append(m_username_hash_info);

    auto* username_hash_note = Gtk::make_managed<Gtk::Label>();
    username_hash_note->set_markup(
        "<span size='small'>⚠️  This setting only affects newly created vaults. "
        "Existing vaults continue to use their original algorithm.</span>");
    username_hash_note->set_halign(Gtk::Align::START);
    username_hash_note->set_wrap(true);
    username_hash_note->set_max_width_chars(60);
    username_hash_note->add_css_class("dim-label");
    username_hash_note->set_margin_top(12);
    username_hash_section->append(*username_hash_note);

    m_username_hash_advanced_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 12);
    m_username_hash_advanced_box->set_margin_top(16);
    m_username_hash_advanced_box->set_visible(false);

    auto* advanced_label = Gtk::make_managed<Gtk::Label>();
    advanced_label->set_markup("<b>Advanced Parameters</b>");
    advanced_label->set_halign(Gtk::Align::START);
    m_username_hash_advanced_box->append(*advanced_label);

    m_pbkdf2_iterations_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    m_pbkdf2_iterations_box->set_visible(false);

    auto* pbkdf2_iter_hbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 12);

    auto* pbkdf2_label = Gtk::make_managed<Gtk::Label>("PBKDF2 Iterations:");
    pbkdf2_label->set_halign(Gtk::Align::START);
    pbkdf2_label->set_width_chars(20);

    auto pbkdf2_adjustment = Gtk::Adjustment::create(100000.0, 10000.0, 1000000.0, 10000.0, 50000.0);
    m_pbkdf2_iterations_spin = Gtk::make_managed<Gtk::SpinButton>(pbkdf2_adjustment, 10000.0, 0);
    m_pbkdf2_iterations_spin->set_numeric(true);
    m_pbkdf2_iterations_spin->set_hexpand(false);

    auto* pbkdf2_suffix = Gtk::make_managed<Gtk::Label>("iterations");
    pbkdf2_suffix->add_css_class("dim-label");

    pbkdf2_iter_hbox->append(*pbkdf2_label);
    pbkdf2_iter_hbox->append(*m_pbkdf2_iterations_spin);
    pbkdf2_iter_hbox->append(*pbkdf2_suffix);

    auto* pbkdf2_help = Gtk::make_managed<Gtk::Label>();
    pbkdf2_help->set_markup(
        "<span size='small'>Higher iterations increase security against brute-force attacks but slow down login and username operations. "
        "OWASP recommends 600,000+ iterations for password KEK derivation (2024). Default: 100,000 for username hashing, "
        "600,000 for password KEK.</span>");
    pbkdf2_help->set_wrap(true);
    pbkdf2_help->set_max_width_chars(60);
    pbkdf2_help->add_css_class("dim-label");
    pbkdf2_help->set_halign(Gtk::Align::START);

    m_pbkdf2_iterations_box->append(*pbkdf2_iter_hbox);
    m_pbkdf2_iterations_box->append(*pbkdf2_help);
    m_username_hash_advanced_box->append(*m_pbkdf2_iterations_box);

    m_argon2_params_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    m_argon2_params_box->set_visible(false);

    auto* argon2_memory_hbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 12);

    auto* argon2_memory_label = Gtk::make_managed<Gtk::Label>("Memory Cost:");
    argon2_memory_label->set_halign(Gtk::Align::START);
    argon2_memory_label->set_width_chars(20);

    auto argon2_memory_adjustment = Gtk::Adjustment::create(64.0, 8.0, 1024.0, 8.0, 64.0);
    m_argon2_memory_spin = Gtk::make_managed<Gtk::SpinButton>(argon2_memory_adjustment, 8.0, 0);
    m_argon2_memory_spin->set_numeric(true);
    m_argon2_memory_spin->set_hexpand(false);

    auto* argon2_memory_suffix = Gtk::make_managed<Gtk::Label>("MB");
    argon2_memory_suffix->add_css_class("dim-label");

    argon2_memory_hbox->append(*argon2_memory_label);
    argon2_memory_hbox->append(*m_argon2_memory_spin);
    argon2_memory_hbox->append(*argon2_memory_suffix);

    auto* argon2_time_hbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 12);
    argon2_time_hbox->set_margin_top(6);

    auto* argon2_time_label = Gtk::make_managed<Gtk::Label>("Time Cost:");
    argon2_time_label->set_halign(Gtk::Align::START);
    argon2_time_label->set_width_chars(20);

    auto argon2_time_adjustment = Gtk::Adjustment::create(3.0, 1.0, 10.0, 1.0, 2.0);
    m_argon2_time_spin = Gtk::make_managed<Gtk::SpinButton>(argon2_time_adjustment, 1.0, 0);
    m_argon2_time_spin->set_numeric(true);
    m_argon2_time_spin->set_hexpand(false);

    auto* argon2_time_suffix = Gtk::make_managed<Gtk::Label>("iterations");
    argon2_time_suffix->add_css_class("dim-label");

    argon2_time_hbox->append(*argon2_time_label);
    argon2_time_hbox->append(*m_argon2_time_spin);
    argon2_time_hbox->append(*argon2_time_suffix);

    auto* argon2_help = Gtk::make_managed<Gtk::Label>();
    argon2_help->set_markup(
        "<span size='small'>Memory cost (MB) provides resistance to GPU/ASIC attacks (higher = more secure). "
        "Time cost is the number of computational passes (higher = slower but more secure). "
        "Default: 256 MB memory, 4 iterations. <b>Warning:</b> High memory values (512+ MB) may cause slow login times.</span>");
    argon2_help->set_wrap(true);
    argon2_help->set_max_width_chars(60);
    argon2_help->add_css_class("dim-label");
    argon2_help->set_halign(Gtk::Align::START);
    argon2_help->set_margin_top(6);

    m_argon2_params_box->append(*argon2_memory_hbox);
    m_argon2_params_box->append(*argon2_time_hbox);
    m_argon2_params_box->append(*argon2_help);

    m_argon2_perf_warning = Gtk::make_managed<Gtk::Label>();
    m_argon2_perf_warning->set_wrap(true);
    m_argon2_perf_warning->set_max_width_chars(60);
    m_argon2_perf_warning->set_halign(Gtk::Align::START);
    m_argon2_perf_warning->set_margin_top(6);
    m_argon2_perf_warning->set_visible(false);
    m_argon2_params_box->append(*m_argon2_perf_warning);

    m_username_hash_advanced_box->append(*m_argon2_params_box);
    username_hash_section->append(*m_username_hash_advanced_box);

    // Put into left column
    left_column->append(*username_hash_section);

    // Current vault security (right column)
    m_current_vault_kek_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    m_current_vault_kek_box->set_margin_top(24);

    auto* current_kek_title = Gtk::make_managed<Gtk::Label>("Current Vault Security");
    current_kek_title->set_halign(Gtk::Align::START);
    current_kek_title->add_css_class("heading");
    m_current_vault_kek_box->append(*current_kek_title);

    auto* current_kek_desc = Gtk::make_managed<Gtk::Label>(
        "Security settings of the currently open vault (read-only)");
    current_kek_desc->set_halign(Gtk::Align::START);
    current_kek_desc->add_css_class("dim-label");
    current_kek_desc->set_wrap(true);
    current_kek_desc->set_max_width_chars(60);
    m_current_vault_kek_box->append(*current_kek_desc);

    m_current_username_hash_label.set_halign(Gtk::Align::START);
    m_current_username_hash_label.set_margin_top(12);
    m_current_vault_kek_box->append(m_current_username_hash_label);

    m_current_kek_label.set_halign(Gtk::Align::START);
    m_current_kek_label.set_margin_top(6);
    m_current_vault_kek_box->append(m_current_kek_label);

    m_current_kek_params_label.set_halign(Gtk::Align::START);
    m_current_kek_params_label.add_css_class("dim-label");
    m_current_kek_params_label.set_margin_top(6);
    m_current_kek_params_label.set_margin_start(12);
    m_current_vault_kek_box->append(m_current_kek_params_label);

    m_current_vault_kek_box->set_visible(false);
    m_security_right_column->append(*m_current_vault_kek_box);

    // Password history (right column)
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

    m_clear_history_button.set_halign(Gtk::Align::START);
    m_clear_history_button.set_margin_top(12);
    m_vault_password_history_box.append(m_clear_history_button);

    m_clear_history_warning.set_markup(
        "<span size='small'>⚠️  This will delete all your password history. You will be able to reuse old passwords.</span>");
    m_clear_history_warning.set_halign(Gtk::Align::START);
    m_clear_history_warning.set_wrap(true);
    m_clear_history_warning.set_max_width_chars(60);
    m_clear_history_warning.add_css_class("dim-label");
    m_clear_history_warning.set_margin_top(6);
    m_vault_password_history_box.append(m_clear_history_warning);

    m_security_right_column->append(m_vault_password_history_box);

    // Signals
    m_auto_lock_enabled_check.signal_toggled().connect(
        sigc::mem_fun(*this, &VaultSecurityPreferencesPage::on_auto_lock_enabled_toggled));

    m_clear_history_button.signal_clicked().connect(
        sigc::mem_fun(*this, &VaultSecurityPreferencesPage::on_clear_password_history_clicked));

    m_username_hash_combo.signal_changed().connect(
        sigc::mem_fun(*this, &VaultSecurityPreferencesPage::on_username_hash_changed));

    if (m_argon2_memory_spin && m_argon2_time_spin) {
        m_argon2_memory_spin->signal_value_changed().connect(
            sigc::mem_fun(*this, &VaultSecurityPreferencesPage::update_argon2_performance_warning));
        m_argon2_time_spin->signal_value_changed().connect(
            sigc::mem_fun(*this, &VaultSecurityPreferencesPage::update_argon2_performance_warning));
    }
}

void VaultSecurityPreferencesPage::load_from_model(const PreferencesModel& model) {
    m_fips_available = model.fips_available;
    m_fips_mode_enabled_pref = model.fips_mode_enabled;

    m_auto_lock_enabled_check.set_active(model.auto_lock_enabled);
    m_auto_lock_timeout_spin.set_value(model.auto_lock_timeout_seconds);
    on_auto_lock_enabled_toggled();

    // FIPS checkbox state
    if (m_fips_available) {
        m_fips_mode_check.set_sensitive(true);
        m_fips_mode_check.set_active(model.fips_mode_enabled);
    } else {
        m_fips_mode_check.set_sensitive(false);
        m_fips_mode_check.set_active(false);
    }

    // Default history depth
    m_vault_password_history_default_spin.set_value(std::clamp(model.vault_user_password_history_depth_default, 0, 24));

    // Username hash algorithm
    m_username_hash_combo.set_active_id(model.username_hash_algorithm);

    if (m_pbkdf2_iterations_spin) {
        m_pbkdf2_iterations_spin->set_value(static_cast<double>(model.username_pbkdf2_iterations));
    }
    if (m_argon2_memory_spin) {
        m_argon2_memory_spin->set_value(static_cast<double>(model.username_argon2_memory_mb));
    }
    if (m_argon2_time_spin) {
        m_argon2_time_spin->set_value(static_cast<double>(model.username_argon2_iterations));
    }

    update_username_hash_info();
    update_username_hash_advanced_params();
    update_argon2_performance_warning();

    update_security_layout();
}

void VaultSecurityPreferencesPage::store_to_model(PreferencesModel& model) const {
    model.auto_lock_enabled = m_auto_lock_enabled_check.get_active();
    model.auto_lock_timeout_seconds = std::clamp(static_cast<int>(m_auto_lock_timeout_spin.get_value()), MIN_AUTO_LOCK_TIMEOUT, MAX_AUTO_LOCK_TIMEOUT);

    model.fips_mode_enabled = m_fips_mode_check.get_active();

    model.vault_user_password_history_depth_default = std::clamp(
        static_cast<int>(m_vault_password_history_default_spin.get_value()), 0, 24);

    model.username_hash_algorithm = m_username_hash_combo.get_active_id();
    if (m_pbkdf2_iterations_spin) {
        model.username_pbkdf2_iterations = static_cast<std::uint32_t>(m_pbkdf2_iterations_spin->get_value());
    }
    if (m_argon2_memory_spin) {
        model.username_argon2_memory_mb = static_cast<std::uint32_t>(m_argon2_memory_spin->get_value());
    }
    if (m_argon2_time_spin) {
        model.username_argon2_iterations = static_cast<std::uint32_t>(m_argon2_time_spin->get_value());
    }
}

void VaultSecurityPreferencesPage::on_dialog_shown() {
    if (!m_history_ui_loaded) {
        m_history_ui_loaded = true;
        update_vault_password_history_ui();
    }

    update_current_vault_kek_info();
    update_security_layout();
}

void VaultSecurityPreferencesPage::on_auto_lock_enabled_toggled() noexcept {
    const bool enabled = m_auto_lock_enabled_check.get_active();
    m_auto_lock_timeout_label.set_sensitive(enabled);
    m_auto_lock_timeout_spin.set_sensitive(enabled);
    m_auto_lock_timeout_suffix.set_sensitive(enabled);
}

void VaultSecurityPreferencesPage::on_username_hash_changed() noexcept {
    update_username_hash_info();
    update_username_hash_advanced_params();

    if (m_fips_mode_enabled_pref) {
        const auto algorithm = m_username_hash_combo.get_active_id();
        if (algorithm == "plaintext" || algorithm == "argon2id") {
            KeepTower::Log::warning(
                "FIPS mode active: Cannot select {} algorithm, reverting to SHA3-256",
                std::string(algorithm));
            m_username_hash_combo.set_active_id("sha3-256");
        }
    }
}

void VaultSecurityPreferencesPage::on_clear_password_history_clicked() noexcept {
    if (!m_vault_manager || !m_vault_manager->is_vault_open()) {
        return;
    }

    auto* parent_window = dynamic_cast<Gtk::Window*>(get_root());
    if (!parent_window) {
        return;
    }

    const auto session = m_vault_manager->get_current_user_session();
    if (!session) {
        return;
    }

    const std::string username = session->username;

    auto* dialog = new Gtk::MessageDialog(
        *parent_window,
        "Clear Password History?",
        false,
        Gtk::MessageType::WARNING,
        Gtk::ButtonsType::OK_CANCEL,
        true);

    dialog->set_secondary_text(
        "This will permanently delete all saved password history for user '" +
        username + "'.\n\n" +
        "This action cannot be undone.");

    dialog->set_modal(true);

    dialog->signal_response().connect([this, dialog, username, parent_window](int response) {
        dialog->hide();

        if (response == Gtk::ResponseType::OK) {
            try {
                auto result = m_vault_manager->clear_user_password_history(username);
                if (!result) {
                    throw std::runtime_error(std::string(KeepTower::to_string(result.error())));
                }

                if (!m_vault_manager->save_vault()) {
                    throw std::runtime_error("Failed to save vault");
                }

                update_vault_password_history_ui();

                auto* success_dialog = new Gtk::MessageDialog(
                    *parent_window,
                    "Password history cleared",
                    false,
                    Gtk::MessageType::INFO,
                    Gtk::ButtonsType::OK,
                    true);
                success_dialog->set_secondary_text("Password history for '" + username + "' has been cleared.");
                success_dialog->set_modal(true);
                success_dialog->signal_response().connect([success_dialog](int) { delete success_dialog; });
                success_dialog->show();

            } catch (const std::exception& e) {
                auto* error_dialog = new Gtk::MessageDialog(
                    *parent_window,
                    "Failed to clear password history",
                    false,
                    Gtk::MessageType::ERROR,
                    Gtk::ButtonsType::OK,
                    true);
                error_dialog->set_secondary_text(std::string(e.what()));
                error_dialog->set_modal(true);
                error_dialog->signal_response().connect([error_dialog](int) { delete error_dialog; });
                error_dialog->show();
            }
        }

        delete dialog;
    });

    dialog->show();
}

void VaultSecurityPreferencesPage::update_username_hash_advanced_params() noexcept {
    const auto active_id = m_username_hash_combo.get_active_id();

    if (m_pbkdf2_iterations_box) {
        m_pbkdf2_iterations_box->set_visible(false);
    }
    if (m_argon2_params_box) {
        m_argon2_params_box->set_visible(false);
    }

    if (!m_username_hash_advanced_box) {
        return;
    }

    if (active_id == "sha3-256" || active_id == "sha3-384" || active_id == "sha3-512" || active_id == "plaintext") {
        m_username_hash_advanced_box->set_visible(false);
    } else if (active_id == "pbkdf2-sha256") {
        m_username_hash_advanced_box->set_visible(true);
        if (m_pbkdf2_iterations_box) {
            m_pbkdf2_iterations_box->set_visible(true);
        }
    } else if (active_id == "argon2id") {
        m_username_hash_advanced_box->set_visible(true);
        if (m_argon2_params_box) {
            m_argon2_params_box->set_visible(true);
        }
    } else {
        m_username_hash_advanced_box->set_visible(false);
    }

    resize_to_content();
}

void VaultSecurityPreferencesPage::update_security_layout() noexcept {
    const bool vault_open = m_vault_manager && m_vault_manager->is_vault_open();

    if (m_security_right_column) {
        m_security_right_column->set_visible(vault_open);
    }

    if (m_security_grid) {
        m_security_grid->set_column_homogeneous(!vault_open);
    }
}

void VaultSecurityPreferencesPage::resize_to_content() noexcept {
    // Only resize when this page is actually on-screen.
    // During Preferences dialog open we may load/update this page while it's hidden in the Gtk::Stack;
    // resizing then causes a visible "open wide then shrink" flicker.
    if (!get_mapped()) {
        return;
    }

    auto* dialog = dynamic_cast<Gtk::Dialog*>(get_root());
    if (!dialog) {
        return;
    }

    dialog->set_default_size(-1, -1);
    dialog->queue_resize();

    Glib::signal_idle().connect_once([dialog]() {
        dialog->set_default_size(650, -1);
    });
}

void VaultSecurityPreferencesPage::update_vault_password_history_ui() noexcept {
    if (!m_vault_manager || !m_vault_manager->is_vault_open()) {
        m_vault_password_history_box.set_visible(false);
        if (auto* parent = m_vault_password_history_default_box.get_parent()) {
            parent->set_visible(true);
        }
        return;
    }

    m_vault_password_history_box.set_visible(true);
    if (auto* parent = m_vault_password_history_default_box.get_parent()) {
        parent->set_visible(false);
    }

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

    const auto session = m_vault_manager->get_current_user_session();
    if (!session) {
        m_current_user_label.set_text("No user logged in");
        m_history_count_label.set_text("Password history: N/A");
        m_clear_history_button.set_sensitive(false);
        return;
    }

    m_current_user_label.set_text(
        KeepTower::make_valid_utf8(std::string{"Logged in as: "} + session->username, "session_username")
    );

    const auto users = m_vault_manager->list_users();
    size_t history_count = 0;
    for (const auto& user : users) {
        if (user.username == session->username) {
            history_count = user.password_history.size();
            break;
        }
    }

    m_history_count_label.set_text("Password history: " + std::to_string(history_count) + " entries");
    m_clear_history_button.set_sensitive(history_count > 0);
}

void VaultSecurityPreferencesPage::update_username_hash_info() noexcept {
    const auto algorithm = m_username_hash_combo.get_active_id();

    if (algorithm == "plaintext") {
        m_username_hash_info.set_markup(
            "<span size='small' foreground='#e01b24'>⚠️  <b>DEPRECATED:</b> Usernames stored in plain text. "
            "Not recommended for security. Use for legacy compatibility only.</span>");
    } else if (algorithm == "sha3-256") {
        m_username_hash_info.set_markup(
            "<span size='small'>ℹ️  <b>Username:</b> SHA3-256 (fast, FIPS-approved)\n"
            "    <b>Password KEK:</b> PBKDF2-SHA256 (600K iterations)\n"
            "    Passwords automatically protected with stronger algorithm.</span>");
    } else if (algorithm == "sha3-384") {
        m_username_hash_info.set_markup(
            "<span size='small'>ℹ️  <b>Username:</b> SHA3-384 (fast, FIPS-approved)\n"
            "    <b>Password KEK:</b> PBKDF2-SHA256 (600K iterations)\n"
            "    Passwords automatically protected with stronger algorithm.</span>");
    } else if (algorithm == "sha3-512") {
        m_username_hash_info.set_markup(
            "<span size='small'>ℹ️  <b>Username:</b> SHA3-512 (fast, FIPS-approved)\n"
            "    <b>Password KEK:</b> PBKDF2-SHA256 (600K iterations)\n"
            "    Passwords automatically protected with stronger algorithm.</span>");
    } else if (algorithm == "pbkdf2-sha256") {
        m_username_hash_info.set_markup(
            "<span size='small'>ℹ️  <b>Username:</b> PBKDF2-SHA256 (configurable iterations)\n"
            "    <b>Password KEK:</b> PBKDF2-SHA256 (same parameters)\n"
            "    Consistent security for both username and password. FIPS-approved.</span>");
    } else if (algorithm == "argon2id") {
        m_username_hash_info.set_markup(
            "<span size='small' foreground='#f57900'>⚠️  <b>Non-FIPS Vault:</b> "
            "<b>Username:</b> Argon2id (memory-hard, configurable)\n"
            "    <b>Password KEK:</b> Argon2id (same parameters)\n"
            "    <b>⚠️  Vaults created with this algorithm are NOT FIPS-140-3 compliant.</b>\n"
            "    Maximum security but slower unlock (2-8 seconds).</span>");
    } else {
        m_username_hash_info.set_text("Unknown algorithm");
    }

    if (m_fips_mode_enabled_pref && (algorithm == "plaintext" || algorithm == "argon2id")) {
        m_username_hash_info.set_markup(
            "<span size='small' foreground='#e01b24'>⚠️  <b>FIPS MODE ACTIVE:</b> "
            "This algorithm is not FIPS-approved and cannot be used. "
            "Please select a FIPS-approved algorithm (SHA3-256/384/512 or PBKDF2).</span>");
    }
}

void VaultSecurityPreferencesPage::update_current_vault_kek_info() noexcept {
    if (!m_vault_manager || !m_vault_manager->is_vault_open()) {
        if (m_current_vault_kek_box) {
            m_current_vault_kek_box->set_visible(false);
        }
        return;
    }

    if (m_current_vault_kek_box) {
        m_current_vault_kek_box->set_visible(true);
    }

    const auto policy_opt = m_vault_manager->get_vault_security_policy();
    if (!policy_opt) {
        m_current_username_hash_label.set_markup("<span>Username Algorithm: <b>N/A</b></span>");
        m_current_kek_label.set_markup("<span>Password KEK Algorithm: <b>N/A</b></span>");
        m_current_kek_params_label.set_text("");
        return;
    }

    const auto& policy = *policy_opt;

    std::string username_algo_display;
    switch (policy.username_hash_algorithm) {
        case 0x00: username_algo_display = "Plaintext (DEPRECATED)"; break;
        case 0x01: username_algo_display = "SHA3-256 (FIPS)"; break;
        case 0x02: username_algo_display = "SHA3-384 (FIPS)"; break;
        case 0x03: username_algo_display = "SHA3-512 (FIPS)"; break;
        case 0x04: username_algo_display = "PBKDF2-HMAC-SHA256 (FIPS)"; break;
        case 0x05: username_algo_display = "Argon2id (⚠️ non-FIPS vault)"; break;
        default: username_algo_display = "Unknown (" + std::to_string(policy.username_hash_algorithm) + ")";
    }

    m_current_username_hash_label.set_markup(
        "<span>Username Algorithm: <b>" + username_algo_display + "</b></span>");

    std::string kek_algo_display;
    std::string params_display;

    if (policy.username_hash_algorithm >= 0x01 && policy.username_hash_algorithm <= 0x03) {
        kek_algo_display = "PBKDF2-HMAC-SHA256 (FIPS)";
        params_display = std::to_string(policy.pbkdf2_iterations) + " iterations";
    } else if (policy.username_hash_algorithm == 0x04) {
        kek_algo_display = "PBKDF2-HMAC-SHA256 (FIPS)";
        params_display = std::to_string(policy.pbkdf2_iterations) + " iterations";
    } else if (policy.username_hash_algorithm == 0x05) {
        kek_algo_display = "Argon2id (⚠️ non-FIPS vault)";
        params_display = std::to_string(policy.argon2_memory_kb) + " KB memory, " +
                         std::to_string(policy.argon2_iterations) + " time cost, " +
                         std::to_string(policy.argon2_parallelism) + " threads";
    } else {
        kek_algo_display = "PBKDF2-HMAC-SHA256 (default fallback)";
        params_display = "600,000 iterations";
    }

    m_current_kek_label.set_markup(
        "<span>Password KEK Algorithm: <b>" + kek_algo_display + "</b></span>");
    m_current_kek_params_label.set_text("  Parameters: " + params_display);
}

void VaultSecurityPreferencesPage::update_argon2_performance_warning() noexcept {
    if (!m_argon2_perf_warning || !m_argon2_memory_spin || !m_argon2_time_spin) {
        return;
    }

    const double memory_mb = m_argon2_memory_spin->get_value();
    const double time_cost = m_argon2_time_spin->get_value();

    const double baseline_time_ms = 500.0;
    const double baseline_memory_mb = 256.0;
    const double baseline_time_cost = 4.0;

    const double estimated_time_ms = baseline_time_ms *
                                     (memory_mb / baseline_memory_mb) *
                                     (time_cost / baseline_time_cost);

    if (estimated_time_ms >= 2000.0) {
        const double seconds = estimated_time_ms / 1000.0;
        m_argon2_perf_warning->set_markup(
            "<span size='small' foreground='#e01b24'><b>⚠️  Performance Warning:</b> "
            "Estimated login time: ~" + std::to_string(static_cast<int>(seconds)) + " seconds. "
            "High memory/time values will significantly slow vault operations. "
            "Consider reducing parameters unless maximum security is required.</span>");
        m_argon2_perf_warning->set_visible(true);
    } else if (estimated_time_ms >= 1000.0) {
        const double seconds = estimated_time_ms / 1000.0;
        m_argon2_perf_warning->set_markup(
            "<span size='small' foreground='#f57900'><b>ℹ️  Performance Note:</b> "
            "Estimated login time: ~" + std::to_string(static_cast<int>(seconds)) + " seconds. "
            "This may be noticeable on slower systems.</span>");
        m_argon2_perf_warning->set_visible(true);
    } else {
        m_argon2_perf_warning->set_visible(false);
    }
}

}  // namespace KeepTower::Ui
