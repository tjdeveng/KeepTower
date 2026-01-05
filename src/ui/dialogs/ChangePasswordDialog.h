// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file ChangePasswordDialog.h
 * @brief Password change dialog for user password updates
 *
 * Provides secure password change interface with:
 * - Current password verification
 * - New password confirmation
 * - Policy validation (min length)
 * - Forced change mode for first login
 */

#ifndef CHANGEPASSWORDDIALOG_H
#define CHANGEPASSWORDDIALOG_H

#include <gtkmm.h>
#include <string>
#include <cstdint>

/**
 * @brief Password change request data
 *
 * Contains current and new passwords for vault password change.
 * Must be securely cleared after use.
 */
struct PasswordChangeRequest {
    Glib::ustring current_password;  ///< Current password for verification
    Glib::ustring new_password;      ///< New password to set
    std::string yubikey_pin;         ///< YubiKey PIN (if YubiKey enrolled)

    /**
     * @brief Securely clear passwords from memory
     *
     * Overwrites password buffers with zeros before destruction.
     */
    void clear() noexcept;
};

/**
 * @brief Dialog for user password changes
 *
 * Modal dialog for changing user password with validation.
 * Supports two modes:
 * - **Voluntary change**: User provides current password + new password
 * - **Forced change**: New user must change temporary password (on_first_login)
 *
 * @section validation Password Validation
 * - Current password must not be empty (except forced mode)
 * - New password must meet minimum length requirement
 * - New password must match confirmation field
 * - New password must differ from current password
 *
 * @section security Security Features
 * - All password fields masked by default
 * - Passwords cleared on dialog close
 * - Optional show password toggle
 * - Real-time validation feedback
 * - No password caching
 *
 * @section usage Usage Example
 * @code
 * ChangePasswordDialog dialog(*parent, 12, false);  // min_length=12, not forced
 * if (dialog.run() == Gtk::ResponseType::OK) {
 *     auto req = dialog.get_request();
 *     auto result = vault_manager.change_user_password("alice", req.current_password, req.new_password);
 *     req.clear();  // CRITICAL: Clear passwords
 * }
 * @endcode
 */
class ChangePasswordDialog : public Gtk::Dialog {
public:
    /**
     * @brief Construct password change dialog
     *
     * @param parent Parent window for modal positioning
     * @param min_password_length Minimum required password length (from vault policy)
     * @param is_forced_change If true, shows first-login warning and skips current password
     */
    explicit ChangePasswordDialog(
        Gtk::Window& parent,
        uint32_t min_password_length = 12,
        bool is_forced_change = false
    );

    /**
     * @brief Destructor - clears sensitive data
     *
     * Overwrites all password entry fields before destruction.
     */
    ~ChangePasswordDialog() override;

    // Non-copyable, non-movable (contains sensitive data)
    ChangePasswordDialog(const ChangePasswordDialog&) = delete;
    ChangePasswordDialog& operator=(const ChangePasswordDialog&) = delete;
    ChangePasswordDialog(ChangePasswordDialog&&) = delete;
    ChangePasswordDialog& operator=(ChangePasswordDialog&&) = delete;

    /**
     * @brief Get password change request data
     *
     * Returns current and new passwords entered by user.
     * Caller MUST call clear() on returned request after use.
     *
     * @return Password change request structure
     * @warning Caller responsible for clearing returned passwords
     * @note Only call after dialog returns ResponseType::OK
     */
    [[nodiscard]] PasswordChangeRequest get_request() const;

    /**
     * @brief Set current password (for forced change scenarios)
     *
     * Pre-fills current password field with temporary password.
     * Used when user is logging in for first time with temp password.
     *
     * @param temp_password Temporary password from authentication
     */
    void set_current_password(std::string_view temp_password);

    /**
     * @brief Show YubiKey PIN entry field (when YubiKey is enrolled)
     *
     * Displays PIN entry field for users with YubiKey enrolled.
     * Call this before running dialog if YubiKey is enrolled.
     */
    void set_yubikey_required(bool required);

protected:
    /**
     * @brief Validate passwords and enable OK button
     *
     * Checks:
     * - All required fields non-empty
     * - New password meets minimum length
     * - New password matches confirmation
     * - New password differs from current
     */
    void on_input_changed();

    /**
     * @brief Clear all password fields securely before closing
     *
     * Overrides Gtk::Dialog::on_response to clear password entries
     * with zeros before dialog closes.
     *
     * @param response_id Dialog response code
     */
    void on_response(int response_id) override;

private:
    /**
     * @brief Securely clear a Gtk::Entry widget
     *
     * Overwrites entry buffer with zeros, then clears widget.
     *
     * @param entry Entry widget to clear
     */
    void secure_clear_entry(Gtk::Entry& entry);

    // Layout containers
    Gtk::Box m_content_box{Gtk::Orientation::VERTICAL, 12};
    Gtk::Box m_warning_box{Gtk::Orientation::HORIZONTAL, 8};
    Gtk::Box m_current_password_box{Gtk::Orientation::VERTICAL, 4};
    Gtk::Box m_new_password_box{Gtk::Orientation::VERTICAL, 4};
    Gtk::Box m_confirm_password_box{Gtk::Orientation::VERTICAL, 4};
    Gtk::Box m_validation_box{Gtk::Orientation::VERTICAL, 4};

    // Labels and messages
    Gtk::Label m_title_label;
    Gtk::Label m_warning_label;
    Gtk::Image m_warning_icon;
    Gtk::Label m_current_password_label{"Current Password:"};
    Gtk::Label m_new_password_label{"New Password:"};
    Gtk::Label m_confirm_password_label{"Confirm New Password:"};
    Gtk::Label m_validation_label;  // Real-time validation feedback
    Gtk::Label m_strength_label;    // Password strength indicator

    // Input fields with eye button boxes
    Gtk::Box m_current_password_entry_box{Gtk::Orientation::HORIZONTAL, 0};
    Gtk::Entry m_current_password_entry;
    Gtk::ToggleButton m_current_password_show_button{"\U0001F441"};
    Gtk::Box m_new_password_entry_box{Gtk::Orientation::HORIZONTAL, 0};
    Gtk::Entry m_new_password_entry;
    Gtk::Box m_confirm_password_entry_box{Gtk::Orientation::HORIZONTAL, 0};
    Gtk::Entry m_confirm_password_entry;

    // YubiKey widgets (conditional compilation)
#ifdef HAVE_YUBIKEY_SUPPORT
    Gtk::Separator m_yubikey_separator{Gtk::Orientation::HORIZONTAL};
    Gtk::Box m_yubikey_pin_box{Gtk::Orientation::VERTICAL, 6};
    Gtk::Label m_yubikey_pin_label{"YubiKey FIDO2 PIN:"};
    Gtk::Box m_yubikey_pin_entry_box{Gtk::Orientation::HORIZONTAL, 0};
    Gtk::Entry m_yubikey_pin_entry;
    Gtk::ToggleButton m_yubikey_pin_show_button{"\U0001F441"};
#endif

    // Dialog buttons (stored for sensitivity control)
    Gtk::Button* m_ok_button{nullptr};
    Gtk::Button* m_cancel_button{nullptr};

    // Configuration
    uint32_t m_min_password_length;
    bool m_is_forced_change;

    // Helper methods
    void update_password_strength();  // Calculate and display password strength
};

#endif // CHANGEPASSWORDDIALOG_H
