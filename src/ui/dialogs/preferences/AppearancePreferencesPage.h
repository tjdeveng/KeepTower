// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#ifndef KEEPTOWER_UI_DIALOGS_PREFERENCES_APPEARANCEPREFERENCESPAGE_H
#define KEEPTOWER_UI_DIALOGS_PREFERENCES_APPEARANCEPREFERENCESPAGE_H

#include "PreferencesModel.h"

#include <gtkmm.h>

namespace KeepTower::Ui {

/**
 * @brief Preferences page for appearance-related settings.
 *
 * Currently contains the application color scheme selector.
 */
class AppearancePreferencesPage final : public Gtk::Box {
public:
    /** @brief Construct the page widget. */
    AppearancePreferencesPage();

    /**
     * @brief Populate widgets from the provided model.
     * @param model Preferences state to load.
     */
    void load_from_model(const PreferencesModel& model);

    /**
     * @brief Store current widget values into the provided model.
     * @param model Preferences state to update.
     */
    void store_to_model(PreferencesModel& model) const;

    /**
     * @brief Access the color scheme dropdown for signal wiring.
     * @return Reference to the dropdown.
     */
    [[nodiscard]] Gtk::DropDown& color_scheme_dropdown() noexcept { return m_color_scheme_dropdown; }

private:
    Gtk::Box m_color_scheme_box;  ///< Container row for label + dropdown
    Gtk::Label m_color_scheme_label;  ///< "Color scheme:" label
    Gtk::DropDown m_color_scheme_dropdown;  ///< Scheme selector (System Default / Light / Dark)
};

}  // namespace KeepTower::Ui

#endif
