// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

/**
 * @file ThemeController.h
 * @brief Controller for applying and monitoring application theme preference
 *
 * ThemeController extracts theme selection logic from MainWindow.
 * It applies the app's color-scheme preference and, when set to "default",
 * follows the system (GNOME) color-scheme setting.
 */

#pragma once

#include <functional>
#include <sigc++/sigc++.h>
#include <giomm/settings.h>
#include <gtkmm/settings.h>
#include <glibmm/ustring.h>

class ThemeController {
public:
    using ApplyPreferDarkFn = std::function<void(bool)>;

    ThemeController();
    ThemeController(
        Glib::RefPtr<Gio::Settings> app_settings,
        Glib::RefPtr<Gtk::Settings> gtk_settings,
        Glib::RefPtr<Gio::Settings> desktop_settings = {}
    );

    ThemeController(
        Glib::RefPtr<Gio::Settings> app_settings,
        ApplyPreferDarkFn apply_prefer_dark,
        Glib::RefPtr<Gio::Settings> desktop_settings = {}
    );

    ~ThemeController();

    ThemeController(const ThemeController&) = delete;
    ThemeController& operator=(const ThemeController&) = delete;
    ThemeController(ThemeController&&) = delete;
    ThemeController& operator=(ThemeController&&) = delete;

    /**
     * @brief Begin applying theme and monitoring relevant settings.
     *
     * Safe to call multiple times.
     */
    void start();

    /**
     * @brief Stop monitoring settings (disconnect signals).
     *
     * Safe to call multiple times.
     */
    void stop();

    /**
     * @brief Apply the current theme immediately.
     */
    void apply_now();

private:
    void apply_color_scheme(const Glib::ustring& color_scheme);
    void apply_default_follow_system();
    void apply_from_gtk_theme_env_fallback();
    void set_prefer_dark(bool prefer_dark);

    [[nodiscard]] bool can_apply() const noexcept;

    Glib::RefPtr<Gio::Settings> m_app_settings;
    Glib::RefPtr<Gio::Settings> m_desktop_settings;
    Glib::RefPtr<Gtk::Settings> m_gtk_settings;
    ApplyPreferDarkFn m_apply_prefer_dark;

    sigc::connection m_app_scheme_changed_connection;
    sigc::connection m_system_scheme_changed_connection;

    bool m_started = false;
};
