// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file CreatePasswordDialog.h
 * @brief Dialog for creating new vault passwords with strength validation
 *
 * Provides a comprehensive password creation interface with real-time strength
 * feedback, NIST SP 800-63B compliance checking, and optional YubiKey integration.
 *
 * @section features Features
 * - Real-time password strength indicator
 * - NIST SP 800-63B password requirements enforcement
 * - Common password blacklist checking
 * - Password confirmation matching
 * - Optional show/hide password toggle
 * - Optional YubiKey hardware token protection
 * - V2 vault username entry
 *
 * @section security Security Requirements
 * - Minimum 8 characters (NIST guideline)
 * - Must not appear in common password blacklist
 * - Visual strength feedback (weak/moderate/strong)
 * - Passwords cleared from memory after dialog closes
 *
 * @section usage Usage Example
 * @code
 * CreatePasswordDialog dialog(main_window);
 * int response = dialog.run();
 * if (response == Gtk::ResponseType::OK) {
 *     Glib::ustring password = dialog.get_password();
 *     Glib::ustring username = dialog.get_username();
 *     bool use_yubikey = dialog.get_yubikey_enabled();
 *     // ... create vault ...
 * }
 * @endcode
 */

#ifndef CREATEPASSWORDDIALOG_H
#define CREATEPASSWORDDIALOG_H

#include <gtkmm.h>

#ifdef HAVE_YUBIKEY_SUPPORT
#include "../../core/managers/YubiKeyManager.h"
#endif

/**
 * @class CreatePasswordDialog
 * @brief Dialog for creating strong vault passwords
 *
 * Interactive dialog that guides users through creating secure vault passwords.
 * Provides real-time validation, strength feedback, and ensures password quality
 * meets security requirements before vault creation.
 */
class CreatePasswordDialog : public Gtk::Dialog {
public:
    /**
     * @brief Construct password creation dialog
     * @param parent Parent window for modal display
     */
    CreatePasswordDialog(Gtk::Window& parent);

    /**
     * @brief Destructor - securely clears password from memory
     */
    virtual ~CreatePasswordDialog();

    /**
     * @brief Get the created password
     * @return User-entered password after validation
     * @note Only call after dialog returns Gtk::ResponseType::OK
     */
    Glib::ustring get_password() const;

    /**
     * @brief Get the username for V2 vaults
     * @return User-entered username
     * @note Used for V2 multi-user vaults
     */
    Glib::ustring get_username() const;

    /**
     * @brief Check if YubiKey protection was requested
     * @return true if YubiKey checkbox is enabled
     * @note Only meaningful if HAVE_YUBIKEY_SUPPORT is defined
     */
    bool get_yubikey_enabled() const;

    /**
     * @brief Get the YubiKey PIN for FIDO2 authentication
     * @return User-entered PIN for YubiKey
     * @note Only call if get_yubikey_enabled() returns true
     */
    std::string get_yubikey_pin() const;

protected:
    /**
     * @name Signal Handlers
     * @{
     */

    /** @brief Handle show/hide password toggle */
    void on_show_password_toggled();

    /** @brief Handle show/hide PIN toggle */
    void on_show_pin_toggled();

    /** @brief Handle password entry changes - triggers validation */
    void on_password_changed();

    /** @brief Handle confirmation entry changes - checks matching */
    void on_confirm_changed();

    /** @brief Handle username entry changes - triggers validation */
    void on_username_changed();

    /** @brief Validate password and confirmation match */
    void validate_passwords();

    /** @brief Validate all fields (username, password, confirmation) */
    void validate_all_fields();

    /** @brief Handle YubiKey checkbox toggle */
    void on_yubikey_toggled();

    /** @} */

    /**
     * @name Password Validation
     * @{
     */

    /**
     * @brief Validate password against NIST SP 800-63B requirements
     * @param password Password to validate
     * @return true if password meets all requirements
     *
     * Checks:
     * - Minimum length (8 characters)
     * - Not in common password blacklist
     * - Not a simple sequence or pattern
     */
    bool validate_nist_requirements(const Glib::ustring& password);

    /**
     * @brief Update visual password strength indicator
     *
     * Updates the progress bar and strength label based on:
     * - Password length
     * - Character diversity (lowercase, uppercase, digits, symbols)
     * - Pattern detection
     * - Entropy estimation
     */
    void update_strength_indicator();

    /** @} */

    /**
     * @name UI Widgets
     * @{
     */

    Gtk::Box m_content_box;              ///< Main content container
    Gtk::Label m_title_label;            ///< Dialog title
    Gtk::Label m_requirements_label;     ///< Password requirements text

    Gtk::Box m_username_box;             ///< Username input container
    Gtk::Label m_username_label;         ///< Username field label
    Gtk::Entry m_username_entry;         ///< Username text entry
    Gtk::Label m_username_error_label;   ///< Username validation errors

    Gtk::Box m_password_box;             ///< Password input container
    Gtk::Label m_password_label;         ///< Password field label
    Gtk::Box m_password_entry_box;       ///< Container for password entry and toggle button
    Gtk::Entry m_password_entry;         ///< Password text entry (masked)
    Gtk::ToggleButton m_password_show_button;  ///< Show/hide password toggle button

    Gtk::Box m_confirm_box;              ///< Confirmation input container
    Gtk::Label m_confirm_label;          ///< Confirmation field label
    Gtk::Box m_confirm_entry_box;        ///< Container for confirm entry (matches password box width)
    Gtk::Entry m_confirm_entry;          ///< Confirmation text entry (masked)

    Gtk::Label m_strength_label;             ///< Strength text (Weak/Moderate/Strong)
    Gtk::ProgressBar m_strength_bar;         ///< Visual strength indicator
    Gtk::Label m_validation_message;         ///< Error/warning messages

    // YubiKey widgets (conditional compilation)
    Gtk::Separator m_yubikey_separator;      ///< Visual separator before YubiKey option
    Gtk::CheckButton m_yubikey_check;        ///< Enable YubiKey protection checkbox
    Gtk::Label m_yubikey_info_label;         ///< YubiKey usage information
    Gtk::Box m_yubikey_pin_box;              ///< YubiKey PIN input container
    Gtk::Label m_yubikey_pin_label;          ///< YubiKey PIN field label
    Gtk::Box m_yubikey_pin_entry_box;        ///< Container for PIN entry and toggle button
    Gtk::Entry m_yubikey_pin_entry;          ///< YubiKey PIN entry (masked)
    Gtk::ToggleButton m_yubikey_pin_show_button;  ///< Show/hide PIN toggle button

    Gtk::Button* m_ok_button;             ///< OK button (enabled only when valid)
    Gtk::Button* m_cancel_button;         ///< Cancel button

    /** @} */
};

#endif // CREATEPASSWORDDIALOG_H
