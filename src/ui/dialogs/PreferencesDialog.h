// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <gtkmm.h>
#include <giomm/settings.h>

// Forward declaration
class VaultManager;

/**
 * @brief Preferences dialog for application settings
 *
 * Provides UI for configuring application preferences including
 * Reed-Solomon error correction settings.
 */
class PreferencesDialog final : public Gtk::Dialog {
public:
    explicit PreferencesDialog(Gtk::Window& parent, VaultManager* vault_manager = nullptr);
    ~PreferencesDialog() override = default;

    // Prevent copying and moving
    PreferencesDialog(const PreferencesDialog&) = delete;
    PreferencesDialog& operator=(const PreferencesDialog&) = delete;
    PreferencesDialog(PreferencesDialog&&) = delete;
    PreferencesDialog& operator=(PreferencesDialog&&) = delete;

private:
    void setup_ui();
    void setup_appearance_page();
    void setup_security_page();
    void setup_storage_page();
    void load_settings();
    void save_settings();
    void apply_color_scheme(const Glib::ustring& scheme);
    void on_rs_enabled_toggled() noexcept;
    void on_backup_enabled_toggled() noexcept;
    void on_auto_lock_enabled_toggled() noexcept;
    void on_apply_to_current_toggled() noexcept;
    void on_color_scheme_changed() noexcept;
    void on_response(int response_id) noexcept override;

    // Constants
    static constexpr int MIN_REDUNDANCY = 5;
    static constexpr int MAX_REDUNDANCY = 50;
    static constexpr int DEFAULT_REDUNDANCY = 10;
    static constexpr int MIN_BACKUP_COUNT = 1;
    static constexpr int MAX_BACKUP_COUNT = 50;
    static constexpr int DEFAULT_BACKUP_COUNT = 5;
    static constexpr int MIN_CLIPBOARD_TIMEOUT = 5;
    static constexpr int MAX_CLIPBOARD_TIMEOUT = 300;
    static constexpr int DEFAULT_CLIPBOARD_TIMEOUT = 30;
    static constexpr int MIN_AUTO_LOCK_TIMEOUT = 60;
    static constexpr int MAX_AUTO_LOCK_TIMEOUT = 3600;
    static constexpr int DEFAULT_AUTO_LOCK_TIMEOUT = 300;
    static constexpr int DEFAULT_WIDTH = 650;
    static constexpr int DEFAULT_HEIGHT = 500;

    // Settings
    Glib::RefPtr<Gio::Settings> m_settings;
    VaultManager* m_vault_manager;  // Non-owning pointer

    // Main layout widgets
    Gtk::Box m_main_box;
    Gtk::StackSidebar m_sidebar;
    Gtk::Stack m_stack;

    // Appearance page
    Gtk::Box m_appearance_box;
    Gtk::Box m_color_scheme_box;
    Gtk::Label m_color_scheme_label;
    Gtk::DropDown m_color_scheme_dropdown;

    // Security page
    Gtk::Box m_security_box;
    Gtk::Box m_clipboard_timeout_box;
    Gtk::Label m_clipboard_timeout_label;
    Gtk::SpinButton m_clipboard_timeout_spin;
    Gtk::Label m_clipboard_timeout_suffix;
    Gtk::CheckButton m_auto_lock_enabled_check;
    Gtk::Box m_auto_lock_timeout_box;
    Gtk::Label m_auto_lock_timeout_label;
    Gtk::SpinButton m_auto_lock_timeout_spin;
    Gtk::Label m_auto_lock_timeout_suffix;
    Gtk::CheckButton m_password_history_enabled_check;

    // Storage page (Reed-Solomon + Backups)
    Gtk::Box m_storage_box;
    Gtk::Label m_rs_section_title;
    Gtk::Label m_rs_description;
    Gtk::CheckButton m_rs_enabled_check;
    Gtk::Box m_redundancy_box;
    Gtk::Label m_redundancy_label;
    Gtk::SpinButton m_redundancy_spin;
    Gtk::Label m_redundancy_suffix;
    Gtk::Label m_redundancy_help;
    Gtk::CheckButton m_apply_to_current_check;
    Gtk::Label m_backup_section_title;
    Gtk::Label m_backup_description;
    Gtk::CheckButton m_backup_enabled_check;
    Gtk::Box m_backup_count_box;
    Gtk::Label m_backup_count_label;
    Gtk::SpinButton m_backup_count_spin;
    Gtk::Label m_backup_count_suffix;
    Gtk::Label m_backup_help;
};

#endif // PREFERENCESDIALOG_H
