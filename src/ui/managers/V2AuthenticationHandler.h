// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file V2AuthenticationHandler.h
 * @brief Handles V2 multi-user vault authentication flows
 *
 * Part of Phase 5 refactoring: MainWindow size reduction
 * Extracts complex V2 authentication logic from MainWindow
 */

#pragma once

#include <gtkmm.h>
#include <string>
#include <functional>

// Forward declarations
class VaultManager;
class MainWindow;

namespace UI {

class DialogManager;

/**
 * @brief Manages V2 vault authentication workflows
 *
 * Handles the complex multi-step authentication flows for V2 vaults:
 * - User login with username/password
 * - YubiKey prompts and verification
 * - Required password changes on first login
 * - Required YubiKey enrollment per policy
 *
 * Design Goals:
 * - Reduce MainWindow size by ~300-350 lines
 * - Centralize V2 authentication logic
 * - Simplify YubiKey integration handling
 * - Clean separation of authentication concerns
 */
class V2AuthenticationHandler {
public:
    /**
     * @brief Callback when authentication completes successfully
     * @param vault_path Path to opened vault
     * @param username Username of authenticated user
     */
    using AuthSuccessCallback = std::function<void(const std::string& vault_path,
                                                     const std::string& username)>;

    /**
     * @brief Construct V2 authentication handler
     * @param window Reference to MainWindow for dialog parenting
     * @param vault_manager Pointer to VaultManager for operations
     * @param dialog_manager Pointer to DialogManager for error dialogs
     */
    V2AuthenticationHandler(MainWindow& window,
                           VaultManager* vault_manager,
                           DialogManager* dialog_manager);

    ~V2AuthenticationHandler() = default;

    // Delete copy and move
    V2AuthenticationHandler(const V2AuthenticationHandler&) = delete;
    V2AuthenticationHandler& operator=(const V2AuthenticationHandler&) = delete;
    V2AuthenticationHandler(V2AuthenticationHandler&&) = delete;
    V2AuthenticationHandler& operator=(V2AuthenticationHandler&&) = delete;

    /**
     * @brief Start V2 vault authentication flow
     * @param vault_path Path to V2 vault file
     * @param on_success Callback when authentication succeeds
     */
    void handle_vault_open(const std::string& vault_path, AuthSuccessCallback on_success);

private:
    MainWindow& m_window;
    VaultManager* m_vault_manager;
    DialogManager* m_dialog_manager;

    // Current authentication state
    std::string m_current_vault_path;
    AuthSuccessCallback m_success_callback;

    /**
     * @brief Handle required password change on first login
     * @param username Username requiring password change
     */
    void handle_password_change_required(const std::string& username);

    /**
     * @brief Handle required YubiKey enrollment per policy
     * @param username Username requiring YubiKey enrollment
     * @param password Optional password (if just changed, avoids asking again)
     */
    void handle_yubikey_enrollment_required(const std::string& username,
                                           const Glib::ustring& password = Glib::ustring());

    /**
     * @brief Check and prompt for YubiKey if required
     * @param vault_path Path to vault being opened
     * @param yubikey_serial Output parameter for required YubiKey serial
     * @return true if YubiKey check passed (present or not required)
     */
    bool check_yubikey_requirement(const std::string& vault_path, std::string& yubikey_serial);
};

} // namespace UI
