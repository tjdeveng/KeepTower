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
    void setup_ui();                                     ///< Initialize main dialog layout with sidebar and stack
    void setup_appearance_page();                        ///< Build appearance preferences page (color scheme)
    void setup_security_page();                          ///< Build security preferences page (clipboard, auto-lock, password history)
    void setup_storage_page();                           ///< Build storage preferences page (FEC, backups)
    void load_settings();                                ///< Load all settings from GSettings into UI controls
    void save_settings();                                ///< Save all UI control values to GSettings
    void apply_color_scheme(const Glib::ustring& scheme);  ///< Apply color scheme to GTK application
    void on_rs_enabled_toggled() noexcept;               ///< Handle Reed-Solomon enabled checkbox toggle
    void on_backup_enabled_toggled() noexcept;           ///< Handle backup enabled checkbox toggle
    void on_auto_lock_enabled_toggled() noexcept;        ///< Handle auto-lock enabled checkbox toggle
    void on_password_history_enabled_toggled() noexcept; ///< Handle password history enabled checkbox toggle
    void on_undo_redo_enabled_toggled() noexcept;        ///< Handle undo/redo enabled checkbox toggle
    void on_apply_to_current_toggled() noexcept;         ///< Handle "Apply to current vault" checkbox toggle
    void on_color_scheme_changed() noexcept;             ///< Handle color scheme dropdown selection change
    void on_response(int response_id) noexcept override; ///< Handle dialog response (Apply/Cancel)

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
    static constexpr int MIN_PASSWORD_HISTORY_LIMIT = 1;
    static constexpr int MAX_PASSWORD_HISTORY_LIMIT = 50;
    static constexpr int DEFAULT_PASSWORD_HISTORY_LIMIT = 5;
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
    Gtk::Box m_password_history_limit_box;
    Gtk::Label m_password_history_limit_label;
    Gtk::SpinButton m_password_history_limit_spin;
    Gtk::Label m_password_history_limit_suffix;
    Gtk::CheckButton m_undo_redo_enabled_check;
    Gtk::Box m_undo_history_limit_box;
    Gtk::Label m_undo_history_limit_label;
    Gtk::SpinButton m_undo_history_limit_spin;
    Gtk::Label m_undo_history_limit_suffix;
    Gtk::Label m_undo_redo_warning;
    Gtk::CheckButton m_fips_mode_check;
    Gtk::Label m_fips_status_label;
    Gtk::Label m_fips_restart_warning;

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
