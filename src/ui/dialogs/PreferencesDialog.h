// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <gtkmm.h>
#include <giomm/settings.h>

/**
 * @brief Preferences dialog for application settings
 *
 * Provides UI for configuring application preferences including
 * Reed-Solomon error correction settings.
 */
class PreferencesDialog final : public Gtk::Dialog {
public:
    explicit PreferencesDialog(Gtk::Window& parent);
    ~PreferencesDialog() override = default;

    // Prevent copying and moving
    PreferencesDialog(const PreferencesDialog&) = delete;
    PreferencesDialog& operator=(const PreferencesDialog&) = delete;
    PreferencesDialog(PreferencesDialog&&) = delete;
    PreferencesDialog& operator=(PreferencesDialog&&) = delete;

private:
    void setup_ui();
    void load_settings();
    void save_settings();
    void apply_color_scheme(const Glib::ustring& scheme);
    void on_rs_enabled_toggled() noexcept;
    void on_color_scheme_changed() noexcept;
    void on_response(int response_id) noexcept;

    // Constants
    static constexpr int MIN_REDUNDANCY = 5;
    static constexpr int MAX_REDUNDANCY = 50;
    static constexpr int DEFAULT_REDUNDANCY = 10;
    static constexpr int DEFAULT_WIDTH = 500;
    static constexpr int DEFAULT_HEIGHT = 300;

    // Settings
    Glib::RefPtr<Gio::Settings> m_settings;

    // UI widgets
    Gtk::Box m_content_box;

    // Appearance section
    Gtk::Box m_appearance_box;
    Gtk::Label m_appearance_title;
    Gtk::Label m_appearance_description;
    Gtk::Box m_color_scheme_box;
    Gtk::Label m_color_scheme_label;
    Gtk::DropDown m_color_scheme_dropdown;

    // Reed-Solomon section
    Gtk::Box m_rs_box;
    Gtk::Label m_rs_title;
    Gtk::Label m_rs_description;
    Gtk::CheckButton m_rs_enabled_check;
    Gtk::Box m_redundancy_box;
    Gtk::Label m_redundancy_label;
    Gtk::SpinButton m_redundancy_spin;
    Gtk::Label m_redundancy_suffix;
    Gtk::Label m_redundancy_help;
};

#endif // PREFERENCESDIALOG_H
