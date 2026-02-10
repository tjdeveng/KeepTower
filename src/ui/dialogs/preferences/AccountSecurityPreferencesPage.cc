// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "AccountSecurityPreferencesPage.h"

#include <algorithm>

namespace KeepTower::Ui {

namespace {
constexpr int MIN_CLIPBOARD_TIMEOUT = 5;
constexpr int MAX_CLIPBOARD_TIMEOUT = 300;
constexpr int DEFAULT_CLIPBOARD_TIMEOUT = 30;

constexpr int MIN_PASSWORD_HISTORY_LIMIT = 0;
constexpr int MAX_PASSWORD_HISTORY_LIMIT = 24;
constexpr int DEFAULT_PASSWORD_HISTORY_LIMIT = 5;
}  // namespace

AccountSecurityPreferencesPage::AccountSecurityPreferencesPage()
    : Gtk::Box(Gtk::Orientation::VERTICAL, 18),
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
            m_undo_history_limit_suffix(" operations") {
    set_margin_start(18);
    set_margin_end(18);
    set_margin_top(18);
    set_margin_bottom(18);

        // Informational note (text depends on vault state; set in load_from_model)
        m_info_label = Gtk::make_managed<Gtk::Label>();
        m_info_label->set_halign(Gtk::Align::START);
        m_info_label->set_wrap(true);
        m_info_label->add_css_class("dim-label");
        m_info_label->set_margin_bottom(12);
        append(*m_info_label);

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
        1.0, 10.0, 0.0);

    m_clipboard_timeout_spin.set_adjustment(clipboard_adjustment);
    m_clipboard_timeout_spin.set_digits(0);
    m_clipboard_timeout_spin.set_value(DEFAULT_CLIPBOARD_TIMEOUT);
    m_clipboard_timeout_box.append(m_clipboard_timeout_spin);

    m_clipboard_timeout_suffix.set_halign(Gtk::Align::START);
    m_clipboard_timeout_box.append(m_clipboard_timeout_suffix);

    m_clipboard_timeout_box.set_halign(Gtk::Align::START);
    clipboard_section->append(m_clipboard_timeout_box);

    append(*clipboard_section);

    // Account Password History section
    auto* pwd_history_section = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    pwd_history_section->set_margin_top(24);

    auto* pwd_history_title = Gtk::make_managed<Gtk::Label>("Account Password History");
    pwd_history_title->set_halign(Gtk::Align::START);
    pwd_history_title->add_css_class("heading");
    pwd_history_section->append(*pwd_history_title);

    auto* pwd_history_desc = Gtk::make_managed<Gtk::Label>(
        "Prevent reusing passwords when updating account entries (Gmail, GitHub, etc.)");
    pwd_history_desc->set_halign(Gtk::Align::START);
    pwd_history_desc->add_css_class("dim-label");
    pwd_history_desc->set_wrap(true);
    pwd_history_desc->set_max_width_chars(60);
    pwd_history_section->append(*pwd_history_desc);

    pwd_history_section->append(m_account_password_history_check);

    m_account_password_history_limit_label.set_halign(Gtk::Align::START);
    m_account_password_history_limit_box.append(m_account_password_history_limit_label);

    // Account password history limit controls
    m_account_password_history_limit_box.set_orientation(Gtk::Orientation::HORIZONTAL);
    m_account_password_history_limit_box.set_spacing(12);
    m_account_password_history_limit_box.set_margin_top(12);
    m_account_password_history_limit_box.set_margin_start(24);

    m_account_password_history_limit_label.set_text("Remember up to");
    m_account_password_history_limit_label.set_halign(Gtk::Align::START);
    m_account_password_history_limit_box.append(m_account_password_history_limit_label);

    auto pwd_history_adjustment = Gtk::Adjustment::create(
        static_cast<double>(DEFAULT_PASSWORD_HISTORY_LIMIT),
        static_cast<double>(MIN_PASSWORD_HISTORY_LIMIT),
        static_cast<double>(MAX_PASSWORD_HISTORY_LIMIT),
        1.0, 5.0, 0.0);

    m_account_password_history_limit_spin.set_adjustment(pwd_history_adjustment);
    m_account_password_history_limit_spin.set_digits(0);
    m_account_password_history_limit_spin.set_value(DEFAULT_PASSWORD_HISTORY_LIMIT);
    m_account_password_history_limit_box.append(m_account_password_history_limit_spin);

    m_account_password_history_limit_suffix.set_halign(Gtk::Align::START);
    m_account_password_history_limit_box.append(m_account_password_history_limit_suffix);

    m_account_password_history_limit_box.set_halign(Gtk::Align::START);
    pwd_history_section->append(m_account_password_history_limit_box);

    append(*pwd_history_section);

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

    auto undo_adjustment = Gtk::Adjustment::create(50.0, 1.0, 100.0, 1.0, 10.0, 0.0);
    m_undo_history_limit_spin.set_adjustment(undo_adjustment);
    m_undo_history_limit_spin.set_digits(0);
    m_undo_history_limit_spin.set_value(50.0);
    m_undo_history_limit_box.append(m_undo_history_limit_spin);

    m_undo_history_limit_suffix.set_halign(Gtk::Align::START);
    m_undo_history_limit_box.append(m_undo_history_limit_suffix);

    m_undo_history_limit_box.set_halign(Gtk::Align::START);
    undo_redo_section->append(m_undo_history_limit_box);

    append(*undo_redo_section);

    // Signals
    m_account_password_history_check.signal_toggled().connect(
        sigc::mem_fun(*this, &AccountSecurityPreferencesPage::on_account_password_history_toggled));
    m_undo_redo_enabled_check.signal_toggled().connect(
        sigc::mem_fun(*this, &AccountSecurityPreferencesPage::on_undo_redo_enabled_toggled));
}

void AccountSecurityPreferencesPage::load_from_model(const PreferencesModel& model) {
    if (m_info_label) {
        if (model.vault_open) {
            m_info_label->set_markup("<span size='small'>ℹ️  Settings for the current vault only (defaults not affected)</span>");
        } else {
            m_info_label->set_markup("<span size='small'>ℹ️  These settings will be used as defaults for new vaults</span>");
        }
    }

    m_clipboard_timeout_spin.set_value(model.clipboard_timeout_seconds);

    m_account_password_history_check.set_active(model.account_password_history_enabled);
    m_account_password_history_limit_spin.set_value(model.account_password_history_limit);

    m_undo_redo_enabled_check.set_active(model.undo_redo_enabled);
    m_undo_history_limit_spin.set_value(model.undo_history_limit);

    on_account_password_history_toggled();
    on_undo_redo_enabled_toggled();
}

void AccountSecurityPreferencesPage::store_to_model(PreferencesModel& model) const {
    model.clipboard_timeout_seconds = std::clamp(static_cast<int>(m_clipboard_timeout_spin.get_value()), MIN_CLIPBOARD_TIMEOUT, MAX_CLIPBOARD_TIMEOUT);

    model.account_password_history_enabled = m_account_password_history_check.get_active();
    model.account_password_history_limit = std::clamp(static_cast<int>(m_account_password_history_limit_spin.get_value()), MIN_PASSWORD_HISTORY_LIMIT, MAX_PASSWORD_HISTORY_LIMIT);

    model.undo_redo_enabled = m_undo_redo_enabled_check.get_active();
    model.undo_history_limit = std::clamp(static_cast<int>(m_undo_history_limit_spin.get_value()), 1, 100);
}

void AccountSecurityPreferencesPage::on_account_password_history_toggled() noexcept {
    const bool enabled = m_account_password_history_check.get_active();
    m_account_password_history_limit_label.set_sensitive(enabled);
    m_account_password_history_limit_spin.set_sensitive(enabled);
    m_account_password_history_limit_suffix.set_sensitive(enabled);
}

void AccountSecurityPreferencesPage::on_undo_redo_enabled_toggled() noexcept {
    const bool enabled = m_undo_redo_enabled_check.get_active();
    m_undo_history_limit_label.set_sensitive(enabled);
    m_undo_history_limit_spin.set_sensitive(enabled);
    m_undo_history_limit_suffix.set_sensitive(enabled);
}

}  // namespace KeepTower::Ui
