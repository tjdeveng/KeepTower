// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "UIStateManager.h"
#include "../../core/VaultManager.h"
#include <iostream>

namespace UI {

UIStateManager::UIStateManager(const UIWidgets& widgets, VaultManager* vault_manager)
    : m_widgets(widgets)
    , m_vault_manager(vault_manager) {
    // Verify all widget pointers
    if (!m_widgets.save_button || !m_widgets.close_button ||
        !m_widgets.add_account_button || !m_widgets.search_entry ||
        !m_widgets.status_label || !m_widgets.session_label) {
        std::cerr << "UIStateManager: Warning - null widget pointer(s) provided\n";
    }
}

void UIStateManager::set_vault_opened(const std::string& vault_path,
                                       const std::string& username) {
    m_current_vault_path = vault_path;
    m_vault_open = true;
    m_is_locked = false;

    std::cout << "UIStateManager: Vault opened - " << vault_path;
    if (!username.empty()) {
        std::cout << " (user: " << username << ")";
    }
    std::cout << "\n";

    update_ui_sensitivity();
}

void UIStateManager::set_vault_closed() {
    std::cout << "UIStateManager: Vault closed\n";

    m_current_vault_path.clear();
    m_vault_open = false;
    m_is_locked = false;

    update_ui_sensitivity();

    // Clear search entry
    if (m_widgets.search_entry) {
        m_widgets.search_entry->set_text("");
    }

    // Hide session label when vault closes
    if (m_widgets.session_label) {
        m_widgets.session_label->set_visible(false);
    }

    // Clear status
    if (m_widgets.status_label) {
        m_widgets.status_label->set_text("No vault open");
    }
}

void UIStateManager::set_vault_locked(bool locked) {
    m_is_locked = locked;

    std::cout << "UIStateManager: Vault "
              << (locked ? "locked" : "unlocked") << "\n";

    update_ui_sensitivity();
}

void UIStateManager::update_session_display(const std::function<void()>& update_menu_callback) {
    if (!m_vault_manager) {
        std::cerr << "UIStateManager: VaultManager not set\n";
        return;
    }

    auto session_opt = m_vault_manager->get_current_user_session();

    if (!session_opt.has_value()) {
        // No session - likely V1 vault or not authenticated
        if (m_widgets.session_label) {
            m_widgets.session_label->set_visible(false);
        }
        return;
    }

    // V2 vault with active session
    const auto& session = session_opt.value();
    std::string session_text = "User: " + session.username;

    // Add role indicator
    if (session.is_admin()) {
        session_text += " (Admin)";
    } else {
        session_text += " (Standard)";
    }

    // Show password change requirement
    if (session.password_change_required) {
        session_text += " [Password Change Required]";
    }

    // Update the session label widget
    if (m_widgets.session_label) {
        m_widgets.session_label->set_text(session_text);
        m_widgets.session_label->set_visible(true);
    }

    std::cout << "UIStateManager: Session display updated - " << session_text << "\n";

    // Update menu based on user role
    if (update_menu_callback) {
        update_menu_callback();
    }
}

void UIStateManager::set_status(const std::string& message) {
    if (m_widgets.status_label) {
        m_widgets.status_label->set_text(message);
    }
    std::cout << "UIStateManager: Status - " << message << "\n";
}

void UIStateManager::update_ui_sensitivity() {
    // Widgets are enabled when vault is open and not locked
    bool should_enable = m_vault_open && !m_is_locked;

    if (m_widgets.save_button) {
        m_widgets.save_button->set_sensitive(should_enable);
    }

    if (m_widgets.close_button) {
        m_widgets.close_button->set_sensitive(m_vault_open); // Can close even if locked
    }

    if (m_widgets.add_account_button) {
        m_widgets.add_account_button->set_sensitive(should_enable);
    }

    if (m_widgets.search_entry) {
        m_widgets.search_entry->set_sensitive(should_enable);
    }

    std::cout << "UIStateManager: UI sensitivity updated - "
              << (should_enable ? "enabled" : "disabled") << "\n";
}

} // namespace UI
