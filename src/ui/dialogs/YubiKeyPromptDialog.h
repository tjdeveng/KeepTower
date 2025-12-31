// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file YubiKeyPromptDialog.h
 * @brief User prompts for YubiKey hardware token operations
 *
 * Provides visual feedback during YubiKey operations, guiding users through
 * the challenge-response authentication process.
 *
 * @section usage Usage Example
 * @code
 * YubiKeyPromptDialog dialog(window,
 *                            YubiKeyPromptDialog::PromptType::TOUCH,
 *                            yubikey_serial);
 * dialog.present();
 * // ... perform YubiKey operation ...
 * dialog.hide();
 * @endcode
 */

#ifndef YUBIKEYPROMPTDIALOG_H
#define YUBIKEYPROMPTDIALOG_H

#include <gtkmm.h>

/**
 * @class YubiKeyPromptDialog
 * @brief Non-blocking dialog for YubiKey user prompts
 *
 * Displays appropriate instructions and visual feedback for YubiKey operations.
 * Uses spinner animation to indicate waiting state.
 */
class YubiKeyPromptDialog : public Gtk::Dialog {
public:
    /**
     * @brief Type of YubiKey prompt to display
     */
    enum class PromptType {
        INSERT,  ///< Prompt user to insert YubiKey device
        TOUCH    ///< Prompt user to touch YubiKey button
    };

    /**
     * @brief Construct YubiKey prompt dialog
     * @param parent Parent window for modal display
     * @param type Type of prompt (INSERT or TOUCH)
     * @param serial Optional YubiKey serial number to display
     */
    YubiKeyPromptDialog(Gtk::Window& parent, PromptType type, const std::string& serial = "");

    /**
     * @brief Destructor
     */
    virtual ~YubiKeyPromptDialog() = default;

private:
    /** @brief Setup UI for "insert YubiKey" prompt */
    void setup_insert_prompt(const std::string& serial);

    /** @brief Setup UI for "touch YubiKey" prompt */
    void setup_touch_prompt();

    Gtk::Box m_content_box;      ///< Main content container
    Gtk::Image m_icon;           ///< YubiKey icon
    Gtk::Label m_message_label;  ///< Instruction text
    Gtk::Spinner m_spinner;      ///< Animated waiting indicator
};

#endif // YUBIKEYPROMPTDIALOG_H
