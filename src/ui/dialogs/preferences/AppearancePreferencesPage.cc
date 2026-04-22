// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "AppearancePreferencesPage.h"

namespace KeepTower::Ui {

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

    auto color_schemes = Gtk::StringList::create({"System Default", "Light", "Dark"});
    m_color_scheme_dropdown.set_model(color_schemes);
    m_color_scheme_dropdown.set_selected(0);
    m_color_scheme_box.append(m_color_scheme_dropdown);

    m_color_scheme_box.set_halign(Gtk::Align::START);
    scheme_row->append(m_color_scheme_box);

    append(*scheme_row);
}

void AppearancePreferencesPage::load_from_model(const PreferencesModel& model) {
    if (model.color_scheme == "light") {
        m_color_scheme_dropdown.set_selected(1);
    } else if (model.color_scheme == "dark") {
        m_color_scheme_dropdown.set_selected(2);
    } else {
        m_color_scheme_dropdown.set_selected(0);
    }
}

void AppearancePreferencesPage::store_to_model(PreferencesModel& model) const {
    const guint selected = m_color_scheme_dropdown.get_selected();
    if (selected == 1) {
        model.color_scheme = "light";
    } else if (selected == 2) {
        model.color_scheme = "dark";
    } else {
        model.color_scheme = "default";
    }
}

}  // namespace KeepTower::Ui
