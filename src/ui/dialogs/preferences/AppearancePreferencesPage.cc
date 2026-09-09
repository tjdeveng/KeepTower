// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "AppearancePreferencesPage.h"

#include <giomm/settingsschemasource.h>

namespace KeepTower::Ui {
namespace {
bool supports_system_default_theme() {
#ifdef _WIN32
    return false;
#else
    try {
        auto schema_source = Gio::SettingsSchemaSource::get_default();
        if (!schema_source) {
            return false;
        }
        return static_cast<bool>(schema_source->lookup("org.gnome.desktop.interface", true));
    } catch (...) {
        return false;
    }
#endif
}
} // namespace

AppearancePreferencesPage::AppearancePreferencesPage()
    : Gtk::Box(Gtk::Orientation::VERTICAL, 18),
      m_color_scheme_box(Gtk::Orientation::HORIZONTAL, 12),
      m_color_scheme_label("Colour scheme:") {
    set_margin_start(18);
    set_margin_end(18);
    set_margin_top(18);
    set_margin_bottom(18);

    // Colour scheme controls
    auto* scheme_row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);

    auto* scheme_label = Gtk::make_managed<Gtk::Label>("Colour Scheme");
    scheme_label->set_halign(Gtk::Align::START);
    scheme_label->add_css_class("heading");
    scheme_row->append(*scheme_label);

    m_color_scheme_label.set_halign(Gtk::Align::START);
    m_color_scheme_box.append(m_color_scheme_label);

    // Only offer "System Default" when the current platform exposes the GNOME
    // desktop schema. On Windows and isolated AppImage/non-GNOME runtimes this
    // capability is not reliably available, so explicit Light/Dark mode is safer.
    if (supports_system_default_theme()) {
        auto color_schemes = Gtk::StringList::create({"System Default", "Light", "Dark"});
        m_color_scheme_dropdown.set_model(color_schemes);
    } else {
        auto color_schemes = Gtk::StringList::create({"Light", "Dark"});
        m_color_scheme_dropdown.set_model(color_schemes);
    }
    m_color_scheme_dropdown.set_selected(0);
    m_color_scheme_box.append(m_color_scheme_dropdown);

    m_color_scheme_box.set_halign(Gtk::Align::START);
    scheme_row->append(m_color_scheme_box);

    append(*scheme_row);
}

void AppearancePreferencesPage::load_from_model(const PreferencesModel& model) {
    if (supports_system_default_theme()) {
        if (model.color_scheme == "light") {
            m_color_scheme_dropdown.set_selected(1);
        } else if (model.color_scheme == "dark") {
            m_color_scheme_dropdown.set_selected(2);
        } else {
            m_color_scheme_dropdown.set_selected(0);
        }
        return;
    }

    if (model.color_scheme == "dark") {
        m_color_scheme_dropdown.set_selected(1);
    } else {
        m_color_scheme_dropdown.set_selected(0);
    }
}

void AppearancePreferencesPage::store_to_model(PreferencesModel& model) const {
    const guint selected = m_color_scheme_dropdown.get_selected();

    if (!supports_system_default_theme()) {
        model.color_scheme = (selected == 1) ? "dark" : "light";
        return;
    }

    if (selected == 1) {
        model.color_scheme = "light";
    } else if (selected == 2) {
        model.color_scheme = "dark";
    } else {
        model.color_scheme = "default";
    }
}

}  // namespace KeepTower::Ui
