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

class VaultUiStateApplier {
public:
    struct UIWidgets {
        Gtk::Button* save_button;
        Gtk::Button* close_button;
        Gtk::Button* add_account_button;
        Gtk::SearchEntry* search_entry;
        Gtk::Label* status_label;
        Gtk::Label* session_label;
    };

    explicit VaultUiStateApplier(const UIWidgets& widgets);
    ~VaultUiStateApplier() = default;

    VaultUiStateApplier(const VaultUiStateApplier&) = delete;
    VaultUiStateApplier& operator=(const VaultUiStateApplier&) = delete;
    VaultUiStateApplier(VaultUiStateApplier&&) = delete;
    VaultUiStateApplier& operator=(VaultUiStateApplier&&) = delete;

    void set_vault_opened(const std::string& vault_path, const std::string& username = "");
    void set_vault_closed();
    void set_vault_locked(bool locked, bool vault_open);
    void set_session_text(const std::optional<std::string>& session_text);
    void set_status(const std::string& message);

private:
    UIWidgets m_widgets;

    void update_ui_sensitivity(bool vault_open, bool is_locked);
};

} // namespace UI
