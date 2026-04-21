// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "PreferencesDialog.h"

#include "preferences/AccountSecurityPreferencesPage.h"
#include "preferences/AppearancePreferencesPage.h"
#include "preferences/PreferencesModel.h"
#include "preferences/PreferencesPresenter.h"
#include "preferences/StoragePreferencesPage.h"
#include "preferences/VaultSecurityPreferencesPage.h"

#include <cstdlib>
#include <string>

PreferencesDialog::PreferencesDialog(Gtk::Window& parent, VaultManager* vault_manager)
    : Gtk::Dialog("Preferences", parent, true)
    , m_vault_manager(vault_manager)
    , m_main_box(Gtk::Orientation::HORIZONTAL)
{
    // Match the intended steady-state size to avoid resize flicker on open.
    set_default_size(650, -1);
    set_modal(true);

    m_presenter = std::make_unique<KeepTower::Ui::PreferencesPresenter>(m_vault_manager);
    m_settings = m_presenter->settings();
    m_model = std::make_unique<KeepTower::Ui::PreferencesModel>();

    setup_ui();
    load_settings();

    signal_show().connect(sigc::mem_fun(*this, &PreferencesDialog::on_dialog_shown));
    signal_response().connect(sigc::mem_fun(*this, &PreferencesDialog::on_response));
}

PreferencesDialog::~PreferencesDialog() = default;

void PreferencesDialog::setup_ui() {
    auto content_area = get_content_area();

    m_stack_sidebar.set_stack(m_stack);
    m_stack_sidebar.set_hexpand(false);
    m_stack_sidebar.set_vexpand(true);

    m_stack.set_hexpand(true);
    m_stack.set_vexpand(true);

    // Wrap the stack in a ScrolledWindow so the dialog height is bounded and
    // Apply/Cancel are always reachable, even when Argon2 params are expanded.
    m_stack_scroll.set_child(m_stack);
    m_stack_scroll.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    m_stack_scroll.set_propagate_natural_height(true);
    m_stack_scroll.set_max_content_height(600);
    m_stack_scroll.set_hexpand(true);
    m_stack_scroll.set_vexpand(true);

    m_main_box.set_spacing(12);
    m_main_box.append(m_stack_sidebar);
    m_main_box.append(m_stack_scroll);

    content_area->append(m_main_box);

    add_button("Cancel", Gtk::ResponseType::CANCEL);
    add_button("Apply", Gtk::ResponseType::APPLY);

    m_appearance_page_widget = Gtk::make_managed<KeepTower::Ui::AppearancePreferencesPage>();
    m_account_security_page_widget = Gtk::make_managed<KeepTower::Ui::AccountSecurityPreferencesPage>();
    m_storage_page_widget = Gtk::make_managed<KeepTower::Ui::StoragePreferencesPage>(m_vault_manager, m_settings);
    m_vault_security_page_widget = Gtk::make_managed<KeepTower::Ui::VaultSecurityPreferencesPage>(m_vault_manager);

    m_stack.add(*m_appearance_page_widget, "appearance", "Appearance");
    m_stack.add(*m_account_security_page_widget, "account_security", "Account Security");
    m_stack.add(*m_storage_page_widget, "storage", "Storage");
    m_stack.add(*m_vault_security_page_widget, "vault_security", "Vault Security");

    // Lazy-load vault security page only when it becomes visible; it can resize the dialog.
    m_stack.property_visible_child_name().signal_changed().connect([this]() {
        if (m_vault_security_lazy_loaded) {
            return;
        }

        if (m_stack.get_visible_child_name() == "vault_security") {
            m_vault_security_lazy_loaded = true;
            if (m_vault_security_page_widget) {
                m_vault_security_page_widget->on_dialog_shown();
            }
        }
    });

    m_appearance_page_widget->color_scheme_dropdown().property_selected().signal_changed().connect(
        sigc::mem_fun(*this, &PreferencesDialog::on_color_scheme_changed));

    m_stack.set_visible_child("appearance");
}

void PreferencesDialog::load_settings() {
    if (!m_presenter || !m_model) {
        return;
    }

    *m_model = m_presenter->load();

    m_original_color_scheme = m_model->color_scheme;

    m_is_loading = true;
    m_appearance_page_widget->load_from_model(*m_model);
    m_account_security_page_widget->load_from_model(*m_model);
    m_storage_page_widget->load_from_model(*m_model);
    m_vault_security_page_widget->load_from_model(*m_model);
    m_is_loading = false;

    // When a vault is open, only admins should be able to change vault-affecting settings.
    // Non-admin users should not see privileged pages.
    const bool vault_open = m_model->vault_open;
    const bool is_admin = m_model->vault_admin;
    const bool show_privileged_pages = (!vault_open) || is_admin;

    if (auto page = m_stack.get_page(*m_account_security_page_widget)) {
        page->set_visible(show_privileged_pages);
    }
    if (auto page = m_stack.get_page(*m_storage_page_widget)) {
        page->set_visible(show_privileged_pages);
    }

    // If a vault is open, only admins should see Vault Security.
    // If no vault is open, allow configuring defaults for new vaults.
    const bool show_vault_security = (!m_model->vault_open) || m_model->vault_admin;
    if (auto page = m_stack.get_page(*m_vault_security_page_widget)) {
        page->set_visible(show_vault_security);
    }

    // Ensure we don't leave the user on a now-hidden page.
    const auto visible_name = m_stack.get_visible_child_name();
    if ((!show_privileged_pages && (visible_name == "account_security" || visible_name == "storage")) ||
        (!show_vault_security && visible_name == "vault_security")) {
        m_stack.set_visible_child("appearance");
    }
}

void PreferencesDialog::save_settings() {
    if (!m_presenter || !m_model) {
        return;
    }

    m_appearance_page_widget->store_to_model(*m_model);

    const bool vault_open = m_model->vault_open;
    const bool is_admin = m_model->vault_admin;
    const bool allow_privileged_pages = (!vault_open) || is_admin;

    if (allow_privileged_pages) {
        m_account_security_page_widget->store_to_model(*m_model);
        m_storage_page_widget->store_to_model(*m_model);
        m_vault_security_page_widget->store_to_model(*m_model);
    } else {
        // Non-admin users in an open vault should not be able to persist privileged settings.
        // We still allow appearance changes.
    }

    m_presenter->save(*m_model);

    m_original_color_scheme = m_model->color_scheme;
    apply_color_scheme(m_original_color_scheme);
}

void PreferencesDialog::apply_color_scheme(const std::string& scheme) {
    auto gtk_settings = Gtk::Settings::get_default();
    if (!gtk_settings) {
        return;
    }

    if (scheme == "dark") {
        gtk_settings->property_gtk_application_prefer_dark_theme() = true;
        return;
    }

    if (scheme == "light") {
        gtk_settings->property_gtk_application_prefer_dark_theme() = false;
        return;
    }

    // System Default: follow GNOME desktop preference.
    bool applied = false;
    try {
        auto desktop_settings = Gio::Settings::create("org.gnome.desktop.interface");
        const auto system_color_scheme = desktop_settings->get_string("color-scheme");
        // color-scheme can be: "default", "prefer-dark", "prefer-light"
        gtk_settings->property_gtk_application_prefer_dark_theme() = (system_color_scheme == "prefer-dark");
        applied = true;
    } catch (...) {
        // ignore
    }

    if (!applied) {
        const char* gtk_theme = std::getenv("GTK_THEME");
        if (gtk_theme && std::string(gtk_theme).find("dark") != std::string::npos) {
            gtk_settings->property_gtk_application_prefer_dark_theme() = true;
        } else {
            gtk_settings->property_gtk_application_prefer_dark_theme() = false;
        }
    }
}

void PreferencesDialog::on_dialog_shown() {
    // Only run vault security lazy-load if that page is actually visible.
    if (!m_vault_security_lazy_loaded && m_stack.get_visible_child_name() == "vault_security") {
        m_vault_security_lazy_loaded = true;
        if (m_vault_security_page_widget) {
            m_vault_security_page_widget->on_dialog_shown();
        }
    }
}

void PreferencesDialog::on_color_scheme_changed() {
    if (!m_model || !m_appearance_page_widget) {
        return;
    }

    if (m_is_loading) {
        return;
    }

    // Update model; persisted on Apply.
    m_appearance_page_widget->store_to_model(*m_model);

    // Preview immediately (matches pre-refactor behavior).
    apply_color_scheme(m_model->color_scheme);
}

void PreferencesDialog::on_response(int response_id) {
    if (response_id == Gtk::ResponseType::APPLY) {
        save_settings();
    } else {
        // Cancel (or close): revert any previewed theme changes.
        if (m_model && !m_original_color_scheme.empty()) {
            m_model->color_scheme = m_original_color_scheme;
            m_is_loading = true;
            m_appearance_page_widget->load_from_model(*m_model);
            m_is_loading = false;
            apply_color_scheme(m_original_color_scheme);
        }
    }

    hide();
}
