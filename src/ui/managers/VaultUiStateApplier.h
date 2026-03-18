// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file VaultUiStateApplier.h
 * @brief Applies vault UI state to MainWindow widgets (sensitivity/labels)
 *
 * Part of Phase 5 refactoring: MainWindow size reduction.
 *
 * This class does not own the vault lifecycle state; it applies UI updates
 * when the coordinator/handlers report state transitions.
 */

#pragma once

#include <gtkmm.h>

#include <optional>
#include <string>

namespace UI {

/**
 * @brief Applies high-level vault state changes to injected MainWindow widgets.
 *
 * This class is a thin UI adapter: it updates widget sensitivity and labels
 * based on vault open/close/lock transitions.
 */
class VaultUiStateApplier {
public:
    /**
     * @brief Collection of widgets that receive state updates.
     */
    struct UIWidgets {
        Gtk::Button* save_button;          ///< Save action button
        Gtk::Button* close_button;         ///< Close-vault action button
        Gtk::Button* add_account_button;   ///< Add-account action button
        Gtk::SearchEntry* search_entry;    ///< Account search entry
        Gtk::Label* status_label;          ///< Status text label
        Gtk::Label* session_label;         ///< Session/user label
    };

    /**
     * @brief Construct a state applier bound to a set of MainWindow widgets.
     * @param widgets Widget collection to update (pointers must remain valid for the lifetime of this object)
     */
    explicit VaultUiStateApplier(const UIWidgets& widgets);
    ~VaultUiStateApplier() = default;

    VaultUiStateApplier(const VaultUiStateApplier&) = delete;
    VaultUiStateApplier& operator=(const VaultUiStateApplier&) = delete;
    VaultUiStateApplier(VaultUiStateApplier&&) = delete;
    VaultUiStateApplier& operator=(VaultUiStateApplier&&) = delete;

    /** @brief Apply UI changes for "vault opened" state. */
    void set_vault_opened(const std::string& vault_path, const std::string& username = "");

    /** @brief Apply UI changes for "vault closed" state. */
    void set_vault_closed();

    /** @brief Apply UI changes for lock/unlock transitions. */
    void set_vault_locked(bool locked, bool vault_open);

    /** @brief Update the session label text. */
    void set_session_text(const std::optional<std::string>& session_text);

    /** @brief Update the status label text. */
    void set_status(const std::string& message);

private:
    UIWidgets m_widgets;

    void update_ui_sensitivity(bool vault_open, bool is_locked);
};

} // namespace UI
