// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "VaultUiStateApplier.h"

#include "../../utils/Log.h"
#include "../../utils/StringHelpers.h"

namespace UI {

VaultUiStateApplier::VaultUiStateApplier(const UIWidgets& widgets)
    : m_widgets(widgets) {
    if (!m_widgets.save_button || !m_widgets.close_button ||
        !m_widgets.add_account_button || !m_widgets.search_entry ||
        !m_widgets.status_label || !m_widgets.session_label) {
        KeepTower::Log::warning("VaultUiStateApplier: null widget pointer(s) provided");
    }
}

void VaultUiStateApplier::set_vault_opened(const std::string& vault_path, const std::string& username) {
    if (!username.empty()) {
        KeepTower::Log::debug("VaultUiStateApplier: vault opened - '{}' (user: '{}')", vault_path, username);
    } else {
        KeepTower::Log::debug("VaultUiStateApplier: vault opened - '{}'", vault_path);
    }

    update_ui_sensitivity(true, false);
}

void VaultUiStateApplier::set_vault_closed() {
    KeepTower::Log::debug("VaultUiStateApplier: vault closed");

    update_ui_sensitivity(false, false);

    if (m_widgets.search_entry) {
        m_widgets.search_entry->set_text("");
    }

    if (m_widgets.session_label) {
        m_widgets.session_label->set_visible(false);
    }

    if (m_widgets.status_label) {
        m_widgets.status_label->set_text(KeepTower::make_valid_utf8("No vault open", "status"));
    }
}

void VaultUiStateApplier::set_vault_locked(bool locked, bool vault_open) {
    KeepTower::Log::debug("VaultUiStateApplier: vault {}", (locked ? "locked" : "unlocked"));

    update_ui_sensitivity(vault_open, locked);
}

void VaultUiStateApplier::set_session_text(const std::optional<std::string>& session_text) {
    if (!m_widgets.session_label) {
        return;
    }

    if (!session_text.has_value() || session_text->empty()) {
        m_widgets.session_label->set_visible(false);
        return;
    }

    m_widgets.session_label->set_text(KeepTower::make_valid_utf8(*session_text, "session"));
    m_widgets.session_label->set_visible(true);
}

void VaultUiStateApplier::set_status(const std::string& message) {
    if (m_widgets.status_label) {
        m_widgets.status_label->set_text(KeepTower::make_valid_utf8(message, "status"));
    }
    KeepTower::Log::debug("VaultUiStateApplier: status - '{}'", message);
}

void VaultUiStateApplier::update_ui_sensitivity(bool vault_open, bool is_locked) {
    const bool should_enable = vault_open && !is_locked;

    if (m_widgets.save_button) {
        m_widgets.save_button->set_sensitive(should_enable);
    }

    if (m_widgets.close_button) {
        m_widgets.close_button->set_sensitive(vault_open);
    }

    if (m_widgets.add_account_button) {
        m_widgets.add_account_button->set_sensitive(should_enable);
    }

    if (m_widgets.search_entry) {
        m_widgets.search_entry->set_sensitive(should_enable);
    }

    KeepTower::Log::debug(
        "VaultUiStateApplier: UI sensitivity updated - {}",
        (should_enable ? "enabled" : "disabled")
    );
}

} // namespace UI
