// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <giomm/settings.h>
#include <gtkmm.h>

#include <memory>
#include <string>

class VaultManager;

namespace KeepTower::Ui {
class PreferencesPresenter;
struct PreferencesModel;

class AppearancePreferencesPage;
class AccountSecurityPreferencesPage;
class StoragePreferencesPage;
class VaultSecurityPreferencesPage;
}  // namespace KeepTower::Ui

/**
 * @brief Preferences dialog for application settings.
 *
 * Thin shell that composes the extracted preferences pages in a Gtk::Stack,
 * loads/saves state via KeepTower::Ui::PreferencesPresenter, and manages dialog
 * level behaviors such as Apply/Cancel and temporary theme preview.
 *
 * Vault-scoped behavior:
 * - When a vault is open, some settings are applied to (and persisted in) the
 *   currently open vault.
 * - The dialog may hide privileged pages when a vault is open and the current
 *   user is not an administrator.
 *
 * Theme preview behavior:
 * - Changes apply immediately when the user selects a different color scheme.
 * - The new scheme is persisted only when the user clicks Apply.
 * - If the dialog is cancelled/closed, the original scheme is restored.
 */
class PreferencesDialog final : public Gtk::Dialog {
public:
    /**
     * @brief Construct the preferences dialog.
     *
     * @param parent Parent window used for transient/modal behavior.
     * @param vault_manager Optional vault manager (non-owning). When provided,
     *        some pages may load/save vault-scoped settings.
     */
    explicit PreferencesDialog(Gtk::Window& parent, VaultManager* vault_manager = nullptr);

    /** @brief Destructor. */
    ~PreferencesDialog() override;

    PreferencesDialog(const PreferencesDialog&) = delete;
    PreferencesDialog& operator=(const PreferencesDialog&) = delete;
    PreferencesDialog(PreferencesDialog&&) = delete;
    PreferencesDialog& operator=(PreferencesDialog&&) = delete;

private:
    /** @brief Build the dialog UI and attach page widgets/signals. */
    void setup_ui();

    /** @brief Load settings into the model and populate page widgets. */
    void load_settings();

    /** @brief Collect values from pages and persist them via the presenter. */
    void save_settings();

    /**
     * @brief Apply the selected color scheme to the running application.
     *
     * @param scheme Application scheme identifier: "default", "light", or "dark".
     */
    void apply_color_scheme(const std::string& scheme);

    /** @brief Handler invoked when the dialog is shown. */
    void on_dialog_shown();

    /** @brief Handler invoked when the appearance color scheme selection changes. */
    void on_color_scheme_changed();

    /** @brief Handle Apply/Cancel/Close responses. */
    void on_response(int response_id) override;

    Glib::RefPtr<Gio::Settings> m_settings;  ///< Application settings (may be null when schema unavailable)
    VaultManager* m_vault_manager = nullptr;  ///< Non-owning pointer to vault manager (optional)

    bool m_is_loading = false;  ///< Guards signal handlers while initializing widgets
    std::string m_original_color_scheme;  ///< Scheme active when dialog opened (for Cancel revert)
    bool m_vault_security_lazy_loaded = false;  ///< Ensures vault security heavy UI is initialized once

    std::unique_ptr<KeepTower::Ui::PreferencesPresenter> m_presenter;  ///< Loads/saves settings from/to storage/vault
    std::unique_ptr<KeepTower::Ui::PreferencesModel> m_model;  ///< In-memory working copy for the dialog session

    Gtk::Box m_main_box;  ///< Horizontal container for sidebar + stack
    Gtk::StackSidebar m_stack_sidebar;  ///< Navigation sidebar
    Gtk::Stack m_stack;  ///< Page stack
    Gtk::ScrolledWindow m_stack_scroll;  ///< Caps dialog height; stack scrolls when content overflows

    KeepTower::Ui::AppearancePreferencesPage* m_appearance_page_widget = nullptr;  ///< Managed by GTK
    KeepTower::Ui::AccountSecurityPreferencesPage* m_account_security_page_widget = nullptr;  ///< Managed by GTK
    KeepTower::Ui::StoragePreferencesPage* m_storage_page_widget = nullptr;  ///< Managed by GTK
    KeepTower::Ui::VaultSecurityPreferencesPage* m_vault_security_page_widget = nullptr;  ///< Managed by GTK
};

#endif  // PREFERENCESDIALOG_H
