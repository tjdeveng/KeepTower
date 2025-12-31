// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file UIStateManager.h
 * @brief Centralized UI state management for MainWindow
 *
 * Part of Phase 5 refactoring: MainWindow size reduction
 * Extracts vault state tracking and UI sensitivity management from MainWindow
 */

#pragma once

#include <gtkmm.h>
#include <string>
#include <functional>

// Forward declarations
class VaultManager;

namespace UI {

/**
 * @brief Manages UI state based on vault status
 *
 * Centralizes vault open/close state tracking and UI element
 * enable/disable logic to reduce MainWindow complexity.
 *
 * Design Goals:
 * - Reduce MainWindow size by ~100-150 lines
 * - Centralize state management
 * - Consistent UI updates across vault operations
 * - Single source of truth for vault state
 *
 * @note This class owns the state flags but not the UI widgets
 */
class UIStateManager {
public:
    /**
     * @brief UI widgets that need state management
     */
    struct UIWidgets {
        Gtk::Button* save_button;            ///< Save vault button
        Gtk::Button* close_button;           ///< Close vault button
        Gtk::Button* add_account_button;     ///< Add account button
        Gtk::SearchEntry* search_entry;      ///< Search text entry
        Gtk::Label* status_label;            ///< Status bar label
        Gtk::Label* session_label;           ///< Session info label (V2 username/role)
    };

    /**
     * @brief Construct UI state manager
     * @param widgets References to UI widgets to manage
     * @param vault_manager Pointer to VaultManager for state queries
     */
    explicit UIStateManager(const UIWidgets& widgets, VaultManager* vault_manager);

    ~UIStateManager() = default;

    // Delete copy and move
    UIStateManager(const UIStateManager&) = delete;
    UIStateManager& operator=(const UIStateManager&) = delete;
    UIStateManager(UIStateManager&&) = delete;
    UIStateManager& operator=(UIStateManager&&) = delete;

    /**
     * @brief Set vault opened state and update UI
     * @param vault_path Path to opened vault
     * @param username Username of logged in user (V2 vaults)
     */
    void set_vault_opened(const std::string& vault_path, const std::string& username = "");

    /**
     * @brief Set vault closed state and update UI
     */
    void set_vault_closed();

    /**
     * @brief Set vault locked state (V1 vaults)
     * @param locked Whether vault is locked
     */
    void set_vault_locked(bool locked);

    /**
     * @brief Update session display (V2 multi-user vaults)
     * @param update_menu_callback Callback to update menu based on role
     */
    void update_session_display(const std::function<void()>& update_menu_callback);

    /**
     * @brief Update status label
     * @param message Status message to display
     */
    void set_status(const std::string& message);

    /**
     * @brief Get current vault open state
     * @return true if vault is open
     */
    bool is_vault_open() const { return m_vault_open; }

    /**
     * @brief Get current locked state
     * @return true if vault is locked
     */
    bool is_locked() const { return m_is_locked; }

    /**
     * @brief Get current vault path
     * @return Path to current vault
     */
    const std::string& get_vault_path() const { return m_current_vault_path; }

private:
    UIWidgets m_widgets;
    VaultManager* m_vault_manager;

    // State flags
    bool m_vault_open = false;
    bool m_is_locked = false;
    std::string m_current_vault_path;

    /**
     * @brief Update UI element sensitivity based on vault state
     */
    void update_ui_sensitivity();
};

} // namespace UI
