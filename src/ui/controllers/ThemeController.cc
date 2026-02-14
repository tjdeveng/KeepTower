// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#include "ThemeController.h"

#include "../../utils/Log.h"

#include <cstdlib>
#include <string>

namespace {
constexpr const char* kAppSchema = "com.tjdeveng.keeptower";
constexpr const char* kAppColorSchemeKey = "color-scheme";

constexpr const char* kDesktopSchema = "org.gnome.desktop.interface";
constexpr const char* kDesktopColorSchemeKey = "color-scheme";
}

ThemeController::ThemeController()
    : ThemeController(
          Gio::Settings::create(kAppSchema),
          Gtk::Settings::get_default(),
          {}
      ) {}

ThemeController::ThemeController(
    Glib::RefPtr<Gio::Settings> app_settings,
    Glib::RefPtr<Gtk::Settings> gtk_settings,
    Glib::RefPtr<Gio::Settings> desktop_settings)
    : m_app_settings(std::move(app_settings)),
      m_desktop_settings(std::move(desktop_settings)),
      m_gtk_settings(std::move(gtk_settings)) {}

ThemeController::ThemeController(
        Glib::RefPtr<Gio::Settings> app_settings,
        ApplyPreferDarkFn apply_prefer_dark,
        Glib::RefPtr<Gio::Settings> desktop_settings)
        : m_app_settings(std::move(app_settings)),
            m_desktop_settings(std::move(desktop_settings)),
            m_apply_prefer_dark(std::move(apply_prefer_dark)) {}

ThemeController::~ThemeController() {
    stop();
}

void ThemeController::start() {
    if (m_started) {
        return;
    }

    if (!m_app_settings) {
        try {
            m_app_settings = Gio::Settings::create(kAppSchema);
        } catch (...) {
            KeepTower::Log::debug("ThemeController: Could not create app GSettings schema");
            return;
        }
    }

    if (!m_gtk_settings && !m_apply_prefer_dark) {
        m_gtk_settings = Gtk::Settings::get_default();
        if (!m_gtk_settings) {
            KeepTower::Log::debug("ThemeController: Gtk::Settings::get_default() returned null");
            return;
        }
    }

    apply_now();

    if (m_app_scheme_changed_connection.connected()) {
        m_app_scheme_changed_connection.disconnect();
    }

    m_app_scheme_changed_connection = m_app_settings->signal_changed(kAppColorSchemeKey).connect(
        [this]([[maybe_unused]] const Glib::ustring& key) {
            apply_now();
        }
    );

    m_started = true;
}

void ThemeController::stop() {
    if (m_app_scheme_changed_connection.connected()) {
        m_app_scheme_changed_connection.disconnect();
    }

    if (m_system_scheme_changed_connection.connected()) {
        m_system_scheme_changed_connection.disconnect();
    }

    m_started = false;
}

void ThemeController::apply_now() {
    if (!m_app_settings || !can_apply()) {
        return;
    }

    Glib::ustring color_scheme;
    try {
        color_scheme = m_app_settings->get_string(kAppColorSchemeKey);
    } catch (...) {
        KeepTower::Log::debug("ThemeController: Failed reading app color-scheme");
        return;
    }

    apply_color_scheme(color_scheme);
}

void ThemeController::apply_color_scheme(const Glib::ustring& color_scheme) {
    if (!can_apply()) {
        return;
    }

    if (m_system_scheme_changed_connection.connected()) {
        m_system_scheme_changed_connection.disconnect();
    }

    if (color_scheme == "light") {
        set_prefer_dark(false);
        return;
    }

    if (color_scheme == "dark") {
        set_prefer_dark(true);
        return;
    }

    apply_default_follow_system();
}

void ThemeController::apply_default_follow_system() {
    if (!can_apply()) {
        return;
    }

    bool applied = false;

    try {
        if (!m_desktop_settings) {
            m_desktop_settings = Gio::Settings::create(kDesktopSchema);
        }

        if (m_desktop_settings) {
            auto system_color_scheme = m_desktop_settings->get_string(kDesktopColorSchemeKey);
            set_prefer_dark(system_color_scheme == "prefer-dark");
            applied = true;

            if (m_system_scheme_changed_connection.connected()) {
                m_system_scheme_changed_connection.disconnect();
            }

            m_system_scheme_changed_connection = m_desktop_settings->signal_changed(kDesktopColorSchemeKey).connect(
                [this]([[maybe_unused]] const Glib::ustring& key) {
                    if (!m_desktop_settings) {
                        return;
                    }
                    auto system_color_scheme = m_desktop_settings->get_string(kDesktopColorSchemeKey);
                    set_prefer_dark(system_color_scheme == "prefer-dark");
                }
            );
        }
    } catch (const std::exception& e) {
        KeepTower::Log::debug("ThemeController: Could not monitor system theme changes: {}", e.what());
    } catch (...) {
        KeepTower::Log::debug("ThemeController: Could not monitor system theme changes (unknown error)");
    }

    if (!applied) {
        apply_from_gtk_theme_env_fallback();
    }
}

void ThemeController::apply_from_gtk_theme_env_fallback() {
    const char* gtk_theme = std::getenv("GTK_THEME");
    if (gtk_theme) {
        const std::string theme(gtk_theme);
        if (theme.find("dark") != std::string::npos) {
            set_prefer_dark(true);
            return;
        }
    }

    set_prefer_dark(false);
}

void ThemeController::set_prefer_dark(bool prefer_dark) {
    if (m_apply_prefer_dark) {
        m_apply_prefer_dark(prefer_dark);
        return;
    }

    if (!m_gtk_settings) {
        return;
    }

    m_gtk_settings->property_gtk_application_prefer_dark_theme() = prefer_dark;
}

bool ThemeController::can_apply() const noexcept {
    return static_cast<bool>(m_apply_prefer_dark) || static_cast<bool>(m_gtk_settings);
}
