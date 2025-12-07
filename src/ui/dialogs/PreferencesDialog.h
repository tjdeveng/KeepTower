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
class PreferencesDialog : public Gtk::Dialog {
public:
    explicit PreferencesDialog(Gtk::Window& parent);
    ~PreferencesDialog() override = default;

private:
    void setup_ui();
    void load_settings();
    void save_settings();
    void on_rs_enabled_toggled();
    void on_response(int response_id);

    // Settings
    Glib::RefPtr<Gio::Settings> m_settings;

    // UI widgets
    Gtk::Box m_content_box;
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
