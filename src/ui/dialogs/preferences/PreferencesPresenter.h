// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#ifndef KEEPTOWER_UI_DIALOGS_PREFERENCES_PREFERENCESPRESENTER_H
#define KEEPTOWER_UI_DIALOGS_PREFERENCES_PREFERENCESPRESENTER_H

#include "PreferencesModel.h"

#include <giomm/settings.h>
#include <glibmm/refptr.h>

class VaultManager;

namespace KeepTower::Ui {

/**
 * @brief Loads and saves user preferences for the Preferences dialog/pages.
 *
 * Acts as the boundary between UI pages and persistent storage (GSettings) and,
 * when applicable, the currently open vault (via VaultManager).
 */
class PreferencesPresenter final {
public:
    /**
     * @brief Construct a presenter.
     * @param vault_manager Optional vault manager (non-owning).
     */
    explicit PreferencesPresenter(VaultManager* vault_manager);

    /**
     * @brief Whether a usable GSettings backend is available.
     * @return True if settings can be read/written.
     */
    [[nodiscard]] bool has_settings() const noexcept { return static_cast<bool>(m_settings); }

    /**
     * @brief Load preferences into a model.
     * @return Populated PreferencesModel (with safe defaults if settings unavailable).
     */
    [[nodiscard]] PreferencesModel load() const;

    /**
     * @brief Persist preferences from a model.
     * @param model Preferences state to save.
     */
    void save(const PreferencesModel& model) const;

    /**
     * @brief Access the underlying settings object.
     * @return GSettings instance or empty refptr when unavailable.
     */
    [[nodiscard]] Glib::RefPtr<Gio::Settings> settings() const noexcept { return m_settings; }

private:
    /**
     * @brief Try to create the application's GSettings instance.
     * @return Settings instance, or empty refptr if schema not available.
     */
    [[nodiscard]] static Glib::RefPtr<Gio::Settings> try_create_settings();

    VaultManager* m_vault_manager;  ///< Non-owning pointer to vault manager (optional)
    Glib::RefPtr<Gio::Settings> m_settings;  ///< Application GSettings backend (may be empty)
};

}  // namespace KeepTower::Ui

#endif
