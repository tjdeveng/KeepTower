// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file V2UserLoginDialog.h
 * @brief User authentication dialog for V2 multi-user vaults
 *
 * Provides username+password authentication for LUKS-style key slot vaults.
 * Supports both password-only and password+YubiKey authentication modes.
 */

#ifndef V2USERLOGINDIALOG_H
#define V2USERLOGINDIALOG_H

#include <gtkmm.h>
#include <string>
#include <optional>

/**
 * @brief Authentication credentials for V2 vault login
 *
 * Contains username and password entered by user.
 * YubiKey authentication is handled separately by vault manager.
 */
struct V2LoginCredentials {
    Glib::ustring username;  ///< User identifier (case-sensitive)
    Glib::ustring password;  ///< User password for KEK derivation

    /**
     * @brief Clear credentials from memory securely
     *
     * Overwrites password with zeros before destruction.
     * Username is not cleared (not considered sensitive).
     */
    void clear() noexcept;
};

/**
 * @brief User authentication dialog for V2 vaults
 *
 * Modal dialog for username+password entry. Validates input before
 * enabling OK button. Supports password visibility toggle and
 * vault-specific security policy hints.
 *
 * @section usage Usage Example
 * @code
 * V2UserLoginDialog dialog(*parent_window, vault_requires_yubikey);
 * if (dialog.run() == Gtk::ResponseType::OK) {
 *     auto creds = dialog.get_credentials();
 *     auto result = vault_manager.open_vault_v2(filepath, creds.username, creds.password);
 *     creds.clear();  // Explicitly clear password
 * }
 * @endcode
 *
 * @section security Security Features
 * - Password masked by default
 * - Credentials cleared on dialog close
 * - Empty username/password validation
 * - YubiKey requirement indicator
 * - No credential caching
 *
 * @note Username is case-sensitive
 * @note Dialog does NOT validate credentials (vault manager does)
 */
class V2UserLoginDialog : public Gtk::Dialog {
public:
    /**
     * @brief Construct user login dialog
     *
     * @param parent Parent window for modal positioning
     * @param vault_requires_yubikey If true, shows YubiKey requirement message
     */
    explicit V2UserLoginDialog(Gtk::Window& parent, bool vault_requires_yubikey = false);

    /**
     * @brief Destructor - clears sensitive data
     *
     * Overwrites password entry content before destruction.
     */
    ~V2UserLoginDialog() override;

    // Non-copyable, non-movable (contains sensitive data)
    V2UserLoginDialog(const V2UserLoginDialog&) = delete;
    V2UserLoginDialog& operator=(const V2UserLoginDialog&) = delete;
    V2UserLoginDialog(V2UserLoginDialog&&) = delete;
    V2UserLoginDialog& operator=(V2UserLoginDialog&&) = delete;

    /**
     * @brief Get entered credentials
     *
     * Returns username and password as entered by user.
     * Caller MUST call clear() on returned credentials after use.
     *
     * @return Credentials structure with username and password
     * @warning Caller responsible for clearing returned credentials
     * @note Only call after dialog returns ResponseType::OK
     */
    [[nodiscard]] V2LoginCredentials get_credentials() const;

    /**
     * @brief Set username field (for retry scenarios)
     *
     * Pre-fills username field (e.g., after authentication failure).
     * Cursor moves to password field automatically.
     *
     * @param username Username to pre-fill
     */
    void set_username(std::string_view username);

protected:
    /**
     * @brief Show/hide password based on checkbox state
     */
    void on_show_password_toggled();

    /**
     * @brief Enable OK button when both fields have content
     */
    void on_input_changed();

    /**
     * @brief Clear password field securely before closing
     *
     * Overrides Gtk::Dialog::on_response to clear password entry
     * with zeros before dialog closes.
     *
     * @param response_id Dialog response code
     */
    void on_response(int response_id) override;

private:
    // Layout containers
    Gtk::Box m_content_box{Gtk::Orientation::VERTICAL, 12};
    Gtk::Box m_yubikey_box{Gtk::Orientation::HORIZONTAL, 8};
    Gtk::Box m_username_box{Gtk::Orientation::VERTICAL, 4};
    Gtk::Box m_password_box{Gtk::Orientation::VERTICAL, 4};

    // Labels and messages
    Gtk::Label m_title_label;
    Gtk::Label m_yubikey_info_label;
    Gtk::Image m_yubikey_icon;
    Gtk::Label m_username_label{"Username:"};
    Gtk::Label m_password_label{"Password:"};

    // Input fields
    Gtk::Entry m_username_entry;
    Gtk::Entry m_password_entry;

    // Controls
    Gtk::CheckButton m_show_password_check{"Show password"};

    // Dialog buttons (stored for sensitivity control)
    Gtk::Button* m_ok_button{nullptr};
    Gtk::Button* m_cancel_button{nullptr};

    // Configuration
    bool m_vault_requires_yubikey{false};
};

#endif // V2USERLOGINDIALOG_H
